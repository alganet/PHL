# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6460/8309 lines (77.75%)

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
|        - |   136 | `/* Uncaught exception code value */` |
|        - |   137 | `#define PH7_EXCEPTION -255` |
|        - |   138 |  |
|        - |   139 | `/*` |
|        - |   140 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   141 | ` */` |
|   908512 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   908514 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   908480 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   908470 |   150 | `	return FALSE;` |
|   454280 |   151 |  |
|        - |   152 | `/*` |
|        - |   153 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   154 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   155 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   156 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   157 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   158 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   159 | ` * still go through the existing numeric coercion.` |
|        - |   160 | ` */` |
|   335184 |   161 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   162 |  |
|        - |   163 | `	SyString sStr;` |
|   335186 |   164 | `	sxu8 bReal = FALSE;` |
|   335186 |   165 | `	const char *zTail = 0;` |
|        - |   166 | `	const char *zEnd;` |
|   335186 |   167 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335128 |   168 | `		return FALSE;` |
|        - |   169 | `	}` |
|       59 |   170 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       59 |   171 | `	if( sStr.nByte == 0 ){` |
|        5 |   172 | `		return TRUE;` |
|        - |   173 | `	}` |
|       55 |   174 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       43 |   175 | `		return TRUE;` |
|        - |   176 | `	}` |
|        - |   177 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   178 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   179 | `	 * still takes the Perl path. */` |
|       13 |   180 | `	zEnd = sStr.zString + sStr.nByte;` |
|       17 |   181 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   182 | `		zTail++;` |
|        1 |   183 | `	}` |
|       13 |   184 | `	return zTail < zEnd;` |
|   167616 |   185 |  |
|        - |   186 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   187 | `/*` |
|        - |   188 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   189 | ` * it can be expanded from the target PHP program.` |
|        - |   190 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   191 | ` * simple and work as follows:` |
|        - |   192 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   193 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   194 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   195 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   196 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   197 | ` * (Windows,Linux,...) and so on.` |
|        - |   198 | ` * Please refer to the official documentation for additional information.` |
|        - |   199 | ` */` |
|   609362 |   200 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   201 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   202 | `	const SyString *pName,  /* Constant name */` |
|        - |   203 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   204 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   205 | `	)` |
|        2 |   206 |  |
|        - |   207 | `	ph7_constant *pCons;` |
|        - |   208 | `	SyHashEntry *pEntry;` |
|        - |   209 | `	char *zDupName;` |
|        - |   210 | `	sxi32 rc;` |
|   609364 |   211 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   609364 |   212 | `	if( pEntry ){` |
|        - |   213 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   214 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   215 | `		pCons->xExpand = xExpand;` |
|        6 |   216 | `		pCons->pUserData = pUserData;` |
|        6 |   217 | `		return SXRET_OK;` |
|        - |   218 | `	}` |
|        - |   219 | `	/* Allocate a new constant instance */` |
|   609360 |   220 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   609360 |   221 | `	if( pCons == 0 ){` |
|      ! 0 |   222 | `		return 0;` |
|        - |   223 | `	}` |
|        - |   224 | `	/* Duplicate constant name */` |
|   609360 |   225 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   609360 |   226 | `	if( zDupName == 0 ){` |
|      ! 0 |   227 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   228 | `		return 0;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Install the constant */` |
|   609360 |   231 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   609360 |   232 | `	pCons->xExpand = xExpand;` |
|   609360 |   233 | `	pCons->pUserData = pUserData;` |
|   609360 |   234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   609360 |   235 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   236 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return rc;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* All done,constant can be invoked from PHP code */` |
|   609360 |   241 | `	return SXRET_OK;` |
|   304683 |   242 |  |
|        - |   243 | `/*` |
|        - |   244 | ` * Allocate a new foreign function instance.` |
|        - |   245 | ` * This function return SXRET_OK on success. Any other` |
|        - |   246 | ` * return value indicates failure.` |
|        - |   247 | ` * Please refer to the official documentation for an introduction to` |
|        - |   248 | ` * the foreign function mechanism.` |
|        - |   249 | ` */` |
|  1353806 |   250 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   251 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   252 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   253 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   254 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   255 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   256 | `	)` |
|        2 |   257 |  |
|        - |   258 | `	ph7_user_func *pFunc;` |
|        - |   259 | `	char *zDup;` |
|        - |   260 | `	/* Allocate a new user function */` |
|  1353808 |   261 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1353808 |   262 | `	if( pFunc == 0 ){` |
|      ! 0 |   263 | `		return SXERR_MEM;` |
|        - |   264 | `	}` |
|        - |   265 | `	/* Duplicate function name */` |
|  1353808 |   266 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1353808 |   267 | `	if( zDup == 0 ){` |
|      ! 0 |   268 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   269 | `		return SXERR_MEM;` |
|        - |   270 | `	}` |
|        - |   271 | `	/* Zero the structure */` |
|  1353808 |   272 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   273 | `	/* Initialize structure fields */` |
|  1353808 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1353808 |   275 | `	pFunc->pVm   = pVm;` |
|  1353808 |   276 | `	pFunc->xFunc = xFunc;` |
|  1353808 |   277 | `	pFunc->pUserData = pUserData;` |
|  1353808 |   278 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   279 | `	/* Write a pointer to the new function */` |
|  1353808 |   280 | `	*ppOut = pFunc;` |
|  1353808 |   281 | `	return SXRET_OK;` |
|   676905 |   282 |  |
|        - |   283 | `/*` |
|        - |   284 | ` * Install a foreign function and it's associated callback so that` |
|        - |   285 | ` * it can be invoked from the target PHP code.` |
|        - |   286 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   287 | ` * return value indicates failure.` |
|        - |   288 | ` * Please refer to the official documentation for an introduction to` |
|        - |   289 | ` * the foreign function mechanism.` |
|        - |   290 | ` */` |
|  1356614 |   291 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   292 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   293 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   294 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   295 | `	void *pUserData           /* Foreign function private data */` |
|        - |   296 | `	)` |
|        2 |   297 |  |
|        - |   298 | `	ph7_user_func *pFunc;` |
|        - |   299 | `	SyHashEntry *pEntry;` |
|        - |   300 | `	sxi32 rc;` |
|        - |   301 | `	/* Overwrite any previously registered function with the same name */` |
|  1356616 |   302 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1356616 |   303 | `	if( pEntry ){` |
|     2810 |   304 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2810 |   305 | `		pFunc->pUserData = pUserData;` |
|     2810 |   306 | `		pFunc->xFunc = xFunc;` |
|     2810 |   307 | `		SySetReset(&pFunc->aAux);` |
|     2810 |   308 | `		return SXRET_OK;` |
|        - |   309 | `	}` |
|        - |   310 | `	/* Create a new user function */` |
|  1353808 |   311 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1353808 |   312 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   313 | `		return rc;` |
|        - |   314 | `	}` |
|        - |   315 | `	/* Install the function in the corresponding hashtable */` |
|  1353808 |   316 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1353808 |   317 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   318 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   319 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   320 | `		return rc;` |
|        - |   321 | `	}` |
|        - |   322 | `	/* User function successfully installed */` |
|  1353808 |   323 | `	return SXRET_OK;` |
|   678309 |   324 |  |
|        - |   325 | `/*` |
|        - |   326 | ` * Initialize a VM function.` |
|        - |   327 | ` */` |
|   273402 |   328 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   329 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   330 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   331 | `	const char *zName,  /* Function name */` |
|        - |   332 | `	sxu32 nByte,        /* zName length */` |
|        - |   333 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   334 | `	void *pUserData     /* Function private data */` |
|        - |   335 | `	)` |
|        2 |   336 |  |
|        - |   337 | `	/* Zero the structure */` |
|   273404 |   338 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   339 | `	/* Initialize structure fields */` |
|        - |   340 | `	/* Arguments container */` |
|   273404 |   341 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   342 | `	/* Static variable container */` |
|   273404 |   343 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   344 | `	/* Bytecode container */` |
|   273404 |   345 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   346 | `    /* Preallocate some instruction slots */` |
|   273404 |   347 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   348 | `	/* Closure environment */` |
|   273404 |   349 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   350 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   273404 |   351 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   273404 |   352 | `	pFunc->iFlags = iFlags;` |
|   273404 |   353 | `	pFunc->pUserData = pUserData;` |
|        - |   354 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   355 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   273404 |   356 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   273404 |   357 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   273404 |   358 | `	return SXRET_OK;` |
|        2 |   359 |  |
|        - |   360 | `/*` |
|        - |   361 | ` * Namespace-aware function lookup.` |
|        - |   362 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   363 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   364 | ` */` |
|        - |   365 | `/*` |
|        - |   366 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   367 | ` */` |
|   754432 |   368 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   369 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   370 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   371 | `	SyString *pName     /* Function name */` |
|        - |   372 | `	)` |
|        2 |   373 |  |
|        - |   374 | `	SyHashEntry *pEntry;` |
|        - |   375 | `	sxi32 rc;` |
|   754434 |   376 | `	if( pName == 0 ){` |
|        - |   377 | `		/* Use the built-in name */` |
|    41556 |   378 | `		pName = &pFunc->sName;` |
|    20777 |   379 | `	}` |
|        - |   380 | `	/* Check for duplicates (functions with the same name) first */` |
|   754434 |   381 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   754434 |   382 | `	if( pEntry ){` |
|   559372 |   383 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   559372 |   384 | `		if( pLink != pFunc ){` |
|        - |   385 | `			/* Link */` |
|      188 |   386 | `			pFunc->pNextName = pLink;` |
|      188 |   387 | `			pEntry->pUserData = pFunc;` |
|       93 |   388 | `		}` |
|   559372 |   389 | `		return SXRET_OK;` |
|        - |   390 | `	}` |
|        - |   391 | `	/* First time seen */` |
|   195064 |   392 | `	pFunc->pNextName = 0;` |
|   195064 |   393 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   195064 |   394 | `	return rc;` |
|   377218 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   398 | ` */` |
|    75908 |   399 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   400 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   401 | `	ph7_class *pClass /* Target Class */` |
|        - |   402 | `	)` |
|        2 |   403 |  |
|    75910 |   404 | `	SyString *pName = &pClass->sName;` |
|        - |   405 | `	SyHashEntry *pEntry;` |
|        - |   406 | `	sxi32 rc;` |
|        - |   407 | `	/* Check for duplicates */` |
|    75910 |   408 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    75910 |   409 | `	if( pEntry ){` |
|       31 |   410 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   411 | `		/* Link entry with the same name */` |
|       31 |   412 | `		pClass->pNextName = pLink;` |
|       31 |   413 | `		pEntry->pUserData = pClass;` |
|       31 |   414 | `		return SXRET_OK;` |
|        - |   415 | `	}` |
|    75880 |   416 | `	pClass->pNextName = 0;` |
|        - |   417 | `	/* Perform a simple hashtable insertion */` |
|    75880 |   418 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    75880 |   419 | `	return rc;` |
|    37956 |   420 |  |
|        - |   421 | `/*` |
|        - |   422 | ` * Instruction builder interface.` |
|        - |   423 | ` */` |
|  4226234 |   424 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   425 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   426 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   427 | `	sxi32 iP1,    /* First operand */` |
|        - |   428 | `	sxu32 iP2,    /* Second operand */` |
|        - |   429 | `	void *p3,     /* Third operand */` |
|        - |   430 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   431 | `	)` |
|        2 |   432 |  |
|        - |   433 | `	VmInstr sInstr;` |
|        - |   434 | `	sxi32 rc;` |
|        - |   435 | `	/* Fill the VM instruction */` |
|  4226236 |   436 | `	sInstr.iOp = (sxu8)iOp;` |
|  4226236 |   437 | `	sInstr.iP1 = iP1;` |
|  4226236 |   438 | `	sInstr.iP2 = iP2;` |
|  4226236 |   439 | `	sInstr.p3  = p3;` |
|  4226236 |   440 | `	if( pIndex ){` |
|        - |   441 | `		/* Instruction index in the bytecode array */` |
|   229606 |   442 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   114802 |   443 | `	}` |
|        - |   444 | `	/* Finally,record the instruction */` |
|  4226236 |   445 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4226236 |   446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   447 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   448 | `		/* Fall throw */` |
|      ! 0 |   449 | `	}` |
|  4226236 |   450 | `	return rc;` |
|        2 |   451 |  |
|        - |   452 | `/*` |
|        - |   453 | ` * Swap the current bytecode container with the given one.` |
|        - |   454 | ` */` |
|   549080 |   455 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   456 |  |
|   549082 |   457 | `	if( pContainer == 0 ){` |
|        - |   458 | `		/* Point to the default container */` |
|      ! 0 |   459 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   460 | `	}else{` |
|        - |   461 | `		/* Change container */` |
|   549082 |   462 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   463 | `	}` |
|   549082 |   464 | `	return SXRET_OK;` |
|        2 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Return the current bytecode container.` |
|        - |   468 | ` */` |
|   274540 |   469 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   470 |  |
|   274542 |   471 | `	return pVm->pByteContainer;` |
|        2 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   475 | ` */` |
|   226406 |   476 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *pInstr;` |
|   226408 |   479 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   226408 |   480 | `	return pInstr;` |
|        2 |   481 |  |
|        - |   482 | `/*` |
|        - |   483 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   484 | ` */` |
|  1270658 |   485 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   486 |  |
|  1270660 |   487 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   488 |  |
|        - |   489 | `/*` |
|        - |   490 | ` * Pop the last VM instruction.` |
|        - |   491 | ` */` |
|   209472 |   492 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   493 |  |
|   209474 |   494 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   495 |  |
|        - |   496 | `/*` |
|        - |   497 | ` * Peek the last VM instruction.` |
|        - |   498 | ` */` |
|   832784 |   499 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   500 |  |
|   832786 |   501 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   502 |  |
|    33108 |   503 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   504 |  |
|        - |   505 | `	VmInstr *aInstr;` |
|        - |   506 | `	sxu32 n;` |
|    33110 |   507 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33110 |   508 | `	if( n < 2 ){` |
|      ! 0 |   509 | `		return 0;` |
|        - |   510 | `	}` |
|    33110 |   511 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33110 |   512 | `	return &aInstr[n - 2];` |
|    16556 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Allocate a new virtual machine frame.` |
|        - |   516 | ` */` |
|    21742 |   517 | `static VmFrame * VmNewFrame(` |
|        - |   518 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   519 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	)` |
|        2 |   522 |  |
|        - |   523 | `	VmFrame *pFrame;` |
|        - |   524 | `	/* Allocate a new vm frame */` |
|    21744 |   525 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    21744 |   526 | `	if( pFrame == 0 ){` |
|      ! 0 |   527 | `		return 0;` |
|        - |   528 | `	}` |
|        - |   529 | `	/* Zero the structure */` |
|    21744 |   530 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   531 | `	/* Initialize frame fields */` |
|    21744 |   532 | `	pFrame->pUserData = pUserData;` |
|    21744 |   533 | `	pFrame->pThis = pThis;` |
|    21744 |   534 | `	pFrame->pVm = pVm;` |
|    21744 |   535 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    21744 |   536 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    21744 |   537 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    21744 |   538 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    21744 |   539 | `	return pFrame;` |
|    10873 |   540 |  |
|        - |   541 | `/* Forward declaration */` |
|        - |   542 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   543 | `/*` |
|        - |   544 | ` * Enter a VM frame.` |
|        - |   545 | ` */` |
|    21696 |   546 | `static sxi32 VmEnterFrame(` |
|        - |   547 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   548 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   549 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   550 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   551 | `	)` |
|        2 |   552 |  |
|        - |   553 | `	VmFrame *pFrame;` |
|        - |   554 | `	/* Allocate a new frame */` |
|    21698 |   555 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    21698 |   556 | `	if( pFrame == 0 ){` |
|      ! 0 |   557 | `		return SXERR_MEM;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the list of active VM frame */` |
|    21698 |   560 | `	pFrame->pParent = pVm->pFrame;` |
|    21698 |   561 | `	pVm->pFrame = pFrame;` |
|    21698 |   562 | `	if( ppFrame ){` |
|        - |   563 | `		/* Write a pointer to the new VM frame */` |
|    18576 |   564 | `		*ppFrame = pFrame;` |
|     9287 |   565 | `	}` |
|    21698 |   566 | `	return SXRET_OK;` |
|    10850 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   570 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   571 | ` * information.` |
|        - |   572 | ` */` |
|       58 |   573 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   574 |  |
|        - |   575 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   576 | `	SyHashEntry *pEntry = 0;` |
|        - |   577 | `	sxi32 rc;` |
|        - |   578 | `	/* Point to the upper frame */` |
|       60 |   579 | `	pFrame = pVm->pFrame;` |
|       60 |   580 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   581 | `	pTarget = pFrame;` |
|       60 |   582 | `	pFrame = pTarget->pParent;` |
|       60 |   583 | `	while( pFrame ){` |
|       60 |   584 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   585 | `			/* Query the current frame */` |
|       60 |   586 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   587 | `			if( pEntry ){` |
|        - |   588 | `				/* Variable found */` |
|       60 |   589 | `				break;` |
|        - |   590 | `			}` |
|      ! 0 |   591 | `		}` |
|        - |   592 | `		/* Point to the upper frame */` |
|      ! 0 |   593 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   594 | `	}` |
|       60 |   595 | `	if( pEntry == 0 ){` |
|        - |   596 | `		/* Inexistant variable */` |
|      ! 0 |   597 | `		return SXERR_NOTFOUND;` |
|        - |   598 | `	}` |
|        - |   599 | `	/* Link to the current frame */` |
|       60 |   600 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   601 | `	if( rc == SXRET_OK ){` |
|        - |   602 | `		sxu32 nIdx;` |
|       60 |   603 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   604 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   605 | `	}` |
|       60 |   606 | `	return rc;` |
|       31 |   607 |  |
|        - |   608 | `/*` |
|        - |   609 | ` * Leave the top-most active frame.` |
|        - |   610 | ` */` |
|    18564 |   611 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   612 |  |
|    18566 |   613 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    18566 |   614 | `	if( pCurFrame ){` |
|        - |   615 | `		/* Unlink from the list of active VM frame */` |
|    18566 |   616 | `		pVm->pFrame = pCurFrame->pParent;` |
|    18566 |   617 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   618 | `			VmSlot  *aSlot;` |
|        - |   619 | `			sxu32 n;` |
|        - |   620 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18240 |   621 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   121398 |   622 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   623 | `				/* Unset the local variable */` |
|   103160 |   624 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    51581 |   625 | `			}` |
|        - |   626 | `			/* Remove local reference */` |
|    18240 |   627 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   121460 |   628 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   103222 |   629 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    51612 |   630 | `			}` |
|     9119 |   631 | `		}` |
|        - |   632 | `		/* Release internal containers */` |
|    18566 |   633 | `		SyHashRelease(&pCurFrame->hVar);` |
|    18566 |   634 | `		SySetRelease(&pCurFrame->sArg);` |
|    18566 |   635 | `		SySetRelease(&pCurFrame->sLocal);` |
|    18566 |   636 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   637 | `		/* Release the whole structure */` |
|    18566 |   638 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9282 |   639 | `	}` |
|    18566 |   640 |  |
|        - |   641 | `/*` |
|        - |   642 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   643 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   644 | ` * should be skipped when looking for the real execution context.` |
|        - |   645 | ` */` |
|  7024824 |   646 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   647 |  |
|  7026908 |   648 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2084 |   649 | `		pFrame = pFrame->pParent;` |
|        2 |   650 | `	}` |
|  7024826 |   651 | `	return pFrame;` |
|        2 |   652 |  |
|        - |   653 | `/*` |
|        - |   654 | ` * Compare two functions signature and return the comparison result.` |
|        - |   655 | ` */` |
|      836 |   656 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   657 |  |
|      837 |   658 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   659 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   660 | `	const char *zSin = pSecond->zString;` |
|      837 |   661 | `	const char *zFin = pFirst->zString;` |
|      837 |   662 | `	const char *zPtr = zFin;` |
|      421 |   663 | `	for(;;){` |
|      843 |   664 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   665 | `			break;` |
|        - |   666 | `		}` |
|       19 |   667 | `		if( zFin[0] != zSin[0] ){` |
|        - |   668 | `			/* mismatch */` |
|       13 |   669 | `			break;` |
|        - |   670 | `		}` |
|        7 |   671 | `		zFin++;` |
|        7 |   672 | `		zSin++;` |
|        1 |   673 | `	}` |
|      837 |   674 | `	return (int)(zFin-zPtr);` |
|        1 |   675 |  |
|        - |   676 | `/*` |
|        - |   677 | ` * Select the appropriate VM function for the current call context.` |
|        - |   678 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   679 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   680 | ` * Refer to the official documentation for more information.` |
|        - |   681 | ` */` |
|      138 |   682 | `static ph7_vm_func * VmOverload(` |
|        - |   683 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   684 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   685 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   686 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   690 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   691 | `	ph7_vm_func *pLink;` |
|        - |   692 | `	SyString sArgSig;` |
|        - |   693 | `	SyBlob sSig;` |
|        - |   694 |  |
|      140 |   695 | `	pLink = pList;` |
|      140 |   696 | `	i = 0;` |
|        - |   697 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   698 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   699 | `		if( pLink == 0 ){` |
|       78 |   700 | `			break;` |
|        - |   701 | `		}` |
|      948 |   702 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   703 | `			/* Candidate for overloading */` |
|      902 |   704 | `			apSet[i++] = pLink;` |
|      450 |   705 | `		}` |
|        - |   706 | `		/* Point to the next entry */` |
|      948 |   707 | `		pLink = pLink->pNextName;` |
|        2 |   708 | `	}` |
|      140 |   709 | `	if( i < 1 ){` |
|        - |   710 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   711 | `		return pList;` |
|        - |   712 | `	}` |
|      140 |   713 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   714 | `		/* Return the only candidate */` |
|       32 |   715 | `		return apSet[0];` |
|        - |   716 | `	}` |
|        - |   717 | `	/* Calculate function signature */` |
|      109 |   718 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   719 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   720 | `		int c = 'n'; /* null */` |
|      259 |   721 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   722 | `			/* Hashmap */` |
|       45 |   723 | `			c = 'h';` |
|      237 |   724 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   725 | `			/* bool */` |
|      ! 0 |   726 | `			c = 'b';` |
|      215 |   727 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   728 | `			/* int */` |
|        7 |   729 | `			c = 'i';` |
|      212 |   730 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   731 | `			/* String */` |
|      107 |   732 | `			c = 's';` |
|      156 |   733 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   734 | `			/* Float */` |
|      ! 0 |   735 | `			c = 'f';` |
|      103 |   736 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   737 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   738 | `			int marker = 'o';` |
|        3 |   739 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   740 | `			SyString *pName = &pClass->sName;` |
|        3 |   741 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   742 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   743 | `			c = -1;` |
|        1 |   744 | `		}` |
|      259 |   745 | `		if( c > 0 ){` |
|      257 |   746 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   747 | `		}` |
|      130 |   748 | `	}` |
|      109 |   749 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   750 | `	iTarget = 0;` |
|      109 |   751 | `	iMax = -1;` |
|        - |   752 | `	/* Select the appropriate function */` |
|      945 |   753 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   754 | `		/* Compare the two signatures */` |
|      837 |   755 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   756 | `		if( iCur > iMax ){` |
|      113 |   757 | `			iMax = iCur;` |
|      113 |   758 | `			iTarget = j;` |
|       56 |   759 | `		}` |
|      419 |   760 | `	}` |
|      109 |   761 | `	SyBlobRelease(&sSig);` |
|        - |   762 | `	/* Appropriate function for the current call context */` |
|      109 |   763 | `	return apSet[iTarget];` |
|       71 |   764 |  |
|        - |   765 | `/* Forward declaration */` |
|        - |   766 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   767 | `/*` |
|        - |   768 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   769 | ` * it can be instanciated from the executed PHP script.` |
|        - |   770 | ` */` |
|   256806 |   771 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   772 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   773 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   774 | `	)` |
|        2 |   775 |  |
|        - |   776 | `	ph7_class_method *pMeth;` |
|        - |   777 | `	ph7_class_attr *pAttr;` |
|        - |   778 | `	SyHashEntry *pEntry;` |
|        - |   779 | `	sxi32 rc;` |
|        - |   780 | `	/* Reset the loop cursor */` |
|   256808 |   781 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   782 | `	/* Process only static and constant attribute */` |
|   786127 |   783 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   784 | `		/* Extract the current attribute */` |
|   400918 |   785 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   400918 |   786 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   787 | `			ph7_value *pMemObj;` |
|        - |   788 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1792 |   789 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1792 |   790 | `			if( pMemObj == 0 ){` |
|      ! 0 |   791 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   792 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   793 | `					&pClass->sName,&pAttr->sName` |
|        - |   794 | `					);` |
|      ! 0 |   795 | `				return SXERR_MEM;` |
|        - |   796 | `			}` |
|     1792 |   797 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   798 | `				/* Initialize attribute default value (any complex expression) */` |
|     1788 |   799 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      893 |   800 | `			}` |
|        - |   801 | `			/* Record attribute index */` |
|     1792 |   802 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   803 | `			/* Install static attribute in the reference table */` |
|     1792 |   804 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   805 | `			/* If this is a typed static property, register the slot so the` |
|        - |   806 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   807 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   808 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1792 |   809 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   810 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   811 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   812 | `					return SXERR_MEM;` |
|        - |   813 | `				}` |
|       10 |   814 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   815 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   816 | `				pVmAttrS->iState = 0;` |
|       10 |   817 | `				pVmAttrS->pOwner = pClass;` |
|        - |   818 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   819 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   820 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   821 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   822 | `				}` |
|       10 |   823 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   824 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   825 | `					return SXERR_MEM;` |
|        - |   826 | `				}` |
|        4 |   827 | `			}` |
|      895 |   828 | `		}` |
|        2 |   829 | `	}` |
|        - |   830 | `	/* Install class methods */` |
|   256808 |   831 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   832 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   833 | `		 */` |
|   172968 |   834 | `		return SXRET_OK;` |
|        - |   835 | `	}` |
|        - |   836 | `	/* Create constructor alias if not yet done */` |
|    83842 |   837 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   838 | `		/* User constructor with the same base class name */` |
|     6612 |   839 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6612 |   840 | `		if( pEntry ){` |
|      ! 0 |   841 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   842 | `			/* Create the alias */` |
|      ! 0 |   843 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   844 | `		}` |
|     3305 |   845 | `	}` |
|        - |   846 | `	/* Install the methods now */` |
|    83842 |   847 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   838648 |   848 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   712888 |   849 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   712888 |   850 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   712880 |   851 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   712880 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				return rc;` |
|        - |   854 | `			}` |
|   356439 |   855 | `		}` |
|        2 |   856 | `	}` |
|        - |   857 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    83842 |   858 | `	pClass->bMounted = TRUE;` |
|    83842 |   859 | `	return SXRET_OK;` |
|   128405 |   860 |  |
|        - |   861 | `/*` |
|        - |   862 | ` * Allocate a private frame for attributes of the given` |
|        - |   863 | ` * class instance (Object in the PHP jargon).` |
|        - |   864 | ` */` |
|     2000 |   865 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   866 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   867 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   868 | `	)` |
|        2 |   869 |  |
|     2002 |   870 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   871 | `	ph7_class_attr *pAttr;` |
|        - |   872 | `	SyHashEntry *pEntry;` |
|        - |   873 | `	sxi32 rc;` |
|        - |   874 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2002 |   875 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8216 |   876 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   877 | `		VmClassAttr *pVmAttr;` |
|        - |   878 | `		/* Extract the current attribute */` |
|     6216 |   879 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6216 |   880 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6216 |   881 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   882 | `			return SXERR_MEM;` |
|        - |   883 | `		}` |
|     6216 |   884 | `		pVmAttr->pAttr = pAttr;` |
|     6216 |   885 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   886 | `			ph7_value *pMemObj;` |
|        - |   887 | `			/* Reserve a memory object for this attribute */` |
|     6192 |   888 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6192 |   889 | `			if( pMemObj == 0 ){` |
|      ! 0 |   890 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   891 | `				return SXERR_MEM;` |
|        - |   892 | `			}` |
|     6192 |   893 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6192 |   894 | `			pVmAttr->iState = 0;` |
|     6192 |   895 | `			pVmAttr->pOwner = pClass;` |
|     6192 |   896 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   897 | `				/* Initialize attribute default value (any complex expression) */` |
|     2136 |   898 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5125 |   899 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   900 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   901 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   902 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   903 | `			}` |
|     6192 |   904 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6192 |   905 | `			if( rc != SXRET_OK ){` |
|        - |   906 | `				VmSlot sSlot;` |
|        - |   907 | `				/* Restore memory object */` |
|      ! 0 |   908 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   909 | `				sSlot.pUserData = 0;` |
|      ! 0 |   910 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   911 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   912 | `				return SXERR_MEM;` |
|        - |   913 | `			}` |
|        - |   914 | `			/* Install attribute in the reference table */` |
|     6192 |   915 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   916 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   917 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   918 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6192 |   919 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      170 |   920 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      170 |   921 | `				if( rc != SXRET_OK ){` |
|        - |   922 | `					VmSlot sSlot;` |
|      ! 0 |   923 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   924 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   925 | `					sSlot.pUserData = 0;` |
|      ! 0 |   926 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   927 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   928 | `					return SXERR_MEM;` |
|        - |   929 | `				}` |
|       84 |   930 | `			}` |
|     3097 |   931 | `		}else{` |
|        - |   932 | `			/* Install static/constant attribute */` |
|       26 |   933 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   934 | `			pVmAttr->iState = 0;` |
|       26 |   935 | `			pVmAttr->pOwner = pClass;` |
|       26 |   936 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   937 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   938 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   939 | `				return SXERR_MEM;` |
|        - |   940 | `			}` |
|        - |   941 | `		}` |
|        2 |   942 | `	}` |
|     2002 |   943 | `	return SXRET_OK;` |
|     1002 |   944 |  |
|        - |   945 | `/* Forward declaration */` |
|        - |   946 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   947 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   948 | `/*` |
|        - |   949 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   950 | ` */` |
|        - |   951 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   952 | `/*` |
|        - |   953 | ` * Reserve a constant memory object.` |
|        - |   954 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   955 | ` */` |
|   451732 |   956 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   957 |  |
|        - |   958 | `	ph7_value *pObj;` |
|        - |   959 | `	sxi32 rc;` |
|   451734 |   960 | `	if( pIndex ){` |
|        - |   961 | `		/* Object index in the object table */` |
|   442368 |   962 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   221183 |   963 | `	}` |
|        - |   964 | `	/* Reserve a slot for the new object */` |
|   451734 |   965 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   451734 |   966 | `	if( rc != SXRET_OK ){` |
|        - |   967 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   968 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   969 | `		 */` |
|      ! 0 |   970 | `		return 0;` |
|        - |   971 | `	}` |
|   451734 |   972 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   451734 |   973 | `	return pObj;` |
|   225868 |   974 |  |
|        - |   975 | `/*` |
|        - |   976 | ` * Reserve a memory object.` |
|        - |   977 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   978 | ` */` |
|  2151338 |   979 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   980 |  |
|        - |   981 | `	ph7_value *pObj;` |
|        - |   982 | `	sxi32 rc;` |
|  2151340 |   983 | `	if( pIndex ){` |
|        - |   984 | `		/* Object index in the object table */` |
|  2151340 |   985 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075669 |   986 | `	}` |
|        - |   987 | `	/* Reserve a slot for the new object */` |
|  2151340 |   988 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151340 |   989 | `	if( rc != SXRET_OK ){` |
|        - |   990 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   991 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   992 | `		 */` |
|      ! 0 |   993 | `		return 0;` |
|        - |   994 | `	}` |
|  2151340 |   995 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151340 |   996 | `	return pObj;` |
|  1075671 |   997 |  |
|        - |   998 | `/* Forward declaration */` |
|        - |   999 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1000 | `/* Forward declarations for Fiber C functions */` |
|        - |  1001 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1002 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1003 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1004 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1005 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1006 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1007 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1008 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1009 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1010 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1011 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1012 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1013 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1014 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1015 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1016 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1017 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1018 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1019 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1020 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1021 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1022 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1023 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1024 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1025 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1026 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1027 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1028 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1029 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1030 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1031 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1032 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1033 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1034 | `/*` |
|        - |  1035 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1036 | ` * directly as foreign functions.` |
|        - |  1037 | ` */` |
|        - |  1038 | `#define PH7_BUILTIN_LIB \` |
|        - |  1039 | `	"interface Throwable {"\` |
|        - |  1040 | `	"public function getMessage();"\` |
|        - |  1041 | `	"public function getCode();"\` |
|        - |  1042 | `	"public function getFile();"\` |
|        - |  1043 | `	"public function getLine();"\` |
|        - |  1044 | `	"public function getTrace();"\` |
|        - |  1045 | `	"public function getTraceAsString();"\` |
|        - |  1046 | `	"public function getPrevious();"\` |
|        - |  1047 | `	"public function __toString();"\` |
|        - |  1048 | `	"}"\` |
|        - |  1049 | `	"interface Traversable {}"\` |
|        - |  1050 | `	"interface ArrayAccess {"\` |
|        - |  1051 | `	"public function offsetExists($offset);"\` |
|        - |  1052 | `	"public function offsetGet($offset);"\` |
|        - |  1053 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1054 | `	"public function offsetUnset($offset);"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"interface Countable {"\` |
|        - |  1057 | `	"public function count();"\` |
|        - |  1058 | `	"}"\` |
|        - |  1059 | `	"interface Stringable {"\` |
|        - |  1060 | `	"public function __toString();"\` |
|        - |  1061 | `	"}"\` |
|        - |  1062 | `	"interface UnitEnum {"\` |
|        - |  1063 | `	"public static function cases();"\` |
|        - |  1064 | `	"}"\` |
|        - |  1065 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1066 | `	"public static function from($value);"\` |
|        - |  1067 | `	"public static function tryFrom($value);"\` |
|        - |  1068 | `	"}"\` |
|        - |  1069 | `	"class Exception implements Throwable { "\` |
|        - |  1070 | `    "protected $message = '';"\` |
|        - |  1071 | `    "protected $code = 0;"\` |
|        - |  1072 | `    "protected $file;"\` |
|        - |  1073 | `    "protected $line;"\` |
|        - |  1074 | `    "protected $trace;"\` |
|        - |  1075 | `    "protected $previous;"\` |
|        - |  1076 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1077 | `	"   if( isset($message) ){"\` |
|        - |  1078 | `	"	  $this->message = $message;"\` |
|        - |  1079 | `	"   }"\` |
|        - |  1080 | `	"   $this->code = $code;"\` |
|        - |  1081 | `	"   $this->file = __FILE__;"\` |
|        - |  1082 | `	"   $this->line = __LINE__;"\` |
|        - |  1083 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1084 | `	"   if( isset($previous) ){"\` |
|        - |  1085 | `	"     $this->previous = $previous;"\` |
|        - |  1086 | `	"   }"\` |
|        - |  1087 | `	"}"\` |
|        - |  1088 | `	"public function getMessage(){"\` |
|        - |  1089 | `	"   return $this->message;"\` |
|        - |  1090 | `	"}"\` |
|        - |  1091 | `	" public function getCode(){"\` |
|        - |  1092 | `	"  return $this->code;"\` |
|        - |  1093 | `	"}"\` |
|        - |  1094 | `	"public function getFile(){"\` |
|        - |  1095 | `	"  return $this->file;"\` |
|        - |  1096 | `	"}"\` |
|        - |  1097 | `	"public function getLine(){"\` |
|        - |  1098 | `	"  return $this->line;"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"public function getTrace(){"\` |
|        - |  1101 | `	"   return $this->trace;"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"public function getTraceAsString(){"\` |
|        - |  1104 | `	"  return debug_string_backtrace();"\` |
|        - |  1105 | `	"}"\` |
|        - |  1106 | `	"public function getPrevious(){"\` |
|        - |  1107 | `	"    return $this->previous;"\` |
|        - |  1108 | `	"}"\` |
|        - |  1109 | `	"public function __toString(){"\` |
|        - |  1110 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1111 | `    "}"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"class Error implements Throwable { "\` |
|        - |  1114 | `    "protected $message = '';"\` |
|        - |  1115 | `    "protected $code = 0;"\` |
|        - |  1116 | `    "protected $file;"\` |
|        - |  1117 | `    "protected $line;"\` |
|        - |  1118 | `    "protected $trace;"\` |
|        - |  1119 | `    "protected $previous;"\` |
|        - |  1120 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1121 | `	"   if( isset($message) ){"\` |
|        - |  1122 | `	"	  $this->message = $message;"\` |
|        - |  1123 | `	"   }"\` |
|        - |  1124 | `	"   $this->code = $code;"\` |
|        - |  1125 | `	"   $this->file = __FILE__;"\` |
|        - |  1126 | `	"   $this->line = __LINE__;"\` |
|        - |  1127 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1128 | `	"   if( isset($previous) ){"\` |
|        - |  1129 | `	"     $this->previous = $previous;"\` |
|        - |  1130 | `	"   }"\` |
|        - |  1131 | `	"}"\` |
|        - |  1132 | `	"public function getMessage(){"\` |
|        - |  1133 | `	"   return $this->message;"\` |
|        - |  1134 | `	"}"\` |
|        - |  1135 | `	"public function getCode(){"\` |
|        - |  1136 | `	"  return $this->code;"\` |
|        - |  1137 | `	"}"\` |
|        - |  1138 | `	"public function getFile(){"\` |
|        - |  1139 | `	"  return $this->file;"\` |
|        - |  1140 | `	"}"\` |
|        - |  1141 | `	"public function getLine(){"\` |
|        - |  1142 | `	"  return $this->line;"\` |
|        - |  1143 | `	"}"\` |
|        - |  1144 | `	"public function getTrace(){"\` |
|        - |  1145 | `	"   return $this->trace;"\` |
|        - |  1146 | `	"}"\` |
|        - |  1147 | `	"public function getTraceAsString(){"\` |
|        - |  1148 | `	"  return debug_string_backtrace();"\` |
|        - |  1149 | `	"}"\` |
|        - |  1150 | `	"public function getPrevious(){"\` |
|        - |  1151 | `	"    return $this->previous;"\` |
|        - |  1152 | `	"}"\` |
|        - |  1153 | `	"public function __toString(){"\` |
|        - |  1154 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1155 | `	"}"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"class TypeError extends Error { }"\` |
|        - |  1158 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1159 | `	"class ValueError extends Error { }"\` |
|        - |  1160 | `	"class FiberError extends Error { }"\` |
|        - |  1161 | `	"class AssertionError extends Error { }"\` |
|        - |  1162 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1163 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1164 | `	"class ErrorException extends Exception { "\` |
|        - |  1165 | `	"protected $severity;"\` |
|        - |  1166 | `	"public function __construct(string $message = null,"\` |
|        - |  1167 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1168 | `	"   if( isset($message) ){"\` |
|        - |  1169 | `	"	  $this->message = $message;"\` |
|        - |  1170 | `	"   }"\` |
|        - |  1171 | `	"   $this->severity = $severity;"\` |
|        - |  1172 | `	"   $this->code = $code;"\` |
|        - |  1173 | `	"   $this->file = $filename;"\` |
|        - |  1174 | `	"   $this->line = $lineno;"\` |
|        - |  1175 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1176 | `	"   if( isset($previous) ){"\` |
|        - |  1177 | `	"     $this->previous = $previous;"\` |
|        - |  1178 | `	"   }"\` |
|        - |  1179 | `	"}"\` |
|        - |  1180 | `	"public function getSeverity(){"\` |
|        - |  1181 | `	"   return $this->severity;"\` |
|        - |  1182 | `    "}"\` |
|        - |  1183 | `	"}"\` |
|        - |  1184 | `	"interface Iterator extends Traversable {"\` |
|        - |  1185 | `	"public function current();"\` |
|        - |  1186 | `	"public function key();"\` |
|        - |  1187 | `	"public function next();"\` |
|        - |  1188 | `	"public function rewind();"\` |
|        - |  1189 | `	"public function valid();"\` |
|        - |  1190 | `	"}"\` |
|        - |  1191 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1192 | `	"public function getIterator();"\` |
|        - |  1193 | `	"}"\` |
|        - |  1194 | `	"interface Serializable {"\` |
|        - |  1195 | `	"public function serialize();"\` |
|        - |  1196 | `	"public function unserialize(string $serialized);"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"/* Directory releated IO */"\` |
|        - |  1199 | `	"class Directory {"\` |
|        - |  1200 | `	"public $handle = null;"\` |
|        - |  1201 | `	"public $path  = null;"\` |
|        - |  1202 | `	"public function __construct(string $path)"\` |
|        - |  1203 | `	"{"\` |
|        - |  1204 | `	"   $this->handle = opendir($path);"\` |
|        - |  1205 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1206 | `	"      $this->path = $path;"\` |
|        - |  1207 | `	"   }"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"public function __destruct()"\` |
|        - |  1210 | `	"{"\` |
|        - |  1211 | `	"  if( $this->handle != null ){"\` |
|        - |  1212 | `	"       closedir($this->handle);"\` |
|        - |  1213 | `	"  }"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `	"public function read()"\` |
|        - |  1216 | `	"{"\` |
|        - |  1217 | `	"    return readdir($this->handle);"\` |
|        - |  1218 | `	"}"\` |
|        - |  1219 | `	"public function rewind()"\` |
|        - |  1220 | `	"{"\` |
|        - |  1221 | `	"    rewinddir($this->handle);"\` |
|        - |  1222 | `	"}"\` |
|        - |  1223 | `	"public function close()"\` |
|        - |  1224 | `	"{"\` |
|        - |  1225 | `	"    closedir($this->handle);"\` |
|        - |  1226 | `	"    $this->handle = null;"\` |
|        - |  1227 | `	"}"\` |
|        - |  1228 | `	"}"\` |
|        - |  1229 | `	"class Fiber {"\` |
|        - |  1230 | `	"  private $__ctx;"\` |
|        - |  1231 | `	"  private $__callable;"\` |
|        - |  1232 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1233 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1234 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1235 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1236 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1237 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1238 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1239 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1240 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1241 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1242 | `	"}"\` |
|        - |  1243 | `	"class Generator implements Iterator {"\` |
|        - |  1244 | `	"  private $__ctx;"\` |
|        - |  1245 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1246 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1247 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1248 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1249 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1250 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1251 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1252 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1253 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1254 | `	"}"\` |
|        - |  1255 | `	"class stdClass{"\` |
|        - |  1256 | `	"  public $value;"\` |
|        - |  1257 | `	" /* Magic methods */"\` |
|        - |  1258 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1259 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1260 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1261 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1262 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1263 | `	"}"\` |
|        - |  1264 | `	"function dir(string $path){"\` |
|        - |  1265 | `	"   return new Directory($path);"\` |
|        - |  1266 | `	"}"\` |
|        - |  1267 | `	"function Dir(string $path){"\` |
|        - |  1268 | `	"   return new Directory($path);"\` |
|        - |  1269 | `	"}"\` |
|        - |  1270 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1271 | `    "{"\` |
|        - |  1272 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1273 | `	"  $aDir = array();"\` |
|        - |  1274 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1275 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1276 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1277 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1278 | `	"   }"\` |
|        - |  1279 | `	"  closedir($pHandle);"\` |
|        - |  1280 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1281 | `	"      rsort($aDir);"\` |
|        - |  1282 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1283 | `	"      sort($aDir);"\` |
|        - |  1284 | `	"  }"\` |
|        - |  1285 | `	"  return $aDir;"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1288 | `	"/* Open the target directory */"\` |
|        - |  1289 | `	"$zDir = dirname($pattern);"\` |
|        - |  1290 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1291 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1292 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1293 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1294 | `	"	return FALSE;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"$pattern = basename($pattern);"\` |
|        - |  1297 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1298 | `	"/* Loop throw available entries */"\` |
|        - |  1299 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1300 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1301 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1302 | `	"	if( $rc ){"\` |
|        - |  1303 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1304 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1305 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1306 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1307 | `	"		  }"\` |
|        - |  1308 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1309 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1310 | `	"		 continue;"\` |
|        - |  1311 | `	"	   }"\` |
|        - |  1312 | `	"	   /* Add the entry */"\` |
|        - |  1313 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1314 | `	"	}"\` |
|        - |  1315 | `	" }"\` |
|        - |  1316 | `	"/* Close the handle */"\` |
|        - |  1317 | `	"closedir($pHandle);"\` |
|        - |  1318 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1319 | `	"  /* Sort the array */"\` |
|        - |  1320 | `	"  sort($pArray);"\` |
|        - |  1321 | `	"}"\` |
|        - |  1322 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1323 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1324 | `	"  $pArray[] = $pattern;"\` |
|        - |  1325 | `	"}"\` |
|        - |  1326 | `	"/* Return the created array */"\` |
|        - |  1327 | `	"return $pArray;"\` |
|        - |  1328 | `   "}"\` |
|        - |  1329 | `   "/* Creates a temporary file */"\` |
|        - |  1330 | `   "function tmpfile(){"\` |
|        - |  1331 | `   "  /* Extract the temp directory */"\` |
|        - |  1332 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1333 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1334 | `   "    /* Use the current dir */"\` |
|        - |  1335 | `   "    $zTempDir = '.';"\` |
|        - |  1336 | `   "  }"\` |
|        - |  1337 | `   "  /* Create the file */"\` |
|        - |  1338 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1339 | `   "  return $pHandle;"\` |
|        - |  1340 | `   "}"\` |
|        - |  1341 | `   "/* Creates a temporary filename */"\` |
|        - |  1342 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1343 | `   "{"\` |
|        - |  1344 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1345 | `   "}"\` |
|        - |  1346 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1347 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1348 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1349 | `   "/* Copy arguments */"\` |
|        - |  1350 | `   "$nArgs = func_num_args();"\` |
|        - |  1351 | `   "$pNew = array();"\` |
|        - |  1352 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1353 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1354 | `    "}"\` |
|        - |  1355 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1356 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1357 | `	"/* Erase */"\` |
|        - |  1358 | `	"array_erase($pArray);"\` |
|        - |  1359 | `	"/* Unshift */"\` |
|        - |  1360 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1361 | `	"return sizeof($pArray);"\` |
|        - |  1362 | `    "}"\` |
|        - |  1363 | `	"function array_merge_recursive(){"\` |
|        - |  1364 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1365 | `    "$arrays = func_get_args();"\` |
|        - |  1366 | `    "$narrays = count($arrays);"\` |
|        - |  1367 | `    "$ret = array();"\` |
|        - |  1368 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1369 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1370 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1371 | `	 " }"\` |
|        - |  1372 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1373 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1374 | `     "  if( $keyIsInt ) {"\` |
|        - |  1375 | `     "   $ret[] = $value;"\` |
|        - |  1376 | `     "  } else {"\` |
|        - |  1377 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1378 | `     "    $cur = $ret[$key];"\` |
|        - |  1379 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1380 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1381 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1382 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1383 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1384 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1385 | `     "    } else {"\` |
|        - |  1386 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1387 | `     "    }"\` |
|        - |  1388 | `     "   } else {"\` |
|        - |  1389 | `     "    $ret[$key] = $value;"\` |
|        - |  1390 | `     "   }"\` |
|        - |  1391 | `     "  }"\` |
|        - |  1392 | `     " }"\` |
|        - |  1393 | `	 " }"\` |
|        - |  1394 | `	 " return $ret;"\` |
|        - |  1395 | `    "}"\` |
|        - |  1396 | `	"function max(){"\` |
|        - |  1397 | `    "  $pArgs = func_get_args();"\` |
|        - |  1398 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1399 | `	"  return null;"\` |
|        - |  1400 | `    " }"\` |
|        - |  1401 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1402 | `    " $pArg = $pArgs[0];"\` |
|        - |  1403 | `	" if( !is_array($pArg) ){"\` |
|        - |  1404 | `	"   return $pArg; "\` |
|        - |  1405 | `	" }"\` |
|        - |  1406 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1407 | `	"   return null;"\` |
|        - |  1408 | `	" }"\` |
|        - |  1409 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1410 | `	" reset($pArg);"\` |
|        - |  1411 | `	" $max = current($pArg);"\` |
|        - |  1412 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1413 | `	"   if( $val > $max ){"\` |
|        - |  1414 | `	"     $max = $val;"\` |
|        - |  1415 | `    " }"\` |
|        - |  1416 | `	" }"\` |
|        - |  1417 | `	" return $max;"\` |
|        - |  1418 | `    " }"\` |
|        - |  1419 | `    " $max = $pArgs[0];"\` |
|        - |  1420 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1421 | `    " $val = $pArgs[$i];"\` |
|        - |  1422 | `	"if( $val > $max ){"\` |
|        - |  1423 | `	" $max = $val;"\` |
|        - |  1424 | `	"}"\` |
|        - |  1425 | `    " }"\` |
|        - |  1426 | `	" return $max;"\` |
|        - |  1427 | `    "}"\` |
|        - |  1428 | `	"function min(){"\` |
|        - |  1429 | `    "  $pArgs = func_get_args();"\` |
|        - |  1430 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1431 | `	"  return null;"\` |
|        - |  1432 | `    " }"\` |
|        - |  1433 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1434 | `    " $pArg = $pArgs[0];"\` |
|        - |  1435 | `	" if( !is_array($pArg) ){"\` |
|        - |  1436 | `	"   return $pArg; "\` |
|        - |  1437 | `	" }"\` |
|        - |  1438 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1439 | `	"   return null;"\` |
|        - |  1440 | `	" }"\` |
|        - |  1441 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1442 | `	" reset($pArg);"\` |
|        - |  1443 | `	" $min = current($pArg);"\` |
|        - |  1444 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1445 | `	"   if( $val < $min ){"\` |
|        - |  1446 | `	"     $min = $val;"\` |
|        - |  1447 | `    " }"\` |
|        - |  1448 | `	" }"\` |
|        - |  1449 | `	" return $min;"\` |
|        - |  1450 | `    " }"\` |
|        - |  1451 | `    " $min = $pArgs[0];"\` |
|        - |  1452 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1453 | `    " $val = $pArgs[$i];"\` |
|        - |  1454 | `	"if( $val < $min ){"\` |
|        - |  1455 | `	" $min = $val;"\` |
|        - |  1456 | `	" }"\` |
|        - |  1457 | `    " }"\` |
|        - |  1458 | `	" return $min;"\` |
|        - |  1459 | `	"}"\` |
|        - |  1460 | `	"function fileowner(string $file){"\` |
|        - |  1461 | `    " $a = stat($file);"\` |
|        - |  1462 | `	" if( !is_array($a) ){"\` |
|        - |  1463 | `	"	return false;"\` |
|        - |  1464 | `	" }"\` |
|        - |  1465 | `	" return $a['uid'];"\` |
|        - |  1466 | `    "}"\` |
|        - |  1467 | `    "function filegroup(string $file){"\` |
|        - |  1468 | `	" $a = stat($file);"\` |
|        - |  1469 | `	" if( !is_array($a) ){"\` |
|        - |  1470 | `	"	return false;"\` |
|        - |  1471 | `	" }"\` |
|        - |  1472 | `	" return $a['gid'];"\` |
|        - |  1473 | `    "}"\` |
|        - |  1474 | `	 "function fileinode(string $file){"\` |
|        - |  1475 | `	" $a = stat($file);"\` |
|        - |  1476 | `	" if( !is_array($a) ){"\` |
|        - |  1477 | `	"	return false;"\` |
|        - |  1478 | `	" }"\` |
|        - |  1479 | `	" return $a['ino'];"\` |
|        - |  1480 | `    "}"` |
|        - |  1481 |  |
|        - |  1482 | `/*` |
|        - |  1483 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1484 | ` * start compiling the target PHP program.` |
|        - |  1485 | ` */` |
|     3122 |  1486 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1487 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1488 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1489 | `	 )` |
|        2 |  1490 |  |
|        - |  1491 | `	SyString sBuiltin;` |
|        - |  1492 | `	ph7_value *pObj;` |
|        - |  1493 | `	sxi32 rc;` |
|        - |  1494 | `	/* Zero the structure */` |
|     3124 |  1495 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1496 | `	/* Initialize VM fields */` |
|     3124 |  1497 | `	pVm->pEngine = &(*pEngine);` |
|     3124 |  1498 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1499 | `	/* Instructions containers */` |
|     3124 |  1500 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3124 |  1501 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3124 |  1502 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1503 | `	/* Object containers */` |
|     3124 |  1504 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3124 |  1505 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1506 | `	/* Virtual machine internal containers */` |
|     3124 |  1507 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3124 |  1508 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3124 |  1509 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3124 |  1510 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3124 |  1511 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3124 |  1512 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3124 |  1513 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3124 |  1514 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3124 |  1515 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3124 |  1516 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3124 |  1517 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3124 |  1518 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3124 |  1519 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3124 |  1520 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3124 |  1521 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3124 |  1522 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3124 |  1523 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3124 |  1524 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3124 |  1525 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3124 |  1526 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3124 |  1527 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3124 |  1528 | `	pVm->pPendingException = 0;` |
|        - |  1529 | `	/* Configuration containers */` |
|     3124 |  1530 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1531 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1532 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1533 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3124 |  1534 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3124 |  1535 | `	pVm->iResponseStatus = 200;` |
|     3124 |  1536 | `	pVm->bHeadersSent = 0;` |
|     3124 |  1537 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1538 | `	/* Error callbacks containers */` |
|     3124 |  1539 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3124 |  1540 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3124 |  1541 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3124 |  1542 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3124 |  1543 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1544 | `	/* Set a default recursion limit */` |
|        - |  1545 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3124 |  1546 | `	pVm->nMaxDepth = 32;` |
|        - |  1547 | `#else` |
|        - |  1548 | `	pVm->nMaxDepth = 16;` |
|        - |  1549 | `#endif` |
|        - |  1550 | `	/* Default assertion flags */` |
|     3124 |  1551 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1552 | `	/* JSON return status */` |
|     3124 |  1553 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1554 | `	/* PRNG context */` |
|     3124 |  1555 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1556 | `	/* Install the null constant */` |
|     3124 |  1557 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1558 | `	if( pObj == 0 ){` |
|      ! 0 |  1559 | `		rc = SXERR_MEM;` |
|      ! 0 |  1560 | `		goto Err;` |
|        - |  1561 | `	}` |
|     3124 |  1562 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1563 | `	/* Install the boolean TRUE constant */` |
|     3124 |  1564 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1565 | `	if( pObj == 0 ){` |
|      ! 0 |  1566 | `		rc = SXERR_MEM;` |
|      ! 0 |  1567 | `		goto Err;` |
|        - |  1568 | `	}` |
|     3124 |  1569 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1570 | `	/* Install the boolean FALSE constant */` |
|     3124 |  1571 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1572 | `	if( pObj == 0 ){` |
|      ! 0 |  1573 | `		rc = SXERR_MEM;` |
|      ! 0 |  1574 | `		goto Err;` |
|        - |  1575 | `	}` |
|     3124 |  1576 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1577 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1578 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1579 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3124 |  1580 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3124 |  1581 | `	if( pObj == 0 ){` |
|      ! 0 |  1582 | `		rc = SXERR_MEM;` |
|      ! 0 |  1583 | `		goto Err;` |
|        - |  1584 | `	}` |
|     3124 |  1585 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1586 | `	/* Create the global frame */` |
|     3124 |  1587 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3124 |  1588 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1589 | `		goto Err;` |
|        - |  1590 | `	}` |
|        - |  1591 | `	/* Initialize the code generator */` |
|     3124 |  1592 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3124 |  1593 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1594 | `		goto Err;` |
|        - |  1595 | `	}` |
|        - |  1596 | `	/* VM correctly initialized,set the magic number */` |
|     3124 |  1597 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3124 |  1598 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1599 | `	/* Compile the built-in library */` |
|     3124 |  1600 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1601 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3124 |  1602 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1603 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3124 |  1604 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3124 |  1605 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3124 |  1606 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|        - |  1607 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3124 |  1608 | `	pVm->pCoalesceObj = 0;` |
|     3124 |  1609 | `	pVm->bCoalesceArmed = 0;` |
|     3124 |  1610 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1611 | `	/* Register Fiber internal C functions */` |
|     3124 |  1612 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3124 |  1613 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3124 |  1614 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3124 |  1615 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3124 |  1616 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3124 |  1617 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3124 |  1618 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3124 |  1619 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3124 |  1620 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3124 |  1621 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1622 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3124 |  1623 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3124 |  1624 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3124 |  1625 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3124 |  1626 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3124 |  1627 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3124 |  1628 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3124 |  1629 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3124 |  1630 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3124 |  1631 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3124 |  1632 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1633 | `	/* Reset the code generator */` |
|     3124 |  1634 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3124 |  1635 | `	return SXRET_OK;` |
|      ! 0 |  1636 | `Err:` |
|      ! 0 |  1637 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1638 | `	return rc;` |
|     1563 |  1639 |  |
|        - |  1640 | `/*` |
|        - |  1641 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1642 | ` * routine which store the output in an internal blob.` |
|        - |  1643 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1644 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1645 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1646 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1647 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1648 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1649 | ` * to finish executing and extracting the output.` |
|        - |  1650 | ` */` |
|       38 |  1651 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1652 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1653 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1654 | `	void *pUserData     /* User private data */` |
|        - |  1655 | `	)` |
|      ! 0 |  1656 |  |
|        - |  1657 | `	 sxi32 rc;` |
|        - |  1658 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1659 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1660 | `	 return rc;` |
|      ! 0 |  1661 |  |
|        - |  1662 | `/*` |
|        - |  1663 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1664 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1665 | ` */` |
|    19158 |  1666 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1667 |  |
|    19160 |  1668 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    19160 |  1669 | `	if( xCons != VmObConsumer ){` |
|     8064 |  1670 | `		pVm->nOutputLen += nLen;` |
|     8064 |  1671 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1010 |  1672 | `			pVm->bHeadersSent = 1;` |
|      504 |  1673 | `		}` |
|     4031 |  1674 | `	}` |
|    19160 |  1675 |  |
|        - |  1676 | `#define VM_STACK_GUARD 16` |
|        - |  1677 | `/*` |
|        - |  1678 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1679 | ` * our compiled PHP program.` |
|        - |  1680 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1681 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1682 | ` */` |
|    43612 |  1683 | `static ph7_value * VmNewOperandStack(` |
|        - |  1684 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1685 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1686 | `	)` |
|        2 |  1687 |  |
|        - |  1688 | `	ph7_value *pStack;` |
|        - |  1689 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1690 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1691 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1692 | `  ** on the maximum stack depth required.` |
|        - |  1693 | `  **` |
|        - |  1694 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1695 | `  */` |
|    43614 |  1696 | `	nInstr += VM_STACK_GUARD;` |
|    43614 |  1697 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    43614 |  1698 | `	if( pStack == 0 ){` |
|      ! 0 |  1699 | `		return 0;` |
|        - |  1700 | `	}` |
|        - |  1701 | `	/* Initialize the operand stack */` |
|  2976140 |  1702 | `	while( nInstr > 0 ){` |
|  2932528 |  1703 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2932528 |  1704 | `		--nInstr;` |
|        2 |  1705 | `	}` |
|        - |  1706 | `	/* Ready for bytecode execution */` |
|    43614 |  1707 | `	return pStack;` |
|    21808 |  1708 |  |
|        - |  1709 | `/* Forward declaration */` |
|        - |  1710 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1711 | `/*` |
|        - |  1712 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1713 | ` * This routine gets called by the PH7 engine after` |
|        - |  1714 | ` * successful compilation of the target PHP program.` |
|        - |  1715 | ` */` |
|     2808 |  1716 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1717 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	sxi32 rc;` |
|     2810 |  1722 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1723 | `		/* Initialize your VM first */` |
|      ! 0 |  1724 | `		return SXERR_CORRUPT;` |
|        - |  1725 | `	}` |
|        - |  1726 | `	/* Mark the VM ready for byte-code execution */` |
|     2810 |  1727 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1728 | `	/* Release the code generator now we have compiled our program */` |
|     2810 |  1729 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1730 | `	/* Emit the DONE instruction */` |
|     2810 |  1731 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2810 |  1732 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1733 | `		return SXERR_MEM;` |
|        - |  1734 | `	}` |
|        - |  1735 | `	/* Script return value */` |
|     2810 |  1736 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1737 | `	/* Allocate a new operand stack */` |
|     2810 |  1738 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2810 |  1739 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1740 | `		return SXERR_MEM;` |
|        - |  1741 | `	}` |
|        - |  1742 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1743 | `	 * private data. */` |
|     2810 |  1744 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2810 |  1745 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1746 | `	/* Allocate the reference table */` |
|     2810 |  1747 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2810 |  1748 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2810 |  1749 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1750 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1751 | `		return SXERR_MEM;` |
|        - |  1752 | `	}` |
|        - |  1753 | `	/* Zero the reference table */` |
|     2810 |  1754 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1755 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2810 |  1756 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2810 |  1757 | `	if( rc != SXRET_OK ){` |
|        - |  1758 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1759 | `		return rc;` |
|        - |  1760 | `	}` |
|        - |  1761 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2810 |  1762 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2810 |  1763 | `	if( rc != SXRET_OK ){` |
|        - |  1764 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1765 | `		return rc;` |
|        - |  1766 | `	}` |
|        - |  1767 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2810 |  1768 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1769 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2810 |  1770 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1771 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2810 |  1772 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1773 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1774 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2810 |  1775 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2810 |  1776 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1777 | `#endif` |
|        - |  1778 | `	/* Initialize and install static and constants class attributes */` |
|     2810 |  1779 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    70546 |  1780 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    67738 |  1781 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    67738 |  1782 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1783 | `			return rc;` |
|        - |  1784 | `		}` |
|        2 |  1785 | `	}` |
|        - |  1786 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2810 |  1787 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1788 | `	/* VM is ready for bytecode execution */` |
|     2810 |  1789 | `	return SXRET_OK;` |
|     1406 |  1790 |  |
|        - |  1791 | `/*` |
|        - |  1792 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1793 | ` */` |
|      ! 0 |  1794 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1795 |  |
|      ! 0 |  1796 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1797 | `		return SXERR_CORRUPT;` |
|        - |  1798 | `	}` |
|        - |  1799 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1800 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1801 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1802 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1803 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1804 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1805 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1806 | `	pVm->bHttpContext = 0;` |
|        - |  1807 | `	/* Set the ready flag */` |
|      ! 0 |  1808 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1809 | `	return SXRET_OK;` |
|      ! 0 |  1810 |  |
|        - |  1811 | `/*` |
|        - |  1812 | ` * Release a Virtual Machine.` |
|        - |  1813 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1814 | ` */` |
|     2800 |  1815 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1816 |  |
|        - |  1817 | `	/* Set the stale magic number */` |
|     2802 |  1818 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1819 | `	/* Release the private memory subsystem */` |
|     2802 |  1820 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2802 |  1821 | `	return SXRET_OK;` |
|        2 |  1822 |  |
|        - |  1823 | `/*` |
|        - |  1824 | ` * Initialize a foreign function call context.` |
|        - |  1825 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1826 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1827 | ` * functions.` |
|        - |  1828 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1829 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1830 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1831 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1832 | ` */` |
|   683168 |  1833 | `static sxi32 VmInitCallContext(` |
|        - |  1834 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1835 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1836 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1837 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1838 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1839 | `	)` |
|        2 |  1840 |  |
|   683170 |  1841 | `	pOut->pFunc = pFunc;` |
|   683170 |  1842 | `	pOut->pVm   = pVm;` |
|   683170 |  1843 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   683170 |  1844 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1845 | `	/* Assume a null return value */` |
|   683170 |  1846 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   683170 |  1847 | `	pOut->pRet = pRet;` |
|   683170 |  1848 | `	pOut->iFlags = iFlags;` |
|   683170 |  1849 | `	return SXRET_OK;` |
|        2 |  1850 |  |
|        - |  1851 | `/*` |
|        - |  1852 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1853 | ` * left behind.` |
|        - |  1854 | ` */` |
|   683168 |  1855 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1856 |  |
|        - |  1857 | `	sxu32 n;` |
|   683170 |  1858 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8360 |  1859 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    24382 |  1860 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16024 |  1861 | `			if( apObj[n] == 0 ){` |
|        - |  1862 | `				/* Already released */` |
|      318 |  1863 | `				continue;` |
|        - |  1864 | `			}` |
|    15708 |  1865 | `			PH7_MemObjRelease(apObj[n]);` |
|    15708 |  1866 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7855 |  1867 | `		}` |
|     8360 |  1868 | `		SySetRelease(&pCtx->sVar);` |
|     4179 |  1869 | `	}` |
|   683170 |  1870 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1871 | `		ph7_aux_data *aAux;` |
|        - |  1872 | `		void *pChunk;` |
|        - |  1873 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1874 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1875 | `		 */` |
|        9 |  1876 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1877 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1878 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1879 | `			/* Release the chunk */` |
|       25 |  1880 | `			if( pChunk ){` |
|       25 |  1881 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1882 | `			}` |
|       13 |  1883 | `		}` |
|        9 |  1884 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1885 | `	}` |
|   683170 |  1886 |  |
|        - |  1887 | `/*` |
|        - |  1888 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1889 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1890 | ` */` |
|      316 |  1891 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1892 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1893 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1894 | `	)` |
|        2 |  1895 |  |
|      318 |  1896 | `	if( pValue == 0 ){` |
|        - |  1897 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1898 | `		return;` |
|        - |  1899 | `	}` |
|      318 |  1900 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1901 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1902 | `		sxu32 n;` |
|     1116 |  1903 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1904 | `			if( apObj[n] == pValue ){` |
|      318 |  1905 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1906 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1907 | `				/* Mark as released */` |
|      318 |  1908 | `				apObj[n] = 0;` |
|      318 |  1909 | `				break;` |
|        - |  1910 | `			}` |
|      401 |  1911 | `		}` |
|      158 |  1912 | `	}` |
|      160 |  1913 |  |
|        - |  1914 | `/*` |
|        - |  1915 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1916 | ` */` |
|  3891244 |  1917 | `static void VmPopOperand(` |
|        - |  1918 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1919 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1920 | `	)` |
|        2 |  1921 |  |
|  3891246 |  1922 | `	ph7_value *pTos = *ppTos;` |
|  8285944 |  1923 | `	while( nPop > 0 ){` |
|  4394700 |  1924 | `		PH7_MemObjRelease(pTos);` |
|  4394700 |  1925 | `		pTos--;` |
|  4394700 |  1926 | `		nPop--;` |
|        2 |  1927 | `	}` |
|        - |  1928 | `	/* Top of the stack */` |
|  3891246 |  1929 | `	*ppTos = pTos;` |
|  3891246 |  1930 |  |
|        - |  1931 | `/*` |
|        - |  1932 | ` * Reserve a memory object.` |
|        - |  1933 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1934 | ` */` |
|  3172090 |  1935 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1936 |  |
|  3172092 |  1937 | `	ph7_value *pObj = 0;` |
|        - |  1938 | `	VmSlot *pSlot;` |
|        - |  1939 | `	sxu32 nIdx;` |
|        - |  1940 | `	/* Check for a free slot */` |
|  3172092 |  1941 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3172092 |  1942 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3172092 |  1943 | `	if( pSlot ){` |
|  1020754 |  1944 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1020754 |  1945 | `		nIdx = pSlot->nIdx;` |
|   510376 |  1946 | `	}` |
|  3172092 |  1947 | `	if( pObj == 0 ){` |
|        - |  1948 | `		/* Reserve a new memory object */` |
|  2151340 |  1949 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151340 |  1950 | `		if( pObj == 0 ){` |
|      ! 0 |  1951 | `			return 0;` |
|        - |  1952 | `		}` |
|  1075669 |  1953 | `	}` |
|        - |  1954 | `	/* Set a null default value */` |
|  3172092 |  1955 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3172092 |  1956 | `	pObj->nIdx = nIdx;` |
|  3172092 |  1957 | `	return pObj;` |
|  1586047 |  1958 |  |
|        - |  1959 | `/*` |
|        - |  1960 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1961 | ` */` |
|    34990 |  1962 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1963 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1964 | `	const char *zKey,  /* Entry key */` |
|        - |  1965 | `	sxu32 nByte,       /* Key length */` |
|        - |  1966 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1967 | `	)` |
|        2 |  1968 |  |
|        - |  1969 | `	ph7_value sKey;` |
|        - |  1970 | `	sxi32 rc;` |
|    34992 |  1971 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    34992 |  1972 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1973 | `	/* Perform the insertion */` |
|    34992 |  1974 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    34992 |  1975 | `	PH7_MemObjRelease(&sKey);` |
|    34992 |  1976 | `	return rc;` |
|        2 |  1977 |  |
|        - |  1978 | `/*` |
|        - |  1979 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1980 | ` * Return a pointer to the variable value on success.` |
|        - |  1981 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1982 | ` */` |
|  3617540 |  1983 | `static ph7_value * VmExtractMemObj(` |
|        - |  1984 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1985 | `	const SyString *pName, /* Variable name */` |
|        - |  1986 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1987 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1988 | `	)` |
|        2 |  1989 |  |
|  3617542 |  1990 | `	int bNullify = FALSE;` |
|        - |  1991 | `	SyHashEntry *pEntry;` |
|        - |  1992 | `	VmFrame *pFrame;` |
|        - |  1993 | `	ph7_value *pObj;` |
|        - |  1994 | `	sxu32 nIdx;` |
|        - |  1995 | `	sxi32 rc;` |
|        - |  1996 | `	/* Point to the top active frame */` |
|  3617542 |  1997 | `	pFrame = pVm->pFrame;` |
|  3617542 |  1998 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1999 | `	/* Perform the lookup */` |
|  3617542 |  2000 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2001 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2002 | `		pName = &sAnnon;` |
|        - |  2003 | `		/* Always nullify the object */` |
|      ! 0 |  2004 | `		bNullify = TRUE;` |
|      ! 0 |  2005 | `		bDup = FALSE;` |
|      ! 0 |  2006 | `	}` |
|        - |  2007 | `	/* Check the superglobals table first */` |
|  3617542 |  2008 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3617542 |  2009 | `	if( pEntry == 0 ){` |
|        - |  2010 | `		/* Query the top active frame */` |
|  3617502 |  2011 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3617502 |  2012 | `		if( pEntry == 0 ){` |
|   111018 |  2013 | `			char *zName = (char *)pName->zString;` |
|        - |  2014 | `			VmSlot sLocal;` |
|   111018 |  2015 | `			if( !bCreate ){` |
|        - |  2016 | `				/* Do not create the variable,return NULL instead */` |
|      930 |  2017 | `				return 0;` |
|        - |  2018 | `			}` |
|        - |  2019 | `			/* No such variable,automatically create a new one and install` |
|        - |  2020 | `			 * it in the current frame.` |
|        - |  2021 | `			 */` |
|   110090 |  2022 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   110090 |  2023 | `			if( pObj == 0 ){` |
|      ! 0 |  2024 | `				return 0;` |
|        - |  2025 | `			}` |
|   110090 |  2026 | `			nIdx = pObj->nIdx;` |
|   110090 |  2027 | `			if( bDup ){` |
|        - |  2028 | `				/* Duplicate name */` |
|      196 |  2029 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      196 |  2030 | `				if( zName == 0 ){` |
|      ! 0 |  2031 | `					return 0;` |
|        - |  2032 | `				}` |
|       97 |  2033 | `			}` |
|        - |  2034 | `			/* Link to the top active VM frame */` |
|   110090 |  2035 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   110090 |  2036 | `			if( rc != SXRET_OK ){` |
|        - |  2037 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2038 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2039 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2040 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2041 | `				return 0;` |
|        - |  2042 | `			}` |
|   110090 |  2043 | `			if( pFrame->pParent != 0 ){` |
|        - |  2044 | `				/* Local variable */` |
|   103208 |  2045 | `				sLocal.nIdx = nIdx;` |
|   103208 |  2046 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    51605 |  2047 | `			}else{` |
|        - |  2048 | `				/* Register in the $GLOBALS array */` |
|     6884 |  2049 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2050 | `			}` |
|        - |  2051 | `			/* Install in the reference table */` |
|   110090 |  2052 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2053 | `			/* Save object index */` |
|   110090 |  2054 | `			pObj->nIdx = nIdx;` |
|    55046 |  2055 | `		}else{` |
|        - |  2056 | `			/* Extract variable contents */` |
|  3506486 |  2057 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3506486 |  2058 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3506486 |  2059 | `			if( bNullify && pObj ){` |
|      ! 0 |  2060 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2061 | `			}` |
|        - |  2062 | `		}` |
|  1808398 |  2063 | `	}else{` |
|        - |  2064 | `		/* Superglobal */` |
|       42 |  2065 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2066 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2067 | `	}` |
|  3616614 |  2068 | `	return pObj;` |
|  1808882 |  2069 |  |
|        - |  2070 | `/*` |
|        - |  2071 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2072 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2073 | ` */` |
|     3112 |  2074 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2075 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2076 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2077 | `	sxu32 nByte        /* zName length */` |
|        - |  2078 | `	)` |
|        2 |  2079 |  |
|        - |  2080 | `	SyHashEntry *pEntry;` |
|        - |  2081 | `	ph7_value *pValue;` |
|        - |  2082 | `	sxu32 nIdx;` |
|        - |  2083 | `	/* Query the superglobal table */` |
|     3114 |  2084 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3114 |  2085 | `	if( pEntry == 0 ){` |
|        - |  2086 | `		/* No such entry */` |
|      ! 0 |  2087 | `		return 0;` |
|        - |  2088 | `	}` |
|        - |  2089 | `	/* Extract the superglobal index in the global object pool */` |
|     3114 |  2090 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2091 | `	/* Extract the variable value  */` |
|     3114 |  2092 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3114 |  2093 | `	return pValue;` |
|     1558 |  2094 |  |
|        - |  2095 | `/*` |
|        - |  2096 | ` * Perform a raw hashmap insertion.` |
|        - |  2097 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2098 | ` */` |
|     3142 |  2099 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2100 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2101 | `	const char *zKey,   /* Entry key */` |
|        - |  2102 | `	int nKeylen,        /* zKey length*/` |
|        - |  2103 | `	const char *zData,  /* Entry data */` |
|        - |  2104 | `	int nLen            /* zData length */` |
|        - |  2105 | `	)` |
|        2 |  2106 |  |
|        - |  2107 | `	ph7_value sKey,sValue;` |
|        - |  2108 | `	sxi32 rc;` |
|     3144 |  2109 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3144 |  2110 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3144 |  2111 | `	if( zKey ){` |
|     3122 |  2112 | `		if( nKeylen < 0 ){` |
|     3070 |  2113 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1534 |  2114 | `		}` |
|     3122 |  2115 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1560 |  2116 | `	}` |
|     3144 |  2117 | `	if( zData ){` |
|     3144 |  2118 | `		if( nLen < 0 ){` |
|        - |  2119 | `			/* Compute length automatically */` |
|      144 |  2120 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2121 | `		}` |
|     3144 |  2122 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1571 |  2123 | `	}` |
|        - |  2124 | `	/* Perform the insertion */` |
|     3144 |  2125 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3144 |  2126 | `	PH7_MemObjRelease(&sKey);` |
|     3144 |  2127 | `	PH7_MemObjRelease(&sValue);` |
|     3144 |  2128 | `	return rc;` |
|        2 |  2129 |  |
|        - |  2130 | `/*` |
|        - |  2131 | ` * Configure a working virtual machine instance.` |
|        - |  2132 | ` *` |
|        - |  2133 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2134 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2135 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2136 | ` * The second argument to this function is an integer configuration option` |
|        - |  2137 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2138 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2139 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2140 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2141 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2142 | ` */` |
|    45258 |  2143 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2144 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2145 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2146 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2147 | `	)` |
|        2 |  2148 |  |
|    45260 |  2149 | `	sxi32 rc = SXRET_OK;` |
|    45260 |  2150 | `	switch(nOp){` |
|     1396 |  2151 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2794 |  2152 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2794 |  2153 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2154 | `		/* VM output consumer callback */` |
|        - |  2155 | `#ifdef UNTRUST` |
|        - |  2156 | `		if( xConsumer == 0 ){` |
|        - |  2157 | `			rc = SXERR_CORRUPT;` |
|        - |  2158 | `			break;` |
|        - |  2159 | `		}` |
|        - |  2160 | `#endif` |
|        - |  2161 | `		/* Install the output consumer */` |
|     2794 |  2162 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2794 |  2163 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2794 |  2164 | `		break;` |
|        - |  2165 | `							   }` |
|     1404 |  2166 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2167 | `		/* Import path */` |
|        - |  2168 | `		  const char *zPath;` |
|        - |  2169 | `		  SyString sPath;` |
|     2810 |  2170 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2171 | `#if defined(UNTRUST)` |
|        - |  2172 | `		  if( zPath == 0 ){` |
|        - |  2173 | `			  rc = SXERR_EMPTY;` |
|        - |  2174 | `			  break;` |
|        - |  2175 | `		  }` |
|        - |  2176 | `#endif` |
|     2810 |  2177 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2178 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2179 | `#ifdef __WINNT__` |
|        2 |  2180 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2181 | `#endif` |
|     5618 |  2182 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2183 | `		  /* Remove leading and trailing white spaces */` |
|     2810 |  2184 | `		  SyStringFullTrim(&sPath);` |
|     2810 |  2185 | `		  if( sPath.nByte > 0 ){` |
|        - |  2186 | `			  /* Store the path in the corresponding conatiner */` |
|     2810 |  2187 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1404 |  2188 | `		  }` |
|     2810 |  2189 | `		  break;` |
|        - |  2190 | `									 }` |
|     1404 |  2191 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2192 | `		/* Run-Time Error report */` |
|     2810 |  2193 | `		pVm->bErrReport = 1;` |
|     2810 |  2194 | `		break;` |
|      ! 0 |  2195 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2196 | `		/* Recursion depth */` |
|      ! 0 |  2197 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2198 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2199 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2200 | `		}` |
|      ! 0 |  2201 | `		break;` |
|        - |  2202 | `									   }` |
|      ! 0 |  2203 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2204 | `		/* VM output length in bytes */` |
|      ! 0 |  2205 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2206 | `#ifdef UNTRUST` |
|        - |  2207 | `		if( pOut == 0 ){` |
|        - |  2208 | `			rc = SXERR_CORRUPT;` |
|        - |  2209 | `			break;` |
|        - |  2210 | `		}` |
|        - |  2211 | `#endif` |
|      ! 0 |  2212 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2213 | `		break;` |
|        - |  2214 | `							   }` |
|        - |  2215 |  |
|    14040 |  2216 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2217 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2218 | `		/* Create a new superglobal/global variable */` |
|    28082 |  2219 | `		const char *zName = va_arg(ap,const char *);` |
|    28082 |  2220 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2221 | `		SyHashEntry *pEntry;` |
|        - |  2222 | `		ph7_value *pObj;` |
|        - |  2223 | `		sxu32 nByte;` |
|        - |  2224 | `		sxu32 nIdx;` |
|        - |  2225 | `#ifdef UNTRUST` |
|        - |  2226 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2227 | `			rc = SXERR_CORRUPT;` |
|        - |  2228 | `			break;` |
|        - |  2229 | `		}` |
|        - |  2230 | `#endif` |
|    28082 |  2231 | `		nByte = SyStrlen(zName);` |
|    28082 |  2232 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2233 | `			/* Check if the superglobal is already installed */` |
|    28082 |  2234 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14042 |  2235 | `		}else{` |
|        - |  2236 | `			/* Query the top active VM frame */` |
|      ! 0 |  2237 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2238 | `		}` |
|    28082 |  2239 | `		if( pEntry ){` |
|        - |  2240 | `			/* Variable already installed */` |
|      ! 0 |  2241 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2242 | `			/* Extract contents */` |
|      ! 0 |  2243 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2244 | `			if( pObj ){` |
|        - |  2245 | `				/* Overwrite old contents */` |
|      ! 0 |  2246 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2247 | `			}` |
|      ! 0 |  2248 | `		}else{` |
|        - |  2249 | `			/* Install a new variable */` |
|    28082 |  2250 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28082 |  2251 | `			if( pObj == 0 ){` |
|      ! 0 |  2252 | `				rc = SXERR_MEM;` |
|      ! 0 |  2253 | `				break;` |
|        - |  2254 | `			}` |
|    28082 |  2255 | `			nIdx = pObj->nIdx;` |
|        - |  2256 | `			/* Copy value */` |
|    28082 |  2257 | `			PH7_MemObjStore(pValue,pObj);` |
|    28082 |  2258 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2259 | `				/* Install the superglobal */` |
|    28082 |  2260 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14042 |  2261 | `			}else{` |
|        - |  2262 | `				/* Install in the current frame */` |
|      ! 0 |  2263 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2264 | `			}` |
|    28082 |  2265 | `			if( rc == SXRET_OK ){` |
|        - |  2266 | `				SyHashEntry *pRef;` |
|    28082 |  2267 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28082 |  2268 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14042 |  2269 | `				}else{` |
|      ! 0 |  2270 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2271 | `				}` |
|        - |  2272 | `				/* Install in the reference table */` |
|    28082 |  2273 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28082 |  2274 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2275 | `					/* Register in the $GLOBALS array */` |
|    28082 |  2276 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14040 |  2277 | `				}` |
|    14040 |  2278 | `			}` |
|        - |  2279 | `		}` |
|    28082 |  2280 | `		break;` |
|        - |  2281 | `									}` |
|     1534 |  2282 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2283 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2284 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2285 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2286 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2287 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2288 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3070 |  2289 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3070 |  2290 | `		const char *zValue = va_arg(ap,const char *);` |
|     3070 |  2291 | `		int nLen = va_arg(ap,int);` |
|        - |  2292 | `		ph7_hashmap *pMap;` |
|        - |  2293 | `		ph7_value *pValue;` |
|     3070 |  2294 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2295 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2296 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3069 |  2297 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2298 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2299 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3068 |  2300 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2301 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2302 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3068 |  2303 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2304 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2305 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3068 |  2306 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2307 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2308 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3068 |  2309 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2310 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2311 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2312 | `		}else{` |
|        - |  2313 | `			/* Extract the $_SERVER superglobal */` |
|     3068 |  2314 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2315 | `		}` |
|     3070 |  2316 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2317 | `			/* No such entry */` |
|      ! 0 |  2318 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2319 | `			break;` |
|        - |  2320 | `		}` |
|        - |  2321 | `		/* Point to the hashmap */` |
|     3070 |  2322 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2323 | `		/* Perform the insertion */` |
|     3070 |  2324 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3070 |  2325 | `		break;` |
|        - |  2326 | `								   }` |
|       11 |  2327 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2328 | `		/* Script arguments */` |
|       24 |  2329 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2330 | `		ph7_hashmap *pMap;` |
|        - |  2331 | `		ph7_value *pValue;` |
|        - |  2332 | `		sxu32 n;` |
|       24 |  2333 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2334 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2335 | `			break;` |
|        - |  2336 | `		}` |
|        - |  2337 | `		/* Extract the $argv array */` |
|       24 |  2338 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2339 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2340 | `			/* No such entry */` |
|      ! 0 |  2341 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2342 | `			break;` |
|        - |  2343 | `		}` |
|        - |  2344 | `		/* Point to the hashmap */` |
|       24 |  2345 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2346 | `		/* Perform the insertion */` |
|       24 |  2347 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2348 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2349 | `		if( rc == SXRET_OK ){` |
|       24 |  2350 | `			if( pMap->nEntry > 1 ){` |
|        - |  2351 | `				/* Append space separator first */` |
|       18 |  2352 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2353 | `			}` |
|       24 |  2354 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2355 | `		}` |
|       24 |  2356 | `		break;` |
|        - |  2357 | `								  }` |
|      ! 0 |  2358 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2359 | `		/* error_log() consumer */` |
|      ! 0 |  2360 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2361 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2362 | `		break;` |
|        - |  2363 | `										}` |
|      ! 0 |  2364 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2365 | `		/* Script return value */` |
|      ! 0 |  2366 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2367 | `#ifdef UNTRUST` |
|        - |  2368 | `		if( ppValue == 0 ){` |
|        - |  2369 | `			rc = SXERR_CORRUPT;` |
|        - |  2370 | `			break;` |
|        - |  2371 | `		}` |
|        - |  2372 | `#endif` |
|      ! 0 |  2373 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2374 | `		break;` |
|        - |  2375 | `								   }` |
|     2808 |  2376 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2377 | `		/* Register an IO stream device */` |
|     5618 |  2378 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2379 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8424 |  2380 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5618 |  2381 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2382 | `				/* Invalid stream */` |
|      ! 0 |  2383 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2384 | `				break;` |
|        - |  2385 | `		}` |
|     5618 |  2386 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2387 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2810 |  2388 | `			pVm->pDefStream = pStream;` |
|     1404 |  2389 | `		}` |
|        - |  2390 | `		/* Insert in the appropriate container */` |
|     5618 |  2391 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5618 |  2392 | `		break;` |
|        - |  2393 | `								  }` |
|        8 |  2394 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2395 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2396 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2397 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2398 | `#ifdef UNTRUST` |
|        - |  2399 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2400 | `			rc = SXERR_CORRUPT;` |
|        - |  2401 | `			break;` |
|        - |  2402 | `		}` |
|        - |  2403 | `#endif` |
|       16 |  2404 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2405 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2406 | `		break;` |
|        - |  2407 | `									   }` |
|        8 |  2408 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2409 | `		/* Raw HTTP request*/` |
|       16 |  2410 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2411 | `		int nByte = va_arg(ap,int);` |
|       16 |  2412 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2413 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2414 | `			break;` |
|        - |  2415 | `		}` |
|       16 |  2416 | `		if( nByte < 0 ){` |
|        - |  2417 | `			/* Compute length automatically */` |
|      ! 0 |  2418 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2419 | `		}` |
|        - |  2420 | `		/* Process the request */` |
|       16 |  2421 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2422 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2423 | `		if( rc == SXRET_OK ){` |
|       16 |  2424 | `			pVm->bHttpContext = 1;` |
|        8 |  2425 | `		}` |
|       16 |  2426 | `		break;` |
|        - |  2427 | `									}` |
|        8 |  2428 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2429 | `		/* Extract HTTP response status code */` |
|       16 |  2430 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2431 | `		if( pStatus ){` |
|       16 |  2432 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2433 | `		}` |
|       16 |  2434 | `		break;` |
|        - |  2435 | `										}` |
|        8 |  2436 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2437 | `		/* Iterate response headers via callback */` |
|        - |  2438 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2439 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2440 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2441 | `		if( xCallback ){` |
|       16 |  2442 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2443 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2444 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2445 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2446 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2447 | `							   pUserData);` |
|       12 |  2448 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2449 | `					break;` |
|        - |  2450 | `				}` |
|        6 |  2451 | `			}` |
|        8 |  2452 | `		}` |
|       16 |  2453 | `		break;` |
|        - |  2454 | `										 }` |
|      ! 0 |  2455 | `	default:` |
|        - |  2456 | `		/* Unknown configuration option */` |
|      ! 0 |  2457 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2458 | `		break;` |
|        - |  2459 | `	}` |
|    45260 |  2460 | `	return rc;` |
|        2 |  2461 |  |
|        - |  2462 | `/* Forward declaration */` |
|        - |  2463 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2464 | `/*` |
|        - |  2465 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2466 | ` * format.` |
|        - |  2467 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2468 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2469 | ` * (STDOUT).` |
|        - |  2470 | ` */` |
|        2 |  2471 | `static sxi32 VmByteCodeDump(` |
|        - |  2472 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2473 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2474 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2475 | `	)` |
|        1 |  2476 |  |
|        - |  2477 | `	static const char zDump[] = {` |
|        - |  2478 | `		"====================================================\n"` |
|        - |  2479 | `		"PH7 VM Dump\n"` |
|        - |  2480 | `		"====================================================\n"` |
|        - |  2481 | `	};` |
|        - |  2482 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2483 | `	sxi32 rc = SXRET_OK;` |
|        - |  2484 | `	sxu32 n;` |
|        - |  2485 | `	/* Point to the PH7 instructions */` |
|        3 |  2486 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2487 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2488 | `	n = 0;` |
|        3 |  2489 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2490 | `	/* Dump instructions */` |
|        7 |  2491 | `	for(;;){` |
|       15 |  2492 | `		if( pInstr >= pEnd ){` |
|        - |  2493 | `			/* No more instructions */` |
|        3 |  2494 | `			break;` |
|        - |  2495 | `		}` |
|        - |  2496 | `		/* Format and call the consumer callback */` |
|       19 |  2497 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2498 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2499 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2500 | `		if( rc != SXRET_OK ){` |
|        - |  2501 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2502 | `			return rc;` |
|        - |  2503 | `		}` |
|       13 |  2504 | `		++n;` |
|       13 |  2505 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2506 | `	}` |
|        3 |  2507 | `	return rc;` |
|        2 |  2508 |  |
|        - |  2509 | `/* Forward declaration */` |
|        - |  2510 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2511 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2512 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2513 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2514 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2515 | `/*` |
|        - |  2516 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2517 | ` * consumer callback.` |
|        - |  2518 | ` */` |
|      600 |  2519 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2520 |  |
|      601 |  2521 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2522 | `	sxi32 rc = SXRET_OK;` |
|        - |  2523 | `	/* Append a new line */` |
|        - |  2524 | `#ifdef __WINNT__` |
|        1 |  2525 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2526 | `#else` |
|      600 |  2527 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2528 | `#endif` |
|        - |  2529 | `	/* Invoke the output consumer callback */` |
|      601 |  2530 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2531 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2532 | `	return rc;` |
|        1 |  2533 |  |
|        - |  2534 | `/*` |
|        - |  2535 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2536 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2537 | ` * information.` |
|        - |  2538 | ` */` |
|      148 |  2539 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2540 |  |
|      150 |  2541 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2542 | `		ph7_value apArg[4];` |
|        - |  2543 | `		ph7_value *apArgPtr[4];` |
|        - |  2544 | `		ph7_value sResult;` |
|        - |  2545 | `		SyString sErr;` |
|        - |  2546 | `		/* Prepare arguments */` |
|       76 |  2547 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2548 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2549 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2550 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2551 | `		if( pFile ){` |
|       76 |  2552 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2553 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2554 | `		}else{` |
|      ! 0 |  2555 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2556 | `		}` |
|       76 |  2557 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2558 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2559 | `		/* Set up pointer array */` |
|       76 |  2560 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2561 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2562 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2563 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2564 | `		/* Call the handler */` |
|       76 |  2565 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2566 | `		/* Check return value */` |
|       76 |  2567 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2568 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2569 | `		}` |
|        - |  2570 | `		/* Release */` |
|       76 |  2571 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2572 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2573 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2574 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2575 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2576 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2577 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2578 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2579 | `	}` |
|        - |  2580 | `	/* No handler, always call error handler */` |
|       75 |  2581 | `	return TRUE;` |
|       76 |  2582 |  |
|      110 |  2583 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2584 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2585 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2586 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2587 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2588 | `	)` |
|        2 |  2589 |  |
|      112 |  2590 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2591 | `	SyString *pFile;` |
|        - |  2592 | `	char *zErr;` |
|      112 |  2593 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2594 | `	if( !pVm->bErrReport ){` |
|        - |  2595 | `		/* Don't bother reporting errors */` |
|        3 |  2596 | `		return SXRET_OK;` |
|        - |  2597 | `	}` |
|        - |  2598 | `	/* Reset the working buffer */` |
|      110 |  2599 | `	SyBlobReset(pWorker);` |
|        - |  2600 | `	/* Peek the processed file if available */` |
|      110 |  2601 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2602 | `	if( pFile ){` |
|        - |  2603 | `		/* Append file name */` |
|      110 |  2604 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2605 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2606 | `	}` |
|        - |  2607 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2608 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2609 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2610 | `	 * E_DEPRECATED). */` |
|      110 |  2611 | `	zErr = "Error:  ";` |
|      110 |  2612 | `	switch(iErr){` |
|       19 |  2613 | `	case PH7_CTX_WARNING:` |
|       40 |  2614 | `		zErr = "Warning:  ";` |
|       40 |  2615 | `		break;` |
|        6 |  2616 | `	case PH7_CTX_NOTICE:` |
|       14 |  2617 | `		zErr = "Notice:  ";` |
|       12 |  2618 | `		break;` |
|       29 |  2619 | `	default:` |
|        - |  2620 | `		/* keep iErr unchanged */` |
|       58 |  2621 | `		break;` |
|        - |  2622 | `	}` |
|      110 |  2623 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2624 | `	if( pFuncName ){` |
|        - |  2625 | `		/* Append function name first */` |
|       23 |  2626 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2627 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2628 | `	}` |
|      110 |  2629 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2630 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2631 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2632 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2633 | `	}` |
|      110 |  2634 | `	return rc;` |
|       57 |  2635 |  |
|        - |  2636 | `/*` |
|        - |  2637 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2638 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2639 | ` * information.` |
|        - |  2640 | ` */` |
|       40 |  2641 | `static sxi32 VmThrowErrorAp(` |
|        - |  2642 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2643 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2644 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2645 | `	const char *zFormat, /* Format message */` |
|        - |  2646 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2647 | `	)` |
|        2 |  2648 |  |
|       42 |  2649 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2650 | `	SyBlob sMsg;` |
|        - |  2651 | `	SyString *pFile;` |
|        - |  2652 | `	char *zErr;` |
|       42 |  2653 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2654 | `	if( !pVm->bErrReport ){` |
|        - |  2655 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2656 | `		return SXRET_OK;` |
|        - |  2657 | `	}` |
|        - |  2658 | `	/* Reset the working buffer */` |
|       42 |  2659 | `	SyBlobReset(pWorker);` |
|        - |  2660 | `	/* Peek the processed file if available */` |
|       42 |  2661 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2662 | `	if( pFile ){` |
|        - |  2663 | `		/* Append file name */` |
|       42 |  2664 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2665 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2666 | `	}` |
|        - |  2667 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2668 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2669 | `	 * the correct errno value. */` |
|       42 |  2670 | `	zErr = "Error:  ";` |
|       42 |  2671 | `	switch(iErr){` |
|        4 |  2672 | `	case PH7_CTX_WARNING:` |
|        9 |  2673 | `		zErr = "Warning:  ";` |
|        9 |  2674 | `		break;` |
|        3 |  2675 | `	case PH7_CTX_NOTICE:` |
|        7 |  2676 | `		zErr = "Notice:  ";` |
|        6 |  2677 | `		break;` |
|       13 |  2678 | `	default:` |
|        - |  2679 | `		/* do not change iErr */` |
|       26 |  2680 | `		break;` |
|        - |  2681 | `	}` |
|       42 |  2682 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2683 | `	if( pFuncName ){` |
|        - |  2684 | `		/* Append function name first */` |
|       26 |  2685 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2686 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2687 | `	}` |
|        - |  2688 | `	/* Format the raw message */` |
|       42 |  2689 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2690 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2691 | `	/* Check if a user error handler is installed */` |
|       42 |  2692 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2693 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2694 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2695 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2696 | `	}` |
|       42 |  2697 | `	SyBlobRelease(&sMsg);` |
|       42 |  2698 | `	return rc;` |
|       22 |  2699 |  |
|        - |  2700 | `/*` |
|        - |  2701 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2702 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2703 | ` * possible.` |
|        - |  2704 | ` */` |
|       38 |  2705 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2706 |  |
|        - |  2707 | `	ph7_class *pClass;` |
|       39 |  2708 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2709 | `	ph7_class_instance *pThis;` |
|        - |  2710 | `	ph7_class_method *pCons;` |
|        - |  2711 | `	ph7_value sArg;` |
|        - |  2712 | `	ph7_value *apArg[1];` |
|        - |  2713 | `	SyBlob sMsg;` |
|        - |  2714 | `	SyString sMsgStr;` |
|        - |  2715 | `	VmFrame *pFrame;` |
|        - |  2716 | `	sxi32 rc;` |
|       39 |  2717 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2718 | `	if( pClass == 0 ){` |
|      ! 0 |  2719 | `		return PH7_ABORT;` |
|        - |  2720 | `	}` |
|       39 |  2721 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2722 | `	if( pThis == 0 ){` |
|      ! 0 |  2723 | `		return PH7_ABORT;` |
|        - |  2724 | `	}` |
|       39 |  2725 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2726 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2727 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2728 | `	{` |
|       39 |  2729 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2730 | `		if( pOwner ){` |
|       39 |  2731 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2732 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2733 | `		}else{` |
|      ! 0 |  2734 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2735 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2736 | `		}` |
|        - |  2737 | `	}` |
|       39 |  2738 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2739 | `	if( pCons ){` |
|       39 |  2740 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2741 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2742 | `		apArg[0] = &sArg;` |
|       39 |  2743 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2744 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2745 | `	}` |
|       39 |  2746 | `	SyBlobRelease(&sMsg);` |
|       39 |  2747 | `	pFrame = pVm->pFrame;` |
|       39 |  2748 | `	if( pFrame ){` |
|       39 |  2749 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2750 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2751 | `	}` |
|       39 |  2752 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2753 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2754 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2755 | `		return PH7_ABORT;` |
|        - |  2756 | `	}` |
|       39 |  2757 | `	return PH7_EXCEPTION;` |
|       20 |  2758 |  |
|        - |  2759 |  |
|        - |  2760 | `/*` |
|        - |  2761 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2762 | ` */` |
|        4 |  2763 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2764 |  |
|        - |  2765 | `	ph7_class *pErrClass;` |
|        - |  2766 | `	ph7_class_instance *pThis;` |
|        - |  2767 | `	ph7_class_method *pCons;` |
|        - |  2768 | `	ph7_value sArg;` |
|        - |  2769 | `	ph7_value *apArg[1];` |
|        - |  2770 | `	SyBlob sMsg;` |
|        - |  2771 | `	SyString sMsgStr;` |
|        - |  2772 | `	VmFrame *pFrame;` |
|        - |  2773 | `	sxi32 rc;` |
|        5 |  2774 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2775 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2776 | `		return PH7_ABORT;` |
|        - |  2777 | `	}` |
|        5 |  2778 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2779 | `	if( pThis == 0 ){` |
|      ! 0 |  2780 | `		return PH7_ABORT;` |
|        - |  2781 | `	}` |
|        5 |  2782 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2783 | `	{` |
|        5 |  2784 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2785 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2786 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2787 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2788 | `	}` |
|        5 |  2789 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2790 | `	if( pCons ){` |
|        5 |  2791 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2792 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2793 | `		apArg[0] = &sArg;` |
|        5 |  2794 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2795 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2796 | `	}` |
|        5 |  2797 | `	SyBlobRelease(&sMsg);` |
|        5 |  2798 | `	pFrame = pVm->pFrame;` |
|        5 |  2799 | `	if( pFrame ){` |
|        5 |  2800 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2801 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2802 | `	}` |
|        5 |  2803 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2804 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2805 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2806 | `		return PH7_ABORT;` |
|        - |  2807 | `	}` |
|        5 |  2808 | `	return PH7_EXCEPTION;` |
|        3 |  2809 |  |
|        - |  2810 |  |
|        - |  2811 | `/*` |
|        - |  2812 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2813 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2814 | ` * For class types, instanceof is verified.` |
|        - |  2815 | ` *` |
|        - |  2816 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2817 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2818 | ` */` |
|        - |  2819 | `/*` |
|        - |  2820 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2821 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2822 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2823 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2824 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2825 | ` */` |
|       20 |  2826 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2827 |  |
|        - |  2828 | `	const char *z, *zEnd, *zTail;` |
|        - |  2829 | `	sxu32 n;` |
|        - |  2830 | `	sxu8 bReal;` |
|        - |  2831 | `	sxi32 rc;` |
|       22 |  2832 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2833 | `		return 0;` |
|        - |  2834 | `	}` |
|       22 |  2835 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2836 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2837 | `	zEnd = z + n;` |
|       22 |  2838 | `	if( n == 0 ){` |
|      ! 0 |  2839 | `		return 0;` |
|        - |  2840 | `	}` |
|       22 |  2841 | `	zTail = 0;` |
|       22 |  2842 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2843 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2844 | `		return 0;` |
|        - |  2845 | `	}` |
|        - |  2846 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2847 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2848 | `		zTail++;` |
|      ! 0 |  2849 | `	}` |
|       16 |  2850 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2851 |  |
|        - |  2852 |  |
|        - |  2853 | `/*` |
|        - |  2854 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2855 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2856 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2857 | ` *   0 if it's not strictly numeric.` |
|        - |  2858 | ` */` |
|       16 |  2859 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2860 |  |
|        - |  2861 | `	const char *z, *zEnd, *zTail;` |
|        - |  2862 | `	sxu32 n;` |
|       18 |  2863 | `	sxu8 bReal = 0;` |
|        - |  2864 | `	sxi32 rc;` |
|       18 |  2865 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2866 | `		return 0;` |
|        - |  2867 | `	}` |
|       18 |  2868 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2869 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2870 | `	zEnd = z + n;` |
|       18 |  2871 | `	if( n == 0 ) return 0;` |
|       18 |  2872 | `	zTail = 0;` |
|       18 |  2873 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2874 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2875 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2876 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2877 | `	return bReal ? 2 : 1;` |
|       10 |  2878 |  |
|        - |  2879 |  |
|        - |  2880 | `/*` |
|        - |  2881 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2882 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2883 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2884 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2885 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2886 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2887 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2888 | ` * throw.` |
|        - |  2889 | ` *` |
|        - |  2890 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2891 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2892 | ` */` |
|       98 |  2893 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2894 |  |
|        - |  2895 | `	sxu32 i;` |
|        - |  2896 | `	ph7_type_alt *aAlts;` |
|        - |  2897 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2898 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2899 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2900 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2901 | `	}` |
|       88 |  2902 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2903 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2904 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2905 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2906 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2907 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2908 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2909 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2910 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2911 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2912 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2913 | `	}` |
|        - |  2914 | `	/* Object handling */` |
|       88 |  2915 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2916 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2917 | `		if( bHasClassAlt ){` |
|       14 |  2918 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2919 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2920 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2921 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2922 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2923 | `			}` |
|       26 |  2924 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2925 | `				ph7_class *pExpected;` |
|        - |  2926 | `				SyString *pCN;` |
|       22 |  2927 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2928 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2929 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2930 | `					pExpected = pSelfNow;` |
|       22 |  2931 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2932 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2933 | `				}else{` |
|       22 |  2934 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2935 | `				}` |
|       22 |  2936 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2937 | `					return SXRET_OK;` |
|        - |  2938 | `				}` |
|        8 |  2939 | `			}` |
|        2 |  2940 | `		}` |
|        9 |  2941 | `		return SXERR_INVALID;` |
|        - |  2942 | `	}` |
|        - |  2943 | `	/* Array handling */` |
|       72 |  2944 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2945 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2946 | `	}` |
|        - |  2947 | `	/* Scalar handling — exact match first */` |
|       66 |  2948 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2949 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2950 | `	}` |
|       42 |  2951 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2952 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2953 | `	}` |
|       38 |  2954 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2955 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2956 | `	}` |
|       18 |  2957 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2958 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2959 | `	}` |
|       18 |  2960 | `	if( bStrict ){` |
|        - |  2961 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2962 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2963 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2964 | `			return SXRET_OK;` |
|        - |  2965 | `		}` |
|      ! 0 |  2966 | `		return SXERR_INVALID;` |
|        - |  2967 | `	}` |
|        - |  2968 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2969 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2970 | `	 * to match PHP's union RFC. */` |
|        - |  2971 | `	{` |
|       18 |  2972 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2973 | `		if( bHasInt ){` |
|        - |  2974 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2975 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2976 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2977 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2978 | `				return SXRET_OK;` |
|        - |  2979 | `			}` |
|       18 |  2980 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2981 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2982 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2983 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2984 | `					return SXRET_OK;` |
|        - |  2985 | `				}` |
|      ! 0 |  2986 | `			}` |
|       18 |  2987 | `			if( kind == 1 ){` |
|        9 |  2988 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2989 | `				return SXRET_OK;` |
|        - |  2990 | `			}` |
|        4 |  2991 | `		}` |
|       10 |  2992 | `		if( bHasFloat ){` |
|       10 |  2993 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2994 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2995 | `				return SXRET_OK;` |
|        - |  2996 | `			}` |
|       10 |  2997 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2998 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2999 | `				return SXRET_OK;` |
|        - |  3000 | `			}` |
|        1 |  3001 | `		}` |
|        3 |  3002 | `		if( bHasString ){` |
|      ! 0 |  3003 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3004 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3005 | `				return SXRET_OK;` |
|        - |  3006 | `			}` |
|      ! 0 |  3007 | `		}` |
|        3 |  3008 | `		if( bHasBool ){` |
|      ! 0 |  3009 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3010 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3011 | `				return SXRET_OK;` |
|        - |  3012 | `			}` |
|      ! 0 |  3013 | `		}` |
|        - |  3014 | `	}` |
|        3 |  3015 | `	return SXERR_INVALID;` |
|       51 |  3016 |  |
|        - |  3017 |  |
|        - |  3018 | `/*` |
|        - |  3019 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3020 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3021 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3022 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3023 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3024 | ` */` |
|       34 |  3025 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3026 |  |
|       36 |  3027 | `	if( bStrict ){` |
|        - |  3028 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  3029 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3030 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3031 | `			return SXRET_OK;` |
|        - |  3032 | `		}` |
|        7 |  3033 | `		return SXERR_INVALID;` |
|        - |  3034 | `	}` |
|        - |  3035 | `	{` |
|       28 |  3036 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3037 | `		if( xCast ) xCast(pVal);` |
|        - |  3038 | `	}` |
|       28 |  3039 | `	return SXRET_OK;` |
|       19 |  3040 |  |
|        - |  3041 |  |
|        - |  3042 | `/*` |
|        - |  3043 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3044 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3045 | ` *` |
|        - |  3046 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3047 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3048 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3049 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3050 | ` */` |
|        8 |  3051 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  3052 |  |
|        9 |  3053 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  3054 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  3055 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  3056 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  3057 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  3058 | `		}` |
|        9 |  3059 | `		zBuf[nCopy] = 0;` |
|        9 |  3060 | `		return zBuf;` |
|        - |  3061 | `	}` |
|      ! 0 |  3062 | `	switch( nType ){` |
|      ! 0 |  3063 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3064 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3065 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3066 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3067 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3068 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3069 | `		default:             return "scalar";` |
|        - |  3070 | `	}` |
|        5 |  3071 |  |
|        - |  3072 |  |
|        - |  3073 | `/*` |
|        - |  3074 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3075 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3076 | ` */` |
|       18 |  3077 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3078 |  |
|       19 |  3079 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3080 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3081 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3082 | `	return zBuf;` |
|        1 |  3083 |  |
|        - |  3084 |  |
|    14290 |  3085 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3086 |  |
|        - |  3087 | `	SyHashEntry *pSlot;` |
|        - |  3088 | `	VmClassAttr *pVmAttr;` |
|        - |  3089 | `	ph7_class_attr *pAttr;` |
|    14292 |  3090 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14292 |  3091 | `	if( pSlot == 0 ){` |
|    14084 |  3092 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3093 | `	}` |
|      210 |  3094 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3095 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3096 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3097 | `		return SXRET_OK;` |
|        - |  3098 | `	}` |
|        - |  3099 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3100 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3101 | `	 * matching PHP's documented behavior. */` |
|      210 |  3102 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3103 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3104 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3105 |  |
|       16 |  3106 | `		if( rc == SXRET_OK ){` |
|        9 |  3107 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3108 | `			return SXRET_OK;` |
|        - |  3109 | `		}` |
|        7 |  3110 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3111 | `			char zBuf[128];` |
|        4 |  3112 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3113 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3114 | `		}` |
|        5 |  3115 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3116 | `	}` |
|        - |  3117 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3118 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3119 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3120 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3121 | `			return SXRET_OK;` |
|        - |  3122 | `		}` |
|        3 |  3123 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3124 | `	}` |
|        - |  3125 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3126 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3127 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3128 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3129 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3130 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3131 | `			return SXRET_OK;` |
|        - |  3132 | `		}` |
|        7 |  3133 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3134 | `	}` |
|      174 |  3135 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3136 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3137 | `		 * currently active on the self-stack. */` |
|       26 |  3138 | `		ph7_class *pExpected = 0;` |
|       26 |  3139 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3140 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3141 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3142 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3143 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3144 | `		}` |
|       26 |  3145 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3146 | `			pExpected = pSelfNow;` |
|       24 |  3147 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3148 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3149 | `		}else{` |
|       22 |  3150 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3151 | `		}` |
|       26 |  3152 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3153 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3154 | `		}` |
|       26 |  3155 | `		if( pExpected ){` |
|       22 |  3156 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3157 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3158 | `				char zBuf[128];` |
|        7 |  3159 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3160 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3161 | `			}` |
|        8 |  3162 | `		}` |
|       22 |  3163 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3164 | `		return SXRET_OK;` |
|        - |  3165 | `	}` |
|        - |  3166 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3167 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3168 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3169 | `		char zBuf[128];` |
|       10 |  3170 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3171 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3172 | `	}` |
|      144 |  3173 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3174 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3175 | `		if( xCast ){` |
|        - |  3176 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3177 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3178 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3179 | `			}` |
|       24 |  3180 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3181 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3182 | `			}` |
|        - |  3183 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3184 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3185 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3186 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3187 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3188 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3189 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3190 | `			}` |
|       12 |  3191 | `			xCast(pValue);` |
|        5 |  3192 | `		}` |
|        5 |  3193 | `	}` |
|      130 |  3194 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3195 | `	return SXRET_OK;` |
|     7147 |  3196 |  |
|        - |  3197 |  |
|        - |  3198 | `/*` |
|        - |  3199 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3200 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3201 | ` * information.` |
|        - |  3202 | ` * ------------------------------------` |
|        - |  3203 | ` * Simple boring wrapper function.` |
|        - |  3204 | ` * ------------------------------------` |
|        - |  3205 | ` */` |
|       16 |  3206 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3207 |  |
|        - |  3208 | `	va_list ap;` |
|        - |  3209 | `	sxi32 rc;` |
|       17 |  3210 | `	va_start(ap,zFormat);` |
|       17 |  3211 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3212 | `	va_end(ap);` |
|       17 |  3213 | `	return rc;` |
|        1 |  3214 |  |
|        - |  3215 | `/*` |
|        - |  3216 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3217 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3218 | ` */` |
|       34 |  3219 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3220 |  |
|        - |  3221 | `	ph7_class *pClass;` |
|        - |  3222 | `	ph7_class_instance *pThis;` |
|        - |  3223 | `	ph7_class_method *pCons;` |
|        - |  3224 | `	ph7_value sArg;` |
|        - |  3225 | `	ph7_value *apArg[1];` |
|        - |  3226 | `	SyBlob sMsg;` |
|        - |  3227 | `	SyString sMsgStr;` |
|        - |  3228 | `	VmFrame *pFrame;` |
|        - |  3229 | `	sxi32 rc;` |
|       35 |  3230 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3231 | `	if( pClass == 0 ){` |
|      ! 0 |  3232 | `		return PH7_ABORT;` |
|        - |  3233 | `	}` |
|       35 |  3234 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3235 | `	if( pThis == 0 ){` |
|      ! 0 |  3236 | `		return PH7_ABORT;` |
|        - |  3237 | `	}` |
|       35 |  3238 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3239 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3240 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3241 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3242 | `	if( pCons ){` |
|       35 |  3243 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3244 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3245 | `		apArg[0] = &sArg;` |
|       35 |  3246 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3247 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3248 | `	}` |
|       35 |  3249 | `	SyBlobRelease(&sMsg);` |
|       35 |  3250 | `	pFrame = pVm->pFrame;` |
|       35 |  3251 | `	if( pFrame ){` |
|       35 |  3252 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3253 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3254 | `	}` |
|       35 |  3255 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3256 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3257 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3258 | `		return PH7_ABORT;` |
|        - |  3259 | `	}` |
|       31 |  3260 | `	return PH7_EXCEPTION;` |
|       18 |  3261 |  |
|        - |  3262 | `/*` |
|        - |  3263 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3264 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3265 | ` */` |
|        6 |  3266 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3267 |  |
|        - |  3268 | `	ph7_class *pClass;` |
|        - |  3269 | `	ph7_class_instance *pThis;` |
|        - |  3270 | `	ph7_class_method *pCons;` |
|        - |  3271 | `	ph7_value sArg;` |
|        - |  3272 | `	ph7_value *apArg[1];` |
|        - |  3273 | `	SyBlob sMsg;` |
|        - |  3274 | `	SyString sMsgStr;` |
|        - |  3275 | `	VmFrame *pFrame;` |
|        - |  3276 | `	sxi32 rc;` |
|        7 |  3277 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3278 | `	if( pClass == 0 ){` |
|      ! 0 |  3279 | `		return PH7_ABORT;` |
|        - |  3280 | `	}` |
|        7 |  3281 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3282 | `	if( pThis == 0 ){` |
|      ! 0 |  3283 | `		return PH7_ABORT;` |
|        - |  3284 | `	}` |
|        7 |  3285 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3286 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3287 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3288 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3289 | `	if( pCons ){` |
|        7 |  3290 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3291 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3292 | `		apArg[0] = &sArg;` |
|        7 |  3293 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3294 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3295 | `	}` |
|        7 |  3296 | `	SyBlobRelease(&sMsg);` |
|        7 |  3297 | `	pFrame = pVm->pFrame;` |
|        7 |  3298 | `	if( pFrame ){` |
|        7 |  3299 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3300 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3301 | `	}` |
|        7 |  3302 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3303 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3304 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3305 | `		return PH7_ABORT;` |
|        - |  3306 | `	}` |
|      ! 0 |  3307 | `	return PH7_EXCEPTION;` |
|        4 |  3308 |  |
|        - |  3309 | `/*` |
|        - |  3310 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3311 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3312 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3313 | ` */` |
|       16 |  3314 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3315 |  |
|       17 |  3316 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3317 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3318 | `	}` |
|       13 |  3319 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3320 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3321 | `		if( pThis && pThis->pClass ){` |
|        5 |  3322 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3323 | `			sxu32 n = pName->nByte;` |
|        5 |  3324 | `			if( n >= nBuf ){` |
|      ! 0 |  3325 | `				n = nBuf - 1;` |
|      ! 0 |  3326 | `			}` |
|        5 |  3327 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3328 | `			zBuf[n] = 0;` |
|        5 |  3329 | `			return zBuf;` |
|        - |  3330 | `		}` |
|      ! 0 |  3331 | `		return "object";` |
|        - |  3332 | `	}` |
|        9 |  3333 | `	return ph7_type_name(pVal);` |
|        9 |  3334 |  |
|        - |  3335 | `/*` |
|        - |  3336 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3337 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3338 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3339 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3340 | ` */` |
|       16 |  3341 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3342 |  |
|        - |  3343 | `	ph7_class *pClass;` |
|        - |  3344 | `	ph7_class_instance *pThis;` |
|        - |  3345 | `	ph7_class_method *pCons;` |
|        - |  3346 | `	ph7_value sArg;` |
|        - |  3347 | `	ph7_value *apArg[1];` |
|        - |  3348 | `	SyBlob sMsg;` |
|        - |  3349 | `	SyString sMsgStr;` |
|        - |  3350 | `	VmFrame *pFrame;` |
|        - |  3351 | `	sxi32 rc;` |
|       17 |  3352 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3353 | `	char zNameBuf[64];` |
|       17 |  3354 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3355 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3356 | `	if( pClass == 0 ){` |
|      ! 0 |  3357 | `		return PH7_ABORT;` |
|        - |  3358 | `	}` |
|       17 |  3359 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3360 | `	if( pThis == 0 ){` |
|      ! 0 |  3361 | `		return PH7_ABORT;` |
|        - |  3362 | `	}` |
|       17 |  3363 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3364 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3365 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3366 | `	if( pCons ){` |
|       17 |  3367 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3368 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3369 | `		apArg[0] = &sArg;` |
|       17 |  3370 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3371 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3372 | `	}` |
|       17 |  3373 | `	SyBlobRelease(&sMsg);` |
|       17 |  3374 | `	pFrame = pVm->pFrame;` |
|       17 |  3375 | `	if( pFrame ){` |
|       17 |  3376 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3377 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3378 | `	}` |
|       17 |  3379 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3380 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3381 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3382 | `		return PH7_ABORT;` |
|        - |  3383 | `	}` |
|       17 |  3384 | `	return PH7_EXCEPTION;` |
|        9 |  3385 |  |
|        - |  3386 | `/*` |
|        - |  3387 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3388 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3389 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3390 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3391 | ` */` |
|        - |  3392 | `/*` |
|        - |  3393 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3394 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3395 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3396 | ` */` |
|       24 |  3397 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3398 |  |
|        - |  3399 | `	sxu32 nCopy;` |
|       26 |  3400 | `	if( nBuf == 0 ) return "";` |
|       26 |  3401 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3402 | `		zBuf[0] = 0;` |
|      ! 0 |  3403 | `		return zBuf;` |
|        - |  3404 | `	}` |
|       26 |  3405 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3406 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3407 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3408 | `	zBuf[nCopy] = 0;` |
|       26 |  3409 | `	return zBuf;` |
|       14 |  3410 |  |
|        - |  3411 |  |
|      376 |  3412 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3413 |  |
|      378 |  3414 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3415 | `	const char *zGiven;` |
|        - |  3416 | `	char zBuf[128];` |
|        - |  3417 | `	char zTypeBuf[128];` |
|        - |  3418 | `	/* Untyped function: no enforcement. */` |
|      378 |  3419 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3420 | `		return SXRET_OK;` |
|        - |  3421 | `	}` |
|        - |  3422 | `	/* void return type: the function must not produce a value. */` |
|      378 |  3423 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3424 | `		if( pValue == 0 ){` |
|      134 |  3425 | `			return SXRET_OK;` |
|        - |  3426 | `		}` |
|        - |  3427 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3428 | `		 * still counts as "returned a value" here. */` |
|        3 |  3429 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3430 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3431 | `	}` |
|        - |  3432 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3433 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      244 |  3434 | `	if( pValue == 0 ){` |
|      ! 0 |  3435 | `		const char *zExpected = "value";` |
|      ! 0 |  3436 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3437 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3438 | `		}` |
|      ! 0 |  3439 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3440 | `	}` |
|        - |  3441 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3442 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3443 | `	 * bNullable=0 here. */` |
|      244 |  3444 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3445 | `		sxi32 rcU;` |
|      ! 0 |  3446 | `		int bNullable = 0;` |
|      ! 0 |  3447 | `		const char *zExpected = "union";` |
|        - |  3448 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3449 | `		{` |
|        - |  3450 | `			sxu32 i;` |
|      ! 0 |  3451 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3452 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3453 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3454 | `			}` |
|        - |  3455 | `		}` |
|      ! 0 |  3456 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3457 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3458 | `			return SXRET_OK;` |
|        - |  3459 | `		}` |
|      ! 0 |  3460 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3461 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3462 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3463 | `			zGiven = "null";` |
|      ! 0 |  3464 | `		}else{` |
|      ! 0 |  3465 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3466 | `		}` |
|      ! 0 |  3467 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3468 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3469 | `		}` |
|      ! 0 |  3470 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3471 | `	}` |
|        - |  3472 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3473 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3474 | `	 * it into the TypeError message. */` |
|      244 |  3475 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3476 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3477 | `		const char *zExpected;` |
|        - |  3478 | `		ph7_class *pExpected;` |
|        6 |  3479 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3480 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3481 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3482 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3483 | `		}` |
|        6 |  3484 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3485 | `			pExpected = pSelfNow;` |
|        4 |  3486 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3487 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3488 | `		}else{` |
|        3 |  3489 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3490 | `		}` |
|        6 |  3491 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3492 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3493 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3494 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3495 | `		}` |
|        6 |  3496 | `		if( pExpected ){` |
|        6 |  3497 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3498 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3499 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3500 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3501 | `			}` |
|        2 |  3502 | `		}` |
|        6 |  3503 | `		return SXRET_OK;` |
|        - |  3504 | `	}` |
|        - |  3505 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3506 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3507 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3508 | `	 * via the type-text leading '?'. */` |
|      240 |  3509 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3510 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3511 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3512 | `			return SXRET_OK;` |
|        - |  3513 | `		}` |
|      ! 0 |  3514 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3515 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3516 | `			"null");` |
|        - |  3517 | `	}` |
|        - |  3518 | `	/* Exact match? Done. */` |
|      234 |  3519 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3520 | `		return SXRET_OK;` |
|        - |  3521 | `	}` |
|        - |  3522 | `	/* Object->scalar is never compatible. */` |
|        8 |  3523 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3524 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3525 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3526 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3527 | `			zGiven);` |
|        - |  3528 | `	}` |
|        - |  3529 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3530 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3531 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3532 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3533 | `			ph7_type_name(pValue));` |
|        - |  3534 | `	}` |
|        - |  3535 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3536 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3537 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3538 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3539 | `	if( !bStrict` |
|        5 |  3540 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3541 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3542 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3543 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3544 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3545 | `			"string");` |
|        - |  3546 | `	}` |
|        6 |  3547 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3548 | `		return SXRET_OK;` |
|        - |  3549 | `	}` |
|        4 |  3550 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3551 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3552 | `		ph7_type_name(pValue));` |
|      190 |  3553 |  |
|        - |  3554 | `/*` |
|        - |  3555 | ` * Report a fatal named-argument error.` |
|        - |  3556 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3557 | ` */` |
|        6 |  3558 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3559 |  |
|        7 |  3560 | `	const char *zFunc = 0;` |
|        7 |  3561 | `	int nFunc = 0;` |
|        7 |  3562 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3563 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3564 |  |
|        - |  3565 | `/*` |
|        - |  3566 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3567 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3568 | ` * information.` |
|        - |  3569 | ` * ------------------------------------` |
|        - |  3570 | ` * Simple boring wrapper function.` |
|        - |  3571 | ` * ------------------------------------` |
|        - |  3572 | ` */` |
|       24 |  3573 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3574 |  |
|        - |  3575 | `	sxi32 rc;` |
|       26 |  3576 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3577 | `	return rc;` |
|        2 |  3578 |  |
|        - |  3579 | `/*` |
|        - |  3580 | ` * Resolve function context from the current frame.` |
|        - |  3581 | ` */` |
|     1018 |  3582 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3583 |  |
|        - |  3584 | `	VmFrame *pFrame;` |
|        - |  3585 | `	ph7_vm_func *pFunc;` |
|     1019 |  3586 | `	*pzFuncName = 0;` |
|     1019 |  3587 | `	*pnFuncLen = 0;` |
|     1019 |  3588 | `	pFrame = pVm->pFrame;` |
|     1019 |  3589 | `	if( pFrame == 0 ){` |
|      ! 0 |  3590 | `		return;` |
|        - |  3591 | `	}` |
|     1019 |  3592 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3593 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3594 | `		return;` |
|        - |  3595 | `	}` |
|       25 |  3596 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3597 | `	if( pFunc == 0 ){` |
|      ! 0 |  3598 | `		return;` |
|        - |  3599 | `	}` |
|       25 |  3600 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3601 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3602 |  |
|        - |  3603 | `/*` |
|        - |  3604 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3605 | ` */` |
|      524 |  3606 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3607 |  |
|        - |  3608 | `	SyBlob sOut;` |
|        - |  3609 | `	SyString *pFile;` |
|      525 |  3610 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3611 | `		return PH7_OK;` |
|        - |  3612 | `	}` |
|      525 |  3613 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3614 | `		zClass = "Exception";` |
|      ! 0 |  3615 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3616 | `	}` |
|      525 |  3617 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3618 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3619 | `	}` |
|      525 |  3620 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3621 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3622 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3623 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3624 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3625 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3626 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3627 | `	}` |
|      525 |  3628 | `	if( pFile ){` |
|      525 |  3629 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3630 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3631 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3632 | `	}` |
|      525 |  3633 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3634 | `	if( pFile ){` |
|      525 |  3635 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3636 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3637 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3638 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3639 | `		}else{` |
|      501 |  3640 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3641 | `		}` |
|      262 |  3642 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3643 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3644 | `	}else{` |
|      ! 0 |  3645 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3646 | `	}` |
|      525 |  3647 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3648 | `	if( pFile ){` |
|      525 |  3649 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3650 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3651 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3652 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3653 | `	}` |
|      525 |  3654 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3655 | `	SyBlobRelease(&sOut);` |
|      525 |  3656 | `	return PH7_ABORT;` |
|      263 |  3657 |  |
|        - |  3658 | `/*` |
|        - |  3659 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3660 | ` *` |
|        - |  3661 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3662 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3663 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3664 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3665 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3666 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3667 | ` */` |
|      828 |  3668 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3669 |  |
|      830 |  3670 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3671 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3672 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3673 | `		}` |
|        7 |  3674 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3675 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3676 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3677 | `	}` |
|      830 |  3678 |  |
|        - |  3679 | `/*` |
|        - |  3680 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3681 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3682 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3683 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3684 | ` *` |
|        - |  3685 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3686 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3687 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3688 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3689 | ` */` |
|        4 |  3690 | `static sxi32 VmThrowFromVm(` |
|        - |  3691 | `	ph7_vm *pVm,` |
|        - |  3692 | `	const char *zClass,` |
|        - |  3693 | `	const char *zMsg,` |
|        - |  3694 | `	sxu32 nMsg` |
|        1 |  3695 | `){` |
|        - |  3696 | `	ph7_class *pClass;` |
|        - |  3697 | `	ph7_class_instance *pThis;` |
|        - |  3698 | `	ph7_class_method *pCons;` |
|        - |  3699 | `	VmFrame *pFrame;` |
|        - |  3700 | `	sxi32 rc;` |
|        5 |  3701 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3702 | `	if( pClass == 0 ){` |
|      ! 0 |  3703 | `		return SXERR_ABORT;` |
|        - |  3704 | `	}` |
|        5 |  3705 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3706 | `	if( pThis == 0 ){` |
|      ! 0 |  3707 | `		return SXERR_ABORT;` |
|        - |  3708 | `	}` |
|        5 |  3709 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3710 | `	if( pCons ){` |
|        - |  3711 | `		ph7_value sArg;` |
|        - |  3712 | `		ph7_value *apArg[1];` |
|        - |  3713 | `		SyString sMsgStr;` |
|        5 |  3714 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3715 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3716 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3717 | `		apArg[0] = &sArg;` |
|        5 |  3718 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3719 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3720 | `	}` |
|        5 |  3721 | `	pFrame = pVm->pFrame;` |
|        5 |  3722 | `	if( pFrame ){` |
|        5 |  3723 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3724 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3725 | `	}` |
|        5 |  3726 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3727 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3728 | `	return rc;` |
|        3 |  3729 |  |
|        - |  3730 | `/*` |
|        - |  3731 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3732 | ` */` |
|      574 |  3733 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3734 |  |
|        - |  3735 | `	ph7_vm *pVm;` |
|        - |  3736 | `	ph7_class *pClass;` |
|        - |  3737 | `	ph7_class_instance *pThis;` |
|        - |  3738 | `	ph7_class_method *pCons;` |
|        - |  3739 | `	ph7_value sArg;` |
|        - |  3740 | `	ph7_value *apArg[1];` |
|        - |  3741 | `	SyBlob sMsg;` |
|        - |  3742 | `	SyString sMsgStr;` |
|        - |  3743 | `	VmFrame *pFrame;` |
|        - |  3744 | `	va_list ap;` |
|        - |  3745 | `	sxi32 rc;` |
|        - |  3746 |  |
|      576 |  3747 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3748 | `		return PH7_ABORT;` |
|        - |  3749 | `	}` |
|      576 |  3750 | `	pVm = pCtx->pVm;` |
|      576 |  3751 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3752 | `		zClass = "Error";` |
|      ! 0 |  3753 | `	}` |
|      576 |  3754 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3755 | `	if( pClass == 0 ){` |
|      ! 0 |  3756 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3757 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3758 | `			zClass` |
|        - |  3759 | `			);` |
|        - |  3760 | `	}` |
|      576 |  3761 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3762 | `	if( pThis == 0 ){` |
|      ! 0 |  3763 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3764 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3765 | `			);` |
|        - |  3766 | `	}` |
|        - |  3767 |  |
|      576 |  3768 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3769 | `	va_start(ap,zFormat);` |
|      576 |  3770 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3771 | `	va_end(ap);` |
|        - |  3772 |  |
|      576 |  3773 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3774 | `	if( pCons ){` |
|      576 |  3775 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3776 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3777 | `		apArg[0] = &sArg;` |
|      576 |  3778 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3779 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3780 | `	}` |
|      576 |  3781 | `	SyBlobRelease(&sMsg);` |
|        - |  3782 |  |
|      576 |  3783 | `	pFrame = pVm->pFrame;` |
|      576 |  3784 | `	if( pFrame ){` |
|      576 |  3785 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3786 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3787 | `	}` |
|      576 |  3788 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3789 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3790 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3791 | `		return PH7_ABORT;` |
|        - |  3792 | `	}` |
|       86 |  3793 | `	return PH7_EXCEPTION;` |
|      289 |  3794 |  |
|        - |  3795 | `/*` |
|        - |  3796 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3797 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3798 | ` */` |
|      ! 0 |  3799 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3800 |  |
|        - |  3801 | `	ph7_vm *pVm;` |
|        - |  3802 | `	SyBlob sMsg;` |
|      ! 0 |  3803 | `	const char *zFuncName = 0;` |
|      ! 0 |  3804 | `	int nFuncLen = 0;` |
|        - |  3805 | `	va_list ap;` |
|        - |  3806 | `	sxi32 rc;` |
|        - |  3807 |  |
|      ! 0 |  3808 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3809 | `		return PH7_OK;` |
|        - |  3810 | `	}` |
|      ! 0 |  3811 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3812 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3813 | `		zClass = "Error";` |
|      ! 0 |  3814 | `	}` |
|        - |  3815 |  |
|      ! 0 |  3816 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3817 |  |
|      ! 0 |  3818 | `	va_start(ap,zFormat);` |
|      ! 0 |  3819 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3820 | `	va_end(ap);` |
|        - |  3821 |  |
|      ! 0 |  3822 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3823 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3824 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3825 | `	}` |
|      ! 0 |  3826 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3827 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3828 | `	}` |
|      ! 0 |  3829 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3830 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3831 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3832 | `	return rc;` |
|      ! 0 |  3833 |  |
|        - |  3834 | `/*` |
|        - |  3835 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3836 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3837 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3838 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3839 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3840 | ` * when VmByteCodeExec returns.` |
|        - |  3841 | ` */` |
|      144 |  3842 | `static sxi32 VmSuspendCtx(` |
|        - |  3843 | `	ph7_vm *pVm,` |
|        - |  3844 | `	ph7_exec_ctx *pCtx,` |
|        - |  3845 | `	sxi32 pc,` |
|        - |  3846 | `	sxi32 nTos` |
|        - |  3847 | `	)` |
|        2 |  3848 |  |
|       72 |  3849 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3850 | `	pCtx->pc = pc;` |
|      146 |  3851 | `	pCtx->nTos = nTos;` |
|      146 |  3852 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3853 | `	return PH7_SUSPEND;` |
|        2 |  3854 |  |
|        - |  3855 | `/*` |
|        - |  3856 | ` * Resolve named-argument mapping.` |
|        - |  3857 | ` *` |
|        - |  3858 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3859 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3860 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3861 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3862 | ` * every formal parameter that received a value.` |
|        - |  3863 | ` *` |
|        - |  3864 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3865 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3866 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3867 | ` */` |
|       98 |  3868 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3869 | `	ph7_vm *pVm,` |
|        - |  3870 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3871 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3872 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3873 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3874 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3875 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3876 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3877 |  |
|        2 |  3878 |  |
|      100 |  3879 | `	sxi32 posIdx = 0;` |
|        - |  3880 | `	sxu32 i;` |
|        - |  3881 | `	char zErrMsg[256];` |
|      100 |  3882 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3883 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3884 | `		aSlot[i] = -2;` |
|      100 |  3885 | `	}` |
|      290 |  3886 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3887 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3888 | `			/* Named argument — find formal by name */` |
|      184 |  3889 | `			int found = 0;` |
|        - |  3890 | `			sxu32 k;` |
|      304 |  3891 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3892 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3893 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3894 | `						pMap->aNames[i].zString,` |
|      402 |  3895 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3896 | `					if( aUsed[k] ){` |
|        7 |  3897 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3898 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3899 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3900 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3901 | `						return PH7_ABORT;` |
|        - |  3902 | `					}` |
|      168 |  3903 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3904 | `					aUsed[k] = 1;` |
|      168 |  3905 | `					found = 1;` |
|      168 |  3906 | `					break;` |
|        - |  3907 | `				}` |
|       62 |  3908 | `			}` |
|      180 |  3909 | `			if( !found ){` |
|       14 |  3910 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3911 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3912 | `				}else{` |
|        4 |  3913 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3914 | `						"Unknown named parameter $%.*s",` |
|        2 |  3915 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3916 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3917 | `					return PH7_ABORT;` |
|        - |  3918 | `				}` |
|        5 |  3919 | `			}` |
|       90 |  3920 | `		}else{` |
|        - |  3921 | `			/* Positional argument */` |
|       16 |  3922 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3923 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3924 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3925 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3926 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3927 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3928 | `					return PH7_ABORT;` |
|        - |  3929 | `				}` |
|       16 |  3930 | `				aSlot[i] = posIdx;` |
|       16 |  3931 | `				aUsed[posIdx] = 1;` |
|        7 |  3932 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3933 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3934 | `			}` |
|       16 |  3935 | `			posIdx++;` |
|        - |  3936 | `		}` |
|       97 |  3937 | `	}` |
|       93 |  3938 | `	return SXRET_OK;` |
|       51 |  3939 |  |
|        - |  3940 | `/*` |
|        - |  3941 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3942 | ` *` |
|        - |  3943 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3944 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3945 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3946 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3947 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3948 | ` * then the program execution is halted.` |
|        - |  3949 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3950 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3951 | ` * or to reset the VM to it's initial state.` |
|        - |  3952 | ` */` |
|    43710 |  3953 | `static sxi32 VmByteCodeExec(` |
|        - |  3954 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3955 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3956 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3957 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3958 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3959 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3960 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3961 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3962 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3963 | `	)` |
|        2 |  3964 |  |
|        - |  3965 | `	VmInstr *pInstr;` |
|        - |  3966 | `	ph7_value *pTos;` |
|        - |  3967 | `	SySet aArg;` |
|        - |  3968 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3969 | `	sxi32 pc;` |
|        - |  3970 | `	sxi32 rc;` |
|        - |  3971 | `	/* Argument container */` |
|    43712 |  3972 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    43712 |  3973 | `	if( nTos < 0 ){` |
|    40742 |  3974 | `		pTos = &pStack[-1];` |
|    20372 |  3975 | `	}else{` |
|     2972 |  3976 | `		pTos = &pStack[nTos];` |
|        - |  3977 | `	}` |
|    43712 |  3978 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    43712 |  3979 | `	pc = nPc;` |
|        - |  3980 | `/*` |
|        - |  3981 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3982 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3983 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3984 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3985 | ` */` |
|        - |  3986 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3987 | `	{ \` |
|        - |  3988 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3989 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3990 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3991 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3992 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3993 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3994 | `				break; \` |
|        - |  3995 | `			} \` |
|        - |  3996 | `			goto Exception; \` |
|        - |  3997 | `		} \` |
|        - |  3998 | `	}` |
|        - |  3999 | `	/* Execute as much as we can */` |
|  5819777 |  4000 | `	for(;;){` |
|        - |  4001 | `		/* Fetch the instruction to execute */` |
| 11638852 |  4002 | `		pInstr = &aInstr[pc];` |
| 11638852 |  4003 | `		rc = SXRET_OK;` |
|        - |  4004 | `/*` |
|        - |  4005 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4006 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4007 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4008 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4009 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4010 | ` */` |
| 11638852 |  4011 | `		switch(pInstr->iOp){` |
|        - |  4012 | `/*` |
|        - |  4013 | ` * DONE: P1 * *` |
|        - |  4014 | ` *` |
|        - |  4015 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4016 | ` * and return immediately.` |
|        - |  4017 | ` */` |
|    21503 |  4018 | `case PH7_OP_DONE:` |
|        - |  4019 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4020 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4021 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4022 | `	 * callback trampolines, and the main script. */` |
|    43008 |  4023 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      378 |  4024 | `		ph7_value *pRetVal = 0;` |
|      378 |  4025 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      246 |  4026 | `			pRetVal = pTos;` |
|      122 |  4027 | `		}` |
|      378 |  4028 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      378 |  4029 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      372 |  4030 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4031 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4032 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4033 | `				pTos--;` |
|      ! 0 |  4034 | `			}` |
|      ! 0 |  4035 | `			goto Exception;` |
|        - |  4036 | `		}` |
|        - |  4037 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4038 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4039 | `		 * defensively we clear the pointer after a successful check). */` |
|      372 |  4040 | `		pEnforceRetFunc = 0;` |
|      185 |  4041 | `	}` |
|    43002 |  4042 | `	if( pInstr->iP1 ){` |
|        - |  4043 | `#ifdef UNTRUST` |
|        - |  4044 | `		if( pTos < pStack ){` |
|        - |  4045 | `			goto Abort;` |
|        - |  4046 | `		}` |
|        - |  4047 | `#endif` |
|    26104 |  4048 | `		if( pLastRef ){` |
|    16044 |  4049 | `			*pLastRef = pTos->nIdx;` |
|     8021 |  4050 | `		}` |
|    26104 |  4051 | `		if( pResult ){` |
|        - |  4052 | `			/* Execution result */` |
|    24684 |  4053 | `			PH7_MemObjStore(pTos,pResult);` |
|    12341 |  4054 | `		}` |
|    26104 |  4055 | `		VmPopOperand(&pTos,1);` |
|    29951 |  4056 | `	}else if( pLastRef ){` |
|        - |  4057 | `		/* Nothing referenced */` |
|     1804 |  4058 | `		*pLastRef = SXU32_HIGH;` |
|      901 |  4059 | `	}` |
|        - |  4060 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4061 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4062 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4063 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4064 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4065 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4066 | `	 * block can override it.` |
|        - |  4067 | `	 */` |
|    43004 |  4068 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4069 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4070 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4071 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4072 | `		pExc->pFrame = 0;` |
|        3 |  4073 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4074 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4075 | `			pExc->iFinallyDone = 1;` |
|        - |  4076 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4077 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4078 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4079 | `				goto Abort;` |
|        - |  4080 | `			}` |
|        1 |  4081 | `		}` |
|        1 |  4082 | `	}` |
|    43002 |  4083 | `	goto Done;` |
|        - |  4084 | `/*` |
|        - |  4085 | ` * HALT: P1 * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4088 | ` * and abort immediately.` |
|        - |  4089 | ` */` |
|        4 |  4090 | `case PH7_OP_HALT:` |
|        9 |  4091 | `	if( pInstr->iP1 ){` |
|        - |  4092 | `#ifdef UNTRUST` |
|        - |  4093 | `		if( pTos < pStack ){` |
|        - |  4094 | `			goto Abort;` |
|        - |  4095 | `		}` |
|        - |  4096 | `#endif` |
|        9 |  4097 | `		if( pLastRef ){` |
|      ! 0 |  4098 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  4099 | `		}` |
|        9 |  4100 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  4101 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4102 | `				/* Output the exit message */` |
|        7 |  4103 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  4104 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  4105 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  4106 | `			}` |
|        7 |  4107 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4108 | `			/* Record exit status */` |
|        5 |  4109 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4110 | `		}` |
|        9 |  4111 | `		VmPopOperand(&pTos,1);` |
|        4 |  4112 | `	}else if( pLastRef ){` |
|        - |  4113 | `		/* Nothing referenced */` |
|      ! 0 |  4114 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4115 | `	}` |
|        - |  4116 | `	/* Check if we're in an included file context */` |
|        9 |  4117 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  4118 | `		/* Terminate the entire process */` |
|        9 |  4119 | `		exit(pVm->iExitStatus);` |
|        - |  4120 | `	}` |
|      ! 0 |  4121 | `	goto Abort;` |
|        - |  4122 | `/*` |
|        - |  4123 | ` * JMP: * P2 *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4126 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4127 | ` */` |
|   248319 |  4128 | `case PH7_OP_JMP:` |
|   496684 |  4129 | `	pc = pInstr->iP2 - 1;` |
|   496684 |  4130 | `	break;` |
|        - |  4131 | `/*` |
|        - |  4132 | ` * JZ: P1 P2 *` |
|        - |  4133 | ` *` |
|        - |  4134 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4135 | ` * entry in the stack if P1 is zero.` |
|        - |  4136 | ` */` |
|   589247 |  4137 | `case PH7_OP_JZ:` |
|        - |  4138 | `#ifdef UNTRUST` |
|        - |  4139 | `	if( pTos < pStack ){` |
|        - |  4140 | `		goto Abort;` |
|        - |  4141 | `	}` |
|        - |  4142 | `#endif` |
|        - |  4143 | `	/* Get a boolean value */` |
|  1178584 |  4144 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  4145 | `		PH7_MemObjToBool(pTos);` |
|       85 |  4146 | `	}` |
|  1178584 |  4147 | `	if( !pTos->x.iVal ){` |
|        - |  4148 | `		/* Take the jump */` |
|   605460 |  4149 | `		pc = pInstr->iP2 - 1;` |
|   302729 |  4150 | `	}` |
|  1178584 |  4151 | `	if( !pInstr->iP1 ){` |
|   935094 |  4152 | `		VmPopOperand(&pTos,1);` |
|   467568 |  4153 | `	}` |
|  1178584 |  4154 | `	break;` |
|        - |  4155 | `/*` |
|        - |  4156 | ` * JNZ: P1 P2 *` |
|        - |  4157 | ` *` |
|        - |  4158 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4159 | ` * entry in the stack if P1 is zero.` |
|        - |  4160 | ` */` |
|    61235 |  4161 | `case PH7_OP_JNZ:` |
|        - |  4162 | `#ifdef UNTRUST` |
|        - |  4163 | `	if( pTos < pStack ){` |
|        - |  4164 | `		goto Abort;` |
|        - |  4165 | `	}` |
|        - |  4166 | `#endif` |
|        - |  4167 | `	/* Get a boolean value */` |
|   122472 |  4168 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4169 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4170 | `	}` |
|   122472 |  4171 | `	if( pTos->x.iVal ){` |
|        - |  4172 | `		/* Take the jump */` |
|     5478 |  4173 | `		pc = pInstr->iP2 - 1;` |
|     2738 |  4174 | `	}` |
|   122472 |  4175 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4176 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4177 | `	}` |
|   122472 |  4178 | `	break;` |
|        - |  4179 | `/*` |
|        - |  4180 | ` * NOOP: * * *` |
|        - |  4181 | ` *` |
|        - |  4182 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4183 | ` * destination.` |
|        - |  4184 | ` */` |
|      ! 0 |  4185 | `case PH7_OP_NOOP:` |
|      ! 0 |  4186 | `	break;` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * POP: P1 * *` |
|        - |  4189 | ` *` |
|        - |  4190 | ` * Pop P1 elements from the operand stack.` |
|        - |  4191 | ` */` |
|   454393 |  4192 | `case PH7_OP_POP: {` |
|   908832 |  4193 | `	sxi32 n = pInstr->iP1;` |
|   908832 |  4194 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4195 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4196 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4197 | `	}` |
|   908832 |  4198 | `	VmPopOperand(&pTos,n);` |
|   908832 |  4199 | `	break;` |
|        - |  4200 | `				 }` |
|        - |  4201 | `/*` |
|        - |  4202 | ` * DUP: * * *` |
|        - |  4203 | ` *` |
|        - |  4204 | ` * Duplicate the top of the stack.` |
|        - |  4205 | ` */` |
|       41 |  4206 | `case PH7_OP_DUP:` |
|        - |  4207 | `#ifdef UNTRUST` |
|        - |  4208 | `	if( pTos < pStack ){` |
|        - |  4209 | `		goto Abort;` |
|        - |  4210 | `	}` |
|        - |  4211 | `#endif` |
|       84 |  4212 | `	pTos++;` |
|       84 |  4213 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4214 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4215 | `	break;` |
|        - |  4216 | `/*` |
|        - |  4217 | ` * NSSWITCH: * * P3` |
|        - |  4218 | ` *` |
|        - |  4219 | ` * Switch the active namespace at runtime.` |
|        - |  4220 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4221 | ` */` |
|     7689 |  4222 | `case PH7_OP_NSSWITCH:` |
|    15380 |  4223 | `	SyBlobReset(&pVm->sNamespace);` |
|    15380 |  4224 | `	if( pInstr->p3 ){` |
|       98 |  4225 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4226 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4227 | `	}` |
|        - |  4228 | `	/* Clear namespace-scoped use-const imports */` |
|    15380 |  4229 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15380 |  4230 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15380 |  4231 | `	break;` |
|        - |  4232 | `/* OP_USECONST P1 * P3` |
|        - |  4233 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4234 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4235 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4236 | ` */` |
|        7 |  4237 | `case PH7_OP_USECONST: {` |
|       16 |  4238 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4239 | `	if( azPair ){` |
|       16 |  4240 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4241 | `	}` |
|       16 |  4242 | `	break;` |
|        - |  4243 | `				}` |
|        - |  4244 | `/*` |
|        - |  4245 | ` * CVT_INT: * * *` |
|        - |  4246 | ` *` |
|        - |  4247 | ` * Force the top of the stack to be an integer.` |
|        - |  4248 | ` */` |
|       78 |  4249 | `case PH7_OP_CVT_INT:` |
|        - |  4250 | `#ifdef UNTRUST` |
|        - |  4251 | `	if( pTos < pStack ){` |
|        - |  4252 | `		goto Abort;` |
|        - |  4253 | `	}` |
|        - |  4254 | `#endif` |
|      158 |  4255 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4256 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4257 | `	}` |
|        - |  4258 | `	/* Invalidate any prior representation */` |
|      158 |  4259 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      158 |  4260 | `	break;` |
|        - |  4261 | `/*` |
|        - |  4262 | ` * CVT_REAL: * * *` |
|        - |  4263 | ` *` |
|        - |  4264 | ` * Force the top of the stack to be a real.` |
|        - |  4265 | ` */` |
|        5 |  4266 | `case PH7_OP_CVT_REAL:` |
|        - |  4267 | `#ifdef UNTRUST` |
|        - |  4268 | `	if( pTos < pStack ){` |
|        - |  4269 | `		goto Abort;` |
|        - |  4270 | `	}` |
|        - |  4271 | `#endif` |
|       11 |  4272 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4273 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4274 | `	}` |
|        - |  4275 | `	/* Invalidate any prior representation */` |
|       11 |  4276 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4277 | `	break;` |
|        - |  4278 | `/*` |
|        - |  4279 | ` * CVT_STR: * * *` |
|        - |  4280 | ` *` |
|        - |  4281 | ` * Force the top of the stack to be a string.` |
|        - |  4282 | ` */` |
|      149 |  4283 | `case PH7_OP_CVT_STR:` |
|        - |  4284 | `#ifdef UNTRUST` |
|        - |  4285 | `	if( pTos < pStack ){` |
|        - |  4286 | `		goto Abort;` |
|        - |  4287 | `	}` |
|        - |  4288 | `#endif` |
|      300 |  4289 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  4290 | `		PH7_MemObjToString(pTos);` |
|      149 |  4291 | `	}` |
|      300 |  4292 | `	break;` |
|        - |  4293 | `/*` |
|        - |  4294 | ` * CVT_BOOL: * * *` |
|        - |  4295 | ` *` |
|        - |  4296 | ` * Force the top of the stack to be a boolean.` |
|        - |  4297 | ` */` |
|        5 |  4298 | `case PH7_OP_CVT_BOOL:` |
|        - |  4299 | `#ifdef UNTRUST` |
|        - |  4300 | `	if( pTos < pStack ){` |
|        - |  4301 | `		goto Abort;` |
|        - |  4302 | `	}` |
|        - |  4303 | `#endif` |
|       11 |  4304 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4305 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4306 | `	}` |
|       11 |  4307 | `	break;` |
|        - |  4308 | `/*` |
|        - |  4309 | ` * CVT_NULL: * * *` |
|        - |  4310 | ` *` |
|        - |  4311 | ` * Nullify the top of the stack.` |
|        - |  4312 | ` */` |
|        3 |  4313 | `case PH7_OP_CVT_NULL:` |
|        - |  4314 | `#ifdef UNTRUST` |
|        - |  4315 | `	if( pTos < pStack ){` |
|        - |  4316 | `		goto Abort;` |
|        - |  4317 | `	}` |
|        - |  4318 | `#endif` |
|        7 |  4319 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4320 | `	break;` |
|        - |  4321 | `/*` |
|        - |  4322 | ` * CVT_NUMC: * * *` |
|        - |  4323 | ` *` |
|        - |  4324 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4325 | ` */` |
|      ! 0 |  4326 | `case PH7_OP_CVT_NUMC:` |
|        - |  4327 | `#ifdef UNTRUST` |
|        - |  4328 | `	if( pTos < pStack ){` |
|        - |  4329 | `		goto Abort;` |
|        - |  4330 | `	}` |
|        - |  4331 | `#endif` |
|        - |  4332 | `	/* Force a numeric cast */` |
|      ! 0 |  4333 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4334 | `	break;` |
|        - |  4335 | `/*` |
|        - |  4336 | ` * CVT_ARRAY: * * *` |
|        - |  4337 | ` *` |
|        - |  4338 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4339 | ` */` |
|       10 |  4340 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4341 | `#ifdef UNTRUST` |
|        - |  4342 | `	if( pTos < pStack ){` |
|        - |  4343 | `		goto Abort;` |
|        - |  4344 | `	}` |
|        - |  4345 | `#endif` |
|        - |  4346 | `	/* Force a hashmap cast */` |
|       21 |  4347 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4348 | `	if( rc != SXRET_OK ){` |
|        - |  4349 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4350 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4351 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4352 | `	}` |
|       21 |  4353 | `	break;` |
|        - |  4354 | `/*` |
|        - |  4355 | ` * CVT_OBJ: * * *` |
|        - |  4356 | ` *` |
|        - |  4357 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4358 | ` */` |
|        8 |  4359 | `case PH7_OP_CVT_OBJ:` |
|        - |  4360 | `#ifdef UNTRUST` |
|        - |  4361 | `	if( pTos < pStack ){` |
|        - |  4362 | `		goto Abort;` |
|        - |  4363 | `	}` |
|        - |  4364 | `#endif` |
|       17 |  4365 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4366 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4367 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4368 | `	}` |
|       17 |  4369 | `	break;` |
|        - |  4370 | `/*` |
|        - |  4371 | ` * ERR_CTRL * * *` |
|        - |  4372 | ` *` |
|        - |  4373 | ` * Error control operator.` |
|        - |  4374 | ` */` |
|    15712 |  4375 | `case PH7_OP_ERR_CTRL:` |
|        - |  4376 | `	/*` |
|        - |  4377 | `	 * TICKET 1433-038:` |
|        - |  4378 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4379 | `	 * use the public API,to control error output.` |
|        - |  4380 | `	 */` |
|    31424 |  4381 | `	break;` |
|        - |  4382 | `/*` |
|        - |  4383 | ` * IS_A * * *` |
|        - |  4384 | ` *` |
|        - |  4385 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4386 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4387 | ` * holding a class name or an object).` |
|        - |  4388 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4389 | ` */` |
|       64 |  4390 | `case PH7_OP_IS_A:{` |
|      130 |  4391 | `	ph7_value *pNos = &pTos[-1];` |
|      130 |  4392 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4393 | `#ifdef UNTRUST` |
|        - |  4394 | `	if( pNos < pStack ){` |
|        - |  4395 | `		goto Abort;` |
|        - |  4396 | `	}` |
|        - |  4397 | `#endif` |
|      130 |  4398 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      128 |  4399 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      128 |  4400 | `		ph7_class *pClass = 0;` |
|        - |  4401 | `		/* Extract the target class */` |
|      128 |  4402 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4403 | `			/* Instance already loaded */` |
|      ! 0 |  4404 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      128 |  4405 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      128 |  4406 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      128 |  4407 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4408 | `			/* Handle self/static/parent keywords */` |
|      128 |  4409 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4410 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      126 |  4411 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4412 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      125 |  4413 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4414 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4415 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4416 | `					pClass = pSelf->pBase;` |
|        2 |  4417 | `				}` |
|        3 |  4418 | `			}else{` |
|      118 |  4419 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4420 | `			}` |
|       63 |  4421 | `		}` |
|      128 |  4422 | `		if( pClass ){` |
|        - |  4423 | `			/* Perform the query */` |
|      128 |  4424 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       63 |  4425 | `		}` |
|       63 |  4426 | `	}` |
|        - |  4427 | `	/* Push result */` |
|      130 |  4428 | `	VmPopOperand(&pTos,1);` |
|      130 |  4429 | `	PH7_MemObjRelease(pTos);` |
|      130 |  4430 | `	pTos->x.iVal = iRes;` |
|      130 |  4431 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      130 |  4432 | `	break;` |
|        - |  4433 | `				 }` |
|        - |  4434 |  |
|        - |  4435 | `/*` |
|        - |  4436 | ` * LOADC P1 P2 *` |
|        - |  4437 | ` *` |
|        - |  4438 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4439 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4440 | ` */` |
|   996082 |  4441 | `case PH7_OP_LOADC: {` |
|        - |  4442 | `	ph7_value *pObj;` |
|        - |  4443 | `	/* Reserve a room */` |
|  1992210 |  4444 | `	pTos++;` |
|  2978682 |  4445 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1992210 |  4446 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4447 | `			SyHashEntry *pEntry;` |
|        - |  4448 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4449 | `			{` |
|        - |  4450 | `				SyHashEntry *pConstImport;` |
|    28970 |  4451 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19312 |  4452 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19314 |  4453 | `				if( pConstImport ){` |
|       11 |  4454 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4455 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4456 | `					if( pEntry ){` |
|       11 |  4457 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4458 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4459 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4460 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4461 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4462 | `						break;` |
|        - |  4463 | `					}` |
|        - |  4464 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4465 | `				}` |
|        - |  4466 | `			}` |
|        - |  4467 | `			/* Candidate for expansion via user defined callbacks */` |
|    19304 |  4468 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19304 |  4469 | `			if( pEntry ){` |
|    19298 |  4470 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4471 | `				/* Set a NULL default value */` |
|    19298 |  4472 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19298 |  4473 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4474 | `				/* Invoke the callback and deal with the expanded value */` |
|    19298 |  4475 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4476 | `				/* Mark as constant */` |
|    19298 |  4477 | `				pTos->nIdx = SXU32_HIGH;` |
|    19298 |  4478 | `				break;` |
|        - |  4479 | `			}` |
|        - |  4480 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4481 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4482 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4483 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4484 | `			{` |
|        8 |  4485 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4486 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4487 | `				sxu32 j;` |
|        8 |  4488 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4489 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4490 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4491 | `				}` |
|        8 |  4492 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4493 | `					/* Try current_namespace\name */` |
|      ! 0 |  4494 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4495 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4496 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4497 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4498 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4499 | `					if( pEntry ){` |
|      ! 0 |  4500 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4501 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4502 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4503 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4504 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4505 | `						break;` |
|        - |  4506 | `					}` |
|        - |  4507 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4508 | `				}` |
|        8 |  4509 | `				if( isQualified ){` |
|        - |  4510 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4511 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4512 | `					SyBlob sErr;` |
|        3 |  4513 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4514 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4515 | `					if( pErrFile ){` |
|        3 |  4516 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4517 | `					}` |
|        3 |  4518 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4519 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4520 | `					SyBlobRelease(&sErr);` |
|        3 |  4521 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4522 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4523 | `					goto LoadC_Done;` |
|        - |  4524 | `				}` |
|        - |  4525 | `			}` |
|        2 |  4526 | `		}` |
|  1972902 |  4527 | `		PH7_MemObjLoad(pObj,pTos);` |
|   986474 |  4528 | `	}else{` |
|        - |  4529 | `		/* Set a NULL value */` |
|      ! 0 |  4530 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4531 | `	}` |
|   986429 |  4532 | `LoadC_Done:` |
|        - |  4533 | `	/* Mark as constant */` |
|  1972904 |  4534 | `	pTos->nIdx = SXU32_HIGH;` |
|  1972904 |  4535 | `	break;` |
|        - |  4536 | `				  }` |
|        - |  4537 | `/*` |
|        - |  4538 | ` * LOAD: P1 * P3` |
|        - |  4539 | ` *` |
|        - |  4540 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4541 | ` * from the P3 operand.` |
|        - |  4542 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4543 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4544 | ` */` |
|  1559120 |  4545 | `case PH7_OP_LOAD:{` |
|        - |  4546 | `	ph7_value *pObj;` |
|        - |  4547 | `	SyString sName;` |
|  3118462 |  4548 | `	if( pInstr->p3 == 0 ){` |
|        - |  4549 | `		/* Take the variable name from the top of the stack */` |
|        - |  4550 | `#ifdef UNTRUST` |
|        - |  4551 | `		if( pTos < pStack ){` |
|        - |  4552 | `			goto Abort;` |
|        - |  4553 | `		}` |
|        - |  4554 | `#endif` |
|        - |  4555 | `		/* Force a string cast */` |
|       19 |  4556 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4557 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4558 | `		}` |
|       19 |  4559 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4560 | `	}else{` |
|  3118444 |  4561 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4562 | `		/* Reserve a room for the target object */` |
|  3118444 |  4563 | `		pTos++;` |
|        - |  4564 | `	}` |
|        - |  4565 | `	/* Extract the requested memory object */` |
|  3118462 |  4566 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3118462 |  4567 | `	if( pObj == 0 ){` |
|      836 |  4568 | `		if( pInstr->iP1 ){` |
|        - |  4569 | `			/* Variable not found,load NULL */` |
|      836 |  4570 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4571 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4572 | `			}else{` |
|      836 |  4573 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4574 | `			}` |
|      836 |  4575 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1559539 |  4576 | `			break;` |
|      ! 0 |  4577 | `		}else{` |
|        - |  4578 | `			/* Fatal error */` |
|      ! 0 |  4579 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4580 | `			goto Abort;` |
|        - |  4581 | `		}` |
|        - |  4582 | `	}` |
|        - |  4583 | `	/* Load variable contents */` |
|  3117628 |  4584 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3117628 |  4585 | `	pTos->nIdx = pObj->nIdx;` |
|  3117628 |  4586 | `	break;` |
|        - |  4587 | `				   }` |
|        - |  4588 | `/*` |
|        - |  4589 | ` * LOAD_MAP P1 * *` |
|        - |  4590 | ` *` |
|        - |  4591 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4592 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4593 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4594 | ` */` |
|    22238 |  4595 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4596 | `	ph7_hashmap *pMap;` |
|        - |  4597 | `	/* Allocate a new hashmap instance */` |
|    44478 |  4598 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    44478 |  4599 | `	if( pMap == 0 ){` |
|      ! 0 |  4600 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4601 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4602 | `		goto Abort;` |
|        - |  4603 | `	}` |
|    44478 |  4604 | `	if( pInstr->iP1 > 0 ){` |
|     2574 |  4605 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2574 |  4606 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4607 | `		/* Perform the insertion */` |
|     7862 |  4608 | `		while( pEntry < pTos ){` |
|     5306 |  4609 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4610 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4611 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4612 | `				 * renumbered. Same routine that backs array_merge. */` |
|       64 |  4613 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       47 |  4614 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       47 |  4615 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4616 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4617 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4618 | `						 * map dangling. */` |
|      ! 0 |  4619 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4620 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4621 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4622 | `						break;` |
|        - |  4623 | `					}` |
|       24 |  4624 | `				}else{` |
|        - |  4625 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4626 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4627 | `					break;` |
|        1 |  4628 | `				}` |
|     5267 |  4629 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4630 | `				/* Insertion by reference */` |
|      142 |  4631 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4632 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4633 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4634 | `					);` |
|       48 |  4635 | `			}else{` |
|        - |  4636 | `				/* Standard insertion */` |
|     7724 |  4637 | `				PH7_HashmapInsert(pMap,` |
|     5148 |  4638 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2574 |  4639 | `					&pEntry[1]` |
|        - |  4640 | `				);` |
|        - |  4641 | `			}` |
|        - |  4642 | `			/* Next pair on the stack */` |
|     5290 |  4643 | `			pEntry += 2;` |
|        2 |  4644 | `		}` |
|        - |  4645 | `		/* Pop P1 elements */` |
|     2574 |  4646 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2574 |  4647 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4648 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4649 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4650 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4651 | `				goto Abort;` |
|        - |  4652 | `			}` |
|        - |  4653 | `			{` |
|       17 |  4654 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4655 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4656 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4657 | `					break;` |
|        - |  4658 | `				}` |
|        - |  4659 | `			}` |
|       15 |  4660 | `			goto Exception;` |
|        - |  4661 | `		}` |
|     1278 |  4662 | `	}` |
|        - |  4663 | `	/* Push the hashmap */` |
|    44462 |  4664 | `	pTos++;` |
|    44462 |  4665 | `	pTos->nIdx = SXU32_HIGH;` |
|    44462 |  4666 | `	pTos->x.pOther = pMap;` |
|    44462 |  4667 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    44462 |  4668 | `	break;` |
|        - |  4669 | `					  }` |
|        - |  4670 | `/*` |
|        - |  4671 | ` * LOAD_LIST: P1 * *` |
|        - |  4672 | ` *` |
|        - |  4673 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4674 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4675 | ` * Caveats:` |
|        - |  4676 | ` *  This implementation support only a single nesting level.` |
|        - |  4677 | ` */` |
|       48 |  4678 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4679 | `	ph7_value *pEntry;` |
|       98 |  4680 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4681 | `		/* Empty list,break immediately */` |
|      ! 0 |  4682 | `		break;` |
|        - |  4683 | `	}` |
|       98 |  4684 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4685 | `#ifdef UNTRUST` |
|        - |  4686 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4687 | `		goto Abort;` |
|        - |  4688 | `	}` |
|        - |  4689 | `#endif` |
|       98 |  4690 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4691 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4692 | `		ph7_hashmap_node *pNode;` |
|        - |  4693 | `		ph7_value sKey,*pObj;` |
|        - |  4694 | `		/* Start Copying */` |
|       91 |  4695 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4696 | `		while( pEntry <= pTos ){` |
|      193 |  4697 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4698 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4699 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4700 | `					if( rc == SXRET_OK ){` |
|        - |  4701 | `						/* Store node value */` |
|      165 |  4702 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4703 | `					}else{` |
|        - |  4704 | `						/* Undefined array key */` |
|        - |  4705 | `						char zMsg[128];` |
|      ! 0 |  4706 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4707 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4708 | `						PH7_MemObjRelease(pObj);` |
|        - |  4709 | `					}` |
|       82 |  4710 | `				}` |
|       82 |  4711 | `			}` |
|      193 |  4712 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4713 | `			pEntry++;` |
|        1 |  4714 | `		}` |
|       46 |  4715 | `	}else{` |
|        - |  4716 | `		/* Source is not an array */` |
|        - |  4717 | `		ph7_value *pObj;` |
|       18 |  4718 | `		while( pEntry <= pTos ){` |
|       12 |  4719 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4720 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4721 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4722 | `				}` |
|        5 |  4723 | `			}` |
|       12 |  4724 | `			pEntry++;` |
|        2 |  4725 | `		}` |
|        8 |  4726 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4727 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4728 | `			const char *zType = "unknown";` |
|        3 |  4729 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4730 | `			char zMsg[256];` |
|        3 |  4731 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4732 | `				zType = "string";` |
|        1 |  4733 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4734 | `				zType = "int";` |
|      ! 0 |  4735 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4736 | `				zType = "float";` |
|      ! 0 |  4737 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4738 | `				zType = "object";` |
|      ! 0 |  4739 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4740 | `				zType = "resource";` |
|      ! 0 |  4741 | `			}` |
|        3 |  4742 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4743 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4744 | `		}` |
|        - |  4745 | `	}` |
|       98 |  4746 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4747 | `	break;` |
|        - |  4748 | `					   }` |
|        - |  4749 | `/*` |
|        - |  4750 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4751 | ` *` |
|        - |  4752 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4753 | ` * from the stack.` |
|        - |  4754 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4755 | ` * instead.` |
|        - |  4756 | ` */` |
|   249553 |  4757 | `case PH7_OP_LOAD_IDX: {` |
|   499152 |  4758 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   499152 |  4759 | `	ph7_hashmap *pMap = 0;` |
|        - |  4760 | `	ph7_value *pIdx;` |
|   499152 |  4761 | `	pIdx = 0;` |
|   499152 |  4762 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4763 | `		if( !pInstr->iP2){` |
|        - |  4764 | `			/* No available index,load NULL */` |
|      ! 0 |  4765 | `			if( pTos >= pStack ){` |
|      ! 0 |  4766 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4767 | `			}else{` |
|        - |  4768 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4769 | `				pTos++;` |
|      ! 0 |  4770 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4771 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4772 | `			}` |
|        - |  4773 | `			/* Emit a notice */` |
|      ! 0 |  4774 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4775 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4776 | `			break;` |
|        - |  4777 | `		}` |
|      ! 0 |  4778 | `	}else{` |
|   499152 |  4779 | `		pIdx = pTos;` |
|   499152 |  4780 | `		pTos--;` |
|        - |  4781 | `	}` |
|   499152 |  4782 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4783 | `		/* String access */` |
|   387716 |  4784 | `		if( pIdx ){` |
|        - |  4785 | `			sxu32 nOfft;` |
|   387716 |  4786 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4787 | `				/* Force an int cast */` |
|      ! 0 |  4788 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4789 | `			}` |
|   387716 |  4790 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4791 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4792 | `				/* Invalid offset,load null */` |
|      ! 0 |  4793 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4794 | `			}else{` |
|   387716 |  4795 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4796 | `				int c = zData[nOfft];` |
|   387716 |  4797 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4798 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4799 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4800 | `			}` |
|   193881 |  4801 | `		}else{` |
|        - |  4802 | `			/* No available index,load NULL */` |
|      ! 0 |  4803 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4804 | `		}` |
|   387716 |  4805 | `		break;` |
|        - |  4806 | `	}` |
|   111438 |  4807 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4808 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4809 | `		 * iP2 codes:` |
|        - |  4810 | `		 *   0 = read       → offsetGet` |
|        - |  4811 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4812 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4813 | `		 *   4 = isset()    → offsetExists` |
|        - |  4814 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4815 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4816 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4817 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4818 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4819 | `			ph7_class_method *pMeth;` |
|        - |  4820 | `			ph7_value sResult;` |
|        - |  4821 | `			ph7_value *apArg[1];` |
|      124 |  4822 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4823 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4824 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4825 | `					"Cannot use [] for reading");` |
|      ! 0 |  4826 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4827 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4828 | `				break;` |
|        - |  4829 | `			}` |
|      124 |  4830 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4831 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4832 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4833 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4834 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4835 | `				apArg[0] = pIdx;` |
|       51 |  4836 | `				if( pMeth ){` |
|       51 |  4837 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4838 | `				}` |
|       99 |  4839 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4840 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4841 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4842 | `				apArg[0] = pIdx;` |
|        9 |  4843 | `				if( pMeth ){` |
|        9 |  4844 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4845 | `				}` |
|        5 |  4846 | `			}else{` |
|       66 |  4847 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4848 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4849 | `				apArg[0] = pIdx;` |
|       66 |  4850 | `				if( pMeth ){` |
|       66 |  4851 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4852 | `				}` |
|        - |  4853 | `			}` |
|      124 |  4854 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4855 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4856 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4857 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4858 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4859 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4860 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4861 | `				if( bExists ){` |
|       17 |  4862 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4863 | `					pTos->x.iVal = 1;` |
|        9 |  4864 | `				}else{` |
|       17 |  4865 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4866 | `				}` |
|      108 |  4867 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4868 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4869 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4870 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4871 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4872 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4873 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4874 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4875 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4876 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4877 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4878 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4879 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4880 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4881 | `				if( !bExists ){` |
|        3 |  4882 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4883 | `				}else{` |
|        9 |  4884 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4885 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4886 | `					ph7_value sValue;` |
|        9 |  4887 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4888 | `					apArg[0] = pIdx;` |
|        9 |  4889 | `					if( pGet ){` |
|        9 |  4890 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4891 | `					}` |
|        9 |  4892 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4893 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4894 | `				}` |
|       11 |  4895 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4896 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4897 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4898 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4899 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4900 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4901 | `				 *     and push NULL.` |
|        - |  4902 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4903 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4904 | `				int bShouldArm = !bExists;` |
|        - |  4905 | `				ph7_value sValue;` |
|        9 |  4906 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4907 | `				/* Reset any prior arming defensively */` |
|        9 |  4908 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4909 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4910 | `				if( bExists ){` |
|        5 |  4911 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4912 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4913 | `					apArg[0] = pIdx;` |
|        5 |  4914 | `					if( pGet ){` |
|        5 |  4915 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4916 | `					}` |
|        5 |  4917 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4918 | `						bShouldArm = 1;` |
|        1 |  4919 | `					}` |
|        2 |  4920 | `				}` |
|        9 |  4921 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4922 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4923 | `				if( bShouldArm ){` |
|        - |  4924 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  4925 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  4926 | `					 * intervening expression evaluation. */` |
|        7 |  4927 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  4928 | `					if( pIdx ){` |
|        7 |  4929 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  4930 | `					}` |
|        7 |  4931 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  4932 | `					pInst->iRef++;` |
|        7 |  4933 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  4934 | `				}else{` |
|        3 |  4935 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  4936 | `				}` |
|        9 |  4937 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  4938 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  4939 | `				break;` |
|      ! 0 |  4940 | `			}else{` |
|        - |  4941 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  4942 | `				PH7_MemObjRelease(pTos);` |
|       66 |  4943 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  4944 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4945 | `			}` |
|      106 |  4946 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  4947 | `			if( pIdx ){` |
|      106 |  4948 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  4949 | `			}` |
|      106 |  4950 | `			break;` |
|        - |  4951 | `		}` |
|        - |  4952 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  4953 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  4954 | `		if( pInst ){` |
|        - |  4955 | `			char zMsg[256];` |
|        3 |  4956 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  4957 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  4958 | `				"Cannot use object of type %.*s as array",` |
|        2 |  4959 | `				(int)pName->nByte,pName->zString);` |
|        3 |  4960 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  4961 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  4962 | `			PH7_MemObjRelease(pTos);` |
|        3 |  4963 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4964 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  4965 | `			break;` |
|        - |  4966 | `		}` |
|      ! 0 |  4967 | `	}` |
|   111314 |  4968 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4969 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4970 | `			ph7_value *pObj;` |
|        3 |  4971 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4972 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4973 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4974 | `			}` |
|        1 |  4975 | `		}` |
|        1 |  4976 | `	}` |
|   111314 |  4977 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   111314 |  4978 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   111314 |  4979 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  4980 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4981 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4982 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4983 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  4984 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  4985 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  4986 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  4987 | `		}` |
|        - |  4988 | `		/* Point to the hashmap */` |
|   111314 |  4989 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   111314 |  4990 | `		if( pIdx ){` |
|        - |  4991 | `			/* Load the desired entry */` |
|   111314 |  4992 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    55656 |  4993 | `		}` |
|   111314 |  4994 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4995 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4996 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4997 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4998 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4999 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5000 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5001 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5002 | `			 * correct for the outermost write. */` |
|       19 |  5003 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5004 | `			if( !needWrite && pNode ){` |
|       13 |  5005 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5006 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5007 | `					needWrite = 1;` |
|        3 |  5008 | `				}` |
|        6 |  5009 | `			}` |
|       19 |  5010 | `			if( needWrite ){` |
|       13 |  5011 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5012 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5013 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5014 | `					 * into the new map's storage. */` |
|        7 |  5015 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5016 | `					if( pIdx ){` |
|        7 |  5017 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5018 | `					}` |
|        3 |  5019 | `				}` |
|        6 |  5020 | `			}` |
|        9 |  5021 | `		}` |
|   111314 |  5022 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5023 | `			/* Create a new empty entry */` |
|      273 |  5024 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5025 | `			if( rc == SXRET_OK ){` |
|        - |  5026 | `				/* Point to the last inserted entry */` |
|      273 |  5027 | `				pNode = pMap->pLast;` |
|      136 |  5028 | `			}` |
|      136 |  5029 | `		}` |
|    55656 |  5030 | `	}` |
|   111314 |  5031 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5032 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5033 | `		char zMsg[128];` |
|      ! 0 |  5034 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5035 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5036 | `		}` |
|      ! 0 |  5037 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5038 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5039 | `	}` |
|   111314 |  5040 | `	if( pIdx ){` |
|   111314 |  5041 | `		PH7_MemObjRelease(pIdx);` |
|    55656 |  5042 | `	}` |
|   111314 |  5043 | `	if( rc == SXRET_OK ){` |
|        - |  5044 | `		/* Load entry contents */` |
|    49404 |  5045 | `		if( pMap->iRef < 2 ){` |
|        - |  5046 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5047 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5048 | `			 */` |
|       28 |  5049 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5050 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5051 | `		}else{` |
|    49378 |  5052 | `			pTos->nIdx = pNode->nValIdx;` |
|    49378 |  5053 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    49378 |  5054 | `			PH7_HashmapUnref(pMap);` |
|        - |  5055 | `		}` |
|    24703 |  5056 | `	}else{` |
|        - |  5057 | `		/* No such entry,load NULL */` |
|    61912 |  5058 | `		PH7_MemObjRelease(pTos);` |
|    61912 |  5059 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5060 | `	}` |
|   111314 |  5061 | `	break;` |
|        - |  5062 | `					  }` |
|        - |  5063 | `/*` |
|        - |  5064 | ` * LOAD_CLOSURE * * P3` |
|        - |  5065 | ` *` |
|        - |  5066 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5067 | ` * name in the stack.` |
|        - |  5068 | ` */` |
|       47 |  5069 | `case PH7_OP_LOAD_CLOSURE:{` |
|       96 |  5070 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       96 |  5071 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5072 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5073 | `		ph7_vm_func *pClosure;` |
|        - |  5074 | `		char *zName;` |
|        - |  5075 | `		sxu32 mLen;` |
|        - |  5076 | `		sxu32 n;` |
|        - |  5077 | `		/* Create a new VM function */` |
|       96 |  5078 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5079 | `		/* Generate an unique closure name */` |
|       96 |  5080 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       96 |  5081 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5082 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5083 | `			goto Abort;` |
|        - |  5084 | `		}` |
|       96 |  5085 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       96 |  5086 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5087 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5088 | `		}` |
|        - |  5089 | `		/* Zero the stucture */` |
|       96 |  5090 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5091 | `		/* Perform a structure assignment on read-only items */` |
|       96 |  5092 | `		pClosure->aArgs = pFunc->aArgs;` |
|       96 |  5093 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       96 |  5094 | `		pClosure->aStatic = pFunc->aStatic;` |
|       96 |  5095 | `		pClosure->iFlags = pFunc->iFlags;` |
|       96 |  5096 | `		pClosure->pUserData = pFunc->pUserData;` |
|       96 |  5097 | `		pClosure->sSignature = pFunc->sSignature;` |
|       96 |  5098 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       96 |  5099 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       96 |  5100 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       96 |  5101 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       96 |  5102 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5103 | `		/* Register the closure */` |
|       96 |  5104 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5105 | `		/* Set up closure environment */` |
|       96 |  5106 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       96 |  5107 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      256 |  5108 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5109 | `			ph7_value *pValue;` |
|      162 |  5110 | `			pEnv = &aEnv[n];` |
|      162 |  5111 | `			sEnv.sName  = pEnv->sName;` |
|      162 |  5112 | `			sEnv.iFlags = pEnv->iFlags;` |
|      162 |  5113 | `			sEnv.nIdx = SXU32_HIGH;` |
|      162 |  5114 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      162 |  5115 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5116 | `				/* Pass by reference */` |
|      ! 0 |  5117 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5118 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5119 | `					);` |
|      ! 0 |  5120 | `			}` |
|        - |  5121 | `			/* Standard pass by value */` |
|      162 |  5122 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      162 |  5123 | `			if( pValue ){` |
|        - |  5124 | `				/* Copy imported value */` |
|       72 |  5125 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5126 | `			}` |
|        - |  5127 | `			/* Insert the imported variable */` |
|      162 |  5128 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       82 |  5129 | `		}` |
|        - |  5130 | `		/* Finally,load the closure name on the stack */` |
|       96 |  5131 | `		pTos++;` |
|       96 |  5132 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       47 |  5133 | `	}` |
|       96 |  5134 | `	break;` |
|        - |  5135 | `						 }` |
|        - |  5136 | `/*` |
|        - |  5137 | ` * STORE * P2 P3` |
|        - |  5138 | ` *` |
|        - |  5139 | ` * Perform a store (Assignment) operation.` |
|        - |  5140 | ` */` |
|   140509 |  5141 | `case PH7_OP_STORE: {` |
|        - |  5142 | `	ph7_value *pObj;` |
|        - |  5143 | `	SyString sName;` |
|        - |  5144 | `#ifdef UNTRUST` |
|        - |  5145 | `	if( pTos < pStack ){` |
|        - |  5146 | `		goto Abort;` |
|        - |  5147 | `	}` |
|        - |  5148 | `#endif` |
|   281020 |  5149 | `	if( pInstr->iP2 ){` |
|        - |  5150 | `		sxu32 nIdx;` |
|        - |  5151 | `		sxi32 rcT;` |
|        - |  5152 | `		/* Member store operation */` |
|     4972 |  5153 | `		nIdx = pTos->nIdx;` |
|     4972 |  5154 | `		VmPopOperand(&pTos,1);` |
|     4972 |  5155 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5156 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5157 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5158 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5159 | `		}else{` |
|        - |  5160 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5161 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4968 |  5162 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4968 |  5163 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5164 | `				goto Abort;` |
|        - |  5165 | `			}` |
|     4968 |  5166 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5167 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5168 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5169 | `				 * propagate out of the VM loop. */` |
|       37 |  5170 | `				VmPopOperand(&pTos,1);` |
|        - |  5171 | `				{` |
|       37 |  5172 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5173 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5174 | `						pc = pFrm2->iExceptionJump - 1;` |
|   140528 |  5175 | `						break;` |
|        - |  5176 | `					}` |
|        - |  5177 | `				}` |
|      ! 0 |  5178 | `				goto Exception;` |
|        - |  5179 | `			}` |
|        - |  5180 | `			/* Point to the desired memory object */` |
|     4932 |  5181 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4932 |  5182 | `			if( pObj ){` |
|        - |  5183 | `				/* Perform the store operation */` |
|     4932 |  5184 | `				PH7_MemObjStore(pTos,pObj);` |
|     2465 |  5185 | `			}` |
|        - |  5186 | `		}` |
|     4936 |  5187 | `		break;` |
|   276050 |  5188 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5189 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5190 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5191 | `			/* Force a string cast */` |
|      ! 0 |  5192 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5193 | `		}` |
|        7 |  5194 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5195 | `		pTos--;` |
|        - |  5196 | `#ifdef UNTRUST` |
|        - |  5197 | `		if( pTos < pStack  ){` |
|        - |  5198 | `			goto Abort;` |
|        - |  5199 | `		}` |
|        - |  5200 | `#endif` |
|        4 |  5201 | `	}else{` |
|   276044 |  5202 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5203 | `	}` |
|        - |  5204 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   276050 |  5205 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   276050 |  5206 | `	if( pObj == 0 ){` |
|      ! 0 |  5207 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5208 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5209 | `		goto Abort;` |
|        - |  5210 | `	}` |
|   276050 |  5211 | `	if( !pInstr->p3 ){` |
|        7 |  5212 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5213 | `	}` |
|        - |  5214 | `	/* Perform the store operation */` |
|   276050 |  5215 | `	PH7_MemObjStore(pTos,pObj);` |
|   276050 |  5216 | `	break;` |
|        - |  5217 | `				   }` |
|        - |  5218 | `/*` |
|        - |  5219 | ` * STORE_IDX:   P1 * P3` |
|        - |  5220 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5221 | ` *` |
|        - |  5222 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5223 | ` */` |
|    95318 |  5224 | `case PH7_OP_STORE_IDX:` |
|        - |  5225 | `case PH7_OP_STORE_IDX_REF: {` |
|   190638 |  5226 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5227 | `	ph7_value *pKey;` |
|        - |  5228 | `	sxu32 nIdx;` |
|   190638 |  5229 | `	if( pInstr->iP1 ){` |
|        - |  5230 | `		/* Key is next on stack */` |
|    62706 |  5231 | `		pKey = pTos;` |
|    62706 |  5232 | `		pTos--;` |
|    31354 |  5233 | `	}else{` |
|   127934 |  5234 | `		pKey = 0;` |
|        - |  5235 | `	}` |
|   190638 |  5236 | `	nIdx = pTos->nIdx;` |
|        - |  5237 | `	{` |
|        - |  5238 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5239 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5240 | `		 * the backing variable slot at nIdx. */` |
|   190638 |  5241 | `		ph7_class_instance *pInst = 0;` |
|   190638 |  5242 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5243 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   190622 |  5244 | `		}else if( nIdx != SXU32_HIGH ){` |
|   190606 |  5245 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   190606 |  5246 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5247 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5248 | `			}` |
|    95302 |  5249 | `		}` |
|   190638 |  5250 | `		if( pInst ){` |
|       34 |  5251 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5252 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5253 | `				ph7_class_method *pMeth;` |
|        - |  5254 | `				ph7_value sNullKey;` |
|        - |  5255 | `				ph7_value *apArg[2];` |
|       32 |  5256 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5257 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5258 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5259 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5260 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5261 | `					break;` |
|        - |  5262 | `				}` |
|       32 |  5263 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5264 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5265 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5266 | `				VmPopOperand(&pTos,1);` |
|       32 |  5267 | `				if( pKey == 0 ){` |
|        7 |  5268 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5269 | `					apArg[0] = &sNullKey;` |
|        4 |  5270 | `				}else{` |
|       26 |  5271 | `					apArg[0] = pKey;` |
|        - |  5272 | `				}` |
|       32 |  5273 | `				apArg[1] = pTos;` |
|       32 |  5274 | `				if( pMeth ){` |
|       32 |  5275 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5276 | `				}` |
|       32 |  5277 | `				if( pKey ){` |
|       26 |  5278 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5279 | `				}else{` |
|        7 |  5280 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5281 | `				}` |
|        - |  5282 | `				/* Pop the value */` |
|       32 |  5283 | `				VmPopOperand(&pTos,1);` |
|       32 |  5284 | `				break;` |
|        - |  5285 | `			}` |
|        - |  5286 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5287 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5288 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5289 | `			 * a few lines below). Match PHP. */` |
|        - |  5290 | `			{` |
|        - |  5291 | `				char zMsg[256];` |
|        3 |  5292 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5293 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5294 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5295 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5296 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5297 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5298 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5299 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5300 | `				break;` |
|        - |  5301 | `			}` |
|        - |  5302 | `		}` |
|        - |  5303 | `	}` |
|   190606 |  5304 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5305 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5306 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5307 | `		 * checking true sharing count, then re-add after separation. */` |
|   190554 |  5308 | `		if( nIdx != SXU32_HIGH ){` |
|   190554 |  5309 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   285830 |  5310 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   190554 |  5311 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5312 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5313 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5314 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5315 | `				 * refcounts if the backing array was already separated. */` |
|   190554 |  5316 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   190554 |  5317 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   190554 |  5318 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   190554 |  5319 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   190554 |  5320 | `					pTos->x.pOther = pMap;` |
|    95278 |  5321 | `				}else{` |
|        - |  5322 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5323 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5324 | `					pMap = pCur;` |
|        - |  5325 | `				}` |
|    95278 |  5326 | `			}else{` |
|      ! 0 |  5327 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5328 | `			}` |
|    95278 |  5329 | `		}else{` |
|      ! 0 |  5330 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5331 | `		}` |
|   190554 |  5332 | `		if( pMap->iRef < 2 ){` |
|        - |  5333 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5334 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5335 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5336 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5337 | `			pMap->iRef = 2;` |
|      ! 0 |  5338 | `		}` |
|    95278 |  5339 | `	}else{` |
|        - |  5340 | `		ph7_value *pObj;` |
|       53 |  5341 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5342 | `		if( pObj == 0 ){` |
|      ! 0 |  5343 | `			if( pKey ){` |
|      ! 0 |  5344 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5345 | `			}` |
|      ! 0 |  5346 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5347 | `			break;` |
|        - |  5348 | `		}` |
|        - |  5349 | `		/* Phase#1: Load the array */` |
|       53 |  5350 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5351 | `			VmPopOperand(&pTos,1);` |
|       53 |  5352 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5353 | `				/* Force a string cast */` |
|      ! 0 |  5354 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5355 | `			}` |
|       53 |  5356 | `			if( pKey == 0 ){` |
|        - |  5357 | `				/* Append string */` |
|        3 |  5358 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5359 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5360 | `				}` |
|        2 |  5361 | `			}else{` |
|        - |  5362 | `				sxu32 nOfft;` |
|       51 |  5363 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5364 | `					/* Force an int cast */` |
|       51 |  5365 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5366 | `				}` |
|       51 |  5367 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5368 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5369 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5370 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5371 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5372 | `				}else{` |
|      ! 0 |  5373 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5374 | `						/* Perform an append operation */` |
|      ! 0 |  5375 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5376 | `					}` |
|        - |  5377 | `				}` |
|        - |  5378 | `			}` |
|       53 |  5379 | `			if( pKey ){` |
|       51 |  5380 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5381 | `			}` |
|       53 |  5382 | `			break;` |
|      ! 0 |  5383 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5384 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5385 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5386 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5387 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5388 | `				goto Abort;` |
|        - |  5389 | `			}` |
|      ! 0 |  5390 | `		}` |
|        - |  5391 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5392 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5393 | `	}` |
|   190554 |  5394 | `	VmPopOperand(&pTos,1);` |
|        - |  5395 | `	/* Phase#2: Perform the insertion */` |
|   190554 |  5396 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5397 | `		/* Insertion by reference */` |
|       15 |  5398 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5399 | `	}else{` |
|   190540 |  5400 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5401 | `	}` |
|   190554 |  5402 | `	if( pKey ){` |
|    62630 |  5403 | `		PH7_MemObjRelease(pKey);` |
|    31314 |  5404 | `	}` |
|   190554 |  5405 | `	break;` |
|        - |  5406 | `					   }` |
|        - |  5407 | `/*` |
|        - |  5408 | ` * INCR: P1 * *` |
|        - |  5409 | ` *` |
|        - |  5410 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5411 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5412 | ` * the stack and increment after that.` |
|        - |  5413 | ` */` |
|   167570 |  5414 | `case PH7_OP_INCR:` |
|        - |  5415 | `#ifdef UNTRUST` |
|        - |  5416 | `	if( pTos < pStack ){` |
|        - |  5417 | `		goto Abort;` |
|        - |  5418 | `	}` |
|        - |  5419 | `#endif` |
|   335186 |  5420 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335186 |  5421 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5422 | `			ph7_value *pObj;` |
|   335186 |  5423 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335186 |  5424 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5425 | `					/* Perl-style string increment.` |
|        - |  5426 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5427 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5428 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5429 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5430 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5431 | `					}` |
|       49 |  5432 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5433 | `					if( pInstr->iP1 ){` |
|        - |  5434 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5435 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5436 | `					}` |
|       25 |  5437 | `				}else{` |
|        - |  5438 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5439 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5440 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5441 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5442 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5443 | `					 * so its old-value view survives the coercion. */` |
|   335138 |  5444 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       11 |  5445 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 |  5446 | `					}` |
|        - |  5447 | `					/* Force a numeric cast on the variable */` |
|   335138 |  5448 | `					PH7_MemObjToNumeric(pObj);` |
|   335138 |  5449 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 |  5450 | `						pObj->rVal++;` |
|        3 |  5451 | `					}else{` |
|   335134 |  5452 | `						pObj->x.iVal++;` |
|        - |  5453 | `					}` |
|   335138 |  5454 | `					if( pInstr->iP1 ){` |
|        - |  5455 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5456 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5457 | `					}` |
|        - |  5458 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5459 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5460 | `				}` |
|   167614 |  5461 | `			}` |
|   167616 |  5462 | `		}else{` |
|      ! 0 |  5463 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5464 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5465 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5466 | `				}else{` |
|        - |  5467 | `					/* Force a numeric cast */` |
|      ! 0 |  5468 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5469 | `					/* Pre-increment */` |
|      ! 0 |  5470 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5471 | `						pTos->rVal++;` |
|        - |  5472 | `						/* Try to get an integer representation */` |
|      ! 0 |  5473 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5474 | `					}else{` |
|      ! 0 |  5475 | `						pTos->x.iVal++;` |
|      ! 0 |  5476 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5477 | `					}` |
|        - |  5478 | `				}` |
|      ! 0 |  5479 | `			}` |
|        - |  5480 | `		}` |
|   167614 |  5481 | `	}` |
|   335186 |  5482 | `	break;` |
|        - |  5483 | `/*` |
|        - |  5484 | ` * DECR: P1 * *` |
|        - |  5485 | ` *` |
|        - |  5486 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5487 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5488 | ` * and decrement after that.` |
|        - |  5489 | ` */` |
|        2 |  5490 | `case PH7_OP_DECR:` |
|        - |  5491 | `#ifdef UNTRUST` |
|        - |  5492 | `	if( pTos < pStack ){` |
|        - |  5493 | `		goto Abort;` |
|        - |  5494 | `	}` |
|        - |  5495 | `#endif` |
|        5 |  5496 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  5497 | `		/* Force a numeric cast */` |
|        5 |  5498 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  5499 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5500 | `			ph7_value *pObj;` |
|        5 |  5501 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  5502 | `				/* Force a numeric cast */` |
|        5 |  5503 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  5504 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5505 | `					pObj->rVal--;` |
|        - |  5506 | `					/* Try to get an integer representation */` |
|      ! 0 |  5507 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5508 | `				}else{` |
|        5 |  5509 | `					pObj->x.iVal--;` |
|        5 |  5510 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5511 | `				}` |
|        5 |  5512 | `				if( pInstr->iP1 ){` |
|        - |  5513 | `					/* Pre-icrement */` |
|      ! 0 |  5514 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5515 | `				}` |
|        2 |  5516 | `			}` |
|        3 |  5517 | `		}else{` |
|      ! 0 |  5518 | `			if( pInstr->iP1 ){` |
|        - |  5519 | `				/* Pre-increment */` |
|      ! 0 |  5520 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5521 | `					pTos->rVal--;` |
|        - |  5522 | `					/* Try to get an integer representation */` |
|      ! 0 |  5523 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5524 | `				}else{` |
|      ! 0 |  5525 | `					pTos->x.iVal--;` |
|      ! 0 |  5526 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5527 | `				}` |
|      ! 0 |  5528 | `			}` |
|        - |  5529 | `		}` |
|        2 |  5530 | `	}` |
|        5 |  5531 | `	break;` |
|        - |  5532 | `/*` |
|        - |  5533 | ` * UMINUS: * * *` |
|        - |  5534 | ` *` |
|        - |  5535 | ` * Perform a unary minus operation.` |
|        - |  5536 | ` */` |
|    29099 |  5537 | `case PH7_OP_UMINUS:` |
|        - |  5538 | `#ifdef UNTRUST` |
|        - |  5539 | `	if( pTos < pStack ){` |
|        - |  5540 | `		goto Abort;` |
|        - |  5541 | `	}` |
|        - |  5542 | `#endif` |
|        - |  5543 | `	/* Force a numeric (integer,real or both) cast */` |
|    58200 |  5544 | `	PH7_MemObjToNumeric(pTos);` |
|    58200 |  5545 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5546 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5547 | `	}` |
|    58200 |  5548 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    58170 |  5549 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29084 |  5550 | `	}` |
|    58200 |  5551 | `	break;` |
|        - |  5552 | `/*` |
|        - |  5553 | ` * UPLUS: * * *` |
|        - |  5554 | ` *` |
|        - |  5555 | ` * Perform a unary plus operation.` |
|        - |  5556 | ` */` |
|       18 |  5557 | `case PH7_OP_UPLUS:` |
|        - |  5558 | `#ifdef UNTRUST` |
|        - |  5559 | `	if( pTos < pStack ){` |
|        - |  5560 | `		goto Abort;` |
|        - |  5561 | `	}` |
|        - |  5562 | `#endif` |
|        - |  5563 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5564 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5565 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5566 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5567 | `	}` |
|       37 |  5568 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5569 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5570 | `	}` |
|       37 |  5571 | `	break;` |
|        - |  5572 | `/*` |
|        - |  5573 | ` * OP_LNOT: * * *` |
|        - |  5574 | ` *` |
|        - |  5575 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5576 | ` * with its complement.` |
|        - |  5577 | ` */` |
|    44603 |  5578 | `case PH7_OP_LNOT:` |
|        - |  5579 | `#ifdef UNTRUST` |
|        - |  5580 | `	if( pTos < pStack ){` |
|        - |  5581 | `		goto Abort;` |
|        - |  5582 | `	}` |
|        - |  5583 | `#endif` |
|        - |  5584 | `	/* Force a boolean cast */` |
|    89252 |  5585 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5586 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5587 | `	}` |
|    89252 |  5588 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89252 |  5589 | `	break;` |
|        - |  5590 | `/*` |
|        - |  5591 | ` * OP_BITNOT: * * *` |
|        - |  5592 | ` *` |
|        - |  5593 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5594 | ` * with its ones-complement.` |
|        - |  5595 | ` */` |
|       15 |  5596 | `case PH7_OP_BITNOT:` |
|        - |  5597 | `#ifdef UNTRUST` |
|        - |  5598 | `	if( pTos < pStack ){` |
|        - |  5599 | `		goto Abort;` |
|        - |  5600 | `	}` |
|        - |  5601 | `#endif` |
|        - |  5602 | `	/* Force an integer cast */` |
|       32 |  5603 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5604 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5605 | `	}` |
|       32 |  5606 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5607 | `	break;` |
|        - |  5608 | `/* OP_MUL * * *` |
|        - |  5609 | ` * OP_MUL_STORE * * *` |
|        - |  5610 | ` *` |
|        - |  5611 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5612 | ` * and push the result back onto the stack.` |
|        - |  5613 | ` */` |
|     1287 |  5614 | `case PH7_OP_MUL:` |
|        - |  5615 | `case PH7_OP_MUL_STORE: {` |
|     2576 |  5616 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5617 | `	/* Force the operand to be numeric */` |
|        - |  5618 | `#ifdef UNTRUST` |
|        - |  5619 | `	if( pNos < pStack ){` |
|        - |  5620 | `		goto Abort;` |
|        - |  5621 | `	}` |
|        - |  5622 | `#endif` |
|     2576 |  5623 | `	PH7_MemObjToNumeric(pTos);` |
|     2576 |  5624 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5625 | `	/* Perform the requested operation */` |
|     2576 |  5626 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5627 | `		/* Floating point arithemic */` |
|        - |  5628 | `		ph7_real a,b,r;` |
|       19 |  5629 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5630 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5631 | `		}` |
|       19 |  5632 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5633 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5634 | `		}` |
|       19 |  5635 | `		a = pNos->rVal;` |
|       19 |  5636 | `		b = pTos->rVal;` |
|       19 |  5637 | `		r = a * b;` |
|        - |  5638 | `		/* Push the result */` |
|       19 |  5639 | `		pNos->rVal = r;` |
|       19 |  5640 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5641 | `		/* Try to get an integer representation */` |
|       19 |  5642 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5643 | `	}else{` |
|        - |  5644 | `		/* Integer arithmetic */` |
|        - |  5645 | `		sxi64 a,b,r;` |
|     2558 |  5646 | `		a = pNos->x.iVal;` |
|     2558 |  5647 | `		b = pTos->x.iVal;` |
|     2558 |  5648 | `		r = a * b;` |
|        - |  5649 | `		/* Push the result */` |
|     2558 |  5650 | `		pNos->x.iVal = r;` |
|     2558 |  5651 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5652 | `	}` |
|     2576 |  5653 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5654 | `		ph7_value *pObj;` |
|       32 |  5655 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5656 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5657 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5658 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5659 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5660 | `		}` |
|       15 |  5661 | `	}` |
|     2576 |  5662 | `	VmPopOperand(&pTos,1);` |
|     2576 |  5663 | `	break;` |
|        - |  5664 | `				 }` |
|        - |  5665 | `/* OP_POW * * *` |
|        - |  5666 | ` * OP_POW_STORE * * *` |
|        - |  5667 | ` *` |
|        - |  5668 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5669 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5670 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5671 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5672 | ` */` |
|       66 |  5673 | `case PH7_OP_POW:` |
|        - |  5674 | `case PH7_OP_POW_STORE: {` |
|      133 |  5675 | `	ph7_value *pNos = &pTos[-1];` |
|      133 |  5676 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5677 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5678 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5679 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5680 | `	 */` |
|      133 |  5681 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      133 |  5682 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5683 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5684 | `	int bBothInt;` |
|      133 |  5685 | `	int usedInt = 0;` |
|        - |  5686 | `	ph7_real a, b, r;` |
|        - |  5687 | `#endif` |
|      133 |  5688 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5689 | `#ifdef UNTRUST` |
|        - |  5690 | `	if( pNos < pStack ){` |
|        - |  5691 | `		goto Abort;` |
|        - |  5692 | `	}` |
|        - |  5693 | `#endif` |
|      133 |  5694 | `	PH7_MemObjToNumeric(pTos);` |
|      133 |  5695 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5696 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      261 |  5697 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      128 |  5698 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      133 |  5699 | `	if( bBothInt ){` |
|      123 |  5700 | `		base_i = pBase->x.iVal;` |
|      123 |  5701 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5702 | `	}` |
|      133 |  5703 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5704 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5705 | `	}` |
|      133 |  5706 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      131 |  5707 | `		PH7_MemObjToReal(pExp);` |
|       65 |  5708 | `	}` |
|      133 |  5709 | `	a = pBase->rVal;` |
|      133 |  5710 | `	b = pExp->rVal;` |
|      133 |  5711 | `	r = pow(a, b);` |
|        - |  5712 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5713 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5714 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5715 | `	 * representable as double but not as signed int64. */` |
|      133 |  5716 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5717 | `		sxi64 result_i = 1;` |
|      117 |  5718 | `		sxi64 cur_base = base_i;` |
|      117 |  5719 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5720 | `		int overflow = 0;` |
|      401 |  5721 | `		while( cur_exp > 0 ){` |
|      289 |  5722 | `			if( cur_exp & 1 ){` |
|      189 |  5723 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5724 | `					overflow = 1;` |
|        3 |  5725 | `					break;` |
|        - |  5726 | `				}` |
|       93 |  5727 | `			}` |
|      287 |  5728 | `			cur_exp >>= 1;` |
|      287 |  5729 | `			if( cur_exp > 0 ){` |
|      181 |  5730 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5731 | `					overflow = 1;` |
|        3 |  5732 | `					break;` |
|        - |  5733 | `				}` |
|       89 |  5734 | `			}` |
|        1 |  5735 | `		}` |
|      117 |  5736 | `		if( !overflow ){` |
|      113 |  5737 | `			pNos->x.iVal = result_i;` |
|      113 |  5738 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5739 | `			usedInt = 1;` |
|       56 |  5740 | `		}` |
|       58 |  5741 | `	}` |
|      133 |  5742 | `	if( !usedInt ){` |
|       21 |  5743 | `		pNos->rVal = r;` |
|       21 |  5744 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       10 |  5745 | `	}` |
|        - |  5746 | `#else` |
|        - |  5747 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5748 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5749 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5750 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5751 | `	 * represented. */` |
|        - |  5752 | `	base_i = pBase->x.iVal;` |
|        - |  5753 | `	exp_i  = pExp->x.iVal;` |
|        - |  5754 | `	{` |
|        - |  5755 | `		sxi64 result_i = 1;` |
|        - |  5756 | `		sxi64 cur_base = base_i;` |
|        - |  5757 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5758 | `		if( cur_exp < 0 ){` |
|        - |  5759 | `			result_i = 0;` |
|        - |  5760 | `		}else{` |
|        - |  5761 | `			while( cur_exp > 0 ){` |
|        - |  5762 | `				if( cur_exp & 1 ){` |
|        - |  5763 | `					result_i *= cur_base;` |
|        - |  5764 | `				}` |
|        - |  5765 | `				cur_exp >>= 1;` |
|        - |  5766 | `				if( cur_exp > 0 ){` |
|        - |  5767 | `					cur_base *= cur_base;` |
|        - |  5768 | `				}` |
|        - |  5769 | `			}` |
|        - |  5770 | `		}` |
|        - |  5771 | `		pNos->x.iVal = result_i;` |
|        - |  5772 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5773 | `	}` |
|        - |  5774 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      133 |  5775 | `	if( bStore ){` |
|        - |  5776 | `		ph7_value *pObj;` |
|       23 |  5777 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5778 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5779 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5780 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5781 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5782 | `		}` |
|       11 |  5783 | `	}` |
|      133 |  5784 | `	VmPopOperand(&pTos,1);` |
|      133 |  5785 | `	break;` |
|        - |  5786 | `				 }` |
|        - |  5787 | `/* OP_ADD * * *` |
|        - |  5788 | ` *` |
|        - |  5789 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5790 | ` * and push the result back onto the stack.` |
|        - |  5791 | ` */` |
|      515 |  5792 | `case PH7_OP_ADD:{` |
|     1032 |  5793 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5794 | `#ifdef UNTRUST` |
|        - |  5795 | `	if( pNos < pStack ){` |
|        - |  5796 | `		goto Abort;` |
|        - |  5797 | `	}` |
|        - |  5798 | `#endif` |
|        - |  5799 | `	/* Perform the addition */` |
|     1032 |  5800 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1032 |  5801 | `	VmPopOperand(&pTos,1);` |
|     1032 |  5802 | `	break;` |
|        - |  5803 | `				}` |
|        - |  5804 | `/*` |
|        - |  5805 | ` * OP_ADD_STORE * * *` |
|        - |  5806 | ` *` |
|        - |  5807 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5808 | ` * and push the result back onto the stack.` |
|        - |  5809 | ` */` |
|      502 |  5810 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5811 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5812 | `	ph7_value *pObj;` |
|        - |  5813 | `	sxu32 nIdx;` |
|        - |  5814 | `#ifdef UNTRUST` |
|        - |  5815 | `	if( pNos < pStack ){` |
|        - |  5816 | `		goto Abort;` |
|        - |  5817 | `	}` |
|        - |  5818 | `#endif` |
|        - |  5819 | `	/* Perform the addition */` |
|     1006 |  5820 | `	nIdx = pTos->nIdx;` |
|     1006 |  5821 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5822 | `	/* Peform the store operation */` |
|     1006 |  5823 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5824 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5825 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5826 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5827 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5828 | `	}` |
|        - |  5829 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5830 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5831 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5832 | `	break;` |
|        - |  5833 | `				}` |
|        - |  5834 | `/* OP_SUB * * *` |
|        - |  5835 | ` *` |
|        - |  5836 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5837 | ` * first (what was next on the stack) from the second (the` |
|        - |  5838 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5839 | ` */` |
|      348 |  5840 | `case PH7_OP_SUB: {` |
|      698 |  5841 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5842 | `#ifdef UNTRUST` |
|        - |  5843 | `	if( pNos < pStack ){` |
|        - |  5844 | `		goto Abort;` |
|        - |  5845 | `	}` |
|        - |  5846 | `#endif` |
|      698 |  5847 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5848 | `		/* Floating point arithemic */` |
|        - |  5849 | `		ph7_real a,b,r;` |
|       95 |  5850 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5851 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5852 | `		}` |
|       95 |  5853 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5854 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5855 | `		}` |
|       95 |  5856 | `		a = pNos->rVal;` |
|       95 |  5857 | `		b = pTos->rVal;` |
|       95 |  5858 | `		r = a - b;` |
|        - |  5859 | `		/* Push the result */` |
|       95 |  5860 | `		pNos->rVal = r;` |
|       95 |  5861 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5862 | `		/* Try to get an integer representation */` |
|       95 |  5863 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5864 | `	}else{` |
|        - |  5865 | `		/* Integer arithmetic */` |
|        - |  5866 | `		sxi64 a,b,r;` |
|      604 |  5867 | `		a = pNos->x.iVal;` |
|      604 |  5868 | `		b = pTos->x.iVal;` |
|      604 |  5869 | `		r = a - b;` |
|        - |  5870 | `		/* Push the result */` |
|      604 |  5871 | `		pNos->x.iVal = r;` |
|      604 |  5872 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5873 | `	}` |
|      698 |  5874 | `	VmPopOperand(&pTos,1);` |
|      698 |  5875 | `	break;` |
|        - |  5876 | `				 }` |
|        - |  5877 | `/* OP_SUB_STORE * * *` |
|        - |  5878 | ` *` |
|        - |  5879 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5880 | ` * first (what was next on the stack) from the second (the` |
|        - |  5881 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5882 | ` */` |
|        4 |  5883 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5884 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5885 | `	ph7_value *pObj;` |
|        - |  5886 | `#ifdef UNTRUST` |
|        - |  5887 | `	if( pNos < pStack ){` |
|        - |  5888 | `		goto Abort;` |
|        - |  5889 | `	}` |
|        - |  5890 | `#endif` |
|       10 |  5891 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5892 | `		/* Floating point arithemic */` |
|        - |  5893 | `		ph7_real a,b,r;` |
|      ! 0 |  5894 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5895 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5896 | `		}` |
|      ! 0 |  5897 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5898 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5899 | `		}` |
|      ! 0 |  5900 | `		a = pTos->rVal;` |
|      ! 0 |  5901 | `		b = pNos->rVal;` |
|      ! 0 |  5902 | `		r = a - b;` |
|        - |  5903 | `		/* Push the result */` |
|      ! 0 |  5904 | `		pNos->rVal = r;` |
|      ! 0 |  5905 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5906 | `		/* Try to get an integer representation */` |
|      ! 0 |  5907 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5908 | `	}else{` |
|        - |  5909 | `		/* Integer arithmetic */` |
|        - |  5910 | `		sxi64 a,b,r;` |
|       10 |  5911 | `		a = pTos->x.iVal;` |
|       10 |  5912 | `		b = pNos->x.iVal;` |
|       10 |  5913 | `		r = a - b;` |
|        - |  5914 | `		/* Push the result */` |
|       10 |  5915 | `		pNos->x.iVal = r;` |
|       10 |  5916 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5917 | `	}` |
|       10 |  5918 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5919 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5920 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5921 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5922 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5923 | `	}` |
|       10 |  5924 | `	VmPopOperand(&pTos,1);` |
|       10 |  5925 | `	break;` |
|        - |  5926 | `				 }` |
|        - |  5927 |  |
|        - |  5928 | `/*` |
|        - |  5929 | ` * OP_MOD * * *` |
|        - |  5930 | ` *` |
|        - |  5931 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5932 | ` * first (what was next on the stack) from the second (the` |
|        - |  5933 | ` * top of the stack) and push the remainder after division` |
|        - |  5934 | ` * onto the stack.` |
|        - |  5935 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5936 | ` */` |
|      308 |  5937 | `case PH7_OP_MOD:{` |
|      618 |  5938 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5939 | `	sxi64 a,b,r;` |
|        - |  5940 | `#ifdef UNTRUST` |
|        - |  5941 | `	if( pNos < pStack ){` |
|        - |  5942 | `		goto Abort;` |
|        - |  5943 | `	}` |
|        - |  5944 | `#endif` |
|        - |  5945 | `	/* Force the operands to be integer */` |
|      618 |  5946 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5947 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5948 | `	}` |
|      618 |  5949 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5950 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5951 | `	}` |
|        - |  5952 | `	/* Perform the requested operation */` |
|      618 |  5953 | `	a = pNos->x.iVal;` |
|      618 |  5954 | `	b = pTos->x.iVal;` |
|      618 |  5955 | `	if( b == 0 ){` |
|        3 |  5956 | `		r = 0;` |
|        3 |  5957 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5958 | `		/* goto Abort; */` |
|        2 |  5959 | `	}else{` |
|      615 |  5960 | `		r = a%b;` |
|        - |  5961 | `	}` |
|        - |  5962 | `	/* Push the result */` |
|      618 |  5963 | `	pNos->x.iVal = r;` |
|      618 |  5964 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5965 | `	VmPopOperand(&pTos,1);` |
|      618 |  5966 | `	break;` |
|        - |  5967 | `				}` |
|        - |  5968 | `/*` |
|        - |  5969 | ` * OP_MOD_STORE * * *` |
|        - |  5970 | ` *` |
|        - |  5971 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5972 | ` * first (what was next on the stack) from the second (the` |
|        - |  5973 | ` * top of the stack) and push the remainder after division` |
|        - |  5974 | ` * onto the stack.` |
|        - |  5975 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5976 | ` */` |
|        1 |  5977 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5978 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5979 | `	ph7_value *pObj;` |
|        - |  5980 | `	sxi64 a,b,r;` |
|        - |  5981 | `#ifdef UNTRUST` |
|        - |  5982 | `	if( pNos < pStack ){` |
|        - |  5983 | `		goto Abort;` |
|        - |  5984 | `	}` |
|        - |  5985 | `#endif` |
|        - |  5986 | `	/* Force the operands to be integer */` |
|        3 |  5987 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5988 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5989 | `	}` |
|        3 |  5990 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5991 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5992 | `	}` |
|        - |  5993 | `	/* Perform the requested operation */` |
|        3 |  5994 | `	a = pTos->x.iVal;` |
|        3 |  5995 | `	b = pNos->x.iVal;` |
|        3 |  5996 | `	if( b == 0 ){` |
|      ! 0 |  5997 | `		r = 0;` |
|      ! 0 |  5998 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5999 | `		/* goto Abort; */` |
|      ! 0 |  6000 | `	}else{` |
|        3 |  6001 | `		r = a%b;` |
|        - |  6002 | `	}` |
|        - |  6003 | `	/* Push the result */` |
|        3 |  6004 | `	pNos->x.iVal = r;` |
|        3 |  6005 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6006 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6007 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6008 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6009 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6010 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6011 | `	}` |
|        3 |  6012 | `	VmPopOperand(&pTos,1);` |
|        3 |  6013 | `	break;` |
|        - |  6014 | `				}` |
|        - |  6015 | `/*` |
|        - |  6016 | ` * OP_DIV * * *` |
|        - |  6017 | ` *` |
|        - |  6018 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6019 | ` * first (what was next on the stack) from the second (the` |
|        - |  6020 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6021 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6022 | ` */` |
|       31 |  6023 | `case PH7_OP_DIV:{` |
|       64 |  6024 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6025 | `	ph7_real a,b,r;` |
|        - |  6026 | `#ifdef UNTRUST` |
|        - |  6027 | `	if( pNos < pStack ){` |
|        - |  6028 | `		goto Abort;` |
|        - |  6029 | `	}` |
|        - |  6030 | `#endif` |
|        - |  6031 | `	/* Force the operands to be real */` |
|       64 |  6032 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       60 |  6033 | `		PH7_MemObjToReal(pTos);` |
|       29 |  6034 | `	}` |
|       64 |  6035 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       26 |  6036 | `		PH7_MemObjToReal(pNos);` |
|       12 |  6037 | `	}` |
|        - |  6038 | `	/* Perform the requested operation */` |
|       64 |  6039 | `	a = pNos->rVal;` |
|       64 |  6040 | `	b = pTos->rVal;` |
|       64 |  6041 | `	if( b == 0 ){` |
|        - |  6042 | `		/* Division by zero */` |
|        3 |  6043 | `		pNos->rVal = 0;` |
|        3 |  6044 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6045 | `		/* goto Abort; */` |
|        2 |  6046 | `	}else{` |
|       61 |  6047 | `		r = a/b;` |
|        - |  6048 | `		/* Push the result */` |
|       61 |  6049 | `		pNos->rVal = r;` |
|       61 |  6050 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6051 | `		/* Try to get an integer representation */` |
|       61 |  6052 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6053 | `	}` |
|       64 |  6054 | `	VmPopOperand(&pTos,1);` |
|       64 |  6055 | `	break;` |
|        - |  6056 | `				}` |
|        - |  6057 | `/*` |
|        - |  6058 | ` * OP_DIV_STORE * * *` |
|        - |  6059 | ` *` |
|        - |  6060 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6061 | ` * first (what was next on the stack) from the second (the` |
|        - |  6062 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6063 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6064 | ` */` |
|        2 |  6065 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6066 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6067 | `	ph7_value *pObj;` |
|        - |  6068 | `	ph7_real a,b,r;` |
|        - |  6069 | `#ifdef UNTRUST` |
|        - |  6070 | `	if( pNos < pStack ){` |
|        - |  6071 | `		goto Abort;` |
|        - |  6072 | `	}` |
|        - |  6073 | `#endif` |
|        - |  6074 | `	/* Force the operands to be real */` |
|        5 |  6075 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6076 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6077 | `	}` |
|        5 |  6078 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6079 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6080 | `	}` |
|        - |  6081 | `	/* Perform the requested operation */` |
|        5 |  6082 | `	a = pTos->rVal;` |
|        5 |  6083 | `	b = pNos->rVal;` |
|        5 |  6084 | `	if( b == 0 ){` |
|        - |  6085 | `		/* Division by zero */` |
|      ! 0 |  6086 | `		r = 0;` |
|      ! 0 |  6087 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6088 | `		/* goto Abort; */` |
|      ! 0 |  6089 | `	}else{` |
|        5 |  6090 | `		r = a/b;` |
|        - |  6091 | `		/* Push the result */` |
|        5 |  6092 | `		pNos->rVal = r;` |
|        5 |  6093 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6094 | `		/* Try to get an integer representation */` |
|        5 |  6095 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6096 | `	}` |
|        5 |  6097 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6098 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6099 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6100 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6101 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6102 | `	}` |
|        5 |  6103 | `	VmPopOperand(&pTos,1);` |
|        5 |  6104 | `	break;` |
|        - |  6105 | `				}` |
|        - |  6106 | `/* OP_BAND * * *` |
|        - |  6107 | ` *` |
|        - |  6108 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6109 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6110 | ` * two elements.` |
|        - |  6111 | `*/` |
|        - |  6112 | `/* OP_BOR * * *` |
|        - |  6113 | ` *` |
|        - |  6114 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6115 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6116 | ` * two elements.` |
|        - |  6117 | ` */` |
|        - |  6118 | `/* OP_BXOR * * *` |
|        - |  6119 | ` *` |
|        - |  6120 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6121 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6122 | ` * two elements.` |
|        - |  6123 | ` */` |
|       44 |  6124 | `case PH7_OP_BAND:` |
|        - |  6125 | `case PH7_OP_BOR:` |
|        - |  6126 | `case PH7_OP_BXOR:{` |
|       90 |  6127 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6128 | `	sxi64 a,b,r;` |
|        - |  6129 | `#ifdef UNTRUST` |
|        - |  6130 | `	if( pNos < pStack ){` |
|        - |  6131 | `		goto Abort;` |
|        - |  6132 | `	}` |
|        - |  6133 | `#endif` |
|        - |  6134 | `	/* Force the operands to be integer */` |
|       90 |  6135 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6136 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6137 | `	}` |
|       90 |  6138 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6139 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6140 | `	}` |
|        - |  6141 | `	/* Perform the requested operation */` |
|       90 |  6142 | `	a = pNos->x.iVal;` |
|       90 |  6143 | `	b = pTos->x.iVal;` |
|       90 |  6144 | `	switch(pInstr->iOp){` |
|        7 |  6145 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6146 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6147 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6148 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  6149 | `	case PH7_OP_BAND_STORE:` |
|       30 |  6150 | `	case PH7_OP_BAND:` |
|       62 |  6151 | `	default:          r = a&b; break;` |
|        - |  6152 | `	}` |
|        - |  6153 | `	/* Push the result */` |
|       90 |  6154 | `	pNos->x.iVal = r;` |
|       90 |  6155 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  6156 | `	VmPopOperand(&pTos,1);` |
|       90 |  6157 | `	break;` |
|        - |  6158 | `				 }` |
|        - |  6159 | `/* OP_BAND_STORE * * *` |
|        - |  6160 | ` *` |
|        - |  6161 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6162 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6163 | ` * two elements.` |
|        - |  6164 | `*/` |
|        - |  6165 | `/* OP_BOR_STORE * * *` |
|        - |  6166 | ` *` |
|        - |  6167 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6168 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6169 | ` * two elements.` |
|        - |  6170 | ` */` |
|        - |  6171 | `/* OP_BXOR_STORE * * *` |
|        - |  6172 | ` *` |
|        - |  6173 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6174 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6175 | ` * two elements.` |
|        - |  6176 | ` */` |
|       10 |  6177 | `case PH7_OP_BAND_STORE:` |
|        - |  6178 | `case PH7_OP_BOR_STORE:` |
|        - |  6179 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6180 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6181 | `	ph7_value *pObj;` |
|        - |  6182 | `	sxi64 a,b,r;` |
|        - |  6183 | `#ifdef UNTRUST` |
|        - |  6184 | `	if( pNos < pStack ){` |
|        - |  6185 | `		goto Abort;` |
|        - |  6186 | `	}` |
|        - |  6187 | `#endif` |
|        - |  6188 | `	/* Force the operands to be integer */` |
|       21 |  6189 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6190 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6191 | `	}` |
|       21 |  6192 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6193 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6194 | `	}` |
|        - |  6195 | `	/* Perform the requested operation */` |
|       21 |  6196 | `	a = pTos->x.iVal;` |
|       21 |  6197 | `	b = pNos->x.iVal;` |
|       21 |  6198 | `	switch(pInstr->iOp){` |
|        3 |  6199 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6200 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6201 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6202 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6203 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6204 | `	case PH7_OP_BAND:` |
|        7 |  6205 | `	default:          r = a&b; break;` |
|        - |  6206 | `	}` |
|        - |  6207 | `	/* Push the result */` |
|       21 |  6208 | `	pNos->x.iVal = r;` |
|       21 |  6209 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6210 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6211 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6212 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6213 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6214 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6215 | `	}` |
|       21 |  6216 | `	VmPopOperand(&pTos,1);` |
|       21 |  6217 | `	break;` |
|        - |  6218 | `				 }` |
|        - |  6219 | `/* OP_SHL * * *` |
|        - |  6220 | ` *` |
|        - |  6221 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6222 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6223 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6224 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6225 | ` */` |
|        - |  6226 | `/* OP_SHR * * *` |
|        - |  6227 | ` *` |
|        - |  6228 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6229 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6230 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6231 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6232 | ` */` |
|       12 |  6233 | `case PH7_OP_SHL:` |
|        - |  6234 | `case PH7_OP_SHR: {` |
|       25 |  6235 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6236 | `	sxi64 a,r;` |
|        - |  6237 | `	sxi32 b;` |
|        - |  6238 | `#ifdef UNTRUST` |
|        - |  6239 | `	if( pNos < pStack ){` |
|        - |  6240 | `		goto Abort;` |
|        - |  6241 | `	}` |
|        - |  6242 | `#endif` |
|        - |  6243 | `	/* Force the operands to be integer */` |
|       25 |  6244 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6245 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6246 | `	}` |
|       25 |  6247 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6248 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6249 | `	}` |
|        - |  6250 | `	/* Perform the requested operation */` |
|       25 |  6251 | `	a = pNos->x.iVal;` |
|       25 |  6252 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6253 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6254 | `		r = a << b;` |
|        8 |  6255 | `	}else{` |
|       11 |  6256 | `		r = a >> b;` |
|        - |  6257 | `	}` |
|        - |  6258 | `	/* Push the result */` |
|       25 |  6259 | `	pNos->x.iVal = r;` |
|       25 |  6260 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6261 | `	VmPopOperand(&pTos,1);` |
|       25 |  6262 | `	break;` |
|        - |  6263 | `				 }` |
|        - |  6264 | `/*  OP_SHL_STORE * * *` |
|        - |  6265 | ` *` |
|        - |  6266 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6267 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6268 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6269 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6270 | ` */` |
|        - |  6271 | `/* OP_SHR_STORE * * *` |
|        - |  6272 | ` *` |
|        - |  6273 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6274 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6275 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6276 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6277 | ` */` |
|        9 |  6278 | `case PH7_OP_SHL_STORE:` |
|        - |  6279 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6280 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6281 | `	ph7_value *pObj;` |
|        - |  6282 | `	sxi64 a,r;` |
|        - |  6283 | `	sxi32 b;` |
|        - |  6284 | `#ifdef UNTRUST` |
|        - |  6285 | `	if( pNos < pStack ){` |
|        - |  6286 | `		goto Abort;` |
|        - |  6287 | `	}` |
|        - |  6288 | `#endif` |
|        - |  6289 | `	/* Force the operands to be integer */` |
|       19 |  6290 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6291 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6292 | `	}` |
|       19 |  6293 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6294 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6295 | `	}` |
|        - |  6296 | `	/* Perform the requested operation */` |
|       19 |  6297 | `	a = pTos->x.iVal;` |
|       19 |  6298 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6299 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6300 | `		r = a << b;` |
|        5 |  6301 | `	}else{` |
|       11 |  6302 | `		r = a >> b;` |
|        - |  6303 | `	}` |
|        - |  6304 | `	/* Push the result */` |
|       19 |  6305 | `	pNos->x.iVal = r;` |
|       19 |  6306 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6307 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6308 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6309 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6310 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6311 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6312 | `	}` |
|       19 |  6313 | `	VmPopOperand(&pTos,1);` |
|       19 |  6314 | `	break;` |
|        - |  6315 | `				 }` |
|        - |  6316 | `/* CAT:  P1 * *` |
|        - |  6317 | ` *` |
|        - |  6318 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6319 | ` * back.` |
|        - |  6320 | ` */` |
|    71009 |  6321 | `case PH7_OP_CAT:{` |
|        - |  6322 | `	ph7_value *pNos,*pCur;` |
|   142020 |  6323 | `	if( pInstr->iP1 < 1 ){` |
|   114548 |  6324 | `		pNos = &pTos[-1];` |
|    57275 |  6325 | `	}else{` |
|    27474 |  6326 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6327 | `	}` |
|        - |  6328 | `#ifdef UNTRUST` |
|        - |  6329 | `	if( pNos < pStack ){` |
|        - |  6330 | `		goto Abort;` |
|        - |  6331 | `	}` |
|        - |  6332 | `#endif` |
|        - |  6333 | `	/* Force a string cast */` |
|   142020 |  6334 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1640 |  6335 | `		PH7_MemObjToString(pNos);` |
|      819 |  6336 | `	}` |
|   142020 |  6337 | `	pCur = &pNos[1];` |
|   286756 |  6338 | `	while( pCur <= pTos ){` |
|   144738 |  6339 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50914 |  6340 | `			PH7_MemObjToString(pCur);` |
|    25456 |  6341 | `		}` |
|        - |  6342 | `		/* Perform the concatenation */` |
|   144738 |  6343 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   144696 |  6344 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    72347 |  6345 | `		}` |
|   144738 |  6346 | `		SyBlobRelease(&pCur->sBlob);` |
|   144738 |  6347 | `		pCur++;` |
|        2 |  6348 | `	}` |
|   142020 |  6349 | `	pTos = pNos;` |
|   142020 |  6350 | `	break;` |
|        - |  6351 | `				}` |
|        - |  6352 | `/*  CAT_STORE: * * *` |
|        - |  6353 | ` *` |
|        - |  6354 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6355 | ` * back.` |
|        - |  6356 | ` */` |
|     4093 |  6357 | `case PH7_OP_CAT_STORE:{` |
|     8188 |  6358 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6359 | `	ph7_value *pObj;` |
|        - |  6360 | `#ifdef UNTRUST` |
|        - |  6361 | `	if( pNos < pStack ){` |
|        - |  6362 | `		goto Abort;` |
|        - |  6363 | `	}` |
|        - |  6364 | `#endif` |
|     8188 |  6365 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6366 | `		/* Force a string cast */` |
|        3 |  6367 | `		PH7_MemObjToString(pTos);` |
|        1 |  6368 | `	}` |
|     8188 |  6369 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6370 | `		/* Force a string cast */` |
|      ! 0 |  6371 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6372 | `	}` |
|        - |  6373 | `	/* Perform the concatenation (Reverse order) */` |
|     8188 |  6374 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8188 |  6375 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     4093 |  6376 | `	}` |
|        - |  6377 | `	/* Perform the store operation */` |
|     8188 |  6378 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6379 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8188 |  6380 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8188 |  6381 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8186 |  6382 | `		PH7_MemObjStore(pTos,pObj);` |
|     4092 |  6383 | `	}` |
|     8186 |  6384 | `	PH7_MemObjStore(pTos,pNos);` |
|     8186 |  6385 | `	VmPopOperand(&pTos,1);` |
|     8186 |  6386 | `	break;` |
|        - |  6387 | `				}` |
|        - |  6388 | `/* OP_AND: * * *` |
|        - |  6389 | ` *` |
|        - |  6390 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6391 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6392 | ` * stack.` |
|        - |  6393 | ` */` |
|        - |  6394 | `/* OP_OR: * * *` |
|        - |  6395 | ` *` |
|        - |  6396 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6397 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6398 | ` * stack.` |
|        - |  6399 | ` */` |
|   107741 |  6400 | `case PH7_OP_LAND:` |
|        - |  6401 | `case PH7_OP_LOR: {` |
|   215528 |  6402 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6403 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6404 | `#ifdef UNTRUST` |
|        - |  6405 | `	if( pNos < pStack ){` |
|        - |  6406 | `		goto Abort;` |
|        - |  6407 | `	}` |
|        - |  6408 | `#endif` |
|        - |  6409 | `	/* Force a boolean cast */` |
|   215528 |  6410 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6411 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6412 | `	}` |
|   215528 |  6413 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6414 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6415 | `	}` |
|   215528 |  6416 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   215528 |  6417 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   215528 |  6418 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6419 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    98536 |  6420 | `		v1 = and_logic[v1*3+v2];` |
|    49291 |  6421 | `	}else{` |
|        - |  6422 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   116994 |  6423 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6424 | `	}` |
|   215528 |  6425 | `	if( v1 == 2 ){` |
|      ! 0 |  6426 | `		v1 = 1;` |
|      ! 0 |  6427 | `	}` |
|   215528 |  6428 | `	VmPopOperand(&pTos,1);` |
|   215528 |  6429 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   215528 |  6430 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   215528 |  6431 | `	break;` |
|        - |  6432 | `				 }` |
|        - |  6433 | `/*` |
|        - |  6434 | ` * OP_NULLC: * * *` |
|        - |  6435 | ` * Null coalescing operator '??'.` |
|        - |  6436 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6437 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6438 | ` */` |
|        - |  6439 | `/*` |
|        - |  6440 | ` * OP_NULLC: * P2 *` |
|        - |  6441 | ` * Short-circuit null coalescing '??'.` |
|        - |  6442 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6443 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6444 | ` */` |
|       93 |  6445 | `case PH7_OP_NULLC: {` |
|        - |  6446 | `#ifdef UNTRUST` |
|        - |  6447 | `	if( pTos < pStack ){` |
|        - |  6448 | `		goto Abort;` |
|        - |  6449 | `	}` |
|        - |  6450 | `#endif` |
|      188 |  6451 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6452 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6453 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6454 | `	}else{` |
|        - |  6455 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6456 | `		VmPopOperand(&pTos, 1);` |
|        - |  6457 | `	}` |
|      188 |  6458 | `	break;` |
|        - |  6459 |  |
|        - |  6460 | `/*` |
|        - |  6461 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6462 | ` * Null coalescing assignment short-circuit.` |
|        - |  6463 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6464 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6465 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6466 | ` */` |
|       28 |  6467 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6468 | `#ifdef UNTRUST` |
|        - |  6469 | `	if( pTos < pStack ){` |
|        - |  6470 | `		goto Abort;` |
|        - |  6471 | `	}` |
|        - |  6472 | `#endif` |
|       58 |  6473 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6474 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6475 | `	}` |
|       58 |  6476 | `	break;` |
|        - |  6477 |  |
|        - |  6478 | `/*` |
|        - |  6479 | ` * OP_NULLC_STORE: * * *` |
|        - |  6480 | ` * Null coalescing assignment store.` |
|        - |  6481 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6482 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6483 | ` * expression result.` |
|        - |  6484 | ` */` |
|        - |  6485 | `/*` |
|        - |  6486 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6487 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6488 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6489 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6490 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6491 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6492 | ` */` |
|       51 |  6493 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6494 | `#ifdef UNTRUST` |
|        - |  6495 | `	if( pTos < pStack ){` |
|        - |  6496 | `		goto Abort;` |
|        - |  6497 | `	}` |
|        - |  6498 | `#endif` |
|      104 |  6499 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6500 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6501 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6502 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6503 | `	}` |
|      104 |  6504 | `	break;` |
|        - |  6505 |  |
|       17 |  6506 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6507 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6508 | `	ph7_value *pObj;` |
|        - |  6509 | `	sxu32 nIdx;` |
|        - |  6510 | `#ifdef UNTRUST` |
|        - |  6511 | `	if( pNos < pStack ){` |
|        - |  6512 | `		goto Abort;` |
|        - |  6513 | `	}` |
|        - |  6514 | `#endif` |
|        - |  6515 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6516 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6517 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6518 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6519 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6520 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6521 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6522 | `		ph7_value *apArg[2];` |
|        5 |  6523 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6524 | `		apArg[1] = pTos;` |
|        5 |  6525 | `		if( pSet ){` |
|        5 |  6526 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6527 | `		}` |
|        - |  6528 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6529 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6530 | `		VmPopOperand(&pTos,1);` |
|        - |  6531 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6532 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6533 | `		break;` |
|        - |  6534 | `	}` |
|       32 |  6535 | `	nIdx = pNos->nIdx;` |
|       32 |  6536 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6537 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6538 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6539 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6540 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6541 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6542 | `	}` |
|       32 |  6543 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6544 | `	VmPopOperand(&pTos,1);` |
|       32 |  6545 | `	break;` |
|        - |  6546 |  |
|        - |  6547 | `/*` |
|        - |  6548 | ` * OP_SPREAD: * * *` |
|        - |  6549 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6550 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6551 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6552 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6553 | ` */` |
|        9 |  6554 | `case PH7_OP_SPREAD: {` |
|        - |  6555 | `#ifdef UNTRUST` |
|        - |  6556 | `	if( pTos < pStack ){` |
|        - |  6557 | `		goto Abort;` |
|        - |  6558 | `	}` |
|        - |  6559 | `#endif` |
|       20 |  6560 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6561 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6562 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6563 | `		if( nEntry == 0 ){` |
|        - |  6564 | `			/* Empty array — remove from stack */` |
|        3 |  6565 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6566 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6567 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6568 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6569 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6570 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6571 | `				VM_STACK_GUARD);` |
|      ! 0 |  6572 | `		}else{` |
|        - |  6573 | `			ph7_hashmap_node *pNode2;` |
|        - |  6574 | `			ph7_value *pElem;` |
|        - |  6575 | `			sxu32 i;` |
|        - |  6576 | `			/* Overwrite TOS with first element */` |
|       18 |  6577 | `			pNode2 = pMap->pFirst;` |
|       18 |  6578 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6579 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6580 | `			if( pElem ){` |
|       18 |  6581 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6582 | `			}` |
|       18 |  6583 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6584 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6585 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6586 | `			pNode2 = pNode2->pPrev;` |
|        - |  6587 | `			/* Push remaining elements */` |
|       44 |  6588 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6589 | `				pTos++;` |
|       28 |  6590 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6591 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6592 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6593 | `				if( pElem ){` |
|       28 |  6594 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6595 | `				}` |
|       28 |  6596 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6597 | `			}` |
|       18 |  6598 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6599 | `		}` |
|        9 |  6600 | `	}` |
|        - |  6601 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6602 | `	break;` |
|        - |  6603 |  |
|        - |  6604 | `/*` |
|        - |  6605 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6606 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6607 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6608 | ` */` |
|       31 |  6609 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6610 | `#ifdef UNTRUST` |
|        - |  6611 | `	if( pTos < pStack ){` |
|        - |  6612 | `		goto Abort;` |
|        - |  6613 | `	}` |
|        - |  6614 | `#endif` |
|       64 |  6615 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       64 |  6616 | `	break;` |
|        - |  6617 |  |
|        - |  6618 | `/* OP_LXOR: * * *` |
|        - |  6619 | ` *` |
|        - |  6620 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6621 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6622 | ` * stack.` |
|        - |  6623 | ` * According to the PHP language reference manual:` |
|        - |  6624 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6625 | ` *  TRUE,but not both.` |
|        - |  6626 | ` */` |
|        5 |  6627 | `case PH7_OP_LXOR:{` |
|       11 |  6628 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6629 | `	sxi32 v = 0;` |
|        - |  6630 | `#ifdef UNTRUST` |
|        - |  6631 | `	if( pNos < pStack ){` |
|        - |  6632 | `		goto Abort;` |
|        - |  6633 | `	}` |
|        - |  6634 | `#endif` |
|        - |  6635 | `	/* Force a boolean cast */` |
|       11 |  6636 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6637 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6638 | `	}` |
|       11 |  6639 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6640 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6641 | `	}` |
|       11 |  6642 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6643 | `		v = 1;` |
|        3 |  6644 | `	}` |
|       11 |  6645 | `	VmPopOperand(&pTos,1);` |
|       11 |  6646 | `	pTos->x.iVal = v;` |
|       11 |  6647 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6648 | `	break;` |
|        - |  6649 | `				 }` |
|        - |  6650 | `/* OP_EQ P1 P2 P3` |
|        - |  6651 | ` *` |
|        - |  6652 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6653 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6654 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6655 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6656 | ` */` |
|        - |  6657 | `/* OP_NEQ P1 P2 P3` |
|        - |  6658 | ` *` |
|        - |  6659 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6660 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6661 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6662 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6663 | ` */` |
|     4493 |  6664 | `case PH7_OP_EQ:` |
|        - |  6665 | `case PH7_OP_NEQ: {` |
|     8988 |  6666 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6667 | `	/* Perform the comparison and act accordingly */` |
|        - |  6668 | `#ifdef UNTRUST` |
|        - |  6669 | `	if( pNos < pStack ){` |
|        - |  6670 | `		goto Abort;` |
|        - |  6671 | `	}` |
|        - |  6672 | `#endif` |
|     8988 |  6673 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8988 |  6674 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6675 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8979 |  6676 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8944 |  6677 | `		rc = rc == 0;` |
|     4473 |  6678 | `	}else{` |
|       28 |  6679 | `		rc = rc != 0;` |
|        - |  6680 | `	}` |
|     8988 |  6681 | `	VmPopOperand(&pTos,1);` |
|     8988 |  6682 | `	if( !pInstr->iP2 ){` |
|        - |  6683 | `		/* Push comparison result without taking the jump */` |
|     8988 |  6684 | `		PH7_MemObjRelease(pTos);` |
|     8988 |  6685 | `		pTos->x.iVal = rc;` |
|        - |  6686 | `		/* Invalidate any prior representation */` |
|     8988 |  6687 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4495 |  6688 | `	}else{` |
|      ! 0 |  6689 | `		if( rc ){` |
|        - |  6690 | `			/* Jump to the desired location */` |
|      ! 0 |  6691 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6692 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6693 | `		}` |
|        - |  6694 | `	}` |
|     8988 |  6695 | `	break;` |
|        - |  6696 | `				 }` |
|        - |  6697 | `/* OP_TEQ P1 P2 *` |
|        - |  6698 | ` *` |
|        - |  6699 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6700 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6701 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6702 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6703 | ` */` |
|   159138 |  6704 | `case PH7_OP_TEQ: {` |
|   318278 |  6705 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6706 | `	/* Perform the comparison and act accordingly */` |
|        - |  6707 | `#ifdef UNTRUST` |
|        - |  6708 | `	if( pNos < pStack ){` |
|        - |  6709 | `		goto Abort;` |
|        - |  6710 | `	}` |
|        - |  6711 | `#endif` |
|   318278 |  6712 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   318278 |  6713 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6714 | `		rc = 0;` |
|        2 |  6715 | `	}else{` |
|   318276 |  6716 | `		rc = rc == 0;` |
|        - |  6717 | `	}` |
|   318278 |  6718 | `	VmPopOperand(&pTos,1);` |
|   318278 |  6719 | `	if( !pInstr->iP2 ){` |
|        - |  6720 | `		/* Push comparison result without taking the jump */` |
|   318278 |  6721 | `		PH7_MemObjRelease(pTos);` |
|   318278 |  6722 | `		pTos->x.iVal = rc;` |
|        - |  6723 | `		/* Invalidate any prior representation */` |
|   318278 |  6724 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   159140 |  6725 | `	}else{` |
|      ! 0 |  6726 | `		if( rc ){` |
|        - |  6727 | `			/* Jump to the desired location */` |
|      ! 0 |  6728 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6729 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6730 | `		}` |
|        - |  6731 | `	}` |
|   318278 |  6732 | `	break;` |
|        - |  6733 | `				 }` |
|        - |  6734 | `/* OP_TNE P1 P2 *` |
|        - |  6735 | ` *` |
|        - |  6736 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6737 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6738 | ` * instruction.` |
|        - |  6739 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6740 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6741 | ` *` |
|        - |  6742 | ` */` |
|   122533 |  6743 | `case PH7_OP_TNE: {` |
|   245068 |  6744 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6745 | `	/* Perform the comparison and act accordingly */` |
|        - |  6746 | `#ifdef UNTRUST` |
|        - |  6747 | `	if( pNos < pStack ){` |
|        - |  6748 | `		goto Abort;` |
|        - |  6749 | `	}` |
|        - |  6750 | `#endif` |
|   245068 |  6751 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   245068 |  6752 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6753 | `		rc = 1;` |
|        2 |  6754 | `	}else{` |
|   245066 |  6755 | `		rc = rc != 0;` |
|        - |  6756 | `	}` |
|   245068 |  6757 | `	VmPopOperand(&pTos,1);` |
|   245068 |  6758 | `	if( !pInstr->iP2 ){` |
|        - |  6759 | `		/* Push comparison result without taking the jump */` |
|   245068 |  6760 | `		PH7_MemObjRelease(pTos);` |
|   245068 |  6761 | `		pTos->x.iVal = rc;` |
|        - |  6762 | `		/* Invalidate any prior representation */` |
|   245068 |  6763 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   122535 |  6764 | `	}else{` |
|      ! 0 |  6765 | `		if( rc ){` |
|        - |  6766 | `			/* Jump to the desired location */` |
|      ! 0 |  6767 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6768 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6769 | `		}` |
|        - |  6770 | `	}` |
|   245068 |  6771 | `	break;` |
|        - |  6772 | `				 }` |
|        - |  6773 | `/* OP_LT P1 P2 P3` |
|        - |  6774 | ` *` |
|        - |  6775 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6776 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6777 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6778 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6779 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6780 | ` *` |
|        - |  6781 | ` */` |
|        - |  6782 | `/* OP_LE P1 P2 P3` |
|        - |  6783 | ` *` |
|        - |  6784 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6785 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6786 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6787 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6788 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6789 | ` *` |
|        - |  6790 | ` */` |
|   112413 |  6791 | `case PH7_OP_LT:` |
|        - |  6792 | `case PH7_OP_LE: {` |
|   224872 |  6793 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6794 | `	/* Perform the comparison and act accordingly */` |
|        - |  6795 | `#ifdef UNTRUST` |
|        - |  6796 | `	if( pNos < pStack ){` |
|        - |  6797 | `		goto Abort;` |
|        - |  6798 | `	}` |
|        - |  6799 | `#endif` |
|   224872 |  6800 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224872 |  6801 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6802 | `		rc = 0;` |
|   224868 |  6803 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6804 | `		rc = rc < 1;` |
|      805 |  6805 | `	}else{` |
|   223258 |  6806 | `		rc = rc < 0;` |
|        - |  6807 | `	}` |
|   224872 |  6808 | `	VmPopOperand(&pTos,1);` |
|   224872 |  6809 | `	if( !pInstr->iP2 ){` |
|        - |  6810 | `		/* Push comparison result without taking the jump */` |
|   224872 |  6811 | `		PH7_MemObjRelease(pTos);` |
|   224872 |  6812 | `		pTos->x.iVal = rc;` |
|        - |  6813 | `		/* Invalidate any prior representation */` |
|   224872 |  6814 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112459 |  6815 | `	}else{` |
|      ! 0 |  6816 | `		if( rc ){` |
|        - |  6817 | `			/* Jump to the desired location */` |
|      ! 0 |  6818 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6819 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6820 | `		}` |
|        - |  6821 | `	}` |
|   224872 |  6822 | `	break;` |
|        - |  6823 | `				}` |
|        - |  6824 | `/* OP_GT P1 P2 P3` |
|        - |  6825 | ` *` |
|        - |  6826 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6827 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6828 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6829 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6830 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6831 | ` *` |
|        - |  6832 | ` */` |
|        - |  6833 | `/* OP_GE P1 P2 P3` |
|        - |  6834 | ` *` |
|        - |  6835 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6836 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6837 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6838 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6839 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6840 | ` *` |
|        - |  6841 | ` */` |
|    55632 |  6842 | `case PH7_OP_GT:` |
|        - |  6843 | `case PH7_OP_GE: {` |
|   111266 |  6844 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6845 | `	/* Perform the comparison and act accordingly */` |
|        - |  6846 | `#ifdef UNTRUST` |
|        - |  6847 | `	if( pNos < pStack ){` |
|        - |  6848 | `		goto Abort;` |
|        - |  6849 | `	}` |
|        - |  6850 | `#endif` |
|   111266 |  6851 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111266 |  6852 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6853 | `		rc = 0;` |
|   111262 |  6854 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110874 |  6855 | `		rc = rc >= 0;` |
|    55438 |  6856 | `	}else{` |
|      386 |  6857 | `		rc = rc > 0;` |
|        - |  6858 | `	}` |
|   111266 |  6859 | `	VmPopOperand(&pTos,1);` |
|   111266 |  6860 | `	if( !pInstr->iP2 ){` |
|        - |  6861 | `		/* Push comparison result without taking the jump */` |
|   111266 |  6862 | `		PH7_MemObjRelease(pTos);` |
|   111266 |  6863 | `		pTos->x.iVal = rc;` |
|        - |  6864 | `		/* Invalidate any prior representation */` |
|   111266 |  6865 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55634 |  6866 | `	}else{` |
|      ! 0 |  6867 | `		if( rc ){` |
|        - |  6868 | `			/* Jump to the desired location */` |
|      ! 0 |  6869 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6870 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6871 | `		}` |
|        - |  6872 | `	}` |
|   111266 |  6873 | `	break;` |
|        - |  6874 | `				}` |
|        - |  6875 | `/* OP_SPACESHIP * * *` |
|        - |  6876 | ` *` |
|        - |  6877 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6878 | ` *   -1 if left < right` |
|        - |  6879 | ` *    0 if left == right` |
|        - |  6880 | ` *    1 if left > right` |
|        - |  6881 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6882 | ` */` |
|       25 |  6883 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6884 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6885 | `#ifdef UNTRUST` |
|        - |  6886 | `	if( pNos < pStack ){` |
|        - |  6887 | `		goto Abort;` |
|        - |  6888 | `	}` |
|        - |  6889 | `#endif` |
|       51 |  6890 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6891 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6892 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6893 | `		rc = 1;` |
|        4 |  6894 | `	}else{` |
|        - |  6895 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6896 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6897 | `	}` |
|       51 |  6898 | `	VmPopOperand(&pTos,1);` |
|       51 |  6899 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6900 | `	pTos->x.iVal = rc;` |
|       51 |  6901 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6902 | `	break;` |
|        - |  6903 | `				}` |
|        - |  6904 | `/* OP_SEQ P1 P2 *` |
|        - |  6905 | ` * Strict string comparison.` |
|        - |  6906 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6907 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6908 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6909 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6910 | ` * use PH7_OP_EQ.` |
|        - |  6911 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6912 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6913 | ` */` |
|        - |  6914 | `/* OP_SNE P1 P2 *` |
|        - |  6915 | ` * Strict string comparison.` |
|        - |  6916 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6917 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6918 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6919 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6920 | ` * use PH7_OP_EQ.` |
|        - |  6921 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6922 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6923 | ` */` |
|       18 |  6924 | `case PH7_OP_SEQ:` |
|        - |  6925 | `case PH7_OP_SNE: {` |
|       38 |  6926 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6927 | `	SyString s1,s2;` |
|        - |  6928 | `	/* Perform the comparison and act accordingly */` |
|        - |  6929 | `#ifdef UNTRUST` |
|        - |  6930 | `	if( pNos < pStack ){` |
|        - |  6931 | `		goto Abort;` |
|        - |  6932 | `	}` |
|        - |  6933 | `#endif` |
|        - |  6934 | `	/* Force a string cast */` |
|       38 |  6935 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6936 | `		PH7_MemObjToString(pTos);` |
|        2 |  6937 | `	}` |
|       38 |  6938 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6939 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6940 | `	}` |
|       38 |  6941 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6942 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6943 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6944 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6945 | `		rc = rc != 0;` |
|      ! 0 |  6946 | `	}else{` |
|       38 |  6947 | `		rc = rc == 0;` |
|        - |  6948 | `	}` |
|       38 |  6949 | `	VmPopOperand(&pTos,1);` |
|       38 |  6950 | `	if( !pInstr->iP2 ){` |
|        - |  6951 | `		/* Push comparison result without taking the jump */` |
|       38 |  6952 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6953 | `		pTos->x.iVal = rc;` |
|        - |  6954 | `		/* Invalidate any prior representation */` |
|       38 |  6955 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6956 | `	}else{` |
|      ! 0 |  6957 | `		if( rc ){` |
|        - |  6958 | `			/* Jump to the desired location */` |
|      ! 0 |  6959 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6960 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6961 | `		}` |
|        - |  6962 | `	}` |
|       38 |  6963 | `	break;` |
|        - |  6964 | `				 }` |
|        - |  6965 | `/*` |
|        - |  6966 | ` * OP_LOAD_REF * * *` |
|        - |  6967 | ` * Push the index of a referenced object on the stack.` |
|        - |  6968 | ` */` |
|       57 |  6969 | `case PH7_OP_LOAD_REF: {` |
|        - |  6970 | `	sxu32 nIdx;` |
|        - |  6971 | `#ifdef UNTRUST` |
|        - |  6972 | `	if( pTos < pStack ){` |
|        - |  6973 | `		goto Abort;` |
|        - |  6974 | `	}` |
|        - |  6975 | `#endif` |
|        - |  6976 | `	/* Extract memory object index */` |
|      115 |  6977 | `	nIdx = pTos->nIdx;` |
|      115 |  6978 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6979 | `		/* Nullify the object */` |
|       95 |  6980 | `		PH7_MemObjRelease(pTos);` |
|        - |  6981 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6982 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6983 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6984 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6985 | `	}` |
|      115 |  6986 | `	break;` |
|        - |  6987 | `					  }` |
|        - |  6988 | `/*` |
|        - |  6989 | ` * OP_STORE_REF * * P3` |
|        - |  6990 | ` * Perform an assignment operation by reference.` |
|        - |  6991 | ` */` |
|       16 |  6992 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6993 | `	 SyString sName = { 0 , 0 };` |
|        - |  6994 | `	 VmFrame *pFrameLocal;` |
|        - |  6995 | `	SyHashEntry *pEntry;` |
|        - |  6996 | `	sxu32 nIdx;` |
|        - |  6997 | `#ifdef UNTRUST` |
|        - |  6998 | `	if( pTos < pStack ){` |
|        - |  6999 | `		goto Abort;` |
|        - |  7000 | `	}` |
|        - |  7001 | `#endif` |
|       34 |  7002 | `	if( pInstr->p3 == 0 ){` |
|        - |  7003 | `		char *zName;` |
|        - |  7004 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7005 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7006 | `			/* Force a string cast */` |
|      ! 0 |  7007 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7008 | `		}` |
|      ! 0 |  7009 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7010 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7011 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7012 | `			if( zName ){` |
|      ! 0 |  7013 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7014 | `			}` |
|      ! 0 |  7015 | `		}` |
|      ! 0 |  7016 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7017 | `		pTos--;` |
|      ! 0 |  7018 | `	}else{` |
|       34 |  7019 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7020 | `	}` |
|       34 |  7021 | `	nIdx = pTos->nIdx;` |
|       34 |  7022 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7023 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7024 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7025 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7026 | `		}else{` |
|        - |  7027 | `			ph7_value *pObj;` |
|        - |  7028 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7029 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7030 | `			if( pObj == 0 ){` |
|      ! 0 |  7031 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7032 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7033 | `				goto Abort;` |
|        - |  7034 | `			}` |
|        - |  7035 | `			/* Perform the store operation */` |
|      ! 0 |  7036 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7037 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7038 | `		}` |
|       34 |  7039 | `	}else if( sName.nByte > 0){` |
|       34 |  7040 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7041 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7042 | `		}else{` |
|       34 |  7043 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7044 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7045 | `			/* Query the local frame */` |
|       34 |  7046 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7047 | `			if( pEntry ){` |
|      ! 0 |  7048 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7049 | `			}else{` |
|       34 |  7050 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7051 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7052 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7053 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7054 | `				}` |
|       34 |  7055 | `				if( rc == SXRET_OK ){` |
|       34 |  7056 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7057 | `				}` |
|        - |  7058 | `			}` |
|        - |  7059 | `		}` |
|       16 |  7060 | `	}` |
|       34 |  7061 | `	break;` |
|        - |  7062 | `				 }` |
|        - |  7063 | `/*` |
|        - |  7064 | ` * OP_UPLINK P1 * *` |
|        - |  7065 | ` * Link a variable to the top active VM frame.` |
|        - |  7066 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7067 | ` */` |
|       28 |  7068 | `case PH7_OP_UPLINK: {` |
|       58 |  7069 | `	if( pVm->pFrame->pParent ){` |
|       58 |  7070 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7071 | `		SyString sName;` |
|        - |  7072 | `		/* Perform the link */` |
|      116 |  7073 | `		while( pLink <= pTos ){` |
|       60 |  7074 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7075 | `				/* Force a string cast */` |
|      ! 0 |  7076 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7077 | `			}` |
|       60 |  7078 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  7079 | `			if( sName.nByte > 0 ){` |
|       60 |  7080 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  7081 | `			}` |
|       60 |  7082 | `			pLink++;` |
|        2 |  7083 | `		}` |
|       28 |  7084 | `	}` |
|       58 |  7085 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  7086 | `	break;` |
|        - |  7087 | `					}` |
|        - |  7088 | `/*` |
|        - |  7089 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7090 | ` * Push an exception in the corresponding container so that` |
|        - |  7091 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7092 | ` */` |
|      163 |  7093 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      328 |  7094 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7095 | `	VmFrame *pFrameLocal;` |
|        - |  7096 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      328 |  7097 | `	pException->iFinallyDone = 0;` |
|      328 |  7098 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7099 | `	/* Create the exception frame */` |
|      328 |  7100 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      328 |  7101 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7102 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7103 | `		goto Abort;` |
|        - |  7104 | `	}` |
|        - |  7105 | `	/* Mark the special frame */` |
|      328 |  7106 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      328 |  7107 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7108 | `	/* Point to the frame that trigger the exception */` |
|      328 |  7109 | `	pFrameLocal = pFrameLocal->pParent;` |
|      328 |  7110 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      328 |  7111 | `	pException->pFrame = pFrameLocal;` |
|      328 |  7112 | `	break;` |
|        - |  7113 | `							}` |
|        - |  7114 | `/*` |
|        - |  7115 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7116 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7117 | ` */` |
|      162 |  7118 | `case PH7_OP_POP_EXCEPTION: {` |
|      326 |  7119 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      326 |  7120 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7121 | `		ph7_exception **apException;` |
|        - |  7122 | `		/* Pop the loaded exception */` |
|       32 |  7123 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7124 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7125 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7126 | `		}` |
|       15 |  7127 | `	}` |
|      326 |  7128 | `	pException->pFrame = 0;` |
|        - |  7129 | `	/* Leave the exception frame */` |
|      326 |  7130 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7131 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      326 |  7132 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7133 | `		sxi32 rcFinally;` |
|       20 |  7134 | `		pException->iFinallyDone = 1;` |
|       20 |  7135 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7136 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7137 | `			goto Abort;` |
|        - |  7138 | `		}` |
|        9 |  7139 | `	}` |
|      326 |  7140 | `	break;` |
|        - |  7141 | `							}` |
|        - |  7142 |  |
|        - |  7143 | `/*` |
|        - |  7144 | ` * OP_THROW * P2 *` |
|        - |  7145 | ` * Throw an user exception.` |
|        - |  7146 | ` */` |
|       59 |  7147 | `case PH7_OP_THROW: {` |
|      120 |  7148 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      120 |  7149 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7150 | `#ifdef UNTRUST` |
|        - |  7151 | `	if( pTos < pStack ){` |
|        - |  7152 | `		goto Abort;` |
|        - |  7153 | `	}` |
|        - |  7154 | `#endif` |
|      120 |  7155 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7156 | `	/* Tell the upper layer that an exception was thrown */` |
|      120 |  7157 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      120 |  7158 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      120 |  7159 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7160 | `		ph7_class *pThrowable;` |
|        - |  7161 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      120 |  7162 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      121 |  7163 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7164 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7165 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7166 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7167 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7168 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7169 | `			if( pErrorClass ){` |
|        3 |  7170 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7171 | `			}` |
|        3 |  7172 | `			if( pErrInst ){` |
|        - |  7173 | `				ph7_class_method *pCons;` |
|        3 |  7174 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7175 | `				if( pCons ){` |
|        - |  7176 | `					ph7_value sArg;` |
|        - |  7177 | `					ph7_value *apArg[1];` |
|        - |  7178 | `					SyString sMsgStr;` |
|        - |  7179 | `					static const char zErrMsg[] =` |
|        - |  7180 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7181 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7182 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7183 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7184 | `					apArg[0] = &sArg;` |
|        3 |  7185 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7186 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7187 | `				}` |
|        3 |  7188 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7189 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7190 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7191 | `					goto Abort;` |
|        - |  7192 | `				}` |
|        2 |  7193 | `			}else{` |
|        - |  7194 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7195 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7196 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7197 | `					goto Abort;` |
|        - |  7198 | `				}` |
|        - |  7199 | `			}` |
|        2 |  7200 | `		}else{` |
|        - |  7201 | `			/* Throw the exception */` |
|      118 |  7202 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      118 |  7203 | `			if( rc == SXERR_ABORT ){` |
|        - |  7204 | `				/* Abort processing immediately */` |
|       11 |  7205 | `				goto Abort;` |
|        - |  7206 | `			}` |
|        - |  7207 | `		}` |
|       56 |  7208 | `	}else{` |
|        - |  7209 | `		/* Expecting a class instance */` |
|      ! 0 |  7210 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7211 | `		if( rc == SXERR_ABORT ){` |
|        - |  7212 | `			/* Abort processing immediately */` |
|      ! 0 |  7213 | `			goto Abort;` |
|        - |  7214 | `		}` |
|        - |  7215 | `	}` |
|        - |  7216 | `	/* Pop the top entry */` |
|      110 |  7217 | `	VmPopOperand(&pTos,1);` |
|        - |  7218 | `	/* Perform an unconditional jump */` |
|      110 |  7219 | `	pc = nJump - 1;` |
|      110 |  7220 | `	break;` |
|        - |  7221 | `				   }` |
|        - |  7222 | `/*` |
|        - |  7223 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7224 | ` * Prepare a foreach step.` |
|        - |  7225 | ` */` |
|     6043 |  7226 | `case PH7_OP_FOREACH_INIT: {` |
|    12088 |  7227 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7228 | `	void *pName;` |
|        - |  7229 | `#ifdef UNTRUST` |
|        - |  7230 | `	if( pTos < pStack ){` |
|        - |  7231 | `		goto Abort;` |
|        - |  7232 | `	}` |
|        - |  7233 | `#endif` |
|    12088 |  7234 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7235 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7236 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7237 | `			/* Force a string cast */` |
|      ! 0 |  7238 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7239 | `		}` |
|        - |  7240 | `		/* Duplicate name */` |
|      ! 0 |  7241 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7242 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7243 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7244 | `		}` |
|      ! 0 |  7245 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7246 | `	}` |
|    12088 |  7247 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7248 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7249 | `			/* Force a string cast */` |
|      ! 0 |  7250 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7251 | `		}` |
|        - |  7252 | `		/* Duplicate name */` |
|      ! 0 |  7253 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7254 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7255 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7256 | `		}` |
|      ! 0 |  7257 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7258 | `	}` |
|        - |  7259 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12088 |  7260 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7261 | `		/* Jump out of the loop */` |
|      ! 0 |  7262 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7263 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7264 | `		}` |
|      ! 0 |  7265 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7266 | `	}else{` |
|        - |  7267 | `		ph7_foreach_step *pStep;` |
|    12088 |  7268 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12088 |  7269 | `		if( pStep == 0 ){` |
|      ! 0 |  7270 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7271 | `			/* Jump out of the loop */` |
|      ! 0 |  7272 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7273 | `		}else{` |
|        - |  7274 | `			/* Zero the structure */` |
|    12088 |  7275 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7276 | `			/* Prepare the step */` |
|    12088 |  7277 | `			pStep->iFlags = pInfo->iFlags;` |
|    12088 |  7278 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7279 | `				ph7_hashmap *pMap;` |
|        - |  7280 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7281 | `				 * source array so mutations don't affect other sharers. */` |
|    12054 |  7282 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7283 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7284 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7285 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7286 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7287 | `						 * variable still points at the same hashmap as` |
|        - |  7288 | `						 * the stack value. */` |
|        9 |  7289 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7290 | `							pCur->iRef--;` |
|        9 |  7291 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7292 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7293 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7294 | `						}` |
|        4 |  7295 | `					}` |
|        4 |  7296 | `				}` |
|    12054 |  7297 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7298 | `				/* Reset the internal loop cursor */` |
|    12054 |  7299 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7300 | `				/* Mark the step */` |
|    12054 |  7301 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12054 |  7302 | `				pStep->xIter.pMap = pMap;` |
|    12054 |  7303 | `				pMap->iRef++;` |
|     6028 |  7304 | `			}else{` |
|       36 |  7305 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7306 | `				ph7_class *pIteratorClass;` |
|        - |  7307 | `				/* Check if the object implements Iterator */` |
|       36 |  7308 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7309 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7310 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7311 | `					ph7_class_method *pRewind;` |
|       24 |  7312 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7313 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7314 | `					pThis->iRef++;` |
|       24 |  7315 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7316 | `					if( pRewind ){` |
|       24 |  7317 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7318 | `					}` |
|       13 |  7319 | `				}else{` |
|        - |  7320 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7321 | `					ph7_class *pIterAggClass;` |
|       14 |  7322 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7323 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7324 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7325 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7326 | `						ph7_class_method *pGetIter;` |
|        3 |  7327 | `						int iterAggOk = 0;` |
|        3 |  7328 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7329 | `						if( pGetIter ){` |
|        - |  7330 | `							ph7_value sResult;` |
|        3 |  7331 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7332 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7333 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7334 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7335 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7336 | `									ph7_class_method *pRewind;` |
|        3 |  7337 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7338 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7339 | `									pIterObj->iRef++;` |
|        - |  7340 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7341 | `									pStep->pOwner = pThis;` |
|        3 |  7342 | `									pThis->iRef++;` |
|        3 |  7343 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7344 | `									if( pRewind ){` |
|        3 |  7345 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7346 | `									}` |
|        3 |  7347 | `									iterAggOk = 1;` |
|        1 |  7348 | `								}` |
|        1 |  7349 | `							}` |
|        3 |  7350 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7351 | `						}` |
|        3 |  7352 | `						if( !iterAggOk ){` |
|        - |  7353 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7354 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7355 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7356 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7357 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7358 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7359 | `						}` |
|        2 |  7360 | `					}else{` |
|        - |  7361 | `						/* Plain object iteration via hAttr */` |
|       12 |  7362 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7363 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7364 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7365 | `						pThis->iRef++;` |
|        - |  7366 | `					}` |
|        - |  7367 | `				}` |
|        - |  7368 | `			}` |
|        - |  7369 | `		}` |
|    12088 |  7370 | `		if( pStep ){` |
|    12088 |  7371 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7372 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7373 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7374 | `				/* Jump out of the loop */` |
|      ! 0 |  7375 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7376 | `			}` |
|     6043 |  7377 | `		}` |
|        - |  7378 | `	}` |
|    12088 |  7379 | `	VmPopOperand(&pTos,1);` |
|    12088 |  7380 | `	break;` |
|        - |  7381 | `						  }` |
|        - |  7382 | `/*` |
|        - |  7383 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7384 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7385 | ` */` |
|    99031 |  7386 | `case PH7_OP_FOREACH_STEP: {` |
|   198064 |  7387 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7388 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7389 | `	ph7_value *pValue;` |
|        - |  7390 | `	VmFrame *pFrameLocal;` |
|        - |  7391 | `	/* Peek the last step */` |
|   198064 |  7392 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   198064 |  7393 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   198064 |  7394 | `	pFrameLocal = pVm->pFrame;` |
|   198064 |  7395 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   198064 |  7396 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   197930 |  7397 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7398 | `		ph7_hashmap_node *pNode;` |
|        - |  7399 | `		/* Extract the current node value */` |
|   197930 |  7400 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   197930 |  7401 | `		if( pNode == 0 ){` |
|        - |  7402 | `			/* No more entry to process */` |
|    12052 |  7403 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12052 |  7404 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7405 | `				/* Break the reference with the last element */` |
|        7 |  7406 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7407 | `			}` |
|        - |  7408 | `			/* Automatically reset the loop cursor */` |
|    12052 |  7409 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7410 | `			/* Cleanup the mess left behind */` |
|    12052 |  7411 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12052 |  7412 | `			SySetPop(&pInfo->aStep);` |
|    12052 |  7413 | `			PH7_HashmapUnref(pMap);` |
|     6027 |  7414 | `		}else{` |
|   185880 |  7415 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      506 |  7416 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      506 |  7417 | `				if( pKey ){` |
|      506 |  7418 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      252 |  7419 | `				}` |
|      252 |  7420 | `			}` |
|   185880 |  7421 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7422 | `				SyHashEntry *pEntry;` |
|        - |  7423 | `				/* Pass by reference */` |
|       23 |  7424 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7425 | `				if( pEntry ){` |
|       21 |  7426 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7427 | `				}else{` |
|        4 |  7428 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7429 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7430 | `				}` |
|       12 |  7431 | `			}else{` |
|        - |  7432 | `				/* Make a copy of the entry value */` |
|   185858 |  7433 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   185858 |  7434 | `				if( pValue ){` |
|   185858 |  7435 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    92928 |  7436 | `				}` |
|        - |  7437 | `			}` |
|        2 |  7438 | `		}` |
|    99100 |  7439 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7440 | `		/* Iterator-based iteration.` |
|        - |  7441 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7442 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7443 | `		 */` |
|      106 |  7444 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7445 | `		ph7_class_method *pMethod;` |
|        - |  7446 | `		ph7_value sResult;` |
|      106 |  7447 | `		int isValid = 0;` |
|        - |  7448 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7449 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7450 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7451 | `		}else{` |
|       82 |  7452 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7453 | `			if( pMethod ){` |
|       82 |  7454 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7455 | `			}` |
|        - |  7456 | `		}` |
|        - |  7457 | `		/* Call valid() */` |
|      106 |  7458 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7459 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7460 | `		if( pMethod ){` |
|      106 |  7461 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7462 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7463 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7464 | `		}` |
|      106 |  7465 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7466 | `		if( !isValid ){` |
|        - |  7467 | `			/* Iterator exhausted */` |
|       24 |  7468 | `			pc = pInstr->iP2 - 1;` |
|        - |  7469 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7470 | `			if( pStep->pOwner ){` |
|        3 |  7471 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7472 | `			}` |
|       24 |  7473 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7474 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7475 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7476 | `		}else{` |
|        - |  7477 | `			/* Call current() to get value */` |
|       84 |  7478 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7479 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7480 | `			if( pMethod ){` |
|       84 |  7481 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7482 | `			}` |
|       84 |  7483 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7484 | `			if( pValue ){` |
|       84 |  7485 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7486 | `			}` |
|       84 |  7487 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7488 | `			/* Call key() if needed */` |
|       84 |  7489 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7490 | `				ph7_value sKey;` |
|       35 |  7491 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7492 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7493 | `				if( pMethod ){` |
|       35 |  7494 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7495 | `				}` |
|       35 |  7496 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7497 | `				if( pValue ){` |
|       35 |  7498 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7499 | `				}` |
|       35 |  7500 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7501 | `			}` |
|        - |  7502 | `		}` |
|       54 |  7503 | `	}else{` |
|       32 |  7504 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7505 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7506 | `		SyHashEntry *pEntry;` |
|        - |  7507 | `		/* Point to the next attribute */` |
|       36 |  7508 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7509 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7510 | `			/* Check access permission */` |
|       38 |  7511 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7512 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7513 | `					break; /* Access is granted */` |
|        - |  7514 | `			}` |
|        1 |  7515 | `		}` |
|       32 |  7516 | `		if( pEntry == 0 ){` |
|        - |  7517 | `			/* Clean up the mess left behind */` |
|       12 |  7518 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7519 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7520 | `				/* Break the reference with the last element */` |
|        3 |  7521 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7522 | `			}` |
|       12 |  7523 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7524 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7525 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7526 | `		}else{` |
|       22 |  7527 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7528 | `			ph7_value *pAttrValue;` |
|       22 |  7529 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7530 | `				/* Fill with the current attribute name */` |
|       22 |  7531 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7532 | `				if( pKey ){` |
|       22 |  7533 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7534 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7535 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7536 | `				}` |
|       10 |  7537 | `			}` |
|        - |  7538 | `			/* Extract attribute value */` |
|       22 |  7539 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7540 | `			if( pAttrValue ){` |
|       22 |  7541 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7542 | `					/* Pass by reference */` |
|        3 |  7543 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7544 | `					if( pEntry ){` |
|        3 |  7545 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7546 | `					}else{` |
|      ! 0 |  7547 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7548 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7549 | `					}` |
|        2 |  7550 | `				}else{` |
|        - |  7551 | `					/* Make a copy of the attribute value */` |
|       20 |  7552 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7553 | `					if( pValue ){` |
|       20 |  7554 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7555 | `					}` |
|        - |  7556 | `				}` |
|       10 |  7557 | `			}` |
|        - |  7558 | `		}` |
|        - |  7559 | `	}` |
|   198064 |  7560 | `	break;` |
|        - |  7561 | `						  }` |
|        - |  7562 | `/*` |
|        - |  7563 | ` * OP_MEMBER P1 P2` |
|        - |  7564 | ` * Load class attribute/method on the stack.` |
|        - |  7565 | ` */` |
|     3840 |  7566 | `case PH7_OP_MEMBER: {` |
|        - |  7567 | `	ph7_class_instance *pThis;` |
|        - |  7568 | `	ph7_value *pNos;` |
|        - |  7569 | `	SyString sName;` |
|     7682 |  7570 | `	if( !pInstr->iP1 ){` |
|     7456 |  7571 | `		pNos = &pTos[-1];` |
|        - |  7572 | `#ifdef UNTRUST` |
|        - |  7573 | `		if( pNos < pStack ){` |
|        - |  7574 | `			goto Abort;` |
|        - |  7575 | `		}` |
|        - |  7576 | `#endif` |
|     7456 |  7577 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7578 | `			ph7_class *pClass;` |
|        - |  7579 | `			/* Class already instantiated */` |
|     7454 |  7580 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7581 | `			/* Point to the instantiated class */` |
|     7454 |  7582 | `			pClass = pThis->pClass;` |
|        - |  7583 | `			/* Extract attribute name first */` |
|     7454 |  7584 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7454 |  7585 | `			if( pInstr->iP2 ){` |
|        - |  7586 | `				/* Method call */` |
|      748 |  7587 | `				ph7_class_method *pMeth = 0;` |
|      748 |  7588 | `				if( sName.nByte > 0 ){` |
|        - |  7589 | `					/* Extract the target method */` |
|      748 |  7590 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      373 |  7591 | `				}` |
|      748 |  7592 | `				if( pMeth == 0 ){` |
|      ! 0 |  7593 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7594 | `						&pClass->sName,&sName` |
|        - |  7595 | `						);` |
|        - |  7596 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7597 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7598 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7599 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7600 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7601 | `				}else{` |
|        - |  7602 | `					/* Push method name on the stack */` |
|      748 |  7603 | `					PH7_MemObjRelease(pTos);` |
|      748 |  7604 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      748 |  7605 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7606 | `				}` |
|      748 |  7607 | `				pTos->nIdx = SXU32_HIGH;` |
|      375 |  7608 | `			}else{` |
|        - |  7609 | `				/* Attribute access */` |
|     6708 |  7610 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7611 | `				SyHashEntry *pEntry;` |
|        - |  7612 | `				/* Extract the target attribute */` |
|     6708 |  7613 | `				if( sName.nByte > 0 ){` |
|     6708 |  7614 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6708 |  7615 | `					if( pEntry ){` |
|        - |  7616 | `						/* Point to the attribute value */` |
|     6706 |  7617 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3352 |  7618 | `					}` |
|     3353 |  7619 | `				}` |
|     6708 |  7620 | `				if( pObjAttr == 0 ){` |
|        - |  7621 | `					/* No such attribute,load null */` |
|        4 |  7622 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7623 | `						&pClass->sName,&sName);` |
|        - |  7624 | `					/* Call the __get magic method if available */` |
|        3 |  7625 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7626 | `				}` |
|     6708 |  7627 | `				VmPopOperand(&pTos,1);` |
|        - |  7628 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7629 | `				 * This is due to the following case:` |
|        - |  7630 | `				 *     (new TestClass())->foo;` |
|        - |  7631 | `				 */` |
|     6708 |  7632 | `				pThis->iRef++;` |
|     6708 |  7633 | `				PH7_MemObjRelease(pTos);` |
|     6708 |  7634 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6708 |  7635 | `				if( pObjAttr ){` |
|     6706 |  7636 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7637 | `					/* Check attribute access */` |
|     6706 |  7638 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7639 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7640 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7641 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7642 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7643 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6704 |  7644 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3394 |  7645 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7646 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7647 | `							int bIsLhs = 0;` |
|       82 |  7648 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7649 | `								bIsLhs = 1;` |
|       39 |  7650 | `							}` |
|       82 |  7651 | `							if( !bIsLhs ){` |
|        3 |  7652 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7653 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7654 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7655 | `									goto Abort;` |
|        - |  7656 | `								}` |
|        - |  7657 | `								{` |
|        3 |  7658 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7659 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7660 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3840 |  7661 | `										break;` |
|        - |  7662 | `									}` |
|        - |  7663 | `								}` |
|      ! 0 |  7664 | `								goto Exception;` |
|        - |  7665 | `							}` |
|       39 |  7666 | `						}` |
|        - |  7667 | `						/* Load attribute */` |
|     6704 |  7668 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6704 |  7669 | `						if( pValue ){` |
|     6704 |  7670 | `							if( pThis->iRef < 2 ){` |
|        - |  7671 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7672 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7673 | `								 */` |
|        7 |  7674 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7675 | `							}else{` |
|        - |  7676 | `								/* Simple load */` |
|     6698 |  7677 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7678 | `							}` |
|     6704 |  7679 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6702 |  7680 | `								if( pThis->iRef > 1 ){` |
|        - |  7681 | `									/* Load attribute index */` |
|     6696 |  7682 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3347 |  7683 | `								}` |
|     3350 |  7684 | `							}` |
|     3351 |  7685 | `						}` |
|     3353 |  7686 | `					}else{` |
|        - |  7687 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7688 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7689 | `						char zMsg[256];` |
|      ! 0 |  7690 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7691 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7692 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7693 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7694 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7695 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7696 | `						goto Abort;` |
|        - |  7697 | `					}` |
|     3351 |  7698 | `				}` |
|        - |  7699 | `				/* Safely unreference the object */` |
|     6706 |  7700 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7701 | `			}` |
|     3727 |  7702 | `		}else{` |
|        3 |  7703 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7704 | `			VmPopOperand(&pTos,1);` |
|        3 |  7705 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7706 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7707 | `		}` |
|     3728 |  7708 | `	}else{` |
|        - |  7709 | `		/* Static member access using class name */` |
|      228 |  7710 | `		pNos = pTos;` |
|      228 |  7711 | `		pThis = 0;` |
|      228 |  7712 | `		if( !pInstr->p3 ){` |
|      190 |  7713 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7714 | `			pNos--;` |
|        - |  7715 | `#ifdef UNTRUST` |
|        - |  7716 | `			if( pNos < pStack ){` |
|        - |  7717 | `				goto Abort;` |
|        - |  7718 | `			}` |
|        - |  7719 | `#endif` |
|       96 |  7720 | `		}else{` |
|        - |  7721 | `			/* Attribute name already computed */` |
|       40 |  7722 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7723 | `		}` |
|      228 |  7724 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7725 | `			ph7_class *pClass = 0;` |
|      228 |  7726 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7727 | `				/* Class already instantiated */` |
|        5 |  7728 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7729 | `				pClass = pThis->pClass;` |
|        5 |  7730 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7731 | `			}else{` |
|        - |  7732 | `				/* Try to extract the target class */` |
|      224 |  7733 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7734 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7735 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7736 | `					/* Handle self/static/parent keywords */` |
|      224 |  7737 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7738 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7739 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7740 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7741 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7742 | `						}` |
|      194 |  7743 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7744 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7745 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7746 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7747 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7748 | `							pClass = pSelf->pBase;` |
|       13 |  7749 | `						}` |
|       15 |  7750 | `					}else{` |
|      112 |  7751 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7752 | `					}` |
|      111 |  7753 | `				}` |
|        - |  7754 | `			}` |
|      228 |  7755 | `			if( pClass == 0 ){` |
|        - |  7756 | `				/* Undefined class */` |
|      ! 0 |  7757 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7758 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7759 | `					);` |
|      ! 0 |  7760 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7761 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7762 | `				}` |
|      ! 0 |  7763 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7764 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7765 | `			}else{` |
|      228 |  7766 | `				if( pInstr->iP2 ){` |
|        - |  7767 | `					/* Method call */` |
|       86 |  7768 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7769 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7770 | `						/* Extract the target method */` |
|       86 |  7771 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7772 | `					}` |
|       86 |  7773 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7774 | `						if( pMeth ){` |
|      ! 0 |  7775 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7776 | `								&pClass->sName,&sName` |
|        - |  7777 | `								);` |
|      ! 0 |  7778 | `						}else{` |
|      ! 0 |  7779 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7780 | `								&pClass->sName,&sName` |
|        - |  7781 | `								);` |
|        - |  7782 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7783 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7784 | `						}` |
|        - |  7785 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7786 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7787 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7788 | `						}` |
|      ! 0 |  7789 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7790 | `					}else{` |
|        - |  7791 | `						/* Push method name on the stack */` |
|       86 |  7792 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7793 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7794 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7795 | `					}` |
|       86 |  7796 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7797 | `				}else{` |
|        - |  7798 | `					/* Attribute access */` |
|      144 |  7799 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7800 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7801 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7802 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7803 | `						/* ::class returns the fully qualified class name */` |
|        - |  7804 | `						/* Pop the attribute name from the stack */` |
|       60 |  7805 | `						if( !pInstr->p3 ){` |
|       60 |  7806 | `							VmPopOperand(&pTos,1);` |
|       29 |  7807 | `						}` |
|       60 |  7808 | `						PH7_MemObjRelease(pTos);` |
|        - |  7809 | `						/* Load the class name */` |
|       60 |  7810 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7811 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7812 | `					}else{` |
|        - |  7813 | `						/* Extract the target attribute */` |
|       86 |  7814 | `						if( sName.nByte > 0 ){` |
|       86 |  7815 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7816 | `						}` |
|       86 |  7817 | `						if( pAttr == 0 ){` |
|        - |  7818 | `							/* No such attribute,load null */` |
|      ! 0 |  7819 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7820 | `								&pClass->sName,&sName);` |
|        - |  7821 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7822 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7823 | `						}` |
|        - |  7824 | `						/* Pop the attribute name from the stack */` |
|       86 |  7825 | `						if( !pInstr->p3 ){` |
|       48 |  7826 | `							VmPopOperand(&pTos,1);` |
|       23 |  7827 | `						}` |
|       86 |  7828 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7829 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7830 | `						if( pAttr ){` |
|       86 |  7831 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7832 | `								/* Access to a non static attribute */` |
|      ! 0 |  7833 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7834 | `									&pClass->sName,&pAttr->sName` |
|        - |  7835 | `									);` |
|      ! 0 |  7836 | `							}else{` |
|        - |  7837 | `								ph7_value *pValue;` |
|        - |  7838 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7839 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7840 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7841 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7842 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7843 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7844 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7845 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7846 | `										if( pS ){` |
|       28 |  7847 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7848 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7849 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7850 | `												int bIsLhs = 0;` |
|        8 |  7851 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7852 | `													bIsLhs = 1;` |
|        2 |  7853 | `												}` |
|        8 |  7854 | `												if( !bIsLhs ){` |
|        3 |  7855 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7856 | `													if( pThis ){` |
|      ! 0 |  7857 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7858 | `													}` |
|        3 |  7859 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7860 | `														goto Abort;` |
|        - |  7861 | `													}` |
|        - |  7862 | `													{` |
|        3 |  7863 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7864 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7865 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7866 | `															break;` |
|        - |  7867 | `														}` |
|        - |  7868 | `													}` |
|      ! 0 |  7869 | `													goto Exception;` |
|        - |  7870 | `												}` |
|        2 |  7871 | `											}` |
|       12 |  7872 | `										}` |
|       12 |  7873 | `									}` |
|        - |  7874 | `									/* Load the desired attribute */` |
|       80 |  7875 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7876 | `									if( pValue ){` |
|       80 |  7877 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7878 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7879 | `											/* Load index number */` |
|       38 |  7880 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7881 | `										}` |
|       39 |  7882 | `									}` |
|       41 |  7883 | `								}else{` |
|        - |  7884 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7885 | `									char zMsg[256];` |
|        5 |  7886 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7887 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7888 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7889 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7890 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7891 | `									}else{` |
|      ! 0 |  7892 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7893 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7894 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7895 | `									}` |
|        5 |  7896 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7897 | `									goto Abort;` |
|        - |  7898 | `								}` |
|        - |  7899 | `							}` |
|       39 |  7900 | `						}` |
|        - |  7901 | `					}` |
|        - |  7902 | `				}` |
|      222 |  7903 | `				if( pThis ){` |
|        - |  7904 | `					/* Safely unreference the object */` |
|        5 |  7905 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7906 | `				}` |
|        - |  7907 | `			}` |
|      112 |  7908 | `		}else{` |
|        - |  7909 | `			/* Pop operands */` |
|      ! 0 |  7910 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7911 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7912 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7913 | `			}` |
|      ! 0 |  7914 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7915 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7916 | `		}` |
|        - |  7917 | `	}` |
|     7674 |  7918 | `	break;` |
|        - |  7919 | `					}` |
|        - |  7920 | `/*` |
|        - |  7921 | ` * OP_NEW P1 * * *` |
|        - |  7922 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7923 | ` */` |
|      614 |  7924 | `case PH7_OP_NEW: {` |
|     1230 |  7925 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1230 |  7926 | `	ph7_class *pClass = 0;` |
|        - |  7927 | `	ph7_class_instance *pNew;` |
|     1230 |  7928 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7929 | `		/* Try to extract the desired class */` |
|     1844 |  7930 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1228 |  7931 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      614 |  7932 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7933 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7934 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7935 | `	}` |
|     1230 |  7936 | `	if( pClass == 0 ){` |
|        - |  7937 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7938 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7939 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7940 | `			);` |
|        - |  7941 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7942 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7943 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7944 | `			/* Pop given arguments */` |
|      ! 0 |  7945 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7946 | `		}` |
|      ! 0 |  7947 | `		goto Abort;` |
|      ! 0 |  7948 | `	}else{` |
|        - |  7949 | `		ph7_class_method *pCons;` |
|        - |  7950 | `		/* Create a new class instance */` |
|     1230 |  7951 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1230 |  7952 | `		if( pNew == 0 ){` |
|      ! 0 |  7953 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7954 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7955 | `				&pClass->sName` |
|        - |  7956 | `			);` |
|      ! 0 |  7957 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7958 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7959 | `				/* Pop given arguments */` |
|      ! 0 |  7960 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7961 | `			}` |
|      ! 0 |  7962 | `			break;` |
|        - |  7963 | `		}` |
|        - |  7964 | `		/* Check if a constructor is available */` |
|     1230 |  7965 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1230 |  7966 | `		if( pCons == 0 ){` |
|      906 |  7967 | `			SyString *pName = &pClass->sName;` |
|        - |  7968 | `			/* Check for a constructor with the same base class name */` |
|      906 |  7969 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      452 |  7970 | `		}` |
|     1230 |  7971 | `		if( pCons ){` |
|        - |  7972 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7973 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7974 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7975 | `			 * (including variadic string-key packing). */` |
|      326 |  7976 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      326 |  7977 | `			SySetReset(&aArg);` |
|      640 |  7978 | `			while( pArg < pTos ){` |
|      316 |  7979 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      316 |  7980 | `				pArg++;` |
|        2 |  7981 | `			}` |
|      326 |  7982 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7983 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7984 | `				sxu32 n;` |
|       81 |  7985 | `				n = SySetUsed(&aArg);` |
|        - |  7986 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7987 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7988 | `				 * after resolution). */` |
|      149 |  7989 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       69 |  7990 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       69 |  7991 | `					if( pFuncArg ){` |
|       69 |  7992 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7993 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7994 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7995 | `						}` |
|       34 |  7996 | `					}` |
|       69 |  7997 | `					n++;` |
|        1 |  7998 | `				}` |
|       40 |  7999 | `			}` |
|      326 |  8000 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8001 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      326 |  8002 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8003 | `				pNew->iRef = 1;` |
|      ! 0 |  8004 | `			}` |
|      162 |  8005 | `		}` |
|     1230 |  8006 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8007 | `			/* Pop given arguments */` |
|      262 |  8008 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      130 |  8009 | `		}` |
|     1230 |  8010 | `		PH7_MemObjRelease(pTos);` |
|     1230 |  8011 | `		pTos->x.pOther = pNew;` |
|     1230 |  8012 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8013 | `	}` |
|     1230 |  8014 | `	break;` |
|        - |  8015 | `				 }` |
|        - |  8016 | `/*` |
|        - |  8017 | ` * OP_CLONE * * *` |
|        - |  8018 | ` * Perfome a clone operation.` |
|        - |  8019 | ` */` |
|       24 |  8020 | `case PH7_OP_CLONE: {` |
|        - |  8021 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8022 | `#ifdef UNTRUST` |
|        - |  8023 | `	if( pTos < pStack ){` |
|        - |  8024 | `		goto Abort;` |
|        - |  8025 | `	}` |
|        - |  8026 | `#endif` |
|        - |  8027 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8028 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8029 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8030 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8031 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8032 | `		break;` |
|        - |  8033 | `	}` |
|        - |  8034 | `	/* Point to the source */` |
|       46 |  8035 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8036 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8037 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8038 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8039 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8040 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8041 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8042 | `		break;` |
|        - |  8043 | `	}` |
|        - |  8044 | `	/* Perform the clone operation */` |
|       46 |  8045 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8046 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8047 | `	if( pClone == 0 ){` |
|      ! 0 |  8048 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8049 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8050 | `	}else{` |
|        - |  8051 | `		/* Load the cloned object */` |
|       46 |  8052 | `		pTos->x.pOther = pClone;` |
|       46 |  8053 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8054 | `	}` |
|       46 |  8055 | `	break;` |
|        - |  8056 | `				   }` |
|        - |  8057 | `/*` |
|        - |  8058 | ` * OP_SWITCH * * P3` |
|        - |  8059 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8060 | ` */` |
|       26 |  8061 | `case PH7_OP_SWITCH: {` |
|       54 |  8062 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8063 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8064 | `	ph7_value sValue,sCaseValue;` |
|        - |  8065 | `	sxu32 n,nEntry;` |
|        - |  8066 | `#ifdef UNTRUST` |
|        - |  8067 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8068 | `		goto Abort;` |
|        - |  8069 | `	}` |
|        - |  8070 | `#endif` |
|        - |  8071 | `	/* Point to the case table  */` |
|       54 |  8072 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8073 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8074 | `	/* Select the appropriate case block to execute */` |
|       54 |  8075 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8076 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8077 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8078 | `		pCase = &aCase[n];` |
|      130 |  8079 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8080 | `		/* Execute the case expression first */` |
|      130 |  8081 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8082 | `		/* Compare the two expression */` |
|      130 |  8083 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8084 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8085 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8086 | `		if( rc == 0 ){` |
|        - |  8087 | `			/* Value match,jump to this block */` |
|       52 |  8088 | `			pc = pCase->nStart - 1;` |
|       52 |  8089 | `			break;` |
|        - |  8090 | `		}` |
|       41 |  8091 | `	}` |
|       54 |  8092 | `	VmPopOperand(&pTos,1);` |
|       54 |  8093 | `	if( n >= nEntry ){` |
|        - |  8094 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8095 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8096 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8097 | `		}else{` |
|        - |  8098 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8099 | `			pc = pSwitch->nOut - 1;` |
|        - |  8100 | `		}` |
|        1 |  8101 | `	}` |
|       54 |  8102 | `	break;` |
|        - |  8103 | `					}` |
|        - |  8104 | `/*` |
|        - |  8105 | ` * OP_MATCH * * P3` |
|        - |  8106 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8107 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8108 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8109 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8110 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8111 | ` */` |
|       54 |  8112 | `case PH7_OP_MATCH: {` |
|      110 |  8113 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8114 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8115 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8116 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8117 | `	int matched = 0;` |
|        - |  8118 | `#ifdef UNTRUST` |
|        - |  8119 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8120 | `		goto Abort;` |
|        - |  8121 | `	}` |
|        - |  8122 | `#endif` |
|      110 |  8123 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8124 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8125 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8126 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8127 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8128 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8129 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8130 | `		pArm = &aArm[i];` |
|      240 |  8131 | `		if( pArm->bDefault ){` |
|       13 |  8132 | `			pDefault = pArm;` |
|       13 |  8133 | `			continue;` |
|        - |  8134 | `		}` |
|      228 |  8135 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8136 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8137 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8138 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8139 | `				continue;` |
|        - |  8140 | `			}` |
|      260 |  8141 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8142 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8143 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8144 | `			if( rc == 0 ){` |
|       93 |  8145 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8146 | `				matched = 1;` |
|       93 |  8147 | `				break;` |
|        - |  8148 | `			}` |
|       85 |  8149 | `		}` |
|      115 |  8150 | `	}` |
|      110 |  8151 | `	if( !matched && pDefault ){` |
|       13 |  8152 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8153 | `		matched = 1;` |
|        6 |  8154 | `	}` |
|      110 |  8155 | `	if( !matched ){` |
|        5 |  8156 | `		const char *zType = "unknown";` |
|        - |  8157 | `		char zMsg[128];` |
|        - |  8158 | `		sxu32 nMsg;` |
|        5 |  8159 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8160 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8161 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8162 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8163 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8164 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8165 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8166 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8167 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8168 | `		default: break;` |
|        - |  8169 | `		}` |
|        7 |  8170 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8171 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8172 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8173 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8174 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8175 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8176 | `		goto Abort;` |
|        - |  8177 | `	}` |
|      105 |  8178 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8179 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8180 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8181 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8182 | `	break;` |
|        - |  8183 | `					}` |
|        - |  8184 | `/*` |
|        - |  8185 | ` * OP_YIELD P1 P2 *` |
|        - |  8186 | ` *  Yield a value from a generator function.` |
|        - |  8187 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8188 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8189 | ` */` |
|       34 |  8190 | `case PH7_OP_YIELD: {` |
|        - |  8191 | `	ph7_generator *pGen;` |
|       70 |  8192 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8193 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8194 | `		goto Abort;` |
|        - |  8195 | `	}` |
|       70 |  8196 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8197 | `	if( pInstr->iP2 ){` |
|        - |  8198 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8199 | `#ifdef UNTRUST` |
|        - |  8200 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8201 | `#endif` |
|        7 |  8202 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8203 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8204 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8205 | `		VmPopOperand(&pTos, 1);` |
|        - |  8206 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8207 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8208 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8209 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8210 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8211 | `			}` |
|        1 |  8212 | `		}` |
|       67 |  8213 | `	}else if( pInstr->iP1 ){` |
|        - |  8214 | `		/* yield $value */` |
|        - |  8215 | `#ifdef UNTRUST` |
|        - |  8216 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8217 | `#endif` |
|       64 |  8218 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8219 | `		VmPopOperand(&pTos, 1);` |
|        - |  8220 | `		/* Auto-increment key */` |
|       64 |  8221 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8222 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8223 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8224 | `	}else{` |
|        - |  8225 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8226 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8227 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8228 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8229 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8230 | `	}` |
|        - |  8231 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8232 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8233 | `	goto Suspend;` |
|        - |  8234 |  |
|        - |  8235 | `/*` |
|        - |  8236 | ` * OP_CALL P1 * *` |
|        - |  8237 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8238 | ` *  function on the stack.` |
|        - |  8239 | ` */` |
|   350591 |  8240 | `case PH7_OP_CALL: {` |
|   701228 |  8241 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8242 | `	ph7_value *pArg;` |
|   701228 |  8243 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   701228 |  8244 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8245 | `	SyHashEntry *pEntry;` |
|        - |  8246 | `	SyString sName;` |
|        - |  8247 | `	/* Extract function name */` |
|   701228 |  8248 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       78 |  8249 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8250 | `			ph7_value sResult;` |
|      ! 0 |  8251 | `			SySetReset(&aArg);` |
|      ! 0 |  8252 | `			while( pArg < pTos ){` |
|      ! 0 |  8253 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8254 | `				pArg++;` |
|      ! 0 |  8255 | `			}` |
|      ! 0 |  8256 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8257 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  8258 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  8259 | `			SySetReset(&aArg);` |
|        - |  8260 | `			/* Pop given arguments */` |
|      ! 0 |  8261 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8262 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8263 | `			}` |
|        - |  8264 | `			/* Copy result */` |
|      ! 0 |  8265 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8266 | `			PH7_MemObjRelease(&sResult);` |
|       78 |  8267 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       78 |  8268 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8269 | `			ph7_value sResult;` |
|        - |  8270 | `			sxi32 rcInv;` |
|       78 |  8271 | `			SySetReset(&aArg);` |
|      192 |  8272 | `			while( pArg < pTos ){` |
|      116 |  8273 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      116 |  8274 | `				pArg++;` |
|        2 |  8275 | `			}` |
|       78 |  8276 | `			PH7_MemObjInit(pVm,&sResult);` |
|      116 |  8277 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       76 |  8278 | `				(int)SySetUsed(&aArg),` |
|       76 |  8279 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8280 | `				&sResult,` |
|       76 |  8281 | `				(VmCallArgMap *)pInstr->p3);` |
|       78 |  8282 | `			SySetReset(&aArg);` |
|       78 |  8283 | `			if( nCallArgs > 0 ){` |
|       74 |  8284 | `				VmPopOperand(&pTos,nCallArgs);` |
|       36 |  8285 | `			}` |
|       78 |  8286 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8287 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8288 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8289 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8290 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8291 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8292 | `				pThis->iRef++;` |
|       13 |  8293 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8294 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8295 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8296 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8297 | `					goto Abort;` |
|        - |  8298 | `				}` |
|        - |  8299 | `				{` |
|       13 |  8300 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8301 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8302 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8303 | `						pc = pFrm2->iExceptionJump - 1;` |
|       13 |  8304 | `						break;` |
|        - |  8305 | `					}` |
|        - |  8306 | `				}` |
|      ! 0 |  8307 | `				goto Exception;` |
|        - |  8308 | `			}` |
|       66 |  8309 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8310 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8311 | `		}else{` |
|        - |  8312 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8313 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8314 | `			/* Pop given arguments */` |
|      ! 0 |  8315 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8316 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8317 | `			}` |
|        - |  8318 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8319 | `			PH7_MemObjRelease(pTos);` |
|        - |  8320 | `		}` |
|       66 |  8321 | `		break;` |
|        - |  8322 | `	}` |
|   701152 |  8323 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8324 | `	/* Check for a compiled function first.` |
|        - |  8325 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8326 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   701152 |  8327 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8328 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8329 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8330 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8331 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8332 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8333 | `	{` |
|   701152 |  8334 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   701152 |  8335 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8336 | `		const char *zFunc;` |
|        - |  8337 | `		const char *zEnd;` |
|        - |  8338 | `		const char *z;` |
|        - |  8339 | `		SyString sGlobal;` |
|       22 |  8340 | `		zFunc = sName.zString;` |
|       22 |  8341 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8342 | `		z = zEnd;` |
|        - |  8343 | `		/* Find last namespace separator */` |
|      194 |  8344 | `		while( z > zFunc ){` |
|      194 |  8345 | `			if( z[-1] == '\\' ){` |
|       22 |  8346 | `				break;` |
|        - |  8347 | `			}` |
|      174 |  8348 | `			z--;` |
|        2 |  8349 | `		}` |
|       22 |  8350 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8351 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8352 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8353 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8354 | `		}` |
|       10 |  8355 | `	}` |
|        - |  8356 | `	} /* end VmCallArgMap namespace scope */` |
|   701152 |  8357 | `	if( pEntry ){` |
|        - |  8358 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8359 | `		ph7_class_instance *pThis;` |
|        - |  8360 | `		ph7_value *pFrameStack;` |
|        - |  8361 | `		ph7_vm_func *pVmFunc;` |
|        - |  8362 | `		ph7_class *pSelf;` |
|        - |  8363 | `		VmFrame *pFrame;` |
|        - |  8364 | `		ph7_value *pObj;` |
|        - |  8365 | `		VmSlot sArg;` |
|        - |  8366 | `		sxu32 n;` |
|        - |  8367 | `		/* initialize fields */` |
|    17980 |  8368 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    17980 |  8369 | `		pThis = 0;` |
|    17980 |  8370 | `		pSelf = 0;` |
|    17980 |  8371 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8372 | `			ph7_class_method *pMeth;` |
|        - |  8373 | `			/* Class method call */` |
|     3212 |  8374 | `			ph7_value *pTarget = &pTos[-1];` |
|     3212 |  8375 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8376 | `				/* Extract the 'this' pointer */` |
|     3212 |  8377 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8378 | `					/* Instance already loaded */` |
|     3122 |  8379 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3122 |  8380 | `					pThis->iRef++;` |
|     3122 |  8381 | `					pSelf = pThis->pClass;` |
|     1560 |  8382 | `				}` |
|     3212 |  8383 | `				if( pSelf == 0 ){` |
|       92 |  8384 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8385 | `						/* "Late Static Binding" class name */` |
|      128 |  8386 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8387 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8388 | `					}` |
|       92 |  8389 | `					if( pSelf == 0 ){` |
|       21 |  8390 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8391 | `					}` |
|       45 |  8392 | `				}` |
|     3212 |  8393 | `				if( pThis == 0  ){` |
|       92 |  8394 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8395 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8396 | `					if( pFrameLocal->pParent ){` |
|        - |  8397 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8398 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8399 | `						if( pThis ){` |
|       21 |  8400 | `							pThis->iRef++;` |
|       10 |  8401 | `						}` |
|       32 |  8402 | `					}` |
|       45 |  8403 | `				}` |
|     3212 |  8404 | `				VmPopOperand(&pTos,1);` |
|     3212 |  8405 | `				PH7_MemObjRelease(pTos);` |
|        - |  8406 | `				/* Synchronize pointers */` |
|     3212 |  8407 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8408 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8409 | `				 * user have already computed the random generated unique class method name` |
|        - |  8410 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8411 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8412 | `				 */` |
|     3212 |  8413 | `				while( pArg < pStack ){` |
|      ! 0 |  8414 | `					pArg++;` |
|      ! 0 |  8415 | `				}` |
|     3212 |  8416 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8417 | `					/* Check if the call is allowed */` |
|     3212 |  8418 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3212 |  8419 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8420 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8421 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8422 | `							char zMsg[256];` |
|      ! 0 |  8423 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8424 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8425 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8426 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8427 | `							/* Pop given arguments */` |
|      ! 0 |  8428 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8429 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8430 | `							}` |
|      ! 0 |  8431 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8432 | `							goto Abort;` |
|        - |  8433 | `						}` |
|        6 |  8434 | `					}` |
|     1605 |  8435 | `				}` |
|     1605 |  8436 | `			}` |
|     1605 |  8437 | `		}` |
|        - |  8438 | `		/* Check The recursion limit */` |
|    17980 |  8439 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8440 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8441 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8442 | `				&pVmFunc->sName);` |
|        - |  8443 | `			/* Pop given arguments */` |
|        3 |  8444 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8445 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8446 | `			}` |
|        - |  8447 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8448 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8449 | `			break;` |
|        - |  8450 | `		}` |
|    17978 |  8451 | `		if( pVmFunc->pNextName ){` |
|        - |  8452 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8453 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8454 | `		}` |
|    17978 |  8455 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8456 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8457 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8458 | `			ph7_generator *pGenerator;` |
|        - |  8459 | `			ph7_class_instance *pGenObj;` |
|        - |  8460 | `			ph7_value *pCtxAttr;` |
|        - |  8461 | `			SyString sAttrName;` |
|        - |  8462 | `			ph7_value **apCallArgs;` |
|        - |  8463 | `			int nGenArgs, iArg;` |
|        - |  8464 | `			/* Collect arguments from the operand stack */` |
|       24 |  8465 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8466 | `			apCallArgs = 0;` |
|       24 |  8467 | `			if( nGenArgs > 0 ){` |
|       14 |  8468 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8469 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8470 | `				if( apCallArgs == 0 ){` |
|        - |  8471 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8472 | `					nGenArgs = 0;` |
|      ! 0 |  8473 | `				}else{` |
|       10 |  8474 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8475 | `					int didReorder = 0;` |
|       10 |  8476 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8477 | `						/* Named-argument reordering for generator */` |
|        5 |  8478 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8479 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8480 | `						sxu32 nNV = nF;` |
|        5 |  8481 | `						sxi32 iVIdx = -1;` |
|        - |  8482 | `						sxi32 *aGSlot;` |
|        - |  8483 | `						sxu8 *aGUsed;` |
|        - |  8484 | `						sxu32 gi;` |
|       13 |  8485 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8486 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8487 | `						}` |
|        7 |  8488 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8489 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8490 | `						if( aGSlot ){` |
|        5 |  8491 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8492 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8493 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8494 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8495 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8496 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8497 | `								goto Abort;` |
|        - |  8498 | `							}` |
|        - |  8499 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8500 | `							 * append overflow (variadic / positional beyond` |
|        - |  8501 | `							 * formals) so downstream sees every argument. */` |
|        - |  8502 | `							{` |
|        5 |  8503 | `								int nOut = 0;` |
|       13 |  8504 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8505 | `									sxu32 gj;` |
|       13 |  8506 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8507 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8508 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8509 | `											break;` |
|        - |  8510 | `										}` |
|        3 |  8511 | `									}` |
|        5 |  8512 | `								}` |
|       13 |  8513 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8514 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8515 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8516 | `									}` |
|        5 |  8517 | `								}` |
|        5 |  8518 | `								nGenArgs = nOut;` |
|        - |  8519 | `							}` |
|        5 |  8520 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8521 | `							didReorder = 1;` |
|        2 |  8522 | `						}` |
|        - |  8523 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8524 | `						 * positional fill below — preserves arg order rather` |
|        - |  8525 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8526 | `					}` |
|       10 |  8527 | `					if( !didReorder ){` |
|       12 |  8528 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8529 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8530 | `						}` |
|        2 |  8531 | `					}` |
|        - |  8532 | `				}` |
|        4 |  8533 | `			}` |
|        - |  8534 | `			/* Create execution context and generator wrapper */` |
|       24 |  8535 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8536 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8537 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8538 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8539 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8540 | `				break;` |
|        - |  8541 | `			}` |
|       24 |  8542 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8543 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8544 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8545 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8546 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8547 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8548 | `				break;` |
|        - |  8549 | `			}` |
|        - |  8550 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8551 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8552 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8553 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8554 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8555 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8556 | `			if( apCallArgs ){` |
|       10 |  8557 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8558 | `			}` |
|       24 |  8559 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8560 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8561 | `				if( pThis ){` |
|      ! 0 |  8562 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8563 | `				}` |
|      ! 0 |  8564 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8565 | `					goto Abort;` |
|        - |  8566 | `				}` |
|      ! 0 |  8567 | `				break;` |
|        - |  8568 | `			}` |
|        - |  8569 | `			/* Create Generator class instance */` |
|       24 |  8570 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8571 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8572 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8573 | `				break;` |
|        - |  8574 | `			}` |
|        - |  8575 | `			/* Store generator in __ctx attribute */` |
|       24 |  8576 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8577 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8578 | `			if( pCtxAttr ){` |
|       24 |  8579 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8580 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8581 | `			}` |
|        - |  8582 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8583 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8584 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8585 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8586 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8587 | `			pGenObj->iRef++;` |
|       24 |  8588 | `			if( pThis ){` |
|      ! 0 |  8589 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8590 | `			}` |
|       24 |  8591 | `			break;` |
|        - |  8592 | `		}` |
|        - |  8593 | `		/* Extract the formal argument set */` |
|    17956 |  8594 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8595 | `		/* Create a new VM frame  */` |
|    17956 |  8596 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    17956 |  8597 | `		if( rc != SXRET_OK ){` |
|        - |  8598 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8599 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8600 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8601 | `				&pVmFunc->sName);` |
|        - |  8602 | `			/* Pop given arguments */` |
|      ! 0 |  8603 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8604 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8605 | `			}` |
|        - |  8606 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8607 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8608 | `			break;` |
|        - |  8609 | `		}` |
|    17956 |  8610 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8611 | `			/* Install the '$this' variable */` |
|        - |  8612 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3140 |  8613 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3140 |  8614 | `			if( pObj ){` |
|        - |  8615 | `				/* Reflect the change */` |
|     3140 |  8616 | `				pObj->x.pOther = pThis;` |
|     3140 |  8617 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1569 |  8618 | `			}` |
|     1569 |  8619 | `		}` |
|    17956 |  8620 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8621 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8622 | `			/* Install static variables */` |
|      ! 0 |  8623 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8624 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8625 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8626 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8627 | `					/* Initialize the static variables */` |
|      ! 0 |  8628 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8629 | `					if( pObj ){` |
|        - |  8630 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8631 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8632 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8633 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8634 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8635 | `						}` |
|      ! 0 |  8636 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8637 | `					}else{` |
|      ! 0 |  8638 | `						continue;` |
|        - |  8639 | `					}` |
|      ! 0 |  8640 | `				}` |
|        - |  8641 | `				/* Install in the current frame */` |
|      ! 0 |  8642 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8643 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8644 | `			}` |
|      ! 0 |  8645 | `		}` |
|        - |  8646 | `		/* Push arguments in the local frame */` |
|        - |  8647 | `		{` |
|    17956 |  8648 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8649 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8650 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    17956 |  8651 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    17956 |  8652 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8653 | `			/* ============================================================` |
|        - |  8654 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8655 | `			 *` |
|        - |  8656 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8657 | `			 * or position, then install them in the frame.` |
|        - |  8658 | `			 * ============================================================ */` |
|       96 |  8659 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8660 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8661 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8662 | `			sxu32 nNonVariadic;` |
|        - |  8663 | `			sxi32 *aSlot;` |
|        - |  8664 | `			sxu8  *aUsed;` |
|        - |  8665 | `			sxu32 i;` |
|        - |  8666 | `			/* Find variadic parameter index */` |
|      292 |  8667 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8668 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8669 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8670 | `					break;` |
|        - |  8671 | `				}` |
|      100 |  8672 | `			}` |
|       96 |  8673 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8674 | `			/* Allocate mapping arrays */` |
|      143 |  8675 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8676 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8677 | `			if( aSlot == 0 ){` |
|      ! 0 |  8678 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8679 | `				goto Abort;` |
|        - |  8680 | `			}` |
|       96 |  8681 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8682 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8683 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8684 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8685 | `			if( rc == PH7_ABORT ){` |
|        7 |  8686 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8687 | `				goto Abort;` |
|        - |  8688 | `			}` |
|        - |  8689 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8690 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8691 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8692 | `				sxi32 iSrc = -1;` |
|      309 |  8693 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8694 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8695 | `						iSrc = (sxi32)i;` |
|      169 |  8696 | `						break;` |
|        - |  8697 | `					}` |
|       62 |  8698 | `				}` |
|      187 |  8699 | `				if( iSrc >= 0 ){` |
|        - |  8700 | `					/* Argument was provided — install with type checking */` |
|      169 |  8701 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8702 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8703 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8704 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8705 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8706 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8707 | `					}` |
|        - |  8708 | `					/* Type checking: union types */` |
|      169 |  8709 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8710 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8711 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8712 | `							bCallIsStrict);` |
|       13 |  8713 | `						if( rcU != SXRET_OK ){` |
|        - |  8714 | `							const char *zGiven;` |
|      ! 0 |  8715 | `							const char *zExpected = "union";` |
|        - |  8716 | `							char zBuf[128];` |
|        - |  8717 | `							char zTypeBuf[128];` |
|      ! 0 |  8718 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8719 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8720 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8721 | `								zGiven = "null";` |
|      ! 0 |  8722 | `							}else{` |
|      ! 0 |  8723 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8724 | `							}` |
|      ! 0 |  8725 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8726 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8727 | `							}` |
|      ! 0 |  8728 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8729 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8730 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8731 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8732 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8733 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8734 | `							pFrameStack = 0;` |
|      ! 0 |  8735 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8736 | `							goto SkipFuncBody;` |
|        - |  8737 | `						}` |
|      171 |  8738 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8739 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8740 | `						/* Scalar/class type checking */` |
|       17 |  8741 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8742 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8743 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8744 | `							if( pClass ){` |
|      ! 0 |  8745 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8746 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8747 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8748 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8749 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8750 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8751 | `									}` |
|      ! 0 |  8752 | `								}else{` |
|      ! 0 |  8753 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8754 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8755 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8756 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8757 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8758 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8759 | `									}` |
|        - |  8760 | `								}` |
|      ! 0 |  8761 | `							}` |
|       17 |  8762 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8763 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8764 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8765 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8766 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8767 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8768 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8769 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8770 | `								pFrameStack = 0;` |
|      ! 0 |  8771 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8772 | `								goto SkipFuncBody;` |
|        7 |  8773 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8774 | `								char zTypeBuf[128];` |
|      ! 0 |  8775 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8776 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8777 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8778 | `									ph7_type_name(pVal));` |
|      ! 0 |  8779 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8780 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8781 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8782 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8783 | `								pFrameStack = 0;` |
|      ! 0 |  8784 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8785 | `								goto SkipFuncBody;` |
|        - |  8786 | `							}` |
|        3 |  8787 | `						}` |
|        8 |  8788 | `					}` |
|        - |  8789 | `					/* Install: by reference or by value */` |
|      169 |  8790 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8791 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8792 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8793 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8794 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8795 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8796 | `							}` |
|      ! 0 |  8797 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8798 | `						}else{` |
|        7 |  8799 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8800 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8801 | `							if( pRefEntry == 0 ){` |
|        7 |  8802 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8803 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8804 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8805 | `								sArg.pUserData = 0;` |
|        5 |  8806 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8807 | `							}` |
|        5 |  8808 | `							pObj = 0;` |
|        - |  8809 | `						}` |
|        3 |  8810 | `					}else{` |
|      165 |  8811 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8812 | `					}` |
|      169 |  8813 | `					if( pObj ){` |
|      165 |  8814 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8815 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8816 | `						sArg.pUserData = 0;` |
|      165 |  8817 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8818 | `					}` |
|       85 |  8819 | `				}else{` |
|        - |  8820 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8821 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8822 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8823 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8824 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8825 | `						if( pObj ){` |
|       19 |  8826 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8827 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8828 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8829 | `							sArg.pUserData = 0;` |
|       19 |  8830 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8831 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8832 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8833 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8834 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8835 | `							}` |
|        9 |  8836 | `						}` |
|        9 |  8837 | `					}` |
|        - |  8838 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8839 | `				}` |
|       94 |  8840 | `			}` |
|        - |  8841 | `			/* Handle variadic parameter */` |
|       89 |  8842 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8843 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8844 | `				if( pObj ){` |
|        9 |  8845 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8846 | `					{` |
|        9 |  8847 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8848 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8849 | `							if( aSlot[i] == -1 ){` |
|       16 |  8850 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8851 | `									/* Named variadic entry: insert with string key */` |
|        - |  8852 | `									ph7_value sKey;` |
|       11 |  8853 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8854 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8855 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8856 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8857 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8858 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8859 | `								}else{` |
|        - |  8860 | `									/* Positional variadic entry */` |
|      ! 0 |  8861 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8862 | `								}` |
|        5 |  8863 | `							}` |
|       12 |  8864 | `						}` |
|        - |  8865 | `					}` |
|        9 |  8866 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8867 | `					sArg.pUserData = 0;` |
|        9 |  8868 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8869 | `				}` |
|        5 |  8870 | `			}else{` |
|        - |  8871 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8872 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8873 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8874 | `				 * the positional-only path's behavior. */` |
|       81 |  8875 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8876 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8877 | `					if( aSlot[i] == -2 ){` |
|        - |  8878 | `						char zAnonBuf[32];` |
|        - |  8879 | `						SyString sAnonName;` |
|      ! 0 |  8880 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8881 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8882 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8883 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8884 | `						if( pObj ){` |
|      ! 0 |  8885 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8886 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8887 | `							sArg.pUserData = 0;` |
|      ! 0 |  8888 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8889 | `						}` |
|      ! 0 |  8890 | `						nAnon++;` |
|      ! 0 |  8891 | `					}` |
|       79 |  8892 | `				}` |
|        - |  8893 | `			}` |
|        - |  8894 | `			/* Release all stack arguments */` |
|      267 |  8895 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  8896 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  8897 | `			}` |
|       89 |  8898 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8899 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  8900 | `			n = nFormal;` |
|       45 |  8901 | `		}else{` |
|        - |  8902 | `		/* ============================================================` |
|        - |  8903 | `		 * Positional-only matching path (original)` |
|        - |  8904 | `		 * ============================================================ */` |
|    17862 |  8905 | `		n = 0;` |
|    47728 |  8906 | `		while( pArg < pTos ){` |
|    29938 |  8907 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8908 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  8909 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  8910 | `				if( pObj ){` |
|        - |  8911 | `					/* Initialize as empty array */` |
|       40 |  8912 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8913 | `					{` |
|       40 |  8914 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  8915 | `						while( pArg < pTos ){` |
|        - |  8916 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8917 | `							 *` |
|        - |  8918 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8919 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8920 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8921 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8922 | `							 * fixing both wants a separate counter for elements` |
|        - |  8923 | `							 * already packed into the variadic array. */` |
|      114 |  8924 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8925 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8926 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8927 | `									bCallIsStrict);` |
|       16 |  8928 | `								if( rcU != SXRET_OK ){` |
|        - |  8929 | `									const char *zGiven;` |
|        3 |  8930 | `									const char *zExpected = "union";` |
|        - |  8931 | `									char zBuf[128];` |
|        - |  8932 | `									char zTypeBuf[128];` |
|        3 |  8933 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8934 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8935 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8936 | `										zGiven = "null";` |
|      ! 0 |  8937 | `									}else{` |
|        3 |  8938 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8939 | `									}` |
|        3 |  8940 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8941 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8942 | `									}` |
|        4 |  8943 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8944 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8945 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8946 | `										goto Abort;` |
|        - |  8947 | `									}` |
|        3 |  8948 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8949 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8950 | `									pFrameStack = 0;` |
|        3 |  8951 | `									rc = PH7_EXCEPTION;` |
|        3 |  8952 | `									goto SkipFuncBody;` |
|        - |  8953 | `								}` |
|       14 |  8954 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8955 | `								pArg++;` |
|       14 |  8956 | `								continue;` |
|        - |  8957 | `							}` |
|        - |  8958 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8959 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  8960 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8961 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8962 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8963 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8964 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8965 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8966 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8967 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8968 | `										goto Abort;` |
|        - |  8969 | `									}` |
|        - |  8970 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8971 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8972 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8973 | `									pFrameStack = 0;` |
|      ! 0 |  8974 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8975 | `									goto SkipFuncBody;` |
|       13 |  8976 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8977 | `									char zTypeBuf[128];` |
|      ! 0 |  8978 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8979 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8980 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8981 | `										ph7_type_name(pArg));` |
|      ! 0 |  8982 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8983 | `										goto Abort;` |
|        - |  8984 | `									}` |
|      ! 0 |  8985 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8986 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8987 | `									pFrameStack = 0;` |
|      ! 0 |  8988 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8989 | `									goto SkipFuncBody;` |
|        - |  8990 | `								}` |
|        6 |  8991 | `							}` |
|      100 |  8992 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  8993 | `							pArg++;` |
|        2 |  8994 | `						}` |
|        - |  8995 | `					}` |
|       38 |  8996 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  8997 | `					sArg.pUserData = 0;` |
|       38 |  8998 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8999 | `				}` |
|       38 |  9000 | `				break; /* All remaining args consumed */` |
|        - |  9001 | `			}` |
|    29900 |  9002 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    29716 |  9003 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       37 |  9004 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9005 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9006 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9007 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9008 | `						goto Abort;` |
|        - |  9009 | `					}` |
|      ! 0 |  9010 | `				}` |
|        - |  9011 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    29718 |  9012 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9013 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9014 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9015 | `						bCallIsStrict);` |
|       60 |  9016 | `					if( rcU != SXRET_OK ){` |
|        - |  9017 | `						const char *zGiven;` |
|       19 |  9018 | `						const char *zExpected = "union";` |
|        - |  9019 | `						char zBuf[128];` |
|        - |  9020 | `						char zTypeBuf[128];` |
|       19 |  9021 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9022 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9023 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9024 | `							zGiven = "null";` |
|        5 |  9025 | `						}else{` |
|        5 |  9026 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9027 | `						}` |
|       19 |  9028 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9029 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9030 | `						}` |
|       28 |  9031 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9032 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9033 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9034 | `							goto Abort;` |
|        - |  9035 | `						}` |
|       19 |  9036 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9037 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9038 | `						pFrameStack = 0;` |
|       19 |  9039 | `						rc = PH7_EXCEPTION;` |
|       19 |  9040 | `						goto SkipFuncBody;` |
|        - |  9041 | `					}` |
|       21 |  9042 | `				}else` |
|        - |  9043 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9044 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    29684 |  9045 | `				if( aFormalArg[n].nType > 0` |
|    15535 |  9046 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1384 |  9047 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9048 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9049 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9050 | `						ph7_class *pClass;` |
|        - |  9051 | `						/* Try to extract the desired class */` |
|       26 |  9052 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9053 | `						if( pClass ){` |
|       22 |  9054 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9055 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9056 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9057 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9058 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9059 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9060 | `								}` |
|      ! 0 |  9061 | `							}else{` |
|        - |  9062 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9063 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9064 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9065 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9066 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9067 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9068 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9069 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9070 | `								}` |
|        - |  9071 | `							}` |
|       12 |  9072 | `						}` |
|     1372 |  9073 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  9074 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9075 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9076 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9077 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9078 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9079 | `								goto Abort;` |
|        - |  9080 | `							}` |
|        - |  9081 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9082 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9083 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9084 | `							pFrameStack = 0;` |
|       11 |  9085 | `							rc = PH7_EXCEPTION;` |
|       11 |  9086 | `							goto SkipFuncBody;` |
|       14 |  9087 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9088 | `							char zTypeBuf[128];` |
|        7 |  9089 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  9090 | `								&aFormalArg[n].sName,` |
|        4 |  9091 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  9092 | `								ph7_type_name(pArg));` |
|        5 |  9093 | `							if( rc == PH7_ABORT ){` |
|        5 |  9094 | `								goto Abort;` |
|        - |  9095 | `							}` |
|      ! 0 |  9096 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9097 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9098 | `							pFrameStack = 0;` |
|      ! 0 |  9099 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9100 | `							goto SkipFuncBody;` |
|        - |  9101 | `						}` |
|        4 |  9102 | `					}` |
|      684 |  9103 | `				}` |
|    29686 |  9104 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9105 | `					/* Pass by reference */` |
|       58 |  9106 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9107 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9108 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9109 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9110 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9111 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9112 | `						}` |
|        - |  9113 | `						/* Switch to pass by value */` |
|      ! 0 |  9114 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9115 | `					}else{` |
|        - |  9116 | `						SyHashEntry *pRefEntry;` |
|        - |  9117 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9118 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9119 | `						if( pRefEntry == 0 ){` |
|       86 |  9120 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9121 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9122 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9123 | `							sArg.pUserData = 0;` |
|       58 |  9124 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9125 | `						}` |
|       58 |  9126 | `						pObj = 0;` |
|        - |  9127 | `					}` |
|       30 |  9128 | `				}else{` |
|        - |  9129 | `					/* Pass by value,make a copy of the given argument */` |
|    29630 |  9130 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9131 | `				}` |
|    14844 |  9132 | `			}else{` |
|        - |  9133 | `				char zName[32];` |
|        - |  9134 | `				SyString sArgName;` |
|        - |  9135 | `				/* Set a dummy name */` |
|      184 |  9136 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      184 |  9137 | `				sArgName.zString = zName;` |
|        - |  9138 | `				/* Annonymous argument */` |
|      184 |  9139 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9140 | `			}` |
|    29868 |  9141 | `			if( pObj ){` |
|    29812 |  9142 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9143 | `				/* Insert argument index  */` |
|    29812 |  9144 | `				sArg.nIdx = pObj->nIdx;` |
|    29812 |  9145 | `				sArg.pUserData = 0;` |
|    29812 |  9146 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    14905 |  9147 | `			}` |
|    29868 |  9148 | `			PH7_MemObjRelease(pArg);` |
|    29868 |  9149 | `			pArg++;` |
|    29868 |  9150 | `			++n;` |
|        2 |  9151 | `		}` |
|        - |  9152 | `		} /* end named vs positional branch */` |
|        - |  9153 | `		/* Set up closure environment */` |
|    17916 |  9154 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9155 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9156 | `			ph7_value *pValue;` |
|        - |  9157 | `			sxu32 iEnv;` |
|      120 |  9158 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      306 |  9159 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      188 |  9160 | `				pEnv = &aEnv[iEnv];` |
|      188 |  9161 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9162 | `					/* Do not install null value */` |
|      114 |  9163 | `					continue;` |
|        - |  9164 | `				}` |
|       76 |  9165 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9166 | `				if( pValue == 0 ){` |
|      ! 0 |  9167 | `					continue;` |
|        - |  9168 | `				}` |
|        - |  9169 | `				/* Invalidate any prior representation */` |
|       76 |  9170 | `				PH7_MemObjRelease(pValue);` |
|        - |  9171 | `				/* Duplicate bound variable value */` |
|       76 |  9172 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9173 | `			}` |
|       59 |  9174 | `		}` |
|        - |  9175 | `		/* Process default values for remaining formal parameters */` |
|    20650 |  9176 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2782 |  9177 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9178 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9179 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9180 | `				if( pObj ){` |
|       48 |  9181 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9182 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9183 | `					sArg.pUserData = 0;` |
|       48 |  9184 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9185 | `				}` |
|       48 |  9186 | `				n++;` |
|       48 |  9187 | `				break; /* Variadic is always last */` |
|        - |  9188 | `			}` |
|     2736 |  9189 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2730 |  9190 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2730 |  9191 | `				if( pObj ){` |
|        - |  9192 | `					/* Evaluate the default value and extract it's result */` |
|     2730 |  9193 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2730 |  9194 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9195 | `						goto Abort;` |
|        - |  9196 | `					}` |
|        - |  9197 | `					/* Insert argument index */` |
|     2730 |  9198 | `					sArg.nIdx = pObj->nIdx;` |
|     2730 |  9199 | `					sArg.pUserData = 0;` |
|     2730 |  9200 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9201 | `					/* Make sure the default argument is of the correct type */` |
|     2728 |  9202 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1786 |  9203 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9204 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9205 | `						/* Cast to the desired type */` |
|        3 |  9206 | `						xCast(pObj);` |
|        1 |  9207 | `					}` |
|     1364 |  9208 | `				}` |
|     1364 |  9209 | `			}` |
|     2736 |  9210 | `			++n;` |
|        2 |  9211 | `		}` |
|        - |  9212 | `		} /* end VmCallArgMap scope */` |
|        - |  9213 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9214 | `		 * does not return anything.` |
|        - |  9215 | `		 */` |
|    17916 |  9216 | `		PH7_MemObjRelease(pTos);` |
|    17916 |  9217 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9218 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    17916 |  9219 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    17916 |  9220 | `		if( pFrameStack == 0 ){` |
|        - |  9221 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9222 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9223 | `				&pVmFunc->sName);` |
|      ! 0 |  9224 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9225 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9226 | `			}` |
|      ! 0 |  9227 | `			break;` |
|        - |  9228 | `		}` |
|     8957 |  9229 | `SkipFuncBody:` |
|    17946 |  9230 | `		if( pSelf ){` |
|        - |  9231 | `			/* Push class name */` |
|     3210 |  9232 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1604 |  9233 | `		}` |
|        - |  9234 | `		/* Increment nesting level */` |
|    17946 |  9235 | `		pVm->nRecursionDepth++;` |
|    17946 |  9236 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9237 | `			/* Execute function body */` |
|    26873 |  9238 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    17914 |  9239 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8957 |  9240 | `		}` |
|        - |  9241 | `		/* Decrement nesting level */` |
|    17946 |  9242 | `		pVm->nRecursionDepth--;` |
|    17946 |  9243 | `		if( pSelf ){` |
|        - |  9244 | `			/* Pop class name */` |
|     3210 |  9245 | `			(void)SySetPop(&pVm->aSelf);` |
|     1604 |  9246 | `		}` |
|        - |  9247 | `		/* Cleanup the mess left behind */` |
|    17946 |  9248 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9249 | `			/* Return by reference,reflect that */` |
|        9 |  9250 | `			if( n != SXU32_HIGH ){` |
|        9 |  9251 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9252 | `				sxu32 i;` |
|        - |  9253 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9254 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9255 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9256 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9257 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9258 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9259 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9260 | `								&pVmFunc->sName);` |
|      ! 0 |  9261 | `						}` |
|      ! 0 |  9262 | `						n = SXU32_HIGH;` |
|      ! 0 |  9263 | `						break;` |
|        - |  9264 | `					}` |
|        3 |  9265 | `				}` |
|        5 |  9266 | `			}else{` |
|      ! 0 |  9267 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9268 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9269 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9270 | `						&pVmFunc->sName);` |
|      ! 0 |  9271 | `				}` |
|        - |  9272 | `			}` |
|        9 |  9273 | `			pTos->nIdx = n;` |
|        4 |  9274 | `		}` |
|        - |  9275 | `		/* Cleanup the mess left behind */` |
|    17946 |  9276 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9277 | `			/* An exception was throw in this frame */` |
|       64 |  9278 | `			pFrame = pFrame->pParent;` |
|       64 |  9279 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9280 | `				/* Pop the resutlt */` |
|       62 |  9281 | `				VmPopOperand(&pTos,1);` |
|        - |  9282 | `				/* Jump to this destination */` |
|       62 |  9283 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9284 | `				rc = PH7_OK;` |
|       32 |  9285 | `			}else{` |
|        3 |  9286 | `				if( pFrame->pParent ){` |
|        3 |  9287 | `					rc = PH7_EXCEPTION;` |
|        2 |  9288 | `				}else{` |
|        - |  9289 | `					/* Continue normal execution */` |
|      ! 0 |  9290 | `					rc = PH7_OK;` |
|        - |  9291 | `				}` |
|        - |  9292 | `			}` |
|       31 |  9293 | `		}` |
|        - |  9294 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    17946 |  9295 | `		if( pFrameStack ){` |
|    17916 |  9296 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8957 |  9297 | `		}` |
|        - |  9298 | `		/* Leave the frame */` |
|    17946 |  9299 | `		VmLeaveFrame(&(*pVm));` |
|    17946 |  9300 | `		if( rc == PH7_ABORT ){` |
|        - |  9301 | `			/* Abort processing immeditaley */` |
|       15 |  9302 | `			goto Abort;` |
|    17932 |  9303 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9304 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9305 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9306 | `			 * overwriting the state saved by the inner level.` |
|        - |  9307 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9308 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9309 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9310 | `			goto Suspend;` |
|    17894 |  9311 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  9312 | `			goto Exception;` |
|        - |  9313 | `		}` |
|     8947 |  9314 | `	}else{` |
|        - |  9315 | `		ph7_user_func *pFunc;` |
|        - |  9316 | `		ph7_context sCtx;` |
|        - |  9317 | `		ph7_value sRet;` |
|        - |  9318 | `		/* Look for an installed foreign function.` |
|        - |  9319 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9320 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9321 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9322 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   683174 |  9323 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9324 | `		{` |
|   683174 |  9325 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   683174 |  9326 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9327 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9328 | `			const char *zShort = sName.zString;` |
|        - |  9329 | `			sxu32 i;` |
|      334 |  9330 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9331 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9332 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9333 | `				}` |
|      158 |  9334 | `			}` |
|       22 |  9335 | `			if( zShort != sName.zString ){` |
|       22 |  9336 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9337 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9338 | `			}` |
|       10 |  9339 | `		}` |
|        - |  9340 | `		} /* end VmCallArgMap namespace scope */` |
|   683174 |  9341 | `		if( pEntry == 0 ){` |
|        - |  9342 | `			/* Call to undefined function */` |
|        5 |  9343 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9344 | `			/* Pop given arguments */` |
|        5 |  9345 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9346 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9347 | `			}` |
|        - |  9348 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9349 | `			PH7_MemObjRelease(pTos);` |
|       45 |  9350 | `			break;` |
|        - |  9351 | `		}` |
|   683170 |  9352 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9353 | `		/* Start collecting function arguments */` |
|   683170 |  9354 | `		SySetReset(&aArg);` |
|  1840404 |  9355 | `		while( pArg < pTos ){` |
|  1157236 |  9356 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1157236 |  9357 | `			pArg++;` |
|        2 |  9358 | `		}` |
|        - |  9359 | `		/* Assume a null return value */` |
|   683170 |  9360 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9361 | `		/* Init the call context */` |
|   683170 |  9362 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9363 | `		/* Call the foreign function */` |
|   683170 |  9364 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9365 | `		/* Release the call context */` |
|   683170 |  9366 | `		VmReleaseCallContext(&sCtx);` |
|   683170 |  9367 | `		if( rc == PH7_ABORT ){` |
|      491 |  9368 | `			goto Abort;` |
|   682680 |  9369 | `		}else if( rc == PH7_EXCEPTION ){` |
|       86 |  9370 | `			VmFrame *pFrm = pVm->pFrame;` |
|       86 |  9371 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       86 |  9372 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9373 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9374 | `				goto Exception;` |
|        - |  9375 | `			}` |
|        - |  9376 | `			/* Exception was caught: pop args and the result slot */` |
|       82 |  9377 | `			PH7_MemObjRelease(&sRet);` |
|       82 |  9378 | `			if( pInstr->iP1 > 0 ){` |
|       66 |  9379 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       32 |  9380 | `			}` |
|        - |  9381 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|       82 |  9382 | `			VmPopOperand(&pTos,1);` |
|        - |  9383 | `			/* Jump past the try/catch block via the exception frame */` |
|       82 |  9384 | `			pFrm = pVm->pFrame;` |
|       82 |  9385 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|       82 |  9386 | `				pc = pFrm->iExceptionJump - 1;` |
|       40 |  9387 | `			}` |
|       82 |  9388 | `			break;` |
|        - |  9389 | `		}` |
|   682596 |  9390 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9391 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9392 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9393 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9394 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9395 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9396 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9397 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9398 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9399 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9400 | `			}` |
|        - |  9401 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9402 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9403 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9404 | `			goto Suspend;` |
|        - |  9405 | `		}` |
|   682558 |  9406 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9407 | `			/* Pop function name and arguments */` |
|   661118 |  9408 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   330580 |  9409 | `		}` |
|        - |  9410 | `		/* Save foreign function return value */` |
|   682558 |  9411 | `		PH7_MemObjStore(&sRet,pTos);` |
|   682558 |  9412 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9413 | `	}` |
|   700448 |  9414 | `	break;` |
|        - |  9415 | `				  }` |
|        - |  9416 | `/*` |
|        - |  9417 | ` * OP_CONSUME: P1 * *` |
|        - |  9418 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9419 | ` */` |
|    15169 |  9420 | `case PH7_OP_CONSUME: {` |
|    30340 |  9421 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    30340 |  9422 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9423 |  |
|    30340 |  9424 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    30340 |  9425 | `	pCur = pOut;` |
|        - |  9426 | `	/* Start the consume process  */` |
|    60678 |  9427 | `	while( pOut <= pTos ){` |
|        - |  9428 | `		/* Force a string cast */` |
|    30340 |  9429 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      928 |  9430 | `			PH7_MemObjToString(pOut);` |
|      463 |  9431 | `		}` |
|    30340 |  9432 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9433 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9434 | `			/* Invoke the output consumer callback */` |
|    18182 |  9435 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    18182 |  9436 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    18182 |  9437 | `			SyBlobRelease(&pOut->sBlob);` |
|    18182 |  9438 | `			if( rc == SXERR_ABORT ){` |
|        - |  9439 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9440 | `				goto Abort;` |
|        - |  9441 | `			}` |
|     9090 |  9442 | `		}` |
|    30340 |  9443 | `		pOut++;` |
|        2 |  9444 | `	}` |
|    30340 |  9445 | `	pTos = &pCur[-1];` |
|    30338 |  9446 | `	break;` |
|        - |  9447 | `					 }` |
|        - |  9448 |  |
|        - |  9449 | `		} /* Switch() */` |
| 11595142 |  9450 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9451 | `	} /* For(;;) */` |
|    21500 |  9452 | `Done:` |
|    43002 |  9453 | `	SySetRelease(&aArg);` |
|    43002 |  9454 | `	return SXRET_OK;` |
|       72 |  9455 | `Suspend:` |
|      146 |  9456 | `	SySetRelease(&aArg);` |
|      146 |  9457 | `	return PH7_SUSPEND;` |
|      269 |  9458 | `Abort:` |
|      539 |  9459 | `	SySetRelease(&aArg);` |
|     1839 |  9460 | `	while( pTos >= pStack ){` |
|     1301 |  9461 | `		PH7_MemObjRelease(pTos);` |
|     1301 |  9462 | `		pTos--;` |
|        1 |  9463 | `	}` |
|      539 |  9464 | `	return PH7_ABORT;` |
|       10 |  9465 | `Exception:` |
|       22 |  9466 | `	SySetRelease(&aArg);` |
|       36 |  9467 | `	while( pTos >= pStack ){` |
|       16 |  9468 | `		PH7_MemObjRelease(pTos);` |
|       16 |  9469 | `		pTos--;` |
|        2 |  9470 | `	}` |
|       22 |  9471 | `	return PH7_EXCEPTION;` |
|    21853 |  9472 |  |
|        - |  9473 | `/*` |
|        - |  9474 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9475 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9476 | ` * See block-comment on that function for additional information.` |
|        - |  9477 | ` */` |
|    19972 |  9478 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9479 |  |
|        - |  9480 | `	ph7_value *pStack;` |
|        - |  9481 | `	sxi32 rc;` |
|        - |  9482 | `	/* Allocate a new operand stack */` |
|    19974 |  9483 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    19974 |  9484 | `	if( pStack == 0 ){` |
|      ! 0 |  9485 | `		return SXERR_MEM;` |
|        - |  9486 | `	}` |
|        - |  9487 | `	/* Execute the program */` |
|    19974 |  9488 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9489 | `	/* Free the operand stack */` |
|    19974 |  9490 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9491 | `	/* Execution result */` |
|    19974 |  9492 | `	return rc;` |
|     9988 |  9493 |  |
|        - |  9494 | `/*` |
|        - |  9495 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9496 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9497 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9498 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9499 | ` * execution ends.` |
|        - |  9500 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9501 | ` * additional information.` |
|        - |  9502 | ` */` |
|     2800 |  9503 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9504 |  |
|        - |  9505 | `	VmShutdownCB *pEntry;` |
|        - |  9506 | `	ph7_value *apArg[10];` |
|        - |  9507 | `	sxu32 n,nEntry;` |
|        - |  9508 | `	int i;` |
|        - |  9509 | `	/* Point to the stack of registered callbacks */` |
|     2802 |  9510 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    30802 |  9511 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28002 |  9512 | `		apArg[i] = 0;` |
|    14002 |  9513 | `	}` |
|     2804 |  9514 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  9515 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9516 | `		if( pEntry ){` |
|        - |  9517 | `			/* Prepare callback arguments if any */` |
|        3 |  9518 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9519 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9520 | `					break;` |
|        - |  9521 | `				}` |
|      ! 0 |  9522 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9523 | `			}` |
|        - |  9524 | `			/* Invoke the callback */` |
|        3 |  9525 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9526 | `			/*` |
|        - |  9527 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9528 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9529 | `			 */` |
|        3 |  9530 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9531 | `			if( pEntry ){` |
|        3 |  9532 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  9533 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9534 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9535 | `				}` |
|        1 |  9536 | `			}` |
|        1 |  9537 | `		}` |
|        2 |  9538 | `	}` |
|     2802 |  9539 | `	SySetReset(&pVm->aShutdown);` |
|     2802 |  9540 |  |
|        - |  9541 | `/*` |
|        - |  9542 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9543 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9544 | ` * See block-comment on that function for additional information.` |
|        - |  9545 | ` */` |
|     2808 |  9546 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9547 |  |
|        - |  9548 | `	/* Make sure we are ready to execute this program */` |
|     2810 |  9549 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9550 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9551 | `	}` |
|        - |  9552 | `	/* Set the execution magic number  */` |
|     2810 |  9553 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9554 | `	/* Execute the program */` |
|     2810 |  9555 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9556 | `	/* Invoke any shutdown callbacks */` |
|     2806 |  9557 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9558 | `	/*` |
|        - |  9559 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9560 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9561 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9562 | `	 */` |
|     2806 |  9563 | `	return SXRET_OK;` |
|     1406 |  9564 |  |
|        - |  9565 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9566 | `/*` |
|        - |  9567 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9568 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9569 | ` */` |
|       46 |  9570 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9571 |  |
|        - |  9572 | `	ph7_exec_ctx *pCtx;` |
|        - |  9573 | `	ph7_value *pStack;` |
|        - |  9574 | `	VmFrame *pFrame;` |
|       48 |  9575 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9576 | `	if( pCtx == 0 ){` |
|      ! 0 |  9577 | `		return 0;` |
|        - |  9578 | `	}` |
|       48 |  9579 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9580 | `	pCtx->pVm = pVm;` |
|       48 |  9581 | `	pCtx->pFunc = pFunc;` |
|       48 |  9582 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9583 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9584 | `	pCtx->pc = 0;` |
|       48 |  9585 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9586 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9587 | `	/* Allocate a private operand stack */` |
|       48 |  9588 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9589 | `	if( pStack == 0 ){` |
|      ! 0 |  9590 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9591 | `		return 0;` |
|        - |  9592 | `	}` |
|       48 |  9593 | `	pCtx->pStack = pStack;` |
|        - |  9594 | `	/* Create a detached frame for the fiber */` |
|       48 |  9595 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9596 | `	if( pFrame == 0 ){` |
|      ! 0 |  9597 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9598 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9599 | `		return 0;` |
|        - |  9600 | `	}` |
|       48 |  9601 | `	pCtx->pFrame = pFrame;` |
|       48 |  9602 | `	return pCtx;` |
|       25 |  9603 |  |
|        - |  9604 | `/*` |
|        - |  9605 | ` * Start executing a fiber context for the first time.` |
|        - |  9606 | ` */` |
|       46 |  9607 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9608 |  |
|        - |  9609 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9610 | `	sxi32 rc;` |
|       48 |  9611 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9612 | `		return SXERR_INVALID;` |
|        - |  9613 | `	}` |
|        - |  9614 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9615 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9616 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9617 | `	/* Save and set the active context */` |
|       48 |  9618 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9619 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9620 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9621 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9622 | `	pVm->nRecursionDepth++;` |
|        - |  9623 | `	/* Execute from the beginning */` |
|       48 |  9624 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9625 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9626 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9627 | `	pVm->nRecursionDepth--;` |
|        - |  9628 | `	/* Restore the previous context */` |
|       48 |  9629 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9630 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9631 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9632 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9633 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9634 | `		if( pResult ){` |
|       24 |  9635 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9636 | `		}` |
|       46 |  9637 | `		return SXRET_OK;` |
|        - |  9638 | `	}` |
|        - |  9639 | `	/* Detach frame */` |
|        3 |  9640 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9641 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9642 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9643 | `	}` |
|        3 |  9644 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9645 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9646 | `		return PH7_ABORT;` |
|        - |  9647 | `	}` |
|        3 |  9648 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9649 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9650 | `		return PH7_EXCEPTION;` |
|        - |  9651 | `	}` |
|        - |  9652 | `	/* Normal completion */` |
|        3 |  9653 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9654 | `	if( pResult ){` |
|        3 |  9655 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9656 | `	}` |
|        3 |  9657 | `	return SXRET_OK;` |
|       25 |  9658 |  |
|        - |  9659 | `/*` |
|        - |  9660 | ` * Resume a suspended fiber context.` |
|        - |  9661 | ` */` |
|       98 |  9662 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9663 |  |
|        - |  9664 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9665 | `	sxi32 rc;` |
|      100 |  9666 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9667 | `		return SXERR_INVALID;` |
|        - |  9668 | `	}` |
|        - |  9669 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9670 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9671 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9672 | `	if( pResumeValue ){` |
|       40 |  9673 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9674 | `	}else{` |
|       62 |  9675 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9676 | `	}` |
|      100 |  9677 | `	pCtx->nTos++;` |
|        - |  9678 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9679 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9680 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9681 | `	/* Save and set the active context */` |
|      100 |  9682 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9683 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9684 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9685 | `	pVm->nRecursionDepth++;` |
|        - |  9686 | `	/* Resume execution from saved PC */` |
|      100 |  9687 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9688 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9689 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9690 | `	pVm->nRecursionDepth--;` |
|        - |  9691 | `	/* Restore the previous context */` |
|      100 |  9692 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9693 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9694 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9695 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9696 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9697 | `		if( pResult ){` |
|       18 |  9698 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9699 | `		}` |
|       64 |  9700 | `		return SXRET_OK;` |
|        - |  9701 | `	}` |
|        - |  9702 | `	/* Detach frame */` |
|       38 |  9703 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9704 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9705 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9706 | `	}` |
|       38 |  9707 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9708 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9709 | `		return PH7_ABORT;` |
|        - |  9710 | `	}` |
|       38 |  9711 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9712 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9713 | `		return PH7_EXCEPTION;` |
|        - |  9714 | `	}` |
|        - |  9715 | `	/* Normal completion */` |
|       38 |  9716 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9717 | `	if( pResult ){` |
|       20 |  9718 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9719 | `	}` |
|       38 |  9720 | `	return SXRET_OK;` |
|       51 |  9721 |  |
|        - |  9722 | `/*` |
|        - |  9723 | ` * Release an execution context and all its resources.` |
|        - |  9724 | ` */` |
|        4 |  9725 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9726 |  |
|        5 |  9727 | `	if( pCtx == 0 ){` |
|      ! 0 |  9728 | `		return;` |
|        - |  9729 | `	}` |
|        5 |  9730 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9731 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9732 | `		return;` |
|        - |  9733 | `	}` |
|        5 |  9734 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9735 | `	/* Release values */` |
|        5 |  9736 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9737 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9738 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9739 | `	if( pCtx->pFrame ){` |
|        - |  9740 | `		VmSlot *aSlot;` |
|        - |  9741 | `		sxu32 n;` |
|        - |  9742 | `		/* Free local variables */` |
|        5 |  9743 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9744 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9745 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9746 | `		}` |
|        - |  9747 | `		/* Remove local references */` |
|        5 |  9748 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9749 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9750 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9751 | `		}` |
|        5 |  9752 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9753 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9754 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9755 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9756 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9757 | `		pCtx->pFrame = 0;` |
|        2 |  9758 | `	}` |
|        - |  9759 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9760 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9761 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9762 | `	if( pCtx->pStack ){` |
|        5 |  9763 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9764 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9765 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9766 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9767 | `				pTos--;` |
|        1 |  9768 | `			}` |
|        2 |  9769 | `		}` |
|        5 |  9770 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9771 | `		pCtx->pStack = 0;` |
|        2 |  9772 | `	}` |
|        - |  9773 | `	/* Free the context itself */` |
|        5 |  9774 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9775 |  |
|        - |  9776 | `/*` |
|        - |  9777 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9778 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9779 | ` */` |
|       90 |  9780 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9781 |  |
|        - |  9782 | `	ph7_class_instance *pThis;` |
|        - |  9783 | `	SyString sAttr;` |
|        - |  9784 | `	ph7_value *pAttr;` |
|       92 |  9785 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9786 | `		return 0;` |
|        - |  9787 | `	}` |
|       92 |  9788 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9789 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9790 | `		return 0;` |
|        - |  9791 | `	}` |
|       92 |  9792 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9793 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9794 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9795 | `		return 0;` |
|        - |  9796 | `	}` |
|       62 |  9797 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9798 |  |
|        - |  9799 | `/*` |
|        - |  9800 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9801 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9802 | ` */` |
|       38 |  9803 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9804 |  |
|       40 |  9805 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9806 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9807 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9808 | `			"Cannot suspend outside of a fiber");` |
|        - |  9809 | `	}` |
|       40 |  9810 | `	if( nArg > 0 ){` |
|       40 |  9811 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9812 | `	}else{` |
|      ! 0 |  9813 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9814 | `	}` |
|       40 |  9815 | `	return PH7_SUSPEND;` |
|       21 |  9816 |  |
|        - |  9817 | `/*` |
|        - |  9818 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9819 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9820 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9821 | ` */` |
|       24 |  9822 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9823 |  |
|        - |  9824 | `	ph7_class_instance *pThis;` |
|        - |  9825 | `	ph7_value *pAttr;` |
|        - |  9826 | `	SyString sAttrName;` |
|       26 |  9827 | `	if( nArg < 2 ){` |
|      ! 0 |  9828 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9829 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9830 | `	}` |
|       26 |  9831 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9832 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9833 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9834 | `	}` |
|       26 |  9835 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9836 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9837 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9838 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9839 | `	}` |
|        - |  9840 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9841 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9842 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9843 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9844 | `	}` |
|        - |  9845 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9846 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9847 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9848 | `	if( pAttr ){` |
|       26 |  9849 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9850 | `	}` |
|       26 |  9851 | `	return PH7_OK;` |
|       14 |  9852 |  |
|        - |  9853 | `/*` |
|        - |  9854 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9855 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9856 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9857 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9858 | ` */` |
|       24 |  9859 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9860 | `	ph7_class_instance **ppThis)` |
|        2 |  9861 |  |
|       26 |  9862 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9863 | `	ph7_value *pCallable;` |
|        - |  9864 | `	SyString sAttrName;` |
|       26 |  9865 | `	*ppThis = 0;` |
|       26 |  9866 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9867 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9868 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9869 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9870 | `		return 0;` |
|        - |  9871 | `	}` |
|       26 |  9872 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9873 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9874 | `		SyString sName;` |
|        - |  9875 | `		SyHashEntry *pEntry;` |
|        - |  9876 | `		ph7_vm_func *pFunc;` |
|       26 |  9877 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9878 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9879 | `		if( pEntry == 0 ){` |
|      ! 0 |  9880 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9881 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9882 | `			return 0;` |
|        - |  9883 | `		}` |
|       26 |  9884 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9885 | `		return pFunc;` |
|      ! 0 |  9886 | `	}else{` |
|        - |  9887 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9888 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9889 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9890 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9891 | `		if( pMethod == 0 ){` |
|      ! 0 |  9892 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9893 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9894 | `			return 0;` |
|        - |  9895 | `		}` |
|      ! 0 |  9896 | `		*ppThis = pClosure;` |
|      ! 0 |  9897 | `		return &pMethod->sFunc;` |
|        - |  9898 | `	}` |
|       14 |  9899 |  |
|        - |  9900 | `/*` |
|        - |  9901 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9902 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9903 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9904 | ` */` |
|       46 |  9905 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9906 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9907 |  |
|       48 |  9908 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9909 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9910 | `	sxu32 nFormal, n;` |
|        - |  9911 | `	VmSlot sSlot;` |
|        - |  9912 | `	sxi32 rc;` |
|        - |  9913 | `	/* Install $this for closure/method callables */` |
|       48 |  9914 | `	if( pClosureThis ){` |
|        - |  9915 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9916 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9917 | `		if( pObj ){` |
|      ! 0 |  9918 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9919 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9920 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9921 | `		}` |
|      ! 0 |  9922 | `	}` |
|        - |  9923 | `	/* Install static variables */` |
|       48 |  9924 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9925 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9926 | `		ph7_value *pVal;` |
|      ! 0 |  9927 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9928 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9929 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9930 | `			if( pVal ){` |
|      ! 0 |  9931 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9932 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9933 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9934 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9935 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9936 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9937 | `				}` |
|      ! 0 |  9938 | `			}` |
|      ! 0 |  9939 | `		}` |
|      ! 0 |  9940 | `	}` |
|        - |  9941 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9942 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9943 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9944 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9945 | `		ph7_value *pObj;` |
|       20 |  9946 | `		if( n < (sxu32)nArg ){` |
|        - |  9947 | `			/* Argument provided — install with type casting */` |
|       20 |  9948 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9949 | `			if( pObj ){` |
|       20 |  9950 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9951 | `				/* Type casting */` |
|       20 |  9952 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9953 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9954 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9955 | `						if( xCast ){` |
|      ! 0 |  9956 | `							xCast(pObj);` |
|      ! 0 |  9957 | `						}` |
|      ! 0 |  9958 | `					}` |
|      ! 0 |  9959 | `				}` |
|       20 |  9960 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9961 | `				sSlot.pUserData = 0;` |
|       20 |  9962 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9963 | `			}` |
|        9 |  9964 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9965 | `			/* Default value */` |
|      ! 0 |  9966 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9967 | `			if( pObj ){` |
|      ! 0 |  9968 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9969 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9970 | `					return rc;` |
|        - |  9971 | `				}` |
|      ! 0 |  9972 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9973 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9974 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9975 | `						if( xCast ){` |
|      ! 0 |  9976 | `							xCast(pObj);` |
|      ! 0 |  9977 | `						}` |
|      ! 0 |  9978 | `					}` |
|      ! 0 |  9979 | `				}` |
|      ! 0 |  9980 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9981 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9982 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9983 | `			}` |
|      ! 0 |  9984 | `		}` |
|       11 |  9985 | `	}` |
|        - |  9986 | `	/* Install closure environment (captured variables) */` |
|       48 |  9987 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9988 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9989 | `		ph7_value *pValue;` |
|        - |  9990 | `		sxu32 iEnv;` |
|        3 |  9991 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9992 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9993 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9994 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9995 | `				continue;` |
|        - |  9996 | `			}` |
|        5 |  9997 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9998 | `			if( pValue == 0 ){` |
|      ! 0 |  9999 | `				continue;` |
|        - | 10000 | `			}` |
|        5 | 10001 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10002 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10003 | `		}` |
|        1 | 10004 | `	}` |
|       48 | 10005 | `	return SXRET_OK;` |
|       25 | 10006 |  |
|        - | 10007 | `/*` |
|        - | 10008 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10009 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10010 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10011 | ` */` |
|       26 | 10012 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10013 |  |
|       28 | 10014 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10015 | `	ph7_class_instance *pThis;` |
|        - | 10016 | `	ph7_class_instance *pClosureThis;` |
|        - | 10017 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10018 | `	ph7_vm_func *pFunc;` |
|        - | 10019 | `	ph7_value sResult;` |
|        - | 10020 | `	ph7_value *pCtxAttr;` |
|        - | 10021 | `	SyString sAttrName;` |
|        - | 10022 | `	sxi32 rc;` |
|       28 | 10023 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10024 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10025 | `	}` |
|       28 | 10026 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10027 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10028 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10029 | `	if( pExecCtx != 0 ){` |
|        3 | 10030 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10031 | `			"Cannot start a fiber that has already been started");` |
|        - | 10032 | `	}` |
|        - | 10033 | `	/* Resolve callable */` |
|       26 | 10034 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10035 | `	if( pFunc == 0 ){` |
|      ! 0 | 10036 | `		return PH7_EXCEPTION;` |
|        - | 10037 | `	}` |
|        - | 10038 | `	/* Create execution context now that we know the function */` |
|       26 | 10039 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10040 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10041 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10042 | `			"Fiber::start(): out of memory");` |
|        - | 10043 | `	}` |
|        - | 10044 | `	/* Store context in $this->__ctx */` |
|       26 | 10045 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10046 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10047 | `	if( pCtxAttr ){` |
|       26 | 10048 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10049 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10050 | `	}` |
|        - | 10051 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10052 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10053 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10054 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10055 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10056 | `	/* Unpack the args array and install into the frame */` |
|        - | 10057 | `	{` |
|       26 | 10058 | `		ph7_value **apValues = 0;` |
|       26 | 10059 | `		int nActual = 0;` |
|       26 | 10060 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10061 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10062 | `			ph7_hashmap_node *pNode;` |
|       26 | 10063 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10064 | `			if( nCount > 0 ){` |
|        3 | 10065 | `				sxu32 idx = 0;` |
|        4 | 10066 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10067 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10068 | `				if( apValues ){` |
|        3 | 10069 | `					pNode = pMap->pFirst;` |
|        7 | 10070 | `					while( pNode && idx < nCount ){` |
|        5 | 10071 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10072 | `						idx++;` |
|        5 | 10073 | `						pNode = pNode->pPrev;` |
|        1 | 10074 | `					}` |
|        3 | 10075 | `					nActual = (int)idx;` |
|        1 | 10076 | `				}` |
|        1 | 10077 | `			}` |
|       12 | 10078 | `		}` |
|       26 | 10079 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10080 | `		if( apValues ){` |
|        3 | 10081 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10082 | `		}` |
|        - | 10083 | `	}` |
|        - | 10084 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10085 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10086 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10087 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10088 | `		return PH7_ABORT;` |
|        - | 10089 | `	}` |
|       26 | 10090 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10091 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10092 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10093 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10094 | `		return PH7_ABORT;` |
|        - | 10095 | `	}` |
|       26 | 10096 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10097 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10098 | `		return PH7_EXCEPTION;` |
|        - | 10099 | `	}` |
|       26 | 10100 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10101 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10102 | `	return PH7_OK;` |
|       15 | 10103 |  |
|        - | 10104 | `/*` |
|        - | 10105 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10106 | ` */` |
|       36 | 10107 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10108 |  |
|       38 | 10109 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10110 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10111 | `	ph7_value sResult;` |
|        - | 10112 | `	ph7_value *pResumeVal;` |
|        - | 10113 | `	sxi32 rc;` |
|       38 | 10114 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10115 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10116 | `		return PH7_OK;` |
|        - | 10117 | `	}` |
|       38 | 10118 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10119 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10120 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10121 | `		return PH7_OK;` |
|        - | 10122 | `	}` |
|       38 | 10123 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10124 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10125 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10126 | `	}` |
|       36 | 10127 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10128 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10129 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10130 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10131 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10132 | `		return PH7_ABORT;` |
|        - | 10133 | `	}` |
|       36 | 10134 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10135 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10136 | `		return PH7_EXCEPTION;` |
|        - | 10137 | `	}` |
|       36 | 10138 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10139 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10140 | `	return PH7_OK;` |
|       20 | 10141 |  |
|        - | 10142 | `/*` |
|        - | 10143 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10144 | ` */` |
|        6 | 10145 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10146 |  |
|        8 | 10147 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10148 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10149 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10150 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10151 | `		return PH7_OK;` |
|        - | 10152 | `	}` |
|        8 | 10153 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10154 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10155 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10156 | `		return PH7_OK;` |
|        - | 10157 | `	}` |
|        8 | 10158 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10159 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10160 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10161 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10162 | `		}` |
|      ! 0 | 10163 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10164 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10165 | `	}` |
|        8 | 10166 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10167 | `	return PH7_OK;` |
|        5 | 10168 |  |
|        - | 10169 | `/*` |
|        - | 10170 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10171 | ` */` |
|        6 | 10172 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10173 |  |
|        - | 10174 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10175 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10176 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10177 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10178 | `	return PH7_OK;` |
|        4 | 10179 |  |
|      ! 0 | 10180 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10181 |  |
|        - | 10182 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10183 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10184 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10185 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10186 | `	return PH7_OK;` |
|      ! 0 | 10187 |  |
|        6 | 10188 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10189 |  |
|        - | 10190 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10191 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10192 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10193 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10194 | `	return PH7_OK;` |
|        4 | 10195 |  |
|        6 | 10196 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10197 |  |
|        - | 10198 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10199 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10200 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10201 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10202 | `	return PH7_OK;` |
|        4 | 10203 |  |
|        - | 10204 | `/*` |
|        - | 10205 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10206 | ` */` |
|        4 | 10207 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10208 |  |
|        5 | 10209 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10210 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10211 | `	if( nArg < 1 ){` |
|      ! 0 | 10212 | `		return PH7_OK;` |
|        - | 10213 | `	}` |
|        5 | 10214 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10215 | `	if( pExecCtx ){` |
|        5 | 10216 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10217 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10218 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10219 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10220 | `			SyString sAttrName;` |
|        - | 10221 | `			ph7_value *pAttr;` |
|        5 | 10222 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10223 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10224 | `			if( pAttr ){` |
|        5 | 10225 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10226 | `			}` |
|        2 | 10227 | `		}` |
|        2 | 10228 | `	}` |
|        5 | 10229 | `	return PH7_OK;` |
|        3 | 10230 |  |
|        - | 10231 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10232 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10233 |  |
|        - | 10234 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10235 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10236 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10237 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10238 |  |
|      ! 0 | 10239 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10240 |  |
|        - | 10241 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10242 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10243 | `	ph7_exec_ctx *pCtx;` |
|        - | 10244 | `	ph7_vm_func *pFunc;` |
|        - | 10245 | `	ph7_value *pCallable;` |
|        - | 10246 | `	ph7_value *pCtxAttr;` |
|        - | 10247 | `	SyString sAttrName;` |
|        - | 10248 | `	/* Must not already be started */` |
|      ! 0 | 10249 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10250 | `	if( pCtx != 0 ){` |
|      ! 0 | 10251 | `		return SXERR_INVALID;` |
|        - | 10252 | `	}` |
|      ! 0 | 10253 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10254 | `		return SXERR_INVALID;` |
|        - | 10255 | `	}` |
|      ! 0 | 10256 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10257 | `	/* Get the callable */` |
|      ! 0 | 10258 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10259 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10260 | `	if( pCallable == 0 ){` |
|      ! 0 | 10261 | `		return SXERR_INVALID;` |
|        - | 10262 | `	}` |
|        - | 10263 | `	/* Resolve callable */` |
|      ! 0 | 10264 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10265 | `		SyString sName;` |
|        - | 10266 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10267 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10268 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10269 | `		if( pEntry == 0 ){` |
|      ! 0 | 10270 | `			return SXERR_NOTFOUND;` |
|        - | 10271 | `		}` |
|      ! 0 | 10272 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10273 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10274 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10275 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10276 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10277 | `		if( pMethod == 0 ){` |
|      ! 0 | 10278 | `			return SXERR_INVALID;` |
|        - | 10279 | `		}` |
|      ! 0 | 10280 | `		pClosureThis = pClosure;` |
|      ! 0 | 10281 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10282 | `	}else{` |
|      ! 0 | 10283 | `		return SXERR_INVALID;` |
|        - | 10284 | `	}` |
|        - | 10285 | `	/* Create context */` |
|      ! 0 | 10286 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10287 | `	if( pCtx == 0 ){` |
|      ! 0 | 10288 | `		return SXERR_MEM;` |
|        - | 10289 | `	}` |
|        - | 10290 | `	/* Store in __ctx */` |
|      ! 0 | 10291 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10292 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10293 | `	if( pCtxAttr ){` |
|      ! 0 | 10294 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10295 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10296 | `	}` |
|        - | 10297 | `	/* Set up frame with args */` |
|      ! 0 | 10298 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10299 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10300 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10301 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10302 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10303 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10304 |  |
|      ! 0 | 10305 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10306 |  |
|      ! 0 | 10307 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10308 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10309 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10310 |  |
|      ! 0 | 10311 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10312 |  |
|      ! 0 | 10313 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10314 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10315 |  |
|      ! 0 | 10316 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10317 |  |
|      ! 0 | 10318 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10319 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10320 |  |
|      ! 0 | 10321 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10322 |  |
|      ! 0 | 10323 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10324 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10325 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10326 |  |
|        - | 10327 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10328 | `/*` |
|        - | 10329 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10330 | ` */` |
|       22 | 10331 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10332 |  |
|        - | 10333 | `	ph7_generator *pGen;` |
|       24 | 10334 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10335 | `	if( pGen == 0 ){` |
|      ! 0 | 10336 | `		return 0;` |
|        - | 10337 | `	}` |
|       24 | 10338 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10339 | `	pGen->pCtx = pCtx;` |
|       24 | 10340 | `	pGen->iImplicitKey = 0;` |
|       24 | 10341 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10342 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10343 | `	/* Link the generator back to the exec context */` |
|       24 | 10344 | `	pCtx->pPrivate = pGen;` |
|       24 | 10345 | `	return pGen;` |
|       13 | 10346 |  |
|        - | 10347 | `/*` |
|        - | 10348 | ` * Release a generator and its execution context.` |
|        - | 10349 | ` */` |
|      ! 0 | 10350 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10351 |  |
|      ! 0 | 10352 | `	if( pGen == 0 ){` |
|      ! 0 | 10353 | `		return;` |
|        - | 10354 | `	}` |
|      ! 0 | 10355 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10356 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10357 | `	if( pGen->pCtx ){` |
|      ! 0 | 10358 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10359 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10360 | `		pGen->pCtx = 0;` |
|      ! 0 | 10361 | `	}` |
|      ! 0 | 10362 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10363 |  |
|        - | 10364 | `/*` |
|        - | 10365 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10366 | ` */` |
|      236 | 10367 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10368 |  |
|        - | 10369 | `	ph7_class_instance *pThis;` |
|        - | 10370 | `	SyString sAttr;` |
|        - | 10371 | `	ph7_value *pAttr;` |
|      238 | 10372 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10373 | `		return 0;` |
|        - | 10374 | `	}` |
|      238 | 10375 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10376 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10377 | `		return 0;` |
|        - | 10378 | `	}` |
|      238 | 10379 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10380 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10381 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10382 | `		return 0;` |
|        - | 10383 | `	}` |
|      238 | 10384 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10385 |  |
|        - | 10386 | `/*` |
|        - | 10387 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10388 | ` */` |
|       22 | 10389 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10390 |  |
|        - | 10391 | `	ph7_generator *pGen;` |
|        - | 10392 | `	sxi32 rc;` |
|       24 | 10393 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10394 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10395 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10396 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10397 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10398 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10399 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10400 | `	}` |
|       24 | 10401 | `	return PH7_OK;` |
|       13 | 10402 |  |
|        - | 10403 | `/*` |
|        - | 10404 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10405 | ` */` |
|       68 | 10406 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10407 |  |
|        - | 10408 | `	ph7_generator *pGen;` |
|       70 | 10409 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10410 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10411 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10412 | `	return PH7_OK;` |
|       36 | 10413 |  |
|        - | 10414 | `/*` |
|        - | 10415 | ` * Generator::current() — return the last yielded value.` |
|        - | 10416 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10417 | ` */` |
|       68 | 10418 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10419 |  |
|        - | 10420 | `	ph7_generator *pGen;` |
|        - | 10421 | `	sxi32 rc;` |
|       70 | 10422 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10423 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10424 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10425 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10426 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10427 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10428 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10429 | `	}` |
|       70 | 10430 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10431 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10432 | `	}else{` |
|      ! 0 | 10433 | `		ph7_result_null(pCtx);` |
|        - | 10434 | `	}` |
|       70 | 10435 | `	return PH7_OK;` |
|       36 | 10436 |  |
|        - | 10437 | `/*` |
|        - | 10438 | ` * Generator::key() — return the last yielded key.` |
|        - | 10439 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10440 | ` */` |
|       12 | 10441 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10442 |  |
|        - | 10443 | `	ph7_generator *pGen;` |
|        - | 10444 | `	sxi32 rc;` |
|       13 | 10445 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10446 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10447 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10448 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10449 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10450 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10451 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10452 | `	}` |
|       13 | 10453 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10454 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10455 | `	}else{` |
|      ! 0 | 10456 | `		ph7_result_null(pCtx);` |
|        - | 10457 | `	}` |
|       13 | 10458 | `	return PH7_OK;` |
|        7 | 10459 |  |
|        - | 10460 | `/*` |
|        - | 10461 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10462 | ` */` |
|       60 | 10463 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10464 |  |
|        - | 10465 | `	ph7_generator *pGen;` |
|        - | 10466 | `	sxi32 rc;` |
|       62 | 10467 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10468 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10469 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10470 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10471 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10472 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10473 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10474 | `	}else{` |
|      ! 0 | 10475 | `		return PH7_OK;` |
|        - | 10476 | `	}` |
|       62 | 10477 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10478 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10479 | `	return PH7_OK;` |
|       32 | 10480 |  |
|        - | 10481 | `/*` |
|        - | 10482 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10483 | ` */` |
|        4 | 10484 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10485 |  |
|        - | 10486 | `	ph7_generator *pGen;` |
|        - | 10487 | `	ph7_value *pSendVal;` |
|        - | 10488 | `	sxi32 rc;` |
|        5 | 10489 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10490 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10491 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10492 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10493 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10494 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10495 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10496 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10497 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10498 | `	}else{` |
|      ! 0 | 10499 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10500 | `		return PH7_OK;` |
|        - | 10501 | `	}` |
|        5 | 10502 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10503 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10504 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10505 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10506 | `	}else{` |
|        3 | 10507 | `		ph7_result_null(pCtx);` |
|        - | 10508 | `	}` |
|        5 | 10509 | `	return PH7_OK;` |
|        3 | 10510 |  |
|        - | 10511 | `/*` |
|        - | 10512 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10513 | ` *` |
|        - | 10514 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10515 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10516 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10517 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10518 | ` * the exception to the caller.` |
|        - | 10519 | ` */` |
|      ! 0 | 10520 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10521 |  |
|        - | 10522 | `	ph7_generator *pGen;` |
|        - | 10523 | `	const char *zMsg;` |
|        - | 10524 | `	int nLen;` |
|      ! 0 | 10525 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10526 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10527 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10528 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10529 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10530 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10531 | `			"Cannot throw into a closed generator");` |
|        - | 10532 | `	}` |
|        - | 10533 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10534 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10535 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10536 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10537 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10538 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10539 | `	nLen = 0;` |
|      ! 0 | 10540 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10541 | `		/* Try to get the exception's message */` |
|        - | 10542 | `		SyString sAttr;` |
|        - | 10543 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10544 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10545 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10546 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10547 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10548 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10549 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10550 | `		}` |
|      ! 0 | 10551 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10552 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10553 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10554 | `	}` |
|      ! 0 | 10555 | `	(void)nLen;` |
|      ! 0 | 10556 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10557 |  |
|        - | 10558 | `/*` |
|        - | 10559 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10560 | ` */` |
|        2 | 10561 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10562 |  |
|        - | 10563 | `	ph7_generator *pGen;` |
|        3 | 10564 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10565 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10566 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10567 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10568 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10569 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10570 | `	}` |
|        3 | 10571 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10572 | `	return PH7_OK;` |
|        2 | 10573 |  |
|        - | 10574 | `/*` |
|        - | 10575 | ` * Generator::__destruct() — clean up.` |
|        - | 10576 | ` */` |
|      ! 0 | 10577 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10578 |  |
|        - | 10579 | `	ph7_generator *pGen;` |
|      ! 0 | 10580 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10581 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10582 | `	if( pGen ){` |
|      ! 0 | 10583 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10584 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10585 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10586 | `			SyString sAttrName;` |
|        - | 10587 | `			ph7_value *pAttr;` |
|      ! 0 | 10588 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10589 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10590 | `			if( pAttr ){` |
|      ! 0 | 10591 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10592 | `			}` |
|      ! 0 | 10593 | `		}` |
|      ! 0 | 10594 | `	}` |
|      ! 0 | 10595 | `	return PH7_OK;` |
|      ! 0 | 10596 |  |
|        - | 10597 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10598 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10599 | `/*` |
|        - | 10600 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10601 | ` * the desired message.` |
|        - | 10602 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10603 | ` * in 'api.c' for additional information.` |
|        - | 10604 | ` */` |
|      370 | 10605 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10606 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10607 | `	SyString *pString /* Message to output */` |
|        - | 10608 | `	)` |
|        2 | 10609 |  |
|      372 | 10610 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10611 | `	sxi32 rc = SXRET_OK;` |
|        - | 10612 | `	/* Call the output consumer */` |
|      372 | 10613 | `	if( pString->nByte > 0 ){` |
|      372 | 10614 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10615 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10616 | `	}` |
|      372 | 10617 | `	return rc;` |
|        2 | 10618 |  |
|        - | 10619 | `/*` |
|        - | 10620 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10621 | ` * callback to consume the formatted message.` |
|        - | 10622 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10623 | ` * in 'api.c' for additional information.` |
|        - | 10624 | ` */` |
|        2 | 10625 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10626 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10627 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10628 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10629 | `	)` |
|        1 | 10630 |  |
|        3 | 10631 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10632 | `	sxi32 rc = SXRET_OK;` |
|        - | 10633 | `	SyBlob sWorker;` |
|        - | 10634 | `	/* Format the message and call the output consumer */` |
|        3 | 10635 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10636 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10637 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10638 | `		/* Consume the formatted message */` |
|        3 | 10639 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10640 | `	}` |
|        3 | 10641 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10642 | `	/* Release the working buffer */` |
|        3 | 10643 | `	SyBlobRelease(&sWorker);` |
|        3 | 10644 | `	return rc;` |
|        1 | 10645 |  |
|        - | 10646 | `/*` |
|        - | 10647 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10648 | ` * This function never fail and always return a pointer` |
|        - | 10649 | ` * to a null terminated string.` |
|        - | 10650 | ` */` |
|       12 | 10651 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10652 |  |
|       13 | 10653 | `	const char *zOp = "Unknown     ";` |
|       13 | 10654 | `	switch(nOp){` |
|        3 | 10655 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10656 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10657 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10658 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10659 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10660 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10661 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10662 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10663 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10664 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10665 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10666 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10667 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10668 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10669 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10670 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10671 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10672 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10673 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10674 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10675 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10676 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10677 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10678 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10679 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10680 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10681 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10682 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10683 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10684 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10685 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10686 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10687 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10688 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10689 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10690 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10691 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10692 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10693 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10694 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10695 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10696 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10697 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10698 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10699 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10700 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10701 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10702 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10703 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10704 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10705 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10706 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10707 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10708 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10709 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10710 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10711 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10712 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10713 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10714 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10715 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10716 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10717 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10718 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10719 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10720 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10721 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10722 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10723 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10724 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10725 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10726 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10727 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10728 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10729 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10730 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10731 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10732 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10733 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10734 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10735 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10736 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10737 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10738 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10739 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10740 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10741 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10742 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10743 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10744 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10745 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10746 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10747 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10748 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10749 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10750 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10751 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10752 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10753 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10754 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10755 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10756 | `	default:` |
|      ! 0 | 10757 | `		break;` |
|        - | 10758 | `	}` |
|       13 | 10759 | `	return zOp;` |
|        1 | 10760 |  |
|        - | 10761 | `/*` |
|        - | 10762 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10763 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10764 | ` * is responsible of consuming the generated dump.` |
|        - | 10765 | ` */` |
|        2 | 10766 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10767 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10768 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10769 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10770 | `	)` |
|        1 | 10771 |  |
|        - | 10772 | `	sxi32 rc;` |
|        3 | 10773 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10774 | `	return rc;` |
|        1 | 10775 |  |
|        - | 10776 | `/*` |
|        - | 10777 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10778 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10779 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10780 | ` * in 'compile.c' for additional information.` |
|        - | 10781 | ` */` |
|       14 | 10782 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10783 |  |
|       15 | 10784 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10785 | `	/* Evaluate and expand constant value */` |
|       15 | 10786 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10787 |  |
|        - | 10788 | `/*` |
|        - | 10789 | ` * Section:` |
|        - | 10790 | ` *  Function handling functions.` |
|        - | 10791 | ` * Status:` |
|        - | 10792 | ` *    Stable.` |
|        - | 10793 | ` */` |
|        - | 10794 | `/*` |
|        - | 10795 | ` * int func_num_args(void)` |
|        - | 10796 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10797 | ` * Parameters` |
|        - | 10798 | ` *   None.` |
|        - | 10799 | ` * Return` |
|        - | 10800 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10801 | ` *  or -1 if called from the globe scope.` |
|        - | 10802 | ` */` |
|      960 | 10803 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10804 |  |
|        - | 10805 | `	VmFrame *pFrame;` |
|        - | 10806 | `	ph7_vm *pVm;` |
|        - | 10807 | `	/* Point to the target VM */` |
|      962 | 10808 | `	pVm = pCtx->pVm;` |
|        - | 10809 | `	/* Current frame */` |
|      962 | 10810 | `	pFrame = pVm->pFrame;` |
|      962 | 10811 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      962 | 10812 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10813 | `		SXUNUSED(nArg);` |
|      ! 0 | 10814 | `		SXUNUSED(apArg);` |
|        - | 10815 | `		/* Global frame,return -1 */` |
|      ! 0 | 10816 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10817 | `		return SXRET_OK;` |
|        - | 10818 | `	}` |
|        - | 10819 | `	/* Total number of arguments passed to the enclosing function */` |
|      962 | 10820 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      962 | 10821 | `	ph7_result_int(pCtx,nArg);` |
|      962 | 10822 | `	return SXRET_OK;` |
|      482 | 10823 |  |
|        - | 10824 | `/*` |
|        - | 10825 | ` * value func_get_arg(int $arg_num)` |
|        - | 10826 | ` *   Return an item from the argument list.` |
|        - | 10827 | ` * Parameters` |
|        - | 10828 | ` *  Argument number(index start from zero).` |
|        - | 10829 | ` * Return` |
|        - | 10830 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10831 | ` */` |
|       22 | 10832 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10833 |  |
|       24 | 10834 | `	ph7_value *pObj = 0;` |
|       24 | 10835 | `	VmSlot *pSlot = 0;` |
|        - | 10836 | `	VmFrame *pFrame;` |
|        - | 10837 | `	ph7_vm *pVm;` |
|        - | 10838 | `	/* Point to the target VM */` |
|       24 | 10839 | `	pVm = pCtx->pVm;` |
|        - | 10840 | `	/* Current frame */` |
|       24 | 10841 | `	pFrame = pVm->pFrame;` |
|       24 | 10842 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10843 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10844 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10845 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10846 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10847 | `		return SXRET_OK;` |
|        - | 10848 | `	}` |
|        - | 10849 | `	/* Extract the desired index */` |
|       21 | 10850 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10851 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10852 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10853 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10854 | `		return SXRET_OK;` |
|        - | 10855 | `	}` |
|        - | 10856 | `	/* Extract the desired argument */` |
|       21 | 10857 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10858 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10859 | `			/* Return the desired argument */` |
|       21 | 10860 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10861 | `		}else{` |
|        - | 10862 | `			/* No such argument,return false */` |
|      ! 0 | 10863 | `			ph7_result_bool(pCtx,0);` |
|        - | 10864 | `		}` |
|       11 | 10865 | `	}else{` |
|        - | 10866 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10867 | `		ph7_result_bool(pCtx,0);` |
|        - | 10868 | `	}` |
|       21 | 10869 | `	return SXRET_OK;` |
|       13 | 10870 |  |
|        - | 10871 | `/*` |
|        - | 10872 | ` * array func_get_args_byref(void)` |
|        - | 10873 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10874 | ` * Parameters` |
|        - | 10875 | ` *  None.` |
|        - | 10876 | ` * Return` |
|        - | 10877 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10878 | ` *  member of the current user-defined function's argument list.` |
|        - | 10879 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10880 | ` * NOTE:` |
|        - | 10881 | ` *  Arguments are returned to the array by reference.` |
|        - | 10882 | ` */` |
|        2 | 10883 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10884 |  |
|        - | 10885 | `	ph7_value *pArray;` |
|        - | 10886 | `	VmFrame *pFrame;` |
|        - | 10887 | `	VmSlot *aSlot;` |
|        - | 10888 | `	sxu32 n;` |
|        - | 10889 | `	/* Point to the current frame */` |
|        3 | 10890 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10891 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10892 | `	if( pFrame->pParent == 0 ){` |
|        - | 10893 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10894 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10895 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10896 | `		return SXRET_OK;` |
|        - | 10897 | `	}` |
|        - | 10898 | `	/* Create a new array */` |
|        3 | 10899 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10900 | `	if( pArray == 0 ){` |
|      ! 0 | 10901 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10902 | `		SXUNUSED(apArg);` |
|      ! 0 | 10903 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10904 | `		return SXRET_OK;` |
|        - | 10905 | `	}` |
|        - | 10906 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10907 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10908 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10909 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10910 | `	}` |
|        - | 10911 | `	/* Return the freshly created array */` |
|        3 | 10912 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10913 | `	return SXRET_OK;` |
|        2 | 10914 |  |
|        - | 10915 | `/*` |
|        - | 10916 | ` * array func_get_args(void)` |
|        - | 10917 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10918 | ` * Parameters` |
|        - | 10919 | ` *  None.` |
|        - | 10920 | ` * Return` |
|        - | 10921 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10922 | ` *  member of the current user-defined function's argument list.` |
|        - | 10923 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10924 | ` */` |
|       88 | 10925 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10926 |  |
|       90 | 10927 | `	ph7_value *pObj = 0;` |
|        - | 10928 | `	ph7_value *pArray;` |
|        - | 10929 | `	VmFrame *pFrame;` |
|        - | 10930 | `	VmSlot *aSlot;` |
|        - | 10931 | `	sxu32 n;` |
|        - | 10932 | `	/* Point to the current frame */` |
|       90 | 10933 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10934 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10935 | `	if( pFrame->pParent == 0 ){` |
|        - | 10936 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10937 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10938 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10939 | `		return SXRET_OK;` |
|        - | 10940 | `	}` |
|        - | 10941 | `	/* Create a new array */` |
|       90 | 10942 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10943 | `	if( pArray == 0 ){` |
|      ! 0 | 10944 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10945 | `		SXUNUSED(apArg);` |
|      ! 0 | 10946 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10947 | `		return SXRET_OK;` |
|        - | 10948 | `	}` |
|        - | 10949 | `	/* Start filling the array with the given arguments */` |
|       90 | 10950 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10951 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10952 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10953 | `		if( pObj ){` |
|      134 | 10954 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10955 | `		}` |
|       68 | 10956 | `	}` |
|        - | 10957 | `	/* Return the freshly created array */` |
|       90 | 10958 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10959 | `	return SXRET_OK;` |
|       46 | 10960 |  |
|        - | 10961 | `/*` |
|        - | 10962 | ` * bool function_exists(string $name)` |
|        - | 10963 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10964 | ` * Parameters` |
|        - | 10965 | ` *  The name of the desired function.` |
|        - | 10966 | ` * Return` |
|        - | 10967 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10968 | ` */` |
|     1714 | 10969 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10970 |  |
|        - | 10971 | `	const char *zName;` |
|        - | 10972 | `	ph7_vm *pVm;` |
|        - | 10973 | `	int nLen;` |
|        - | 10974 | `	int res;` |
|     1716 | 10975 | `	if( nArg < 1 ){` |
|        - | 10976 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10977 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10978 | `		return SXRET_OK;` |
|        - | 10979 | `	}` |
|        - | 10980 | `	/* Point to the target VM */` |
|     1716 | 10981 | `	pVm = pCtx->pVm;` |
|        - | 10982 | `	/* Extract the function name */` |
|     1716 | 10983 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10984 | `	/* Assume the function is not defined */` |
|     1716 | 10985 | `	res = 0;` |
|        - | 10986 | `	/* Perform the lookup */` |
|     2571 | 10987 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1710 | 10988 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10989 | `			/* Function is defined */` |
|      238 | 10990 | `			res = 1;` |
|      118 | 10991 | `	}` |
|     1716 | 10992 | `	ph7_result_bool(pCtx,res);` |
|     1716 | 10993 | `	return SXRET_OK;` |
|      859 | 10994 |  |
|        - | 10995 | `/*` |
|        - | 10996 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10997 | ` * [i.e: Whether it is callable or not].` |
|        - | 10998 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10999 | ` */` |
|    22252 | 11000 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11001 |  |
|    22254 | 11002 | `	int res = 0;` |
|    22254 | 11003 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11004 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11005 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11006 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11007 | `		 * standard PHP behavior. */` |
|       20 | 11008 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11009 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11010 | `			res = 1;` |
|       10 | 11011 | `		}` |
|        9 | 11012 | `		(void)CallInvoke;` |
|    22245 | 11013 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11014 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11015 | `		if( pMap->nEntry == 2 ){` |
|        - | 11016 | `			ph7_class *pClass;` |
|        - | 11017 | `			ph7_value *pV;` |
|        - | 11018 | `			/* Extract the target class */` |
|       12 | 11019 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11020 | `			if( pV ){` |
|       12 | 11021 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11022 | `				if( pClass ){` |
|        - | 11023 | `					ph7_class_method *pMethod;` |
|        - | 11024 | `					/* Extract the target method */` |
|       10 | 11025 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11026 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11027 | `						/* Perform the lookup */` |
|       10 | 11028 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11029 | `						if( pMethod ){` |
|        - | 11030 | `							/* Method is callable */` |
|        5 | 11031 | `							res = 1;` |
|        2 | 11032 | `						}` |
|        4 | 11033 | `					}` |
|        4 | 11034 | `				}` |
|        5 | 11035 | `			}` |
|        7 | 11036 | `		}` |
|    22223 | 11037 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11038 | `		const char *zName;` |
|        - | 11039 | `		int nLen;` |
|        - | 11040 | `		/* Extract the name */` |
|     5684 | 11041 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11042 | `		/* Perform the lookup */` |
|     5699 | 11043 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11044 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11045 | `				/* Function is callable */` |
|     5666 | 11046 | `				res = 1;` |
|     2832 | 11047 | `		}` |
|     2841 | 11048 | `	}` |
|    22254 | 11049 | `	return res;` |
|        2 | 11050 |  |
|        - | 11051 | `/*` |
|        - | 11052 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11053 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11054 | ` * Parameters` |
|        - | 11055 | ` * $name` |
|        - | 11056 | ` *    The callback function to check` |
|        - | 11057 | ` * $syntax_only` |
|        - | 11058 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11059 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11060 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11061 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11062 | ` *    a string.` |
|        - | 11063 | ` * Return` |
|        - | 11064 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11065 | ` */` |
|       20 | 11066 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11067 |  |
|        - | 11068 | `	ph7_vm *pVm;` |
|        - | 11069 | `	int res;` |
|       21 | 11070 | `	if( nArg < 1 ){` |
|        - | 11071 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11073 | `		return SXRET_OK;` |
|        - | 11074 | `	}` |
|        - | 11075 | `	/* Point to the target VM */` |
|       21 | 11076 | `	pVm = pCtx->pVm;` |
|        - | 11077 | `	/* Perform the requested operation */` |
|       21 | 11078 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11079 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11080 | `	return SXRET_OK;` |
|       11 | 11081 |  |
|        - | 11082 | `/*` |
|        - | 11083 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11084 | ` * defined below.` |
|        - | 11085 | ` */` |
|     1228 | 11086 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11087 |  |
|     1229 | 11088 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11089 | `	ph7_value sName;` |
|        - | 11090 | `	sxi32 rc;` |
|        - | 11091 | `	/* Prepare the function name for insertion */` |
|     1229 | 11092 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1229 | 11093 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11094 | `	/* Perform the insertion */` |
|     1229 | 11095 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1229 | 11096 | `	PH7_MemObjRelease(&sName);` |
|     1229 | 11097 | `	return rc;` |
|        1 | 11098 |  |
|        - | 11099 | `/*` |
|        - | 11100 | ` * array get_defined_functions(void)` |
|        - | 11101 | ` *  Returns an array of all defined functions.` |
|        - | 11102 | ` * Parameter` |
|        - | 11103 | ` *  None.` |
|        - | 11104 | ` * Return` |
|        - | 11105 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11106 | ` *  both built-in (internal) and user-defined.` |
|        - | 11107 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11108 | ` *  defined ones using $arr["user"].` |
|        - | 11109 | ` * Note:` |
|        - | 11110 | ` *  NULL is returned on failure.` |
|        - | 11111 | ` */` |
|        2 | 11112 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11113 |  |
|        - | 11114 | `	ph7_value *pArray,*pEntry;` |
|        - | 11115 | `	/* NOTE:` |
|        - | 11116 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11117 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11118 | `	 */` |
|        3 | 11119 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11120 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11121 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11122 | `		SXUNUSED(apArg);` |
|        - | 11123 | `		/* Return NULL */` |
|      ! 0 | 11124 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11125 | `		return SXRET_OK;` |
|        - | 11126 | `	}` |
|        3 | 11127 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11128 | `	if( pEntry == 0 ){` |
|        - | 11129 | `		/* Return NULL */` |
|      ! 0 | 11130 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11131 | `		return SXRET_OK;` |
|        - | 11132 | `	}` |
|        - | 11133 | `	/* Fill with the appropriate information */` |
|        3 | 11134 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11135 | `	/* Create the 'internal' index */` |
|        3 | 11136 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11137 | `	/* Create the user-func array */` |
|        3 | 11138 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11139 | `	if( pEntry == 0 ){` |
|        - | 11140 | `		/* Return NULL */` |
|      ! 0 | 11141 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11142 | `		return SXRET_OK;` |
|        - | 11143 | `	}` |
|        - | 11144 | `	/* Fill with the appropriate information */` |
|        3 | 11145 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11146 | `	/* Create the 'user' index */` |
|        3 | 11147 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11148 | `	/* Return the multi-dimensional array */` |
|        3 | 11149 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11150 | `	return SXRET_OK;` |
|        2 | 11151 |  |
|        - | 11152 | `/*` |
|        - | 11153 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11154 | ` *  Register a function for execution on shutdown.` |
|        - | 11155 | ` * Note` |
|        - | 11156 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11157 | ` *  be called in the same order as they were registered.` |
|        - | 11158 | ` * Parameters` |
|        - | 11159 | ` *  $callback` |
|        - | 11160 | ` *   The shutdown callback to register.` |
|        - | 11161 | ` * $param` |
|        - | 11162 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11163 | ` * Return` |
|        - | 11164 | ` *  Nothing.` |
|        - | 11165 | ` */` |
|        2 | 11166 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11167 |  |
|        - | 11168 | `	VmShutdownCB sEntry;` |
|        - | 11169 | `	int i,j;` |
|        3 | 11170 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11171 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11172 | `		return PH7_OK;` |
|        - | 11173 | `	}` |
|        - | 11174 | `	/* Zero the Entry */` |
|        3 | 11175 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11176 | `	/* Initialize fields */` |
|        3 | 11177 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11178 | `	/* Save the callback name for later invocation name */` |
|        3 | 11179 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 11180 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 11181 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 11182 | `	}` |
|        - | 11183 | `	/* Copy arguments */` |
|        3 | 11184 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11185 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11186 | `			/* Limit reached */` |
|      ! 0 | 11187 | `			break;` |
|        - | 11188 | `		}` |
|      ! 0 | 11189 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11190 | `	}` |
|        3 | 11191 | `	sEntry.nArg = j;` |
|        - | 11192 | `	/* Install the callback */` |
|        3 | 11193 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 11194 | `	return PH7_OK;` |
|        2 | 11195 |  |
|        - | 11196 | `/*` |
|        - | 11197 | ` * Section:` |
|        - | 11198 | ` *  Class handling functions.` |
|        - | 11199 | ` * Status:` |
|        - | 11200 | ` *    Stable.` |
|        - | 11201 | ` */` |
|        - | 11202 | `/*` |
|        - | 11203 | ` * Extract the top active class. NULL is returned` |
|        - | 11204 | ` * if the class stack is empty.` |
|        - | 11205 | ` */` |
|      926 | 11206 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11207 |  |
|      928 | 11208 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11209 | `	ph7_class **apClass;` |
|      928 | 11210 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11211 | `		/* Empty stack,return NULL */` |
|       15 | 11212 | `		return 0;` |
|        - | 11213 | `	}` |
|        - | 11214 | `	/* Peek the last entry */` |
|      914 | 11215 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      914 | 11216 | `	return apClass[pSet->nUsed - 1];` |
|      465 | 11217 |  |
|        - | 11218 | `/*` |
|        - | 11219 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11220 | ` *   Get the class that declared the currently executing method.` |
|        - | 11221 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11222 | ` *` |
|        - | 11223 | ` * Parameters` |
|        - | 11224 | ` *   pVm: Target VM` |
|        - | 11225 | ` *` |
|        - | 11226 | ` * Return` |
|        - | 11227 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11228 | ` *   - Not executing within a class method` |
|        - | 11229 | ` *` |
|        - | 11230 | ` * Note` |
|        - | 11231 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11232 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11233 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11234 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11235 | ` *   declaring class.` |
|        - | 11236 | ` */` |
|       98 | 11237 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11238 |  |
|      100 | 11239 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11240 | `	ph7_vm_func *pVmFunc;` |
|        - | 11241 |  |
|        - | 11242 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11243 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11244 |  |
|        - | 11245 | `	/* Check if we're in a method context */` |
|      100 | 11246 | `	if( pFrame->pParent ){` |
|       96 | 11247 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11248 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11249 | `			/* Return the declaring class */` |
|       96 | 11250 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11251 | `		}` |
|      ! 0 | 11252 | `	}` |
|        - | 11253 |  |
|        5 | 11254 | `	return 0;` |
|       51 | 11255 |  |
|        - | 11256 |  |
|        - | 11257 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11258 | `/*` |
|        - | 11259 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11260 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11261 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11262 | ` * return value indicates failure.` |
|        - | 11263 | ` */` |
|        - | 11264 | `/*` |
|        - | 11265 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11266 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11267 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11268 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11269 | ` */` |
|     2380 | 11270 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11271 | `	ph7_vm *pVm,` |
|        - | 11272 | `	ph7_class_instance *pThis,` |
|        - | 11273 | `	ph7_class_method *pMethod,` |
|        - | 11274 | `	ph7_value *pResult,` |
|        - | 11275 | `	int nArg,` |
|        - | 11276 | `	ph7_value **apArg,` |
|        - | 11277 | `	VmCallArgMap *pMap` |
|        - | 11278 | `	)` |
|        2 | 11279 |  |
|        - | 11280 | `	ph7_value *aStack;` |
|        - | 11281 | `	VmInstr aInstr[2];` |
|        - | 11282 | `	int iCursor;` |
|        - | 11283 | `	int i;` |
|     2382 | 11284 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2382 | 11285 | `	if( aStack == 0 ){` |
|      ! 0 | 11286 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11287 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11288 | `		return SXERR_MEM;` |
|        - | 11289 | `	}` |
|     3854 | 11290 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1474 | 11291 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1474 | 11292 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      738 | 11293 | `	}` |
|     2382 | 11294 | `	iCursor = nArg + 1;` |
|     2382 | 11295 | `	if( pThis ){` |
|     2376 | 11296 | `		pThis->iRef++;` |
|     2376 | 11297 | `		aStack[i].x.pOther = pThis;` |
|     2376 | 11298 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1187 | 11299 | `	}` |
|     2382 | 11300 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2382 | 11301 | `	i++;` |
|     2382 | 11302 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2382 | 11303 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2382 | 11304 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2382 | 11305 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2382 | 11306 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2382 | 11307 | `	aInstr[0].iP1 = nArg;` |
|     2382 | 11308 | `	aInstr[0].iP2 = 0;` |
|     2382 | 11309 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2382 | 11310 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2382 | 11311 | `	aInstr[1].iP1 = 1;` |
|     2382 | 11312 | `	aInstr[1].iP2 = 0;` |
|     2382 | 11313 | `	aInstr[1].p3  = 0;` |
|     2382 | 11314 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2382 | 11315 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     2382 | 11316 | `	return PH7_OK;` |
|     1192 | 11317 |  |
|     1902 | 11318 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11319 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11320 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11321 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11322 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11323 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11324 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11325 | `	)` |
|        2 | 11326 |  |
|     1904 | 11327 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11328 |  |
|        - | 11329 | `/*` |
|        - | 11330 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11331 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11332 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11333 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11334 | ` *` |
|        - | 11335 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11336 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11337 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11338 | ` *` |
|        - | 11339 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11340 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11341 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11342 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11343 | ` *` |
|        - | 11344 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11345 | ` */` |
|      166 | 11346 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11347 | `	ph7_vm *pVm,` |
|        - | 11348 | `	ph7_class_instance *pThis,` |
|        - | 11349 | `	int nArg,` |
|        - | 11350 | `	ph7_value **apArg,` |
|        - | 11351 | `	ph7_value *pResult,` |
|        - | 11352 | `	VmCallArgMap *pMap` |
|        - | 11353 | `	)` |
|        2 | 11354 |  |
|        - | 11355 | `	ph7_class_method *pMethod;` |
|      168 | 11356 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      168 | 11357 | `	if( pMethod == 0 ){` |
|       13 | 11358 | `		if( pResult ){` |
|       13 | 11359 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11360 | `		}` |
|       13 | 11361 | `		return SXERR_INVALID;` |
|        - | 11362 | `	}` |
|      156 | 11363 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       85 | 11364 |  |
|        - | 11365 | `/*` |
|        - | 11366 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11367 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11368 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11369 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11370 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11371 | ` * lookup or 'goto Exception').` |
|        - | 11372 | ` *` |
|        - | 11373 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11374 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11375 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11376 | ` * reported.` |
|        - | 11377 | ` */` |
|       12 | 11378 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11379 |  |
|        - | 11380 | `	ph7_class *pErrorClass;` |
|       13 | 11381 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11382 | `	ph7_class_method *pCons;` |
|        - | 11383 | `	VmFrame *pThrowFrame;` |
|        - | 11384 | `	char zMsg[256];` |
|        - | 11385 | `	int nMsg;` |
|        - | 11386 | `	sxi32 rc;` |
|       25 | 11387 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11388 | `		"Object of type %.*s is not callable",` |
|       12 | 11389 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11390 | `		pThis->pClass->sName.zString);` |
|       13 | 11391 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11392 | `	if( pErrorClass ){` |
|       13 | 11393 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11394 | `	}` |
|       13 | 11395 | `	if( pErrInst == 0 ){` |
|        - | 11396 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11397 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11398 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11399 | `		 * visible to the user. */` |
|      ! 0 | 11400 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11401 | `		return SXERR_ABORT;` |
|        - | 11402 | `	}` |
|       13 | 11403 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11404 | `	if( pCons ){` |
|        - | 11405 | `		ph7_value sArg;` |
|        - | 11406 | `		ph7_value *apMsg[1];` |
|        - | 11407 | `		SyString sMsgStr;` |
|       13 | 11408 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11409 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11410 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11411 | `		apMsg[0] = &sArg;` |
|       13 | 11412 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11413 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11414 | `	}` |
|        - | 11415 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11416 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11417 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11418 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11419 | `	if( pThrowFrame ){` |
|       13 | 11420 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11421 | `	}` |
|       13 | 11422 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11423 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11424 | `	return rc;` |
|        7 | 11425 |  |
|        - | 11426 | `/*` |
|        - | 11427 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11428 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11429 | ` * in the apArg[] array.` |
|        - | 11430 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11431 | ` * return value indicates failure.` |
|        - | 11432 | ` */` |
|     1102 | 11433 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11434 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11435 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11436 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11437 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11438 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11439 | `	)` |
|        2 | 11440 |  |
|        - | 11441 | `	ph7_value *aStack;` |
|        - | 11442 | `	VmInstr aInstr[2];` |
|        - | 11443 | `	int i;` |
|     1104 | 11444 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11445 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11446 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11447 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      137 | 11448 | `		return VmCallObjectInvoke(&(*pVm),` |
|       90 | 11449 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       45 | 11450 | `			nArg,apArg,pResult,0);` |
|        - | 11451 | `	}` |
|     1014 | 11452 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11453 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11454 | `		if( pResult ){` |
|        - | 11455 | `			/* Assume a null return value */` |
|      ! 0 | 11456 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11457 | `		}` |
|      511 | 11458 | `		return SXERR_INVALID;` |
|        - | 11459 | `	}` |
|      504 | 11460 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11461 | `		/* Class method */` |
|       11 | 11462 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 11463 | `		ph7_class_method *pMethod = 0;` |
|       11 | 11464 | `		ph7_class_instance *pThis = 0;` |
|       11 | 11465 | `		ph7_class *pClass = 0;` |
|        - | 11466 | `		ph7_value *pValue;` |
|        - | 11467 | `		sxi32 rc;` |
|       11 | 11468 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11469 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11470 | `			if( pResult ){` |
|        - | 11471 | `				/* Assume a null return value */` |
|      ! 0 | 11472 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11473 | `			}` |
|      ! 0 | 11474 | `			return SXRET_OK;` |
|        - | 11475 | `		}` |
|        - | 11476 | `		/* Extract the class name or an instance of it */` |
|       11 | 11477 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 11478 | `		if( pValue ){` |
|       11 | 11479 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 11480 | `		}` |
|       11 | 11481 | `		if( pClass == 0 ){` |
|        - | 11482 | `			/* No such class,return NULL */` |
|      ! 0 | 11483 | `			if( pResult ){` |
|      ! 0 | 11484 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11485 | `			}` |
|      ! 0 | 11486 | `			return SXRET_OK;` |
|        - | 11487 | `		}` |
|       11 | 11488 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11489 | `			/* Point to the class instance */` |
|        5 | 11490 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 11491 | `		}` |
|        - | 11492 | `		/* Try to extract the method */` |
|       11 | 11493 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 11494 | `		if( pValue ){` |
|       11 | 11495 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 11496 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 11497 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 11498 | `			}` |
|        5 | 11499 | `		}` |
|       11 | 11500 | `		if( pMethod == 0 ){` |
|        - | 11501 | `			/* No such method,return NULL */` |
|      ! 0 | 11502 | `			if( pResult ){` |
|      ! 0 | 11503 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11504 | `			}` |
|      ! 0 | 11505 | `			return SXRET_OK;` |
|        - | 11506 | `		}` |
|        - | 11507 | `		/* Call the class method */` |
|       11 | 11508 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 11509 | `		return rc;` |
|        - | 11510 | `	}` |
|        - | 11511 | `	/* Create a new operand stack */` |
|      494 | 11512 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      494 | 11513 | `	if( aStack == 0 ){` |
|      ! 0 | 11514 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11515 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11516 | `		if( pResult ){` |
|        - | 11517 | `			/* Assume a null return value */` |
|      ! 0 | 11518 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11519 | `		}` |
|      ! 0 | 11520 | `		return SXERR_MEM;` |
|        - | 11521 | `	}` |
|        - | 11522 | `	/* Fill the operand stack with the given arguments */` |
|     1604 | 11523 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1112 | 11524 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11525 | `		/*` |
|        - | 11526 | `		 * Symisc eXtension:` |
|        - | 11527 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11528 | `		 */` |
|     1112 | 11529 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      557 | 11530 | `	}` |
|        - | 11531 | `	/* Push the function name */` |
|      494 | 11532 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      494 | 11533 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11534 | `	/* Emit the CALL istruction */` |
|      494 | 11535 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      494 | 11536 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      494 | 11537 | `	aInstr[0].iP2 = 0;` |
|      494 | 11538 | `	aInstr[0].p3  = 0;` |
|        - | 11539 | `	/* Emit the DONE instruction */` |
|      494 | 11540 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      494 | 11541 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      494 | 11542 | `	aInstr[1].iP2 = 0;` |
|      494 | 11543 | `	aInstr[1].p3  = 0;` |
|        - | 11544 | `	/* Execute the function body (if available) */` |
|      494 | 11545 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11546 | `	/* Clean up the mess left behind */` |
|      494 | 11547 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      494 | 11548 | `	return PH7_OK;` |
|      553 | 11549 |  |
|        - | 11550 | `/*` |
|        - | 11551 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11552 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11553 | ` * parameter.` |
|        - | 11554 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11555 | ` * return value indicates failure.` |
|        - | 11556 | ` */` |
|      236 | 11557 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11558 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11559 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11560 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11561 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11562 | `	)` |
|        1 | 11563 |  |
|        - | 11564 | `	ph7_value *pArg;` |
|        - | 11565 | `	SySet aArg;` |
|        - | 11566 | `	va_list ap;` |
|        - | 11567 | `	sxi32 rc;` |
|      237 | 11568 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11569 | `	/* Copy arguments one after one */` |
|      237 | 11570 | `	va_start(ap,pResult);` |
|      393 | 11571 | `	for(;;){` |
|      787 | 11572 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 11573 | `		if( pArg == 0 ){` |
|      237 | 11574 | `			break;` |
|        - | 11575 | `		}` |
|      551 | 11576 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11577 | `	}` |
|        - | 11578 | `	/* Call the core routine */` |
|      237 | 11579 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11580 | `	/* Cleanup */` |
|      237 | 11581 | `	SySetRelease(&aArg);` |
|      237 | 11582 | `	return rc;` |
|        1 | 11583 |  |
|        - | 11584 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11585 | `/*` |
|        - | 11586 | ` * bool defined(string $name)` |
|        - | 11587 | ` *  Checks whether a given named constant exists.` |
|        - | 11588 | ` * Parameter:` |
|        - | 11589 | ` *  Name of the desired constant.` |
|        - | 11590 | ` * Return` |
|        - | 11591 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11592 | ` */` |
|       16 | 11593 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11594 |  |
|        - | 11595 | `	const char *zName;` |
|       18 | 11596 | `	int nLen = 0;` |
|       18 | 11597 | `	int res = 0;` |
|       18 | 11598 | `	if( nArg < 1 ){` |
|        - | 11599 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11600 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11601 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11602 | `		return SXRET_OK;` |
|        - | 11603 | `	}` |
|        - | 11604 | `	/* Extract constant name */` |
|       18 | 11605 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11606 | `	/* Perform the lookup */` |
|       18 | 11607 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11608 | `		/* Already defined */` |
|       12 | 11609 | `		res = 1;` |
|        5 | 11610 | `	}` |
|       18 | 11611 | `	ph7_result_bool(pCtx,res);` |
|       18 | 11612 | `	return SXRET_OK;` |
|       10 | 11613 |  |
|        - | 11614 | `/*` |
|        - | 11615 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11616 | ` * below.` |
|        - | 11617 | ` */` |
|       10 | 11618 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11619 |  |
|       12 | 11620 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11621 | `	/* Expand constant value */` |
|       12 | 11622 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11623 |  |
|        - | 11624 | `/*` |
|        - | 11625 | ` * bool define(string $constant_name,expression value)` |
|        - | 11626 | ` *  Defines a named constant at runtime.` |
|        - | 11627 | ` * Parameter:` |
|        - | 11628 | ` *  $constant_name` |
|        - | 11629 | ` *   The name of the constant` |
|        - | 11630 | ` *  $value` |
|        - | 11631 | ` *   Constant value` |
|        - | 11632 | ` * Return:` |
|        - | 11633 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11634 | ` */` |
|       12 | 11635 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11636 |  |
|        - | 11637 | `	const char *zName;  /* Constant name */` |
|        - | 11638 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11639 | `	int nLen = 0;       /* Name length */` |
|        - | 11640 | `	sxi32 rc;` |
|       14 | 11641 | `	if( nArg < 2 ){` |
|        - | 11642 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11643 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11644 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11645 | `		return SXRET_OK;` |
|        - | 11646 | `	}` |
|       14 | 11647 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11648 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11649 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11650 | `		return SXRET_OK;` |
|        - | 11651 | `	}` |
|        - | 11652 | `	/* Extract constant name */` |
|       14 | 11653 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11654 | `	if( nLen < 1 ){` |
|      ! 0 | 11655 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11656 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11657 | `		return SXRET_OK;` |
|        - | 11658 | `	}` |
|        - | 11659 | `	/* Duplicate constant value */` |
|       14 | 11660 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11661 | `	if( pValue == 0 ){` |
|      ! 0 | 11662 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11663 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11664 | `		return SXRET_OK;` |
|        - | 11665 | `	}` |
|        - | 11666 | `	/* Initialize the memory object */` |
|       14 | 11667 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11668 | `	/* Register the constant */` |
|       14 | 11669 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11670 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11671 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11672 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11673 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11674 | `		return SXRET_OK;` |
|        - | 11675 | `	}` |
|        - | 11676 | `	/* Duplicate constant value */` |
|       14 | 11677 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11678 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11679 | `		/* Lower case the constant name */` |
|      ! 0 | 11680 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11681 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11682 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11683 | `				/* UTF-8 stream */` |
|      ! 0 | 11684 | `				zCur++;` |
|      ! 0 | 11685 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11686 | `					zCur++;` |
|      ! 0 | 11687 | `				}` |
|      ! 0 | 11688 | `				continue;` |
|        - | 11689 | `			}` |
|      ! 0 | 11690 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11691 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11692 | `				zCur[0] = (char)c;` |
|      ! 0 | 11693 | `			}` |
|      ! 0 | 11694 | `			zCur++;` |
|      ! 0 | 11695 | `		}` |
|        - | 11696 | `		/* Finally,register the constant */` |
|      ! 0 | 11697 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11698 | `	}` |
|        - | 11699 | `	/* All done,return TRUE */` |
|       14 | 11700 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11701 | `	return SXRET_OK;` |
|        8 | 11702 |  |
|        - | 11703 | `/*` |
|        - | 11704 | ` * value constant(string $name)` |
|        - | 11705 | ` *  Returns the value of a constant` |
|        - | 11706 | ` * Parameter` |
|        - | 11707 | ` *  $name` |
|        - | 11708 | ` *    Name of the constant.` |
|        - | 11709 | ` * Return` |
|        - | 11710 | ` *  Constant value or NULL if not defined.` |
|        - | 11711 | ` */` |
|        8 | 11712 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11713 |  |
|        - | 11714 | `	SyHashEntry *pEntry;` |
|        - | 11715 | `	ph7_constant *pCons;` |
|        - | 11716 | `	const char *zName; /* Constant name */` |
|        - | 11717 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11718 | `	int nLen;` |
|       10 | 11719 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11720 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11721 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11722 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11723 | `		return SXRET_OK;` |
|        - | 11724 | `	}` |
|        - | 11725 | `	/* Extract the constant name */` |
|       10 | 11726 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11727 | `	/* Perform the query */` |
|       10 | 11728 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11729 | `	if( pEntry == 0 ){` |
|        3 | 11730 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11731 | `		ph7_result_null(pCtx);` |
|        3 | 11732 | `		return SXRET_OK;` |
|        - | 11733 | `	}` |
|        8 | 11734 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11735 | `	/* Point to the structure that describe the constant */` |
|        8 | 11736 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11737 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11738 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11739 | `	/* Return that value */` |
|        8 | 11740 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11741 | `	/* Cleanup */` |
|        8 | 11742 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11743 | `	return SXRET_OK;` |
|        6 | 11744 |  |
|        - | 11745 | `/*` |
|        - | 11746 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11747 | ` * defined below.` |
|        - | 11748 | ` */` |
|      452 | 11749 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11750 |  |
|      453 | 11751 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11752 | `	ph7_value sName;` |
|        - | 11753 | `	sxi32 rc;` |
|        - | 11754 | `	/* Prepare the constant name for insertion */` |
|      453 | 11755 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11756 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11757 | `	/* Perform the insertion */` |
|      453 | 11758 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11759 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11760 | `	return rc;` |
|        1 | 11761 |  |
|        - | 11762 | `/*` |
|        - | 11763 | ` * array get_defined_constants(void)` |
|        - | 11764 | ` *  Returns an associative array with the names of all defined` |
|        - | 11765 | ` *  constants.` |
|        - | 11766 | ` * Parameters` |
|        - | 11767 | ` *  NONE.` |
|        - | 11768 | ` * Returns` |
|        - | 11769 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11770 | ` */` |
|        2 | 11771 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11772 |  |
|        - | 11773 | `	ph7_value *pArray;` |
|        - | 11774 | `	/* Create the array first*/` |
|        3 | 11775 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11776 | `	if( pArray == 0 ){` |
|      ! 0 | 11777 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11778 | `		SXUNUSED(apArg);` |
|        - | 11779 | `		/* Return NULL */` |
|      ! 0 | 11780 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11781 | `		return SXRET_OK;` |
|        - | 11782 | `	}` |
|        - | 11783 | `	/* Fill the array with the defined constants */` |
|        3 | 11784 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11785 | `	/* Return the created array */` |
|        3 | 11786 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11787 | `	return SXRET_OK;` |
|        2 | 11788 |  |
|        - | 11789 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11790 | `/*` |
|        - | 11791 | ` * Section:` |
|        - | 11792 | ` *  Random numbers/string generators.` |
|        - | 11793 | ` * Status:` |
|        - | 11794 | ` *    Stable.` |
|        - | 11795 | ` */` |
|        - | 11796 | `/*` |
|        - | 11797 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11798 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11799 | ` * used by te SQLite3 library.` |
|        - | 11800 | ` */` |
|     2880 | 11801 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11802 |  |
|        - | 11803 | `	sxu32 iNum;` |
|     2882 | 11804 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2882 | 11805 | `	return iNum;` |
|        2 | 11806 |  |
|        - | 11807 | `/*` |
|        - | 11808 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11809 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11810 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11811 | ` * by te SQLite3 library.` |
|        - | 11812 | ` */` |
|   231980 | 11813 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11814 |  |
|        - | 11815 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11816 | `	int i;` |
|        - | 11817 | `	/* Generate a binary string first */` |
|   231982 | 11818 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11819 | `	/* Turn the binary string into english based alphabet */` |
|  2551950 | 11820 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2319970 | 11821 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1159986 | 11822 | `	 }` |
|   231982 | 11823 |  |
|        - | 11824 | `/*` |
|        - | 11825 | ` * int rand()` |
|        - | 11826 | ` * int mt_rand()` |
|        - | 11827 | ` * int rand(int $min,int $max)` |
|        - | 11828 | ` * int mt_rand(int $min,int $max)` |
|        - | 11829 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11830 | ` * Parameter` |
|        - | 11831 | ` *  $min` |
|        - | 11832 | ` *    The lowest value to return (default: 0)` |
|        - | 11833 | ` *  $max` |
|        - | 11834 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11835 | ` * Return` |
|        - | 11836 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11837 | ` * Note:` |
|        - | 11838 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11839 | ` *  by te SQLite3 library.` |
|        - | 11840 | ` */` |
|       20 | 11841 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11842 |  |
|        - | 11843 | `	sxu32 iNum;` |
|        - | 11844 | `	/* Generate the random number */` |
|       21 | 11845 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11846 | `	if( nArg > 1 ){` |
|        - | 11847 | `		sxu32 iMin,iMax;` |
|        3 | 11848 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11849 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11850 | `		if( iMin < iMax ){` |
|        3 | 11851 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11852 | `			if( iDiv > 0 ){` |
|        3 | 11853 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11854 | `			}` |
|        1 | 11855 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11856 | `			iNum %= iMax;` |
|      ! 0 | 11857 | `		}` |
|        1 | 11858 | `	}` |
|        - | 11859 | `	/* Return the number */` |
|       21 | 11860 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11861 | `	return SXRET_OK;` |
|        1 | 11862 |  |
|        - | 11863 | `/*` |
|        - | 11864 | ` * int getrandmax(void)` |
|        - | 11865 | ` * int mt_getrandmax(void)` |
|        - | 11866 | ` * int rc4_getrandmax(void)` |
|        - | 11867 | ` *   Show largest possible random value` |
|        - | 11868 | ` * Return` |
|        - | 11869 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11870 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11871 | ` * Note:` |
|        - | 11872 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11873 | ` *  by te SQLite3 library.` |
|        - | 11874 | ` */` |
|        4 | 11875 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11876 |  |
|        2 | 11877 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11878 | `	SXUNUSED(apArg);` |
|        5 | 11879 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11880 | `	return SXRET_OK;` |
|        1 | 11881 |  |
|        - | 11882 | `/*` |
|        - | 11883 | ` * string rand_str()` |
|        - | 11884 | ` * string rand_str(int $len)` |
|        - | 11885 | ` *  Generate a random string (English alphabet).` |
|        - | 11886 | ` * Parameter` |
|        - | 11887 | ` *  $len` |
|        - | 11888 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11889 | ` * Return` |
|        - | 11890 | ` *   A pseudo random string.` |
|        - | 11891 | ` * Note:` |
|        - | 11892 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11893 | ` *  by te SQLite3 library.` |
|        - | 11894 | ` *  This function is a symisc extension.` |
|        - | 11895 | ` */` |
|      120 | 11896 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11897 |  |
|        - | 11898 | `	char zString[1024];` |
|      122 | 11899 | `	int iLen = 0x10;` |
|      122 | 11900 | `	if( nArg > 0 ){` |
|        - | 11901 | `		/* Get the desired length */` |
|      122 | 11902 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11903 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11904 | `			/* Default length */` |
|        3 | 11905 | `			iLen = 0x10;` |
|        1 | 11906 | `		}` |
|       60 | 11907 | `	}` |
|        - | 11908 | `	/* Generate the random string */` |
|      122 | 11909 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11910 | `	/* Return the generated string */` |
|      122 | 11911 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11912 | `	return SXRET_OK;` |
|        2 | 11913 |  |
|        - | 11914 | `/*` |
|        - | 11915 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 11916 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 11917 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 11918 | ` */` |
|      488 | 11919 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 11920 |  |
|      488 | 11921 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 11922 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 11923 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11924 | `			"TypeError",` |
|        - | 11925 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 11926 | `			zFunc,iArgPos,zParamName,` |
|        3 | 11927 | `			ph7_type_name(pArg)` |
|        - | 11928 | `			);` |
|        - | 11929 | `	}` |
|      483 | 11930 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 11931 | `		int len;` |
|        9 | 11932 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 11933 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 11934 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11935 | `				"TypeError",` |
|        - | 11936 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 11937 | `				zFunc,iArgPos,zParamName` |
|        - | 11938 | `				);` |
|        - | 11939 | `		}` |
|        2 | 11940 | `	}` |
|      479 | 11941 | `	return SXRET_OK;` |
|      245 | 11942 |  |
|        - | 11943 | `/*` |
|        - | 11944 | ` * int random_int(int $min, int $max)` |
|        - | 11945 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 11946 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 11947 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 11948 | ` *  power-of-two mask covering the range.` |
|        - | 11949 | ` */` |
|      242 | 11950 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11951 |  |
|        - | 11952 | `	sxi64 iMin,iMax;` |
|        - | 11953 | `	sxu64 uRange,uMask,uResult;` |
|        - | 11954 | `	unsigned int nAttempt;` |
|        - | 11955 | `	int rc;` |
|      243 | 11956 | `	if( nArg != 2 ){` |
|       10 | 11957 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11958 | `			"ArgumentCountError",` |
|        - | 11959 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 11960 | `			nArg` |
|        - | 11961 | `			);` |
|        - | 11962 | `	}` |
|      237 | 11963 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 11964 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 11965 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 11966 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 11967 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 11968 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 11969 | `	if( iMin > iMax ){` |
|        3 | 11970 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11971 | `			"ValueError",` |
|        - | 11972 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 11973 | `			);` |
|        - | 11974 | `	}` |
|      229 | 11975 | `	if( iMin == iMax ){` |
|        5 | 11976 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 11977 | `		return SXRET_OK;` |
|        - | 11978 | `	}` |
|      225 | 11979 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 11980 | `	uMask = uRange;` |
|      225 | 11981 | `	uMask \|= uMask >> 1;` |
|      225 | 11982 | `	uMask \|= uMask >> 2;` |
|      225 | 11983 | `	uMask \|= uMask >> 4;` |
|      225 | 11984 | `	uMask \|= uMask >> 8;` |
|      225 | 11985 | `	uMask \|= uMask >> 16;` |
|      225 | 11986 | `	uMask \|= uMask >> 32;` |
|      225 | 11987 | `	uResult = 0;` |
|      355 | 11988 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 11989 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 11990 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 11991 | `		 * and the low-half mask would always read 0). */` |
|        - | 11992 | `		sxu64 uDraw;` |
|      355 | 11993 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 11994 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 11995 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 11996 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11997 | `				"Exception",` |
|        - | 11998 | `				"Cannot gather sufficient random data"` |
|        - | 11999 | `				);` |
|        - | 12000 | `		}` |
|      355 | 12001 | `		uDraw &= uMask;` |
|      355 | 12002 | `		if( uDraw <= uRange ){` |
|      225 | 12003 | `			uResult = uDraw;` |
|      225 | 12004 | `			break;` |
|        - | 12005 | `		}` |
|       56 | 12006 | `	}` |
|      225 | 12007 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12008 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12009 | `			"Exception",` |
|        - | 12010 | `			"Cannot gather sufficient random data"` |
|        - | 12011 | `			);` |
|        - | 12012 | `	}` |
|      225 | 12013 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12014 | `	return SXRET_OK;` |
|      122 | 12015 |  |
|        - | 12016 | `/*` |
|        - | 12017 | ` * string random_bytes(int $length)` |
|        - | 12018 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12019 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12020 | ` */` |
|       24 | 12021 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12022 |  |
|        - | 12023 | `	sxi64 iLen;` |
|        - | 12024 | `	unsigned char zStack[256];` |
|        - | 12025 | `	void *pBuf;` |
|        - | 12026 | `	int rc;` |
|       25 | 12027 | `	int bHeap = 0;` |
|       25 | 12028 | `	if( nArg != 1 ){` |
|        7 | 12029 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12030 | `			"ArgumentCountError",` |
|        - | 12031 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12032 | `			nArg` |
|        - | 12033 | `			);` |
|        - | 12034 | `	}` |
|       21 | 12035 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12036 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12037 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12038 | `	if( iLen < 1 ){` |
|        5 | 12039 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12040 | `			"ValueError",` |
|        - | 12041 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12042 | `			);` |
|        - | 12043 | `	}` |
|        - | 12044 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12045 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12046 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12047 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12048 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12049 | `			"ValueError",` |
|        - | 12050 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12051 | `			);` |
|        - | 12052 | `	}` |
|       13 | 12053 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12054 | `		pBuf = zStack;` |
|        7 | 12055 | `	}else{` |
|      ! 0 | 12056 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12057 | `		if( pBuf == 0 ){` |
|      ! 0 | 12058 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12059 | `				"Exception",` |
|        - | 12060 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12061 | `				iLen` |
|        - | 12062 | `				);` |
|        - | 12063 | `		}` |
|      ! 0 | 12064 | `		bHeap = 1;` |
|        - | 12065 | `	}` |
|       13 | 12066 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12067 | `		if( bHeap ){` |
|      ! 0 | 12068 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12069 | `		}` |
|      ! 0 | 12070 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12071 | `			"Exception",` |
|        - | 12072 | `			"Cannot gather sufficient random data"` |
|        - | 12073 | `			);` |
|        - | 12074 | `	}` |
|       13 | 12075 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12076 | `	if( bHeap ){` |
|      ! 0 | 12077 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12078 | `	}` |
|       13 | 12079 | `	return SXRET_OK;` |
|       13 | 12080 |  |
|        - | 12081 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12082 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12083 | `/* Unique ID private data */` |
|        - | 12084 | `struct unique_id_data` |
|        - | 12085 |  |
|        - | 12086 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12087 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12088 | `};` |
|        - | 12089 | `/*` |
|        - | 12090 | ` * Binary to hex consumer callback.` |
|        - | 12091 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12092 | ` * defined below.` |
|        - | 12093 | ` */` |
|      192 | 12094 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12095 |  |
|      193 | 12096 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12097 | `	sxu32 nBuflen;` |
|        - | 12098 | `	/* Extract result buffer length */` |
|      193 | 12099 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12100 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12101 | `			/*` |
|        - | 12102 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12103 | `			 * string will be 13 characters long` |
|        - | 12104 | `			 */` |
|       25 | 12105 | `		return SXERR_ABORT;` |
|        - | 12106 | `	}` |
|      169 | 12107 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12108 | `		return SXERR_ABORT;` |
|        - | 12109 | `	}` |
|        - | 12110 | `	/* Safely Consume the hex stream */` |
|      169 | 12111 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12112 | `	return SXRET_OK;` |
|       97 | 12113 |  |
|        - | 12114 | `/*` |
|        - | 12115 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12116 | ` *  Generate a unique ID` |
|        - | 12117 | ` * Parameter` |
|        - | 12118 | ` * $prefix` |
|        - | 12119 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12120 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12121 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12122 | ` * $more_entropy` |
|        - | 12123 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12124 | ` *  that the result will be unique.` |
|        - | 12125 | ` * Return` |
|        - | 12126 | ` *  Returns the unique identifier, as a string.` |
|        - | 12127 | ` */` |
|       24 | 12128 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12129 |  |
|        - | 12130 | `	struct unique_id_data sUniq;` |
|        - | 12131 | `	unsigned char zDigest[20];` |
|       25 | 12132 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12133 | `	const char *zPrefix;` |
|        - | 12134 | `	SHA1Context sCtx;` |
|        - | 12135 | `	char zRandom[7];` |
|        - | 12136 | `	int nPrefix;` |
|        - | 12137 | `	int entropy;` |
|        - | 12138 | `	/* Generate a random string first */` |
|       25 | 12139 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12140 | `	/* Initialize fields */` |
|       25 | 12141 | `	zPrefix = 0;` |
|       25 | 12142 | `	nPrefix = 0;` |
|       25 | 12143 | `	entropy = 0;` |
|       25 | 12144 | `	if( nArg > 0 ){` |
|        - | 12145 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12146 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12147 | `		if( nArg > 1 ){` |
|      ! 0 | 12148 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12149 | `		}` |
|      ! 0 | 12150 | `	}` |
|       25 | 12151 | `	SHA1Init(&sCtx);` |
|        - | 12152 | `	/* Generate the random ID */` |
|       25 | 12153 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12154 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12155 | `	}` |
|        - | 12156 | `	/* Append the random ID */` |
|       25 | 12157 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12158 | `	/* Append the random string */` |
|       25 | 12159 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12160 | `	/* Increment the number */` |
|       25 | 12161 | `	pVm->unique_id++;` |
|       25 | 12162 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12163 | `	/* Hexify the digest */` |
|       25 | 12164 | `	sUniq.pCtx = pCtx;` |
|       25 | 12165 | `	sUniq.entropy = entropy;` |
|       25 | 12166 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12167 | `	/* All done */` |
|       25 | 12168 | `	return PH7_OK;` |
|        1 | 12169 |  |
|        - | 12170 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12171 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12172 | `/*` |
|        - | 12173 | ` * Section:` |
|        - | 12174 | ` *  Language construct implementation as foreign functions.` |
|        - | 12175 | ` * Status:` |
|        - | 12176 | ` *    Stable.` |
|        - | 12177 | ` */` |
|        - | 12178 | `/*` |
|        - | 12179 | ` * void echo($string...)` |
|        - | 12180 | ` *  Output one or more messages.` |
|        - | 12181 | ` * Parameters` |
|        - | 12182 | ` *  $string` |
|        - | 12183 | ` *   Message to output.` |
|        - | 12184 | ` * Return` |
|        - | 12185 | ` *  NULL.` |
|        - | 12186 | ` */` |
|      ! 0 | 12187 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12188 |  |
|        - | 12189 | `	const char *zData;` |
|      ! 0 | 12190 | `	int nDataLen = 0;` |
|        - | 12191 | `	ph7_vm *pVm;` |
|        - | 12192 | `	int i,rc;` |
|        - | 12193 | `	/* Point to the target VM */` |
|      ! 0 | 12194 | `	pVm = pCtx->pVm;` |
|        - | 12195 | `	/* Output */` |
|      ! 0 | 12196 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12197 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12198 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12199 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12200 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12201 | `			if( rc == SXERR_ABORT ){` |
|        - | 12202 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12203 | `				return PH7_ABORT;` |
|        - | 12204 | `			}` |
|      ! 0 | 12205 | `		}` |
|      ! 0 | 12206 | `	}` |
|      ! 0 | 12207 | `	return SXRET_OK;` |
|      ! 0 | 12208 |  |
|        - | 12209 | `/*` |
|        - | 12210 | ` * int print($string...)` |
|        - | 12211 | ` *  Output one or more messages.` |
|        - | 12212 | ` * Parameters` |
|        - | 12213 | ` *  $string` |
|        - | 12214 | ` *   Message to output.` |
|        - | 12215 | ` * Return` |
|        - | 12216 | ` *  1 always.` |
|        - | 12217 | ` */` |
|        2 | 12218 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12219 |  |
|        - | 12220 | `	const char *zData;` |
|        3 | 12221 | `	int nDataLen = 0;` |
|        - | 12222 | `	ph7_vm *pVm;` |
|        - | 12223 | `	int i,rc;` |
|        - | 12224 | `	/* Point to the target VM */` |
|        3 | 12225 | `	pVm = pCtx->pVm;` |
|        - | 12226 | `	/* Output */` |
|        5 | 12227 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12228 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12229 | `		if( nDataLen > 0 ){` |
|        3 | 12230 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12231 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12232 | `			if( rc == SXERR_ABORT ){` |
|        - | 12233 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12234 | `				return PH7_ABORT;` |
|        - | 12235 | `			}` |
|        1 | 12236 | `		}` |
|        2 | 12237 | `	}` |
|        - | 12238 | `	/* Return 1 */` |
|        3 | 12239 | `	ph7_result_int(pCtx,1);` |
|        3 | 12240 | `	return SXRET_OK;` |
|        2 | 12241 |  |
|        - | 12242 | `/*` |
|        - | 12243 | ` * void exit(string $msg)` |
|        - | 12244 | ` * void exit(int $status)` |
|        - | 12245 | ` * void die(string $ms)` |
|        - | 12246 | ` * void die(int $status)` |
|        - | 12247 | ` *   Output a message and terminate program execution.` |
|        - | 12248 | ` * Parameter` |
|        - | 12249 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12250 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12251 | ` *  and not printed` |
|        - | 12252 | ` * Return` |
|        - | 12253 | ` *  NULL` |
|        - | 12254 | ` */` |
|      ! 0 | 12255 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12256 |  |
|      ! 0 | 12257 | `	if( nArg > 0 ){` |
|      ! 0 | 12258 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12259 | `			const char *zData;` |
|      ! 0 | 12260 | `			int iLen = 0;` |
|        - | 12261 | `			/* Print exit message */` |
|      ! 0 | 12262 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12263 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12264 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12265 | `			sxi32 iExitStatus;` |
|        - | 12266 | `			/* Record exit status code */` |
|      ! 0 | 12267 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12268 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12269 | `		}` |
|      ! 0 | 12270 | `	}` |
|        - | 12271 | `	/* Check if we are in an included file */` |
|      ! 0 | 12272 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 12273 | `		/* Exit the entire process */` |
|      ! 0 | 12274 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 12275 | `	}` |
|        - | 12276 | `	/* Abort processing immediately */` |
|      ! 0 | 12277 | `	return PH7_ABORT;` |
|      ! 0 | 12278 |  |
|        - | 12279 | `/*` |
|        - | 12280 | ` * bool isset($var,...)` |
|        - | 12281 | ` *  Finds out whether a variable is set.` |
|        - | 12282 | ` * Parameters` |
|        - | 12283 | ` *  One or more variable to check.` |
|        - | 12284 | ` * Return` |
|        - | 12285 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12286 | ` */` |
|    90748 | 12287 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12288 |  |
|        - | 12289 | `	ph7_value *pObj;` |
|    90750 | 12290 | `	int res = 0;` |
|        - | 12291 | `	int i;` |
|    90750 | 12292 | `	if( nArg < 1 ){` |
|        - | 12293 | `		/* Missing arguments,return false */` |
|      ! 0 | 12294 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12295 | `		return SXRET_OK;` |
|        - | 12296 | `	}` |
|        - | 12297 | `	/* Iterate over available arguments */` |
|   118680 | 12298 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    90760 | 12299 | `		pObj = apArg[i];` |
|    90760 | 12300 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12301 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12302 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12303 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    61932 | 12304 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12305 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12306 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12307 | `			}` |
|    30965 | 12308 | `		}` |
|    90760 | 12309 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    90760 | 12310 | `		if( !res ){` |
|        - | 12311 | `			/* Variable not set,return FALSE */` |
|    62830 | 12312 | `			ph7_result_bool(pCtx,0);` |
|    62830 | 12313 | `			return SXRET_OK;` |
|        - | 12314 | `		}` |
|    13967 | 12315 | `	}` |
|        - | 12316 | `	/* All given variable are set,return TRUE */` |
|    27922 | 12317 | `	ph7_result_bool(pCtx,1);` |
|    27922 | 12318 | `	return SXRET_OK;` |
|    45376 | 12319 |  |
|        - | 12320 | `/*` |
|        - | 12321 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12322 | ` * frame,the reference table and discard it's contents.` |
|        - | 12323 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12324 | ` */` |
|  3128430 | 12325 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12326 |  |
|        - | 12327 | `	ph7_value *pObj;` |
|        - | 12328 | `	VmRefObj *pRef;` |
|  3128432 | 12329 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3128432 | 12330 | `	if( pObj ){` |
|        - | 12331 | `		/* Release the object */` |
|  3128432 | 12332 | `		PH7_MemObjRelease(pObj);` |
|  1564215 | 12333 | `	}` |
|        - | 12334 | `	/* Remove old reference links */` |
|  3128432 | 12335 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3128432 | 12336 | `	if( pRef ){` |
|  3128426 | 12337 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12338 | `		/* Unlink from the reference table */` |
|  3128426 | 12339 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3128426 | 12340 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12341 | `			VmSlot sFree;` |
|        - | 12342 | `			/* Restore to the free list */` |
|  3128418 | 12343 | `			sFree.nIdx = nObjIdx;` |
|  3128418 | 12344 | `			sFree.pUserData = 0;` |
|  3128418 | 12345 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1564208 | 12346 | `		}` |
|  1564212 | 12347 | `	}` |
|  3128432 | 12348 | `	return SXRET_OK;` |
|        2 | 12349 |  |
|        - | 12350 | `/*` |
|        - | 12351 | ` * void unset($var,...)` |
|        - | 12352 | ` *   Unset one or more given variable.` |
|        - | 12353 | ` * Parameters` |
|        - | 12354 | ` *  One or more variable to unset.` |
|        - | 12355 | ` * Return` |
|        - | 12356 | ` *  Nothing.` |
|        - | 12357 | ` */` |
|     7438 | 12358 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12359 |  |
|        - | 12360 | `	ph7_value *pObj;` |
|        - | 12361 | `	ph7_vm *pVm;` |
|        - | 12362 | `	int i;` |
|        - | 12363 | `	/* Point to the target VM */` |
|     7440 | 12364 | `	pVm = pCtx->pVm;` |
|        - | 12365 | `	/* Iterate and unset */` |
|    14878 | 12366 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7440 | 12367 | `		pObj = apArg[i];` |
|     7440 | 12368 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12369 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12370 | `				/* Throw an error */` |
|      ! 0 | 12371 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12372 | `			}` |
|      410 | 12373 | `		}else{` |
|     6624 | 12374 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12375 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6624 | 12376 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6618 | 12377 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3308 | 12378 | `			}` |
|        - | 12379 | `		}` |
|     3721 | 12380 | `	}` |
|     7440 | 12381 | `	return SXRET_OK;` |
|        2 | 12382 |  |
|        - | 12383 | `/*` |
|        - | 12384 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12385 | ` */` |
|      110 | 12386 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12387 |  |
|      111 | 12388 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 12389 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12390 | `	ph7_value *pObj;` |
|        - | 12391 | `	sxu32 nIdx;` |
|        - | 12392 | `	/* Extract the memory object */` |
|      111 | 12393 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 12394 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 12395 | `	if( pObj ){` |
|      111 | 12396 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 12397 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12398 | `				SyString sName;` |
|        - | 12399 | `				ph7_value sKey;` |
|        - | 12400 | `				/* Perform the insertion */` |
|      109 | 12401 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 12402 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 12403 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 12404 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 12405 | `			}` |
|       54 | 12406 | `		}` |
|       55 | 12407 | `	}` |
|      111 | 12408 | `	return SXRET_OK;` |
|        1 | 12409 |  |
|        - | 12410 | `/*` |
|        - | 12411 | ` * array get_defined_vars(void)` |
|        - | 12412 | ` *  Returns an array of all defined variables.` |
|        - | 12413 | ` * Parameter` |
|        - | 12414 | ` *  None` |
|        - | 12415 | ` * Return` |
|        - | 12416 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12417 | ` */` |
|        2 | 12418 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12419 |  |
|        3 | 12420 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12421 | `	ph7_value *pArray;` |
|        - | 12422 | `	/* Create a new array */` |
|        3 | 12423 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12424 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12425 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12426 | `		SXUNUSED(apArg);` |
|        - | 12427 | `		/* Return NULL */` |
|      ! 0 | 12428 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12429 | `		return SXRET_OK;` |
|        - | 12430 | `	}` |
|        - | 12431 | `	/* Superglobals first */` |
|        3 | 12432 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12433 | `	/* Then variable defined in the current frame */` |
|        3 | 12434 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12435 | `	/* Finally,return the created array */` |
|        3 | 12436 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12437 | `	return SXRET_OK;` |
|        2 | 12438 |  |
|        - | 12439 | `/*` |
|        - | 12440 | ` * bool gettype($var)` |
|        - | 12441 | ` *  Get the type of a variable` |
|        - | 12442 | ` * Parameters` |
|        - | 12443 | ` *   $var` |
|        - | 12444 | ` *    The variable being type checked.` |
|        - | 12445 | ` * Return` |
|        - | 12446 | ` *   String representation of the given variable type.` |
|        - | 12447 | ` */` |
|       32 | 12448 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12449 |  |
|       34 | 12450 | `	const char *zType = "Empty";` |
|       34 | 12451 | `	if( nArg > 0 ){` |
|       34 | 12452 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12453 | `	}` |
|        - | 12454 | `	/* Return the variable type */` |
|       34 | 12455 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12456 | `	return SXRET_OK;` |
|        2 | 12457 |  |
|        - | 12458 | `/*` |
|        - | 12459 | ` * string get_resource_type(resource $handle)` |
|        - | 12460 | ` *  This function gets the type of the given resource.` |
|        - | 12461 | ` * Parameters` |
|        - | 12462 | ` *  $handle` |
|        - | 12463 | ` *  The evaluated resource handle.` |
|        - | 12464 | ` * Return` |
|        - | 12465 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12466 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12467 | ` *  the return value will be the string Unknown.` |
|        - | 12468 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12469 | ` *  is not a resource.` |
|        - | 12470 | ` */` |
|        2 | 12471 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12472 |  |
|        3 | 12473 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12474 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12476 | `		return PH7_OK;` |
|        - | 12477 | `	}` |
|        3 | 12478 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12479 | `	return SXRET_OK;` |
|        2 | 12480 |  |
|        - | 12481 | `/*` |
|        - | 12482 | ` * void var_dump(expression,....)` |
|        - | 12483 | ` *   var_dump � Dumps information about a variable` |
|        - | 12484 | ` * Parameters` |
|        - | 12485 | ` *   One or more expression to dump.` |
|        - | 12486 | ` * Returns` |
|        - | 12487 | ` *  Nothing.` |
|        - | 12488 | ` */` |
|      218 | 12489 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12490 |  |
|        - | 12491 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12492 | `	int i;` |
|      220 | 12493 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12494 | `	/* Dump one or more expressions */` |
|      444 | 12495 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12496 | `		ph7_value *pObj = apArg[i];` |
|        - | 12497 | `		/* Reset the working buffer */` |
|      226 | 12498 | `		SyBlobReset(&sDump);` |
|        - | 12499 | `		/* Dump the given expression */` |
|      226 | 12500 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12501 | `		/* Output */` |
|      226 | 12502 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12503 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12504 | `		}` |
|      114 | 12505 | `	}` |
|        - | 12506 | `	/* Release the working buffer */` |
|      220 | 12507 | `	SyBlobRelease(&sDump);` |
|      220 | 12508 | `	return SXRET_OK;` |
|        2 | 12509 |  |
|        - | 12510 | `/*` |
|        - | 12511 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12512 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12513 | ` * Parameters` |
|        - | 12514 | ` *   expression: Expression to dump` |
|        - | 12515 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12516 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12517 | ` *            print_r() will return the information rather than print it.` |
|        - | 12518 | ` * Return` |
|        - | 12519 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12520 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12521 | ` */` |
|       16 | 12522 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12523 |  |
|       17 | 12524 | `	int ret_string = 0;` |
|        - | 12525 | `	SyBlob sDump;` |
|       17 | 12526 | `	if( nArg < 1 ){` |
|        - | 12527 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12528 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12529 | `		return SXRET_OK;` |
|        - | 12530 | `	}` |
|       17 | 12531 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12532 | `	if ( nArg > 1 ){` |
|        - | 12533 | `		/* Where to redirect output */` |
|       11 | 12534 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12535 | `	}` |
|        - | 12536 | `	/* Generate dump */` |
|       17 | 12537 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12538 | `	if( !ret_string ){` |
|        - | 12539 | `		/* Output dump */` |
|        7 | 12540 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12541 | `		/* Return true */` |
|        7 | 12542 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12543 | `	}else{` |
|        - | 12544 | `		/* Generated dump as return value */` |
|       11 | 12545 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12546 | `	}` |
|        - | 12547 | `	/* Release the working buffer */` |
|       17 | 12548 | `	SyBlobRelease(&sDump);` |
|       17 | 12549 | `	return SXRET_OK;` |
|        9 | 12550 |  |
|        - | 12551 | `/*` |
|        - | 12552 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12553 | ` * Same job as print_r. (see coment above)` |
|        - | 12554 | ` */` |
|        2 | 12555 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12556 |  |
|        3 | 12557 | `	int ret_string = 0;` |
|        - | 12558 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12559 | `	if( nArg < 1 ){` |
|        - | 12560 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12561 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12562 | `		return SXRET_OK;` |
|        - | 12563 | `	}` |
|        3 | 12564 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12565 | `	if ( nArg > 1 ){` |
|        - | 12566 | `		/* Where to redirect output */` |
|        3 | 12567 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12568 | `	}` |
|        - | 12569 | `	/* Generate dump */` |
|        3 | 12570 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12571 | `	if( !ret_string ){` |
|        - | 12572 | `		/* Output dump */` |
|      ! 0 | 12573 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12574 | `		/* Return NULL */` |
|      ! 0 | 12575 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12576 | `	}else{` |
|        - | 12577 | `		/* Generated dump as return value */` |
|        3 | 12578 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12579 | `	}` |
|        - | 12580 | `	/* Release the working buffer */` |
|        3 | 12581 | `	SyBlobRelease(&sDump);` |
|        3 | 12582 | `	return SXRET_OK;` |
|        2 | 12583 |  |
|        - | 12584 | `/*` |
|        - | 12585 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12586 | ` *  Set/get the various assert flags.` |
|        - | 12587 | ` * Parameter` |
|        - | 12588 | ` * $what` |
|        - | 12589 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12590 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12591 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12592 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12593 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12594 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12595 | ` * $value` |
|        - | 12596 | ` *   An optional new value for the option.` |
|        - | 12597 | ` * Return` |
|        - | 12598 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12599 | ` */` |
|       28 | 12600 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12601 |  |
|       30 | 12602 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12603 | `	int iOption;` |
|        - | 12604 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12605 | `	if( nArg < 1 ){` |
|        3 | 12606 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12607 | `			"ArgumentCountError",` |
|        - | 12608 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12609 | `			);` |
|        - | 12610 | `	}` |
|        - | 12611 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12612 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12613 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12614 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12615 | `			"TypeError",` |
|        - | 12616 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12617 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12618 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12619 | `			);` |
|        - | 12620 | `	}` |
|       28 | 12621 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12622 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12623 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12624 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12625 | `	switch( iOption ){` |
|        5 | 12626 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12627 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12628 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12629 | `		if( nArg > 1 ){` |
|        5 | 12630 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12631 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12632 | `			}else{` |
|        3 | 12633 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12634 | `			}` |
|        2 | 12635 | `		}` |
|       12 | 12636 | `		break;` |
|        1 | 12637 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12638 | `		/* Return old callback or null */` |
|        3 | 12639 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12640 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12641 | `		}else{` |
|        3 | 12642 | `			ph7_result_null(pCtx);` |
|        - | 12643 | `		}` |
|        3 | 12644 | `		if( nArg > 1 ){` |
|      ! 0 | 12645 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12646 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12647 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12648 | `			}else{` |
|      ! 0 | 12649 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12650 | `			}` |
|      ! 0 | 12651 | `		}` |
|        3 | 12652 | `		break;` |
|        5 | 12653 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12654 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12655 | `		if( nArg > 1 ){` |
|        5 | 12656 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12657 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12658 | `			}else{` |
|        3 | 12659 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12660 | `			}` |
|        2 | 12661 | `		}` |
|       11 | 12662 | `		break;` |
|      ! 0 | 12663 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12664 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12665 | `		break;` |
|        1 | 12666 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12667 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12668 | `		break;` |
|      ! 0 | 12669 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12670 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12671 | `		break;` |
|        1 | 12672 | `	default:` |
|        - | 12673 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12674 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12675 | `			"ValueError",` |
|        - | 12676 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12677 | `			);` |
|        - | 12678 | `	}` |
|       26 | 12679 | `	return PH7_OK;` |
|       16 | 12680 |  |
|        - | 12681 | `/*` |
|        - | 12682 | ` * bool assert(mixed $assertion)` |
|        - | 12683 | ` *  Checks if assertion is FALSE.` |
|        - | 12684 | ` * Parameter` |
|        - | 12685 | ` *  $assertion` |
|        - | 12686 | ` *    The assertion to test.` |
|        - | 12687 | ` * Return` |
|        - | 12688 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12689 | ` */` |
|       24 | 12690 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12691 |  |
|       26 | 12692 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12693 | `	int iFlags,iResult;` |
|        - | 12694 | `	const char *zDesc;` |
|        - | 12695 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12696 | `	if( nArg < 1 ){` |
|        3 | 12697 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12698 | `			"ArgumentCountError",` |
|        - | 12699 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12700 | `			);` |
|        - | 12701 | `	}` |
|       24 | 12702 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12703 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12704 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12705 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12706 | `		return PH7_OK;` |
|        - | 12707 | `	}` |
|        - | 12708 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12709 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12710 | `	if( !iResult ){` |
|        - | 12711 | `		/* Assertion failed */` |
|        - | 12712 | `		/* Extract optional description */` |
|       13 | 12713 | `		zDesc = 0;` |
|       13 | 12714 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12715 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12716 | `		}` |
|       13 | 12717 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12718 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12719 | `			ph7_value sFile,sLine;` |
|        - | 12720 | `			ph7_value *apCbArg[3];` |
|        - | 12721 | `			SyString *pFile;` |
|        - | 12722 | `			/* Extract the processed script */` |
|      ! 0 | 12723 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12724 | `			if( pFile == 0 ){` |
|      ! 0 | 12725 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12726 | `			}` |
|        - | 12727 | `			/* Invoke the callback */` |
|      ! 0 | 12728 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12729 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12730 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12731 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12732 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12733 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12734 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12735 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12736 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12737 | `		}` |
|       13 | 12738 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12739 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12740 | `			return PH7_ABORT;` |
|        - | 12741 | `		}` |
|        - | 12742 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12743 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12744 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12745 | `				"AssertionError",` |
|        - | 12746 | `				"%s",` |
|        1 | 12747 | `				zDesc` |
|        - | 12748 | `				);` |
|      ! 0 | 12749 | `		}else{` |
|       11 | 12750 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12751 | `				"AssertionError",` |
|        - | 12752 | `				"assert(false)"` |
|        - | 12753 | `				);` |
|        - | 12754 | `		}` |
|        - | 12755 | `	}` |
|        - | 12756 | `	/* Assertion passed */` |
|       11 | 12757 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12758 | `	return PH7_OK;` |
|       14 | 12759 |  |
|        - | 12760 | `/*` |
|        - | 12761 | ` * Section:` |
|        - | 12762 | ` *  Error reporting functions.` |
|        - | 12763 | ` * Status:` |
|        - | 12764 | ` *    Stable.` |
|        - | 12765 | ` */` |
|        - | 12766 | `/*` |
|        - | 12767 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12768 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12769 | ` * Parameters` |
|        - | 12770 | ` *  $error_msg` |
|        - | 12771 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12772 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12773 | ` * $error_type` |
|        - | 12774 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12775 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12776 | ` * Return` |
|        - | 12777 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12778 | ` */` |
|       12 | 12779 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12780 |  |
|       14 | 12781 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12782 | `	int rc = PH7_OK;` |
|       14 | 12783 | `	if( nArg > 0 ){` |
|        - | 12784 | `		const char *zErr;` |
|        - | 12785 | `		int nLen;` |
|        - | 12786 | `		/* Extract the error message */` |
|       12 | 12787 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12788 | `		if( nArg > 1 ){` |
|        - | 12789 | `			/* Extract the error type */` |
|       12 | 12790 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12791 | `			switch( nErr ){` |
|        1 | 12792 | `			case 1:   /* E_ERROR */` |
|        - | 12793 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12794 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12795 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12796 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12797 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12798 | `				break;` |
|        1 | 12799 | `			case 2:   /* E_WARNING */` |
|        - | 12800 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12801 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12802 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12803 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12804 | `				break;` |
|        3 | 12805 | `			default:` |
|        8 | 12806 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12807 | `				break;` |
|        - | 12808 | `			}` |
|        5 | 12809 | `		}` |
|        - | 12810 | `		/* Report error */` |
|       12 | 12811 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12812 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12813 | `			return rc;` |
|        - | 12814 | `		}` |
|        - | 12815 | `		/* Return true */` |
|       12 | 12816 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12817 | `	}else{` |
|        - | 12818 | `		/* Missing arguments,return FALSE */` |
|        3 | 12819 | `		ph7_result_bool(pCtx,0);` |
|        - | 12820 | `	}` |
|       14 | 12821 | `	return rc;` |
|        8 | 12822 |  |
|        - | 12823 | `/*` |
|        - | 12824 | ` * int error_reporting([int $level])` |
|        - | 12825 | ` *  Sets which PHP errors are reported.` |
|        - | 12826 | ` * Parameters` |
|        - | 12827 | ` *  $level` |
|        - | 12828 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12829 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12830 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12831 | ` *   levels will not always behave as expected.` |
|        - | 12832 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12833 | ` *   in the predefined constants.` |
|        - | 12834 | ` * Return` |
|        - | 12835 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12836 | ` *   parameter is given.` |
|        - | 12837 | ` */` |
|       38 | 12838 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12839 |  |
|       40 | 12840 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12841 | `	int nOld;` |
|        - | 12842 | `	/* Extract the old reporting level */` |
|       40 | 12843 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 12844 | `	if( nArg > 0 ){` |
|        - | 12845 | `		int nNew;` |
|        - | 12846 | `		/* Extract the desired error reporting level */` |
|       32 | 12847 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 12848 | `		if( !nNew ){` |
|        - | 12849 | `			/* Do not report errors at all */` |
|        5 | 12850 | `			pVm->bErrReport = 0;` |
|        3 | 12851 | `		}else{` |
|        - | 12852 | `			/* Report all errors */` |
|       28 | 12853 | `			pVm->bErrReport = 1;` |
|        - | 12854 | `		}` |
|       15 | 12855 | `	}` |
|        - | 12856 | `	/* Return the old level */` |
|       40 | 12857 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12858 | `	return PH7_OK;` |
|        2 | 12859 |  |
|        - | 12860 | `/*` |
|        - | 12861 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12862 | ` *  Send an error message somewhere.` |
|        - | 12863 | ` * Parameter` |
|        - | 12864 | ` *  $message` |
|        - | 12865 | ` *   The error message that should be logged.` |
|        - | 12866 | ` *  $message_type` |
|        - | 12867 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12868 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12869 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12870 | ` *       This is the default option.` |
|        - | 12871 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12872 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12873 | ` *    2  No longer an option.` |
|        - | 12874 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12875 | ` *       to the end of the message string.` |
|        - | 12876 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12877 | ` *  $destination` |
|        - | 12878 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12879 | ` *  $extra_headers` |
|        - | 12880 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12881 | ` * Return` |
|        - | 12882 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12883 | ` * NOTE:` |
|        - | 12884 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12885 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12886 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12887 | ` *  Otherwise this function is no-op.` |
|        - | 12888 | ` */` |
|        4 | 12889 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12890 |  |
|        - | 12891 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12892 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12893 | `	int iType = 0;` |
|        5 | 12894 | `	if( nArg < 1 ){` |
|        - | 12895 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12896 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12897 | `		return PH7_OK;` |
|        - | 12898 | `	}` |
|        5 | 12899 | `	if( pVm->xErrLog  ){` |
|        - | 12900 | `		/* Invoke the user callback */` |
|      ! 0 | 12901 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12902 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12903 | `		if( nArg > 1 ){` |
|      ! 0 | 12904 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12905 | `			if( nArg > 2 ){` |
|      ! 0 | 12906 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12907 | `				if( nArg > 3 ){` |
|      ! 0 | 12908 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12909 | `				}` |
|      ! 0 | 12910 | `			}` |
|      ! 0 | 12911 | `		}` |
|      ! 0 | 12912 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12913 | `	}` |
|        - | 12914 | `	/* Retun TRUE */` |
|        5 | 12915 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12916 | `	return PH7_OK;` |
|        3 | 12917 |  |
|        - | 12918 | `/*` |
|        - | 12919 | ` * bool restore_exception_handler(void)` |
|        - | 12920 | ` *  Restores the previously defined exception handler function.` |
|        - | 12921 | ` * Parameter` |
|        - | 12922 | ` *  None` |
|        - | 12923 | ` * Return` |
|        - | 12924 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12925 | ` */` |
|        4 | 12926 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12927 |  |
|        5 | 12928 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12929 | `	ph7_value *pOld,*pNew;` |
|        - | 12930 | `	/* Point to the old and the new handler */` |
|        5 | 12931 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12932 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12933 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12934 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12935 | `		SXUNUSED(apArg);` |
|        - | 12936 | `		/* No installed handler,return FALSE */` |
|        5 | 12937 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12938 | `		return PH7_OK;` |
|        - | 12939 | `	}` |
|        - | 12940 | `	/* Copy the old handler */` |
|      ! 0 | 12941 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12942 | `	PH7_MemObjRelease(pOld);` |
|        - | 12943 | `	/* Return TRUE */` |
|      ! 0 | 12944 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12945 | `	return PH7_OK;` |
|        3 | 12946 |  |
|        - | 12947 | `/*` |
|        - | 12948 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12949 | ` *  Sets a user-defined exception handler function.` |
|        - | 12950 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12951 | ` * NOTE` |
|        - | 12952 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12953 | ` *  the satndard PHP engine.` |
|        - | 12954 | ` * Parameters` |
|        - | 12955 | ` *  $exception_handler` |
|        - | 12956 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12957 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12958 | ` *   that was thrown.` |
|        - | 12959 | ` *  Note:` |
|        - | 12960 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12961 | ` * Return` |
|        - | 12962 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12963 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12964 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12965 | ` */` |
|        4 | 12966 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12967 |  |
|        6 | 12968 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12969 | `	ph7_value *pOld,*pNew;` |
|        - | 12970 | `	/* Point to the old and the new handler */` |
|        6 | 12971 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 12972 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 12973 | `	/* Return the old handler */` |
|        6 | 12974 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 12975 | `	if( nArg > 0 ){` |
|        6 | 12976 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12977 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 12978 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 12979 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12980 | `		}else{` |
|        6 | 12981 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12982 | `			/* Install the new handler */` |
|        6 | 12983 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12984 | `		}` |
|        2 | 12985 | `	}` |
|        6 | 12986 | `	return PH7_OK;` |
|        2 | 12987 |  |
|        - | 12988 | `/*` |
|        - | 12989 | ` * bool restore_error_handler(void)` |
|        - | 12990 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12991 | ` * Parameters:` |
|        - | 12992 | ` *  None.` |
|        - | 12993 | ` * Return` |
|        - | 12994 | ` *  Always TRUE.` |
|        - | 12995 | ` */` |
|        6 | 12996 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12997 |  |
|        7 | 12998 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12999 | `	ph7_value *pOld,*pNew;` |
|        - | 13000 | `	/* Point to the old and the new handler */` |
|        7 | 13001 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13002 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13003 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13004 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13005 | `		SXUNUSED(apArg);` |
|        - | 13006 | `		/* No installed callback,return FALSE */` |
|        7 | 13007 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13008 | `		return PH7_OK;` |
|        - | 13009 | `	}` |
|        - | 13010 | `	/* Copy the old callback */` |
|      ! 0 | 13011 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13012 | `	PH7_MemObjRelease(pOld);` |
|        - | 13013 | `	/* Return TRUE */` |
|      ! 0 | 13014 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13015 | `	return PH7_OK;` |
|        4 | 13016 |  |
|        - | 13017 | `/*` |
|        - | 13018 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13019 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13020 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13021 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13022 | ` *  Sets a user-defined error handler function.` |
|        - | 13023 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13024 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13025 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13026 | ` *  conditions (using trigger_error()).` |
|        - | 13027 | ` * Parameters` |
|        - | 13028 | ` *  $error_handler` |
|        - | 13029 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13030 | ` *   describing the error.` |
|        - | 13031 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13032 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13033 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13034 | ` *   The function can be shown as:` |
|        - | 13035 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13036 | ` *     errno` |
|        - | 13037 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13038 | ` *   errstr` |
|        - | 13039 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13040 | ` *   errfile` |
|        - | 13041 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13042 | ` *     was raised in, as a string.` |
|        - | 13043 | ` *  Note:` |
|        - | 13044 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13045 | ` * Return` |
|        - | 13046 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13047 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13048 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13049 | ` */` |
|    10592 | 13050 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13051 |  |
|    10594 | 13052 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13053 | `	ph7_value *pOld,*pNew;` |
|        - | 13054 | `	/* Point to the old and the new handler */` |
|    10594 | 13055 | `	pOld = &pVm->aErrCB[0];` |
|    10594 | 13056 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13057 | `	/* Return the old handler */` |
|    10594 | 13058 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10594 | 13059 | `	if( nArg > 0 ){` |
|    10594 | 13060 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13061 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5291 | 13062 | `			PH7_MemObjRelease(pNew);` |
|     5291 | 13063 | `			ph7_result_bool(pCtx,1);` |
|     2646 | 13064 | `		}else{` |
|     5304 | 13065 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13066 | `			/* Install the new handler */` |
|     5304 | 13067 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13068 | `		}` |
|     5296 | 13069 | `	}` |
|    10594 | 13070 | `	return PH7_OK;` |
|        2 | 13071 |  |
|        - | 13072 | `/*` |
|        - | 13073 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13074 | ` *  Generates a backtrace.` |
|        - | 13075 | ` * Paramaeter` |
|        - | 13076 | ` *  $options` |
|        - | 13077 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13078 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13079 | ` *   all the function/method arguments, to save memory.` |
|        - | 13080 | ` * $limit` |
|        - | 13081 | ` *   (Not Used)` |
|        - | 13082 | ` * Return` |
|        - | 13083 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13084 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13085 | ` *          Name        Type      Description` |
|        - | 13086 | ` *          ------      ------     -----------` |
|        - | 13087 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13088 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13089 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13090 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13091 | ` *          object      object    The current object.` |
|        - | 13092 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13093 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13094 | ` */` |
|      868 | 13095 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13096 |  |
|      870 | 13097 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13098 | `	ph7_value *pArray;` |
|        - | 13099 | `	ph7_class *pClass;` |
|        - | 13100 | `	ph7_value *pValue;` |
|        - | 13101 | `	SyString *pFile;` |
|        - | 13102 | `	/* Create a new array */` |
|      870 | 13103 | `	pArray = ph7_context_new_array(pCtx);` |
|      870 | 13104 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      870 | 13105 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13106 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13107 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13108 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13109 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13110 | `		SXUNUSED(apArg);` |
|      ! 0 | 13111 | `		return PH7_OK;` |
|        - | 13112 | `	}` |
|        - | 13113 | `	/* Dump running function name and it's arguments  */` |
|      870 | 13114 | `	if( pVm->pFrame->pParent ){` |
|      870 | 13115 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13116 | `		ph7_vm_func *pFunc;` |
|        - | 13117 | `		ph7_value *pArg;` |
|      870 | 13118 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      870 | 13119 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      870 | 13120 | `		if( pFrame->pParent && pFunc ){` |
|      870 | 13121 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      870 | 13122 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      870 | 13123 | `			ph7_value_reset_string_cursor(pValue);` |
|      434 | 13124 | `		}` |
|        - | 13125 | `		/* Function arguments */` |
|      870 | 13126 | `		pArg = ph7_context_new_array(pCtx);` |
|      870 | 13127 | `		if( pArg  ){` |
|        - | 13128 | `			ph7_value *pObj;` |
|        - | 13129 | `			VmSlot *aSlot;` |
|        - | 13130 | `			sxu32 n;` |
|        - | 13131 | `			/* Start filling the array with the given arguments */` |
|      870 | 13132 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3478 | 13133 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2610 | 13134 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2610 | 13135 | `				if( pObj ){` |
|     2610 | 13136 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1304 | 13137 | `				}` |
|     1306 | 13138 | `			}` |
|        - | 13139 | `			/* Save the array */` |
|      870 | 13140 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      434 | 13141 | `		}` |
|      434 | 13142 | `	}` |
|      870 | 13143 | `	ph7_value_int(pValue,1);` |
|        - | 13144 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13145 | `	 * line numbers at run-time. )` |
|        - | 13146 | `	 */` |
|      870 | 13147 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13148 | `	/* Current processed script */` |
|      870 | 13149 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      870 | 13150 | `	if( pFile ){` |
|      870 | 13151 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      870 | 13152 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      870 | 13153 | `		ph7_value_reset_string_cursor(pValue);` |
|      434 | 13154 | `	}` |
|        - | 13155 | `	/* Top class */` |
|      870 | 13156 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      870 | 13157 | `	if( pClass ){` |
|      866 | 13158 | `		ph7_value_reset_string_cursor(pValue);` |
|      866 | 13159 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      866 | 13160 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      432 | 13161 | `	}` |
|        - | 13162 | `	/* Return the freshly created array */` |
|      870 | 13163 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13164 | `	/*` |
|        - | 13165 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13166 | `	 * as soon we return from this function.` |
|        - | 13167 | `	 */` |
|      870 | 13168 | `	return PH7_OK;` |
|      436 | 13169 |  |
|        - | 13170 | `/*` |
|        - | 13171 | ` * Generate a small backtrace.` |
|        - | 13172 | ` * Store the generated dump in the given BLOB` |
|        - | 13173 | ` */` |
|        4 | 13174 | `static int VmMiniBacktrace(` |
|        - | 13175 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13176 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13177 | `	)` |
|        1 | 13178 |  |
|        5 | 13179 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13180 | `	ph7_vm_func *pFunc;` |
|        - | 13181 | `	ph7_class *pClass;` |
|        - | 13182 | `	SyString *pFile;` |
|        - | 13183 | `	/* Called function */` |
|        5 | 13184 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13185 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13186 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13187 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13188 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13189 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13190 | `	}else{` |
|      ! 0 | 13191 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13192 | `	}` |
|        5 | 13193 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13194 | `	/* Current processed script */` |
|        5 | 13195 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13196 | `	if( pFile ){` |
|        5 | 13197 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13198 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13199 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13200 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13201 | `	}` |
|        - | 13202 | `	/* Top class */` |
|        5 | 13203 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13204 | `	if( pClass ){` |
|      ! 0 | 13205 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13206 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13207 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13208 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13209 | `	}` |
|        5 | 13210 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13211 | `	/* All done */` |
|        5 | 13212 | `	return SXRET_OK;` |
|        1 | 13213 |  |
|        - | 13214 | `/*` |
|        - | 13215 | ` * void debug_print_backtrace()` |
|        - | 13216 | ` *  Prints a backtrace` |
|        - | 13217 | ` * Parameters` |
|        - | 13218 | ` * None` |
|        - | 13219 | ` * Return` |
|        - | 13220 | ` * NULL` |
|        - | 13221 | ` */` |
|        2 | 13222 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13223 |  |
|        3 | 13224 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13225 | `	SyBlob sDump;` |
|        3 | 13226 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13227 | `	/* Generate the backtrace */` |
|        3 | 13228 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13229 | `	/* Output backtrace */` |
|        3 | 13230 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13231 | `	/* All done,cleanup */` |
|        3 | 13232 | `	SyBlobRelease(&sDump);` |
|        1 | 13233 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13234 | `	SXUNUSED(apArg);` |
|        3 | 13235 | `	return PH7_OK;` |
|        1 | 13236 |  |
|        - | 13237 | `/*` |
|        - | 13238 | ` * string debug_string_backtrace()` |
|        - | 13239 | ` *  Generate a backtrace` |
|        - | 13240 | ` * Parameters` |
|        - | 13241 | ` * None` |
|        - | 13242 | ` * Return` |
|        - | 13243 | ` *  A mini backtrace().` |
|        - | 13244 | ` * Note that this is a symisc extension.` |
|        - | 13245 | ` */` |
|        2 | 13246 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13247 |  |
|        3 | 13248 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13249 | `	SyBlob sDump;` |
|        3 | 13250 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13251 | `	/* Generate the backtrace */` |
|        3 | 13252 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13253 | `	/* Return the backtrace */` |
|        3 | 13254 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13255 | `	/* All done,cleanup */` |
|        3 | 13256 | `	SyBlobRelease(&sDump);` |
|        1 | 13257 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13258 | `	SXUNUSED(apArg);` |
|        3 | 13259 | `	return PH7_OK;` |
|        1 | 13260 |  |
|        - | 13261 | `/*` |
|        - | 13262 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13263 | ` * exception is triggered.` |
|        - | 13264 | ` */` |
|      512 | 13265 | `static sxi32 VmUncaughtException(` |
|        - | 13266 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13267 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13268 | `	)` |
|        1 | 13269 |  |
|        - | 13270 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13271 | `	int nArg = 1;` |
|        - | 13272 | `	sxi32 rc;` |
|      513 | 13273 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13274 | `		/* Nesting limit reached */` |
|      ! 0 | 13275 | `		return SXRET_OK;` |
|        - | 13276 | `	}` |
|        - | 13277 | `	/* Call any exception handler if available */` |
|      513 | 13278 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13279 | `	if( pThis ){` |
|        - | 13280 | `		/* Load the exception instance */` |
|      513 | 13281 | `		sArg.x.pOther = pThis;` |
|      513 | 13282 | `		pThis->iRef++;` |
|      513 | 13283 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13284 | `	}else{` |
|      ! 0 | 13285 | `		nArg = 0;` |
|        - | 13286 | `	}` |
|      513 | 13287 | `	apArg[0] = &sArg;` |
|        - | 13288 | `	/* Call the exception handler if available */` |
|      513 | 13289 | `	pVm->nExceptDepth++;` |
|      513 | 13290 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13291 | `	pVm->nExceptDepth--;` |
|      513 | 13292 | `	if( rc != SXRET_OK ){` |
|        - | 13293 | `		SyBlob sMsgBuf;` |
|      511 | 13294 | `		const char *zClass = "Exception";` |
|      511 | 13295 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13296 | `		const char *zMsg;` |
|        - | 13297 | `		sxu32 nMsg;` |
|        - | 13298 | `		const char *zFuncName;` |
|        - | 13299 | `		int nFuncLen;` |
|      511 | 13300 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13301 | `		if( pThis ){` |
|        - | 13302 | `			ph7_class_method *pGetMessage;` |
|        - | 13303 | `			ph7_value sMsg;` |
|        - | 13304 | `			const char *zTmp;` |
|        - | 13305 | `			int nTmp;` |
|      511 | 13306 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13307 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13308 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13309 | `			if( pGetMessage ){` |
|      511 | 13310 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13311 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13312 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13313 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13314 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13315 | `					}` |
|      255 | 13316 | `				}` |
|      511 | 13317 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13318 | `			}` |
|      255 | 13319 | `		}` |
|      511 | 13320 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13321 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13322 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13323 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13324 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13325 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13326 | `		rc = SXERR_ABORT;` |
|      255 | 13327 | `	}` |
|      513 | 13328 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13329 | `	return rc;` |
|      257 | 13330 |  |
|        - | 13331 | `/*` |
|        - | 13332 | ` * Throw a user exception.` |
|        - | 13333 | ` *` |
|        - | 13334 | ` * Exception dispatch follows this sequence:` |
|        - | 13335 | ` *` |
|        - | 13336 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13337 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13338 | ` *` |
|        - | 13339 | ` * 2. If NO catch matches:` |
|        - | 13340 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13341 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13342 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13343 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13344 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13345 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13346 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13347 | ` *` |
|        - | 13348 | ` * 3. If a catch DOES match:` |
|        - | 13349 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13350 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13351 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13352 | ` *       finally block.` |
|        - | 13353 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13354 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13355 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13356 | ` *       in pPendingException (step 2c).` |
|        - | 13357 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13358 | ` *    d. Run finally (if present).` |
|        - | 13359 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13360 | ` *       that handlers are restored and finally has run.` |
|        - | 13361 | ` */` |
|      816 | 13362 | `static sxi32 VmThrowException(` |
|        - | 13363 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13364 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13365 | `	)` |
|        2 | 13366 |  |
|        - | 13367 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13368 | `	ph7_exception **apException;` |
|        - | 13369 | `	ph7_exception *pException;` |
|        - | 13370 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13371 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13372 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      818 | 13373 | `	VmCoalesceDisarm(pVm);` |
|        - | 13374 | `	/* Point to the stack of loaded exceptions */` |
|      818 | 13375 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      818 | 13376 | `	pException = 0;` |
|      818 | 13377 | `	pCatch = 0;` |
|      818 | 13378 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13379 | `		ph7_exception_block *aCatch;` |
|        - | 13380 | `		ph7_class *pClass;` |
|        - | 13381 | `		SyString *aNames;` |
|        - | 13382 | `		sxu32 nNames;` |
|        - | 13383 | `		int matched;` |
|        - | 13384 | `		sxu32 j,k;` |
|        - | 13385 | `		/* Locate the appropriate block to execute */` |
|      298 | 13386 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      298 | 13387 | `		(void)SySetPop(&pVm->aException);` |
|      298 | 13388 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      306 | 13389 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13390 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      304 | 13391 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      304 | 13392 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      304 | 13393 | `			matched = 0;` |
|      330 | 13394 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13395 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13396 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13397 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      322 | 13398 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      322 | 13399 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13400 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13401 | `					continue;` |
|        - | 13402 | `				}` |
|      322 | 13403 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      296 | 13404 | `					matched = 1;` |
|      296 | 13405 | `					break;` |
|        - | 13406 | `				}` |
|       14 | 13407 | `			}` |
|      304 | 13408 | `			if( matched ){` |
|        - | 13409 | `				/* Catch block found,break immediately */` |
|      296 | 13410 | `				pCatch = &aCatch[j];` |
|      296 | 13411 | `				break;` |
|        - | 13412 | `			}` |
|        5 | 13413 | `		}` |
|      148 | 13414 | `	}` |
|        - | 13415 | `	/* Execute the cached block if available */` |
|      818 | 13416 | `	if( pCatch == 0 ){` |
|        - | 13417 | `		sxi32 rc;` |
|        - | 13418 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13419 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13420 | `			pException->iFinallyDone = 1;` |
|        3 | 13421 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13422 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13423 | `				return SXERR_ABORT;` |
|        - | 13424 | `			}` |
|        1 | 13425 | `		}` |
|        - | 13426 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13427 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13428 | `			/* Re-throw to the outer handler */` |
|        3 | 13429 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13430 | `		}` |
|        - | 13431 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13432 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13433 | `		 * exception instead of reporting it uncaught.` |
|        - | 13434 | `		 */` |
|      522 | 13435 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13436 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13437 | `			 * by looking for a catch frame on the stack.` |
|        - | 13438 | `			 */` |
|      522 | 13439 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13440 | `			int inCatch = 0;` |
|     1050 | 13441 | `			while( pF ){` |
|      538 | 13442 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13443 | `					inCatch = 1;` |
|        9 | 13444 | `					break;` |
|        - | 13445 | `				}` |
|      529 | 13446 | `				pF = pF->pParent;` |
|        1 | 13447 | `			}` |
|      522 | 13448 | `			if( inCatch ){` |
|        - | 13449 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13450 | `				pThis->iRef++;` |
|        9 | 13451 | `				pVm->pPendingException = pThis;` |
|        9 | 13452 | `				return SXRET_OK;` |
|        - | 13453 | `			}` |
|      256 | 13454 | `		}` |
|        - | 13455 | `		/* Truly uncaught */` |
|      513 | 13456 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13457 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13458 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13459 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13460 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13461 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13462 | `			}` |
|      ! 0 | 13463 | `		}` |
|      513 | 13464 | `		return rc;` |
|      ! 0 | 13465 | `	}else{` |
|      296 | 13466 | `		VmFrame *pFrame = pVm->pFrame;` |
|      296 | 13467 | `		ph7_exception **apSaved = 0;` |
|        - | 13468 | `		sxu32 nSavedCount;` |
|        - | 13469 | `		sxi32 rc;` |
|      296 | 13470 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      296 | 13471 | `		if( pException->pFrame == pFrame ){` |
|      230 | 13472 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      114 | 13473 | `		}` |
|        - | 13474 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13475 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13476 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13477 | `		 */` |
|      296 | 13478 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      296 | 13479 | `		if( nSavedCount > 0 ){` |
|       16 | 13480 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13481 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13482 | `			if( apSaved ){` |
|       16 | 13483 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13484 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13485 | `				SySetReset(&pVm->aException);` |
|        5 | 13486 | `			}` |
|        5 | 13487 | `		}` |
|        - | 13488 | `		/* Create a private frame first */` |
|      296 | 13489 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      296 | 13490 | `		if( rc == SXRET_OK ){` |
|      296 | 13491 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      296 | 13492 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      296 | 13493 | `			if( pObj ){` |
|      296 | 13494 | `				pThis->iRef++;` |
|      296 | 13495 | `				pObj->x.pOther = pThis;` |
|      296 | 13496 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      147 | 13497 | `			}` |
|        - | 13498 | `			/* Execute the catch block */` |
|      296 | 13499 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13500 | `			/* Leave the frame */` |
|      296 | 13501 | `			VmLeaveFrame(&(*pVm));` |
|      147 | 13502 | `		}` |
|        - | 13503 | `		/* Restore the outer exception handlers */` |
|      296 | 13504 | `		if( apSaved ){` |
|        - | 13505 | `			sxu32 k;` |
|        - | 13506 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13507 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13508 | `			 * Restore the original outer entries.` |
|        - | 13509 | `			 */` |
|       11 | 13510 | `			SySetReset(&pVm->aException);` |
|       21 | 13511 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13512 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13513 | `			}` |
|       11 | 13514 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13515 | `		}` |
|        - | 13516 | `		/* Execute the finally block after catch */` |
|      296 | 13517 | `		if( pException->iHasFinally ){` |
|       16 | 13518 | `			pException->iFinallyDone = 1;` |
|        - | 13519 | `			{` |
|       16 | 13520 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13521 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13522 | `					return SXERR_ABORT;` |
|        - | 13523 | `				}` |
|        - | 13524 | `			}` |
|        7 | 13525 | `		}` |
|      296 | 13526 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13527 | `			return SXERR_ABORT;` |
|        - | 13528 | `		}` |
|        - | 13529 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13530 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13531 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13532 | `		 */` |
|      296 | 13533 | `		if( pVm->pPendingException ){` |
|        9 | 13534 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13535 | `			pVm->pPendingException = 0;` |
|        9 | 13536 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13537 | `		}` |
|        - | 13538 | `	}` |
|        - | 13539 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13540 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13541 | `	 */` |
|      288 | 13542 | `	return SXRET_OK;` |
|      410 | 13543 |  |
|        - | 13544 | `/*` |
|        - | 13545 | ` * Section:` |
|        - | 13546 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13547 | ` * Status:` |
|        - | 13548 | ` *    Stable.` |
|        - | 13549 | ` */` |
|        - | 13550 | `/*` |
|        - | 13551 | ` * string ph7version(void)` |
|        - | 13552 | ` *  Returns the running version of the PH7 version.` |
|        - | 13553 | ` * Parameters` |
|        - | 13554 | ` *  None` |
|        - | 13555 | ` * Return` |
|        - | 13556 | ` * Current PH7 version.` |
|        - | 13557 | ` */` |
|        2 | 13558 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13559 |  |
|        1 | 13560 | `	SXUNUSED(nArg);` |
|        1 | 13561 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13562 | `	/* Current engine version */` |
|        3 | 13563 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13564 | `	return PH7_OK;` |
|        1 | 13565 |  |
|        - | 13566 | `/*` |
|        - | 13567 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13568 | ` */` |
|        - | 13569 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13570 | ` "<html><head>"\` |
|        - | 13571 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13572 | ` "<style type=\"text/css\">"\` |
|        - | 13573 | ` "div {"\` |
|        - | 13574 | `     "border: 1px solid #cccccc;"\` |
|        - | 13575 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13576 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13577 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13578 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13579 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13580 | `     "-o-border-radius: 10px;"\` |
|        - | 13581 | `     "border-radius: 10px;"\` |
|        - | 13582 | `     "padding-left: 2em;"\` |
|        - | 13583 | `     "background-color: white;"\` |
|        - | 13584 | `     "margin-left: auto;"\` |
|        - | 13585 | `     "font-family: verdana;"\` |
|        - | 13586 | `     "padding-right: 2em;"\` |
|        - | 13587 | `     "margin-right: auto;"\` |
|        - | 13588 | `     "}"\` |
|        - | 13589 | `     "body {"\` |
|        - | 13590 | `     "padding: 0.2em;"\` |
|        - | 13591 | `     "font-style: normal;"\` |
|        - | 13592 | `     "font-size: medium;"\` |
|        - | 13593 | `     "background-color: #f2f2f2;"\` |
|        - | 13594 | `     "}"\` |
|        - | 13595 | `     "hr {"\` |
|        - | 13596 | `     "border-style: solid none none;"\` |
|        - | 13597 | `     "border-width: 1px medium medium;"\` |
|        - | 13598 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13599 | `     "height: 1px;"\` |
|        - | 13600 | `     "}"\` |
|        - | 13601 | `     "a {"\` |
|        - | 13602 | `     "color: #3366cc;"\` |
|        - | 13603 | `     "text-decoration: none;"\` |
|        - | 13604 | `     "}"\` |
|        - | 13605 | `     "a:hover {"\` |
|        - | 13606 | `     "color: #999999;"\` |
|        - | 13607 | `     "}"\` |
|        - | 13608 | `     "a:active {"\` |
|        - | 13609 | `     "color: #663399;"\` |
|        - | 13610 | `     "}"\` |
|        - | 13611 | `     "h1 {"\` |
|        - | 13612 | `     "margin: 0;"\` |
|        - | 13613 | `     "padding: 0;"\` |
|        - | 13614 | `     "font-family: Verdana;"\` |
|        - | 13615 | `     "font-weight: bold;"\` |
|        - | 13616 | `     "font-style: normal;"\` |
|        - | 13617 | `     "font-size: medium;"\` |
|        - | 13618 | `     "text-transform: capitalize;"\` |
|        - | 13619 | `     "color: #0a328c;"\` |
|        - | 13620 | `     "}"\` |
|        - | 13621 | `     "p {"\` |
|        - | 13622 | `     "margin: 0 auto;"\` |
|        - | 13623 | `     "font-size: medium;"\` |
|        - | 13624 | `     "font-style: normal;"\` |
|        - | 13625 | `     "font-family: verdana;"\` |
|        - | 13626 | `     "}"\` |
|        - | 13627 | `"</style></head><body>"\` |
|        - | 13628 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13629 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13630 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13631 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13632 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13633 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13634 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13635 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13636 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13637 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13638 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13639 |  |
|        - | 13640 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13641 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13642 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13643 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13644 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13645 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13646 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13647 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13648 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13649 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13650 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13651 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13652 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13653 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13654 |  |
|        - | 13655 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13656 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13657 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13658 | `"&nbsp;*<br>"\` |
|        - | 13659 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13660 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13661 | `"&nbsp;* are met:<br>"\` |
|        - | 13662 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13663 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13664 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13665 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13666 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13667 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13668 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13669 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13670 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13671 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13672 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13673 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13674 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13675 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13676 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13677 | `"&nbsp;*<br>"\` |
|        - | 13678 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13679 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13680 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13681 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13682 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13683 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13684 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13685 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13686 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13687 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13688 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13689 | `"&nbsp;*/<br>"\` |
|        - | 13690 | `"</span></small></small></p>"\` |
|        - | 13691 | `"</div></body></html>"` |
|        - | 13692 | `/*` |
|        - | 13693 | ` * bool ph7credits(void)` |
|        - | 13694 | ` * bool ph7info(void)` |
|        - | 13695 | ` * bool ph7copyright(void)` |
|        - | 13696 | ` *  Prints out the credits for PH7 engine` |
|        - | 13697 | ` * Parameters` |
|        - | 13698 | ` *  None` |
|        - | 13699 | ` * Return` |
|        - | 13700 | ` *  Always TRUE` |
|        - | 13701 | ` */` |
|        2 | 13702 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13703 |  |
|        3 | 13704 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13705 | `	/* Expand the HTML page above*/` |
|        3 | 13706 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13707 | `	ph7_context_output_format(` |
|        1 | 13708 | `		pCtx,` |
|        - | 13709 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13710 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13711 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13712 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13713 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13714 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13715 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13716 | `#ifdef __WINNT__` |
|        - | 13717 | `		"Windows NT"` |
|        - | 13718 | `#elif defined(__UNIXES__)` |
|        - | 13719 | `		"UNIX-Like"` |
|        - | 13720 | `#else` |
|        - | 13721 | `		"Other OS"` |
|        - | 13722 | `#endif` |
|        - | 13723 | `		);` |
|        3 | 13724 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13725 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13726 | `	SXUNUSED(apArg);` |
|        - | 13727 | `	/* Return TRUE */` |
|        - | 13728 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13729 | `	return PH7_OK;` |
|        1 | 13730 |  |
|        - | 13731 | `/*` |
|        - | 13732 | ` * Section:` |
|        - | 13733 | ` *    URL related routines.` |
|        - | 13734 | ` * Status:` |
|        - | 13735 | ` *    Stable.` |
|        - | 13736 | ` */` |
|        - | 13737 | `/*` |
|        - | 13738 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13739 | ` *  Parse a URL and return its fields.` |
|        - | 13740 | ` * Parameters` |
|        - | 13741 | ` *  $url` |
|        - | 13742 | ` *   The URL to parse.` |
|        - | 13743 | ` * $component` |
|        - | 13744 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13745 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13746 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13747 | ` *  in which case the return value will be an integer).` |
|        - | 13748 | ` * Return` |
|        - | 13749 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13750 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13751 | ` *  this array are:` |
|        - | 13752 | ` *   scheme - e.g. http` |
|        - | 13753 | ` *   host` |
|        - | 13754 | ` *   port` |
|        - | 13755 | ` *   user` |
|        - | 13756 | ` *   pass` |
|        - | 13757 | ` *   path` |
|        - | 13758 | ` *   query - after the question mark ?` |
|        - | 13759 | ` *   fragment - after the hashmark #` |
|        - | 13760 | ` * Note:` |
|        - | 13761 | ` *  FALSE is returned on failure.` |
|        - | 13762 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13763 | ` *  with the standard PHP engine.` |
|        - | 13764 | ` */` |
|       28 | 13765 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13766 |  |
|        - | 13767 | `	const char *zStr; /* Input string */` |
|        - | 13768 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13769 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13770 | `	int nLen;` |
|        - | 13771 | `	sxi32 rc;` |
|       29 | 13772 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13773 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13774 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13775 | `		return PH7_OK;` |
|        - | 13776 | `	}` |
|        - | 13777 | `	/* Extract the given URI */` |
|       29 | 13778 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13779 | `	if( nLen < 1 ){` |
|        - | 13780 | `		/* Nothing to process,return FALSE */` |
|        3 | 13781 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13782 | `		return PH7_OK;` |
|        - | 13783 | `	}` |
|        - | 13784 | `	/* Get a parse */` |
|       27 | 13785 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13786 | `	if( rc != SXRET_OK ){` |
|        - | 13787 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13788 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13789 | `		return PH7_OK;` |
|        - | 13790 | `	}` |
|       27 | 13791 | `	if( nArg > 1 ){` |
|      ! 0 | 13792 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13793 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13794 | `		switch(nComponent){` |
|      ! 0 | 13795 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13796 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13797 | `			if( pComp->nByte < 1 ){` |
|        - | 13798 | `				/* No available value,return NULL */` |
|      ! 0 | 13799 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13800 | `			}else{` |
|      ! 0 | 13801 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13802 | `			}` |
|      ! 0 | 13803 | `			break;` |
|      ! 0 | 13804 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13805 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13806 | `			if( pComp->nByte < 1 ){` |
|        - | 13807 | `				/* No available value,return NULL */` |
|      ! 0 | 13808 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13809 | `			}else{` |
|      ! 0 | 13810 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13811 | `			}` |
|      ! 0 | 13812 | `			break;` |
|      ! 0 | 13813 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13814 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13815 | `			if( pComp->nByte < 1 ){` |
|        - | 13816 | `				/* No available value,return NULL */` |
|      ! 0 | 13817 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13818 | `			}else{` |
|      ! 0 | 13819 | `				int iPort = 0;` |
|        - | 13820 | `				/* Cast the value to integer */` |
|      ! 0 | 13821 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13822 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13823 | `			}` |
|      ! 0 | 13824 | `			break;` |
|      ! 0 | 13825 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13826 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13827 | `			if( pComp->nByte < 1 ){` |
|        - | 13828 | `				/* No available value,return NULL */` |
|      ! 0 | 13829 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13830 | `			}else{` |
|      ! 0 | 13831 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13832 | `			}` |
|      ! 0 | 13833 | `			break;` |
|      ! 0 | 13834 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13835 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13836 | `			if( pComp->nByte < 1 ){` |
|        - | 13837 | `				/* No available value,return NULL */` |
|      ! 0 | 13838 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13839 | `			}else{` |
|      ! 0 | 13840 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13841 | `			}` |
|      ! 0 | 13842 | `			break;` |
|      ! 0 | 13843 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13844 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13845 | `			if( pComp->nByte < 1 ){` |
|        - | 13846 | `				/* No available value,return NULL */` |
|      ! 0 | 13847 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13848 | `			}else{` |
|      ! 0 | 13849 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13850 | `			}` |
|      ! 0 | 13851 | `			break;` |
|      ! 0 | 13852 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13853 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13854 | `			if( pComp->nByte < 1 ){` |
|        - | 13855 | `				/* No available value,return NULL */` |
|      ! 0 | 13856 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13857 | `			}else{` |
|      ! 0 | 13858 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13859 | `			}` |
|      ! 0 | 13860 | `			break;` |
|      ! 0 | 13861 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13862 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13863 | `			if( pComp->nByte < 1 ){` |
|        - | 13864 | `				/* No available value,return NULL */` |
|      ! 0 | 13865 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13866 | `			}else{` |
|      ! 0 | 13867 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13868 | `			}` |
|      ! 0 | 13869 | `			break;` |
|      ! 0 | 13870 | `		default:` |
|        - | 13871 | `			/* No such entry,return NULL */` |
|      ! 0 | 13872 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13873 | `			break;` |
|        - | 13874 | `		}` |
|      ! 0 | 13875 | `	}else{` |
|        - | 13876 | `		ph7_value *pArray,*pValue;` |
|        - | 13877 | `		/* Return an associative array */` |
|       27 | 13878 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13879 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13880 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13881 | `			/* Out of memory */` |
|      ! 0 | 13882 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13883 | `			/* Return false */` |
|      ! 0 | 13884 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13885 | `			return PH7_OK;` |
|        - | 13886 | `		}` |
|        - | 13887 | `		/* Fill the array */` |
|       27 | 13888 | `		pComp = &sURI.sScheme;` |
|       27 | 13889 | `		if( pComp->nByte > 0 ){` |
|       19 | 13890 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13891 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13892 | `		}` |
|        - | 13893 | `		/* Reset the string cursor */` |
|       27 | 13894 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13895 | `		pComp = &sURI.sHost;` |
|       27 | 13896 | `		if( pComp->nByte > 0 ){` |
|       25 | 13897 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13898 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13899 | `		}` |
|        - | 13900 | `		/* Reset the string cursor */` |
|       27 | 13901 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13902 | `		pComp = &sURI.sPort;` |
|       27 | 13903 | `		if( pComp->nByte > 0 ){` |
|       11 | 13904 | `			int iPort = 0;/* cc warning */` |
|        - | 13905 | `			/* Convert to integer */` |
|       11 | 13906 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13907 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13908 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13909 | `		}` |
|        - | 13910 | `		/* Reset the string cursor */` |
|       27 | 13911 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13912 | `		pComp = &sURI.sUser;` |
|       27 | 13913 | `		if( pComp->nByte > 0 ){` |
|        7 | 13914 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13915 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13916 | `		}` |
|        - | 13917 | `		/* Reset the string cursor */` |
|       27 | 13918 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13919 | `		pComp = &sURI.sPass;` |
|       27 | 13920 | `		if( pComp->nByte > 0 ){` |
|        7 | 13921 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13922 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13923 | `		}` |
|        - | 13924 | `		/* Reset the string cursor */` |
|       27 | 13925 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13926 | `		pComp = &sURI.sPath;` |
|       27 | 13927 | `		if( pComp->nByte > 0 ){` |
|       17 | 13928 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13929 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13930 | `		}` |
|        - | 13931 | `		/* Reset the string cursor */` |
|       27 | 13932 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13933 | `		pComp = &sURI.sQuery;` |
|       27 | 13934 | `		if( pComp->nByte > 0 ){` |
|        5 | 13935 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13936 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13937 | `		}` |
|        - | 13938 | `		/* Reset the string cursor */` |
|       27 | 13939 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13940 | `		pComp = &sURI.sFragment;` |
|       27 | 13941 | `		if( pComp->nByte > 0 ){` |
|        5 | 13942 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13943 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13944 | `		}` |
|        - | 13945 | `		/* Return the created array */` |
|       27 | 13946 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13947 | `		/* NOTE:` |
|        - | 13948 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13949 | `		 * automatically as soon we return from this function.` |
|        - | 13950 | `		 */` |
|        - | 13951 | `	}` |
|        - | 13952 | `	/* All done */` |
|       27 | 13953 | `	return PH7_OK;` |
|       15 | 13954 |  |
|        - | 13955 | `/*` |
|        - | 13956 | ` * Section:` |
|        - | 13957 | ` *   Array related routines.` |
|        - | 13958 | ` * Status:` |
|        - | 13959 | ` *    Stable.` |
|        - | 13960 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13961 | ` *  Array related functions that need access to the underlying` |
|        - | 13962 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13963 | ` */` |
|        - | 13964 | `/*` |
|        - | 13965 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13966 | ` * of the following structure.` |
|        - | 13967 | ` */` |
|        - | 13968 | `struct compact_data` |
|        - | 13969 |  |
|        - | 13970 | `	ph7_value *pArray;  /* Target array */` |
|        - | 13971 | `	int nRecCount;      /* Recursion count */` |
|        - | 13972 | `};` |
|        - | 13973 | `/*` |
|        - | 13974 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 13975 | ` */` |
|      ! 0 | 13976 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 13977 |  |
|      ! 0 | 13978 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 13979 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 13980 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13981 | `	/* Act according to the hashmap value */` |
|      ! 0 | 13982 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 13983 | `		SyString sVar;` |
|      ! 0 | 13984 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 13985 | `		if( sVar.nByte > 0 ){` |
|        - | 13986 | `			/* Query the current frame */` |
|      ! 0 | 13987 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 13988 | `			/* ^` |
|        - | 13989 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 13990 | `			 */` |
|      ! 0 | 13991 | `			if( pKey ){` |
|        - | 13992 | `				/* Perform the insertion */` |
|      ! 0 | 13993 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 13994 | `			}` |
|      ! 0 | 13995 | `		}` |
|      ! 0 | 13996 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 13997 | `		int rc;` |
|        - | 13998 | `		/* Recursively traverse this array */` |
|      ! 0 | 13999 | `		pData->nRecCount++;` |
|      ! 0 | 14000 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14001 | `		pData->nRecCount--;` |
|      ! 0 | 14002 | `		return rc;` |
|        - | 14003 | `	}` |
|      ! 0 | 14004 | `	return SXRET_OK;` |
|      ! 0 | 14005 |  |
|        - | 14006 | `/*` |
|        - | 14007 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14008 | ` *  Create array containing variables and their values.` |
|        - | 14009 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14010 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14011 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14012 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14013 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14014 | ` * Parameters` |
|        - | 14015 | ` *  $varname` |
|        - | 14016 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14017 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14018 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14019 | ` *   it recursively.` |
|        - | 14020 | ` * Return` |
|        - | 14021 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14022 | ` */` |
|        2 | 14023 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14024 |  |
|        - | 14025 | `	ph7_value *pArray,*pObj;` |
|        3 | 14026 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14027 | `	const char *zName;` |
|        - | 14028 | `	SyString sVar;` |
|        - | 14029 | `	int i,nLen;` |
|        3 | 14030 | `	if( nArg < 1 ){` |
|        - | 14031 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14032 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14033 | `		return PH7_OK;` |
|        - | 14034 | `	}` |
|        - | 14035 | `	/* Create the array */` |
|        3 | 14036 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14037 | `	if( pArray == 0 ){` |
|        - | 14038 | `		/* Out of memory */` |
|      ! 0 | 14039 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14040 | `		/* Return NULL */` |
|      ! 0 | 14041 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14042 | `		return PH7_OK;` |
|        - | 14043 | `	}` |
|        - | 14044 | `	/* Perform the requested operation */` |
|        7 | 14045 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14046 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14047 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14048 | `				struct compact_data sData;` |
|      ! 0 | 14049 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14050 | `				/* Recursively walk the array */` |
|      ! 0 | 14051 | `				sData.nRecCount = 0;` |
|      ! 0 | 14052 | `				sData.pArray = pArray;` |
|      ! 0 | 14053 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14054 | `			}` |
|      ! 0 | 14055 | `		}else{` |
|        - | 14056 | `			/* Extract variable name */` |
|        5 | 14057 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14058 | `			if( nLen > 0 ){` |
|        5 | 14059 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14060 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14061 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14062 | `				if( pObj ){` |
|        5 | 14063 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14064 | `				}` |
|        2 | 14065 | `			}` |
|        - | 14066 | `		}` |
|        3 | 14067 | `	}` |
|        - | 14068 | `	/* Return the array */` |
|        3 | 14069 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14070 | `	return PH7_OK;` |
|        2 | 14071 |  |
|        - | 14072 | `/*` |
|        - | 14073 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14074 | ` * of the following structure.` |
|        - | 14075 | ` */` |
|        - | 14076 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14077 | `struct extract_aux_data` |
|        - | 14078 |  |
|        - | 14079 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14080 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14081 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14082 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14083 | `	int iFlags;           /* Control flags */` |
|        - | 14084 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14085 | `};` |
|        - | 14086 | `/* Forward declaration */` |
|        - | 14087 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14088 | `/*` |
|        - | 14089 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14090 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14091 | ` * Parameters` |
|        - | 14092 | ` * $var_array` |
|        - | 14093 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14094 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14095 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14096 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14097 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14098 | ` * $extract_type` |
|        - | 14099 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14100 | ` *  It can be one of the following values:` |
|        - | 14101 | ` *   EXTR_OVERWRITE` |
|        - | 14102 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14103 | ` *   EXTR_SKIP` |
|        - | 14104 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14105 | ` *   EXTR_PREFIX_SAME` |
|        - | 14106 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14107 | ` *   EXTR_PREFIX_ALL` |
|        - | 14108 | ` *       Prefix all variable names with prefix.` |
|        - | 14109 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14110 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14111 | ` *   EXTR_IF_EXISTS` |
|        - | 14112 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14113 | ` *       otherwise do nothing.` |
|        - | 14114 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14115 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14116 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14117 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14118 | ` *      the current symbol table.` |
|        - | 14119 | ` * $prefix` |
|        - | 14120 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14121 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14122 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14123 | ` *  underscore character.` |
|        - | 14124 | ` * Return` |
|        - | 14125 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14126 | ` */` |
|        4 | 14127 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14128 |  |
|        - | 14129 | `	extract_aux_data sAux;` |
|        - | 14130 | `	ph7_hashmap *pMap;` |
|        5 | 14131 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14132 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14133 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14134 | `		return PH7_OK;` |
|        - | 14135 | `	}` |
|        - | 14136 | `	/* Point to the target hashmap */` |
|        5 | 14137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14138 | `	if( pMap->nEntry < 1 ){` |
|        - | 14139 | `		/* Empty map,return  0 */` |
|      ! 0 | 14140 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14141 | `		return PH7_OK;` |
|        - | 14142 | `	}` |
|        - | 14143 | `	/* Prepare the aux data */` |
|        5 | 14144 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14145 | `	if( nArg > 1 ){` |
|        3 | 14146 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14147 | `		if( nArg > 2 ){` |
|      ! 0 | 14148 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14149 | `		}` |
|        1 | 14150 | `	}` |
|        5 | 14151 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14152 | `	/* Invoke the worker callback */` |
|        5 | 14153 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14154 | `	/* Number of variables successfully imported */` |
|        5 | 14155 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14156 | `	return PH7_OK;` |
|        3 | 14157 |  |
|        - | 14158 | `/*` |
|        - | 14159 | ` * Worker callback for the [extract()] function defined` |
|        - | 14160 | ` * below.` |
|        - | 14161 | ` */` |
|        8 | 14162 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14163 |  |
|        9 | 14164 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14165 | `	int iFlags = pAux->iFlags;` |
|        9 | 14166 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14167 | `	ph7_value *pObj;` |
|        - | 14168 | `	SyString sVar;` |
|        9 | 14169 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14170 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14171 | `	}` |
|        - | 14172 | `	/* Perform a string cast */` |
|        9 | 14173 | `	PH7_MemObjToString(pKey);` |
|        9 | 14174 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14175 | `		/* Unavailable variable name */` |
|      ! 0 | 14176 | `		return SXRET_OK;` |
|        - | 14177 | `	}` |
|        9 | 14178 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14179 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14180 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14181 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14182 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14183 | `			);` |
|      ! 0 | 14184 | `	}else{` |
|       13 | 14185 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14186 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14187 | `	}` |
|        9 | 14188 | `	sVar.zString = pAux->zWorker;` |
|        - | 14189 | `	/* Try to extract the variable */` |
|        9 | 14190 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14191 | `	if( pObj ){` |
|        - | 14192 | `		/* Collision */` |
|        5 | 14193 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14194 | `			return SXRET_OK;` |
|        - | 14195 | `		}` |
|        5 | 14196 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14197 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14198 | `				/* Already prefixed */` |
|      ! 0 | 14199 | `				return SXRET_OK;` |
|        - | 14200 | `			}` |
|      ! 0 | 14201 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14202 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14203 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14204 | `				);` |
|      ! 0 | 14205 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14206 | `		}` |
|        3 | 14207 | `	}else{` |
|        - | 14208 | `		/* Create the variable */` |
|        5 | 14209 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14210 | `	}` |
|        9 | 14211 | `	if( pObj ){` |
|        - | 14212 | `		/* Overwrite the old value */` |
|        9 | 14213 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14214 | `		/* Increment counter */` |
|        9 | 14215 | `		pAux->iCount++;` |
|        4 | 14216 | `	}` |
|        9 | 14217 | `	return SXRET_OK;` |
|        5 | 14218 |  |
|        - | 14219 | `/*` |
|        - | 14220 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14221 | ` * defined below.` |
|        - | 14222 | ` */` |
|        2 | 14223 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14224 |  |
|        3 | 14225 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14226 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14227 | `	ph7_value *pObj;` |
|        - | 14228 | `	SyString sVar;` |
|        - | 14229 | `	/* Perform a string cast */` |
|        3 | 14230 | `	PH7_MemObjToString(pKey);` |
|        3 | 14231 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14232 | `		/* Unavailable variable name */` |
|      ! 0 | 14233 | `		return SXRET_OK;` |
|        - | 14234 | `	}` |
|        3 | 14235 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14236 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14237 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14238 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14239 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14240 | `			);` |
|        2 | 14241 | `	}else{` |
|      ! 0 | 14242 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14243 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14244 | `	}` |
|        3 | 14245 | `	sVar.zString = pAux->zWorker;` |
|        - | 14246 | `	/* Extract the variable */` |
|        3 | 14247 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14248 | `	if( pObj ){` |
|        3 | 14249 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14250 | `	}` |
|        3 | 14251 | `	return SXRET_OK;` |
|        2 | 14252 |  |
|        - | 14253 | `/*` |
|        - | 14254 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14255 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14256 | ` * Parameters` |
|        - | 14257 | ` * $types` |
|        - | 14258 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14259 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14260 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14261 | ` *  POST includes the POST uploaded file information.` |
|        - | 14262 | ` *  Note:` |
|        - | 14263 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14264 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14265 | ` * $prefix` |
|        - | 14266 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14267 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14268 | ` *  variable named $pref_userid.` |
|        - | 14269 | ` * Return` |
|        - | 14270 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14271 | ` */` |
|        2 | 14272 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14273 |  |
|        - | 14274 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14275 | `	extract_aux_data sAux;` |
|        - | 14276 | `	int nLen,nPrefixLen;` |
|        - | 14277 | `	ph7_value *pSuper;` |
|        - | 14278 | `	ph7_vm *pVm;` |
|        - | 14279 | `	/* By default import only $_GET variables  */` |
|        3 | 14280 | `	zImport = "G";` |
|        3 | 14281 | `	nLen = (int)sizeof(char);` |
|        3 | 14282 | `	zPrefix = 0;` |
|        3 | 14283 | `	nPrefixLen = 0;` |
|        3 | 14284 | `	if( nArg > 0 ){` |
|        3 | 14285 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14286 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14287 | `		}` |
|        3 | 14288 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14289 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14290 | `		}` |
|        1 | 14291 | `	}` |
|        - | 14292 | `	/* Point to the underlying VM */` |
|        3 | 14293 | `	pVm = pCtx->pVm;` |
|        - | 14294 | `	/* Initialize the aux data */` |
|        3 | 14295 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14296 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14297 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14298 | `	sAux.pVm = pVm;` |
|        - | 14299 | `	/* Extract */` |
|        3 | 14300 | `	zEnd = &zImport[nLen];` |
|        5 | 14301 | `	while( zImport < zEnd ){` |
|        3 | 14302 | `		int c = zImport[0];` |
|        3 | 14303 | `		pSuper = 0;` |
|        3 | 14304 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14305 | `			/* Import $_GET variables */` |
|        3 | 14306 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14307 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14308 | `			/* Import $_POST variables */` |
|      ! 0 | 14309 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14310 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14311 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14312 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14313 | `		}` |
|        3 | 14314 | `		if( pSuper ){` |
|        - | 14315 | `			/* Iterate throw array entries */` |
|        3 | 14316 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14317 | `		}` |
|        - | 14318 | `		/* Advance the cursor */` |
|        3 | 14319 | `		zImport++;` |
|        1 | 14320 | `	}` |
|        - | 14321 | `	/* All done,return TRUE*/` |
|        3 | 14322 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14323 | `	return PH7_OK;` |
|        1 | 14324 |  |
|        - | 14325 | `/*` |
|        - | 14326 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14327 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14328 | ` * information.` |
|        - | 14329 | ` */` |
|    12474 | 14330 | `static sxi32 VmEvalChunk(` |
|        - | 14331 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14332 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14333 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14334 | `	int iFlags,         /* Compile flag */` |
|        - | 14335 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14336 | `	)` |
|        2 | 14337 |  |
|        - | 14338 | `	SySet *pByteCode,aByteCode;` |
|        - | 14339 | `	SyBlob sSavedNs;` |
|    12476 | 14340 | `	ProcConsumer xErr = 0;` |
|    12476 | 14341 | `	void *pErrData = 0;` |
|        - | 14342 | `	/* Initialize bytecode container */` |
|    12476 | 14343 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12476 | 14344 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14345 | `	/* Reset the code generator */` |
|    12476 | 14346 | `	if( bTrueReturn ){` |
|        - | 14347 | `		/* Included file,log compile-time errors */` |
|     9334 | 14348 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9334 | 14349 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4666 | 14350 | `	}` |
|    12476 | 14351 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14352 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14353 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14354 | `	 * the caller's namespace is restored. */` |
|    12476 | 14355 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12476 | 14356 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12476 | 14357 | `	if( bTrueReturn ){` |
|        - | 14358 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9334 | 14359 | `		SyBlobReset(&pVm->sNamespace);` |
|     4666 | 14360 | `	}` |
|        - | 14361 | `	/* Swap bytecode container */` |
|    12476 | 14362 | `	pByteCode = pVm->pByteContainer;` |
|    12476 | 14363 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14364 | `	/* Compile the chunk */` |
|    12476 | 14365 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18713 | 14366 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14367 | `		/* Compilation error,return false */` |
|        3 | 14368 | `		if( pCtx ){` |
|        3 | 14369 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14370 | `		}` |
|        2 | 14371 | `	}else{` |
|        - | 14372 | `		/* Mount any newly defined classes */` |
|        - | 14373 | `		SyHashEntry *pEntry;` |
|        - | 14374 | `		ph7_class *pClass;` |
|        - | 14375 | `		ph7_value sResult; /* Return value */` |
|        - | 14376 | `		sxi32 rc;` |
|    12474 | 14377 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   721608 | 14378 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   702900 | 14379 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14380 | `			/* Only mount classes that haven't been mounted yet */` |
|   702900 | 14381 | `			if( !pClass->bMounted ){` |
|   189072 | 14382 | `				rc = VmMountUserClass(pVm,pClass);` |
|   189072 | 14383 | `				if( rc != SXRET_OK ){` |
|        - | 14384 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14385 | `					if( pCtx ){` |
|      ! 0 | 14386 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14387 | `					}` |
|      ! 0 | 14388 | `					goto Cleanup;` |
|        - | 14389 | `				}` |
|    94535 | 14390 | `			}` |
|        2 | 14391 | `		}` |
|    12474 | 14392 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14393 | `			/* Out of memory */` |
|      ! 0 | 14394 | `			if( pCtx ){` |
|      ! 0 | 14395 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14396 | `			}` |
|      ! 0 | 14397 | `			goto Cleanup;` |
|        - | 14398 | `		}` |
|    12474 | 14399 | `		if( bTrueReturn ){` |
|        - | 14400 | `			/* Assume a boolean true return value */` |
|     9334 | 14401 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4668 | 14402 | `		}else{` |
|        - | 14403 | `			/* Assume a null return value */` |
|     3142 | 14404 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14405 | `		}` |
|        - | 14406 | `		/* Execute the compiled chunk */` |
|    12474 | 14407 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12474 | 14408 | `		if( pCtx ){` |
|        - | 14409 | `			/* Set the execution result */` |
|     9352 | 14410 | `			ph7_result_value(pCtx,&sResult);` |
|     4675 | 14411 | `		}` |
|    12474 | 14412 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14413 | `	}` |
|     6237 | 14414 | `Cleanup:` |
|        - | 14415 | `	/* Cleanup the mess left behind */` |
|    12476 | 14416 | `	pVm->pByteContainer = pByteCode;` |
|    12476 | 14417 | `	SySetRelease(&aByteCode);` |
|        - | 14418 | `	/* Restore caller's namespace state */` |
|    12476 | 14419 | `	SyBlobReset(&pVm->sNamespace);` |
|    12476 | 14420 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12476 | 14421 | `	SyBlobRelease(&sSavedNs);` |
|    12476 | 14422 | `	return SXRET_OK;` |
|        2 | 14423 |  |
|        - | 14424 | `/*` |
|        - | 14425 | ` * value eval(string $code)` |
|        - | 14426 | ` *   Evaluate a string as PHP code.` |
|        - | 14427 | ` * Parameter` |
|        - | 14428 | ` *  code: PHP code to evaluate.` |
|        - | 14429 | ` * Return` |
|        - | 14430 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14431 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14432 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14433 | ` */` |
|       22 | 14434 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14435 |  |
|        - | 14436 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 14437 | `	if( nArg < 1 ){` |
|        - | 14438 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14439 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14440 | `		return SXRET_OK;` |
|        - | 14441 | `	}` |
|        - | 14442 | `	/* Chunk to evaluate */` |
|       24 | 14443 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 14444 | `	if( sChunk.nByte < 1 ){` |
|        - | 14445 | `		/* Empty string,return NULL */` |
|        3 | 14446 | `		ph7_result_null(pCtx);` |
|        3 | 14447 | `		return SXRET_OK;` |
|        - | 14448 | `	}` |
|        - | 14449 | `	/* Eval the chunk */` |
|       22 | 14450 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 14451 | `	return SXRET_OK;` |
|       13 | 14452 |  |
|        - | 14453 | `/*` |
|        - | 14454 | ` * Check if a file path is already included.` |
|        - | 14455 | ` */` |
|    18660 | 14456 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14457 |  |
|        - | 14458 | `	SyString *aEntries;` |
|        - | 14459 | `	sxu32 n;` |
|    18662 | 14460 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14461 | `	/* Perform a linear search */` |
| 86993766 | 14462 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 86975112 | 14463 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14464 | `			/* Already included */` |
|        7 | 14465 | `			return TRUE;` |
|        - | 14466 | `		}` |
| 43487554 | 14467 | `	}` |
|    18656 | 14468 | `	return FALSE;` |
|     9332 | 14469 |  |
|        - | 14470 | `/*` |
|        - | 14471 | ` * Push a file path in the appropriate VM container.` |
|        - | 14472 | ` */` |
|    21774 | 14473 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14474 |  |
|        - | 14475 | `	SyString sPath;` |
|        - | 14476 | `	char *zDup;` |
|        - | 14477 | `#ifdef __WINNT__` |
|        - | 14478 | `	char *zCur;` |
|        - | 14479 | `#endif` |
|        - | 14480 | `	sxi32 rc;` |
|    21776 | 14481 | `	if( nLen < 0 ){` |
|     3116 | 14482 | `		nLen = SyStrlen(zPath);` |
|     1557 | 14483 | `	}` |
|        - | 14484 | `	/* Duplicate the file path first */` |
|    21776 | 14485 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    21776 | 14486 | `	if( zDup == 0 ){` |
|      ! 0 | 14487 | `		return SXERR_MEM;` |
|        - | 14488 | `	}` |
|        - | 14489 | `#ifdef __WINNT__` |
|        - | 14490 | `	/* Normalize path on windows` |
|        - | 14491 | `	 * Example:` |
|        - | 14492 | `	 *    Path/To/File.php` |
|        - | 14493 | `	 * becomes` |
|        - | 14494 | `	 *   path\to\file.php` |
|        - | 14495 | `	 */` |
|        2 | 14496 | `	zCur = zDup;` |
|        2 | 14497 | `	while( zCur[0] != 0 ){` |
|        2 | 14498 | `		if( zCur[0] == '/' ){` |
|        2 | 14499 | `			zCur[0] = '\\';` |
|        2 | 14500 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14501 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14502 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14503 | `		}` |
|        2 | 14504 | `		zCur++;` |
|        2 | 14505 | `	}` |
|        - | 14506 | `#endif` |
|        - | 14507 | `	/* Install the file path */` |
|    21776 | 14508 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    21776 | 14509 | `	if( !bMain ){` |
|    18662 | 14510 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14511 | `			/* Already included */` |
|        7 | 14512 | `			*pNew = 0;` |
|        4 | 14513 | `		}else{` |
|        - | 14514 | `			/* Insert in the corresponding container */` |
|    18656 | 14515 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18656 | 14516 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14517 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14518 | `				return rc;` |
|        - | 14519 | `			}` |
|    18656 | 14520 | `			*pNew = 1;` |
|        - | 14521 | `		}` |
|     9330 | 14522 | `	}` |
|    21776 | 14523 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    21776 | 14524 | `	return SXRET_OK;` |
|    10889 | 14525 |  |
|        - | 14526 | `/*` |
|        - | 14527 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14528 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14529 | ` * indicates failure.` |
|        - | 14530 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14531 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14532 | ` * operations.` |
|        - | 14533 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14534 | ` * this function is a no-op.` |
|        - | 14535 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14536 | ` * constructs for more information.` |
|        - | 14537 | ` */` |
|     9342 | 14538 | `static sxi32 VmExecIncludedFile(` |
|        - | 14539 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14540 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14541 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14542 | `	 )` |
|        2 | 14543 |  |
|        - | 14544 | `	sxi32 rc;` |
|        - | 14545 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14546 | `	const ph7_io_stream *pStream;` |
|        - | 14547 | `	SyBlob sContents;` |
|        - | 14548 | `	void *pHandle;` |
|        - | 14549 | `	ph7_vm *pVm;` |
|        - | 14550 | `	int isNew;` |
|        - | 14551 | `	/* Initialize fields */` |
|     9344 | 14552 | `	pVm = pCtx->pVm;` |
|     9344 | 14553 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9344 | 14554 | `	isNew = 0;` |
|        - | 14555 | `	/* Extract the associated stream */` |
|     9344 | 14556 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14557 | `	/*` |
|        - | 14558 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14559 | `	 * in a read-only mode.` |
|        - | 14560 | `	 */` |
|     9344 | 14561 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9344 | 14562 | `	if( pHandle == 0 ){` |
|        8 | 14563 | `		return SXERR_IO;` |
|        - | 14564 | `	}` |
|     9338 | 14565 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9338 | 14566 | `	if( IncludeOnce && !isNew ){` |
|        - | 14567 | `		/* Already included */` |
|        5 | 14568 | `		rc = SXERR_EXISTS;` |
|        3 | 14569 | `	}else{` |
|        - | 14570 | `		/* Read the whole file contents */` |
|     9334 | 14571 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9334 | 14572 | `		if( rc == SXRET_OK ){` |
|        - | 14573 | `			SyString sScript;` |
|        - | 14574 | `			/* Compile and execute the script */` |
|     9334 | 14575 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9334 | 14576 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4666 | 14577 | `		}` |
|        - | 14578 | `	}` |
|        - | 14579 | `	/* Pop from the set of included file */` |
|     9338 | 14580 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14581 | `	/* Close the handle */` |
|     9338 | 14582 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14583 | `	/* Release the working buffer */` |
|     9338 | 14584 | `	SyBlobRelease(&sContents);` |
|        - | 14585 | `#else` |
|        - | 14586 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14587 | `	SXUNUSED(pPath);` |
|        - | 14588 | `	SXUNUSED(IncludeOnce);` |
|        - | 14589 | `	rc = SXERR_IO;` |
|        - | 14590 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9338 | 14591 | `	return rc;` |
|     4673 | 14592 |  |
|        - | 14593 | `/*` |
|        - | 14594 | ` * string get_include_path(void)` |
|        - | 14595 | ` *  Gets the current include_path configuration option.` |
|        - | 14596 | ` * Parameter` |
|        - | 14597 | ` *  None` |
|        - | 14598 | ` * Return` |
|        - | 14599 | ` *  Included paths as a string` |
|        - | 14600 | ` */` |
|        2 | 14601 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14602 |  |
|        3 | 14603 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14604 | `	SyString *aEntry;` |
|        - | 14605 | `	int dir_sep;` |
|        - | 14606 | `	sxu32 n;` |
|        - | 14607 | `#ifdef __WINNT__` |
|        1 | 14608 | `	dir_sep = ';';` |
|        - | 14609 | `#else` |
|        - | 14610 | `	/* Assume UNIX path separator */` |
|        2 | 14611 | `	dir_sep = ':';` |
|        - | 14612 | `#endif` |
|        1 | 14613 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14614 | `	SXUNUSED(apArg);` |
|        - | 14615 | `	/* Point to the list of import paths */` |
|        3 | 14616 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14617 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14618 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14619 | `		if( n > 0 ){` |
|        - | 14620 | `			/* Append dir seprator */` |
|      ! 0 | 14621 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14622 | `		}` |
|        - | 14623 | `		/* Append path */` |
|        3 | 14624 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14625 | `	}` |
|        3 | 14626 | `	return PH7_OK;` |
|        1 | 14627 |  |
|        - | 14628 | `/*` |
|        - | 14629 | ` * string get_get_included_files(void)` |
|        - | 14630 | ` *  Gets the current include_path configuration option.` |
|        - | 14631 | ` * Parameter` |
|        - | 14632 | ` *  None` |
|        - | 14633 | ` * Return` |
|        - | 14634 | ` *  Included paths as a string` |
|        - | 14635 | ` */` |
|        2 | 14636 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14637 |  |
|        3 | 14638 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14639 | `	ph7_value *pArray,*pWorker;` |
|        - | 14640 | `	SyString *pEntry;` |
|        - | 14641 | `	int c,d;` |
|        - | 14642 | `	/* Create an array and a working value */` |
|        3 | 14643 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14644 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14645 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14646 | `		/* Out of memory,return null */` |
|      ! 0 | 14647 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14648 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14649 | `		SXUNUSED(apArg);` |
|      ! 0 | 14650 | `		return PH7_OK;` |
|        - | 14651 | `	}` |
|        3 | 14652 | `	c = d = '/';` |
|        - | 14653 | `#ifdef __WINNT__` |
|        1 | 14654 | `	d = '\\';` |
|        - | 14655 | `#endif` |
|        - | 14656 | `	/* Iterate throw entries */` |
|        3 | 14657 | `	SySetResetCursor(pFiles);` |
|     3839 | 14658 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14659 | `		const char *zBase,*zEnd;` |
|        - | 14660 | `		int iLen;` |
|        - | 14661 | `		/* reset the string cursor */` |
|     3837 | 14662 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14663 | `		/* Extract base name */` |
|     3837 | 14664 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14665 | `		/* Ignore trailing '/' */` |
|     5755 | 14666 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14667 | `			zEnd--;` |
|      ! 0 | 14668 | `		}` |
|     3837 | 14669 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 14670 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 14671 | `			zEnd--;` |
|        1 | 14672 | `		}` |
|     3837 | 14673 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 14674 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14675 | `		/* Copy entry name */` |
|     3837 | 14676 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14677 | `		/* Perform the insertion */` |
|     3837 | 14678 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14679 | `	}` |
|        - | 14680 | `	/* All done,return the created array */` |
|        3 | 14681 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14682 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14683 | `	 * by the engine as soon we return from this foreign` |
|        - | 14684 | `	 * function.` |
|        - | 14685 | `	 */` |
|        3 | 14686 | `	return PH7_OK;` |
|        2 | 14687 |  |
|        - | 14688 | `/*` |
|        - | 14689 | ` * include:` |
|        - | 14690 | ` * According to the PHP reference manual.` |
|        - | 14691 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14692 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14693 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14694 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14695 | ` *  and the current working directory before failing. The include()` |
|        - | 14696 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14697 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14698 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14699 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14700 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14701 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14702 | ` *  directory to find the requested file.` |
|        - | 14703 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14704 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14705 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14706 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14707 | ` */` |
|     9324 | 14708 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14709 |  |
|        - | 14710 | `	SyString sFile;` |
|        - | 14711 | `	sxi32 rc;` |
|     9326 | 14712 | `	if( nArg < 1 ){` |
|        - | 14713 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14714 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14715 | `		return SXRET_OK;` |
|        - | 14716 | `	}` |
|        - | 14717 | `	/* File to include */` |
|     9326 | 14718 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9326 | 14719 | `	if( sFile.nByte < 1 ){` |
|        - | 14720 | `		/* Empty string,return NULL */` |
|      ! 0 | 14721 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14722 | `		return SXRET_OK;` |
|        - | 14723 | `	}` |
|        - | 14724 | `	/* Open,compile and execute the desired script */` |
|     9326 | 14725 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9326 | 14726 | `	if( rc != SXRET_OK ){` |
|        - | 14727 | `		/* Emit a warning and return false */` |
|        3 | 14728 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14729 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14730 | `	}` |
|     9326 | 14731 | `	return SXRET_OK;` |
|     4664 | 14732 |  |
|        - | 14733 | `/*` |
|        - | 14734 | ` * include_once:` |
|        - | 14735 | ` *  According to the PHP reference manual.` |
|        - | 14736 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14737 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14738 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14739 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14740 | ` *   just once.` |
|        - | 14741 | ` */` |
|        4 | 14742 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14743 |  |
|        - | 14744 | `	SyString sFile;` |
|        - | 14745 | `	sxi32 rc;` |
|        5 | 14746 | `	if( nArg < 1 ){` |
|        - | 14747 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14748 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14749 | `		return SXRET_OK;` |
|        - | 14750 | `	}` |
|        - | 14751 | `	/* File to include */` |
|        5 | 14752 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14753 | `	if( sFile.nByte < 1 ){` |
|        - | 14754 | `		/* Empty string,return NULL */` |
|      ! 0 | 14755 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14756 | `		return SXRET_OK;` |
|        - | 14757 | `	}` |
|        - | 14758 | `	/* Open,compile and execute the desired script */` |
|        5 | 14759 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14760 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14761 | `		/* File already included,return TRUE */` |
|        3 | 14762 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14763 | `		return SXRET_OK;` |
|        - | 14764 | `	}` |
|        3 | 14765 | `	if( rc != SXRET_OK ){` |
|        - | 14766 | `		/* Emit a warning and return false */` |
|      ! 0 | 14767 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14768 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14769 | ` 	}` |
|        3 | 14770 | `	return SXRET_OK;` |
|        3 | 14771 |  |
|        - | 14772 | `/*` |
|        - | 14773 | ` * require.` |
|        - | 14774 | ` *  According to the PHP reference manual.` |
|        - | 14775 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14776 | ` *   also produce a fatal level error.` |
|        - | 14777 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14778 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14779 | ` */` |
|        6 | 14780 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14781 |  |
|        - | 14782 | `	SyString sFile;` |
|        - | 14783 | `	sxi32 rc;` |
|        8 | 14784 | `	if( nArg < 1 ){` |
|        - | 14785 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14786 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14787 | `		return SXRET_OK;` |
|        - | 14788 | `	}` |
|        - | 14789 | `	/* File to include */` |
|        8 | 14790 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14791 | `	if( sFile.nByte < 1 ){` |
|        - | 14792 | `		/* Empty string,return NULL */` |
|      ! 0 | 14793 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14794 | `		return SXRET_OK;` |
|        - | 14795 | `	}` |
|        - | 14796 | `	/* Open,compile and execute the desired script */` |
|        8 | 14797 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14798 | `	if( rc != SXRET_OK ){` |
|        - | 14799 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14800 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14801 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14802 | `		return PH7_ABORT;` |
|        - | 14803 | `	}` |
|        8 | 14804 | `	return SXRET_OK;` |
|        5 | 14805 |  |
|        - | 14806 | `/*` |
|        - | 14807 | ` * require_once:` |
|        - | 14808 | ` *  According to the PHP reference manual.` |
|        - | 14809 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14810 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14811 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14812 | ` *   and how it differs from its non _once siblings.` |
|        - | 14813 | ` */` |
|        4 | 14814 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14815 |  |
|        - | 14816 | `	SyString sFile;` |
|        - | 14817 | `	sxi32 rc;` |
|        5 | 14818 | `	if( nArg < 1 ){` |
|        - | 14819 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14820 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14821 | `		return SXRET_OK;` |
|        - | 14822 | `	}` |
|        - | 14823 | `	/* File to include */` |
|        5 | 14824 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14825 | `	if( sFile.nByte < 1 ){` |
|        - | 14826 | `		/* Empty string,return NULL */` |
|      ! 0 | 14827 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14828 | `		return SXRET_OK;` |
|        - | 14829 | `	}` |
|        - | 14830 | `	/* Open,compile and execute the desired script */` |
|        5 | 14831 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14832 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14833 | `		/* File already included,return TRUE */` |
|        3 | 14834 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14835 | `		return SXRET_OK;` |
|        - | 14836 | `	}` |
|        3 | 14837 | `	if( rc != SXRET_OK ){` |
|        - | 14838 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14839 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14840 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14841 | `		return PH7_ABORT;` |
|        - | 14842 | `	}` |
|        3 | 14843 | `	return SXRET_OK;` |
|        3 | 14844 |  |
|        - | 14845 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 14846 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 14847 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 14848 | `/*` |
|        - | 14849 | ` * Section:` |
|        - | 14850 | ` *  SPL Autoloading functions.` |
|        - | 14851 | ` * Status:` |
|        - | 14852 | ` *  Stable.` |
|        - | 14853 | ` */` |
|        - | 14854 | `/*` |
|        - | 14855 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14856 | ` *  Register given function as __autoload() implementation.` |
|        - | 14857 | ` * Parameters` |
|        - | 14858 | ` *  callback` |
|        - | 14859 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14860 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14861 | ` *  throw` |
|        - | 14862 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14863 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14864 | ` *  prepend` |
|        - | 14865 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14866 | ` *   autoload stack instead of appending it.` |
|        - | 14867 | ` * Return` |
|        - | 14868 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14869 | ` */` |
|       34 | 14870 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14871 |  |
|        - | 14872 | `	VmAutoloadCB sEntry;` |
|       36 | 14873 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14874 | `	int iPrepend = 0;` |
|        - | 14875 | `	sxu32 n;` |
|       36 | 14876 | `	if( nArg < 1 ){` |
|        - | 14877 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14878 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14879 | `		/* Check for duplicates first */` |
|        9 | 14880 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14881 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14882 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14883 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14884 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14885 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14886 | `				return SXRET_OK;` |
|        - | 14887 | `			}` |
|      ! 0 | 14888 | `		}` |
|        5 | 14889 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14890 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14891 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14892 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14893 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14894 | `		return SXRET_OK;` |
|        - | 14895 | `	}` |
|        - | 14896 | `	/* Validate that the callback is callable */` |
|       28 | 14897 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14898 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14899 | `		if( nArg >= 2 ){` |
|      ! 0 | 14900 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14901 | `		}` |
|      ! 0 | 14902 | `		if( iThrow ){` |
|      ! 0 | 14903 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14904 | `				"Argument is not callable");` |
|      ! 0 | 14905 | `		}` |
|      ! 0 | 14906 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14907 | `		return SXRET_OK;` |
|        - | 14908 | `	}` |
|        - | 14909 | `	/* Check for duplicates */` |
|       46 | 14910 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14911 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14912 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14913 | `			/* Already registered */` |
|      ! 0 | 14914 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14915 | `			return SXRET_OK;` |
|        - | 14916 | `		}` |
|       11 | 14917 | `	}` |
|        - | 14918 | `	/* Check prepend flag */` |
|       28 | 14919 | `	if( nArg >= 3 ){` |
|        3 | 14920 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14921 | `	}` |
|        - | 14922 | `	/* Store the callback */` |
|       28 | 14923 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14924 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14925 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14926 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14927 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14928 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14929 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14930 | `		VmAutoloadCB *aBase;` |
|        3 | 14931 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14932 | `		/* Rotate: move last entry to front */` |
|        3 | 14933 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14934 | `		if( aBase ){` |
|        - | 14935 | `			VmAutoloadCB sTemp;` |
|        - | 14936 | `			sxu32 i;` |
|        3 | 14937 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14938 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14939 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14940 | `			}` |
|        3 | 14941 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14942 | `		}` |
|        2 | 14943 | `	}else{` |
|       26 | 14944 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14945 | `	}` |
|       28 | 14946 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14947 | `	return SXRET_OK;` |
|       19 | 14948 |  |
|        - | 14949 | `/*` |
|        - | 14950 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14951 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14952 | ` * Parameters` |
|        - | 14953 | ` *  callback` |
|        - | 14954 | ` *   The autoload function being unregistered.` |
|        - | 14955 | ` * Return` |
|        - | 14956 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14957 | ` */` |
|       32 | 14958 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14959 |  |
|       34 | 14960 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14961 | `	sxu32 n,nEntry;` |
|       34 | 14962 | `	if( nArg < 1 ){` |
|      ! 0 | 14963 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14964 | `		return SXRET_OK;` |
|        - | 14965 | `	}` |
|       34 | 14966 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14967 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14968 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14969 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14970 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 14971 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 14972 | `			sxu32 i;` |
|       32 | 14973 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 14974 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 14975 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 14976 | `			}` |
|        - | 14977 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 14978 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 14979 | `			ph7_result_bool(pCtx,1);` |
|       32 | 14980 | `			return SXRET_OK;` |
|        - | 14981 | `		}` |
|        3 | 14982 | `	}` |
|        3 | 14983 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14984 | `	return SXRET_OK;` |
|       18 | 14985 |  |
|        - | 14986 | `/*` |
|        - | 14987 | ` * array spl_autoload_functions(void)` |
|        - | 14988 | ` *  Return all registered __autoload() functions.` |
|        - | 14989 | ` * Return` |
|        - | 14990 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 14991 | ` *  an empty array is returned.` |
|        - | 14992 | ` */` |
|       20 | 14993 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14994 |  |
|       21 | 14995 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14996 | `	ph7_value *pArray;` |
|        - | 14997 | `	sxu32 n,nEntry;` |
|       10 | 14998 | `	SXUNUSED(nArg);` |
|       10 | 14999 | `	SXUNUSED(apArg);` |
|       21 | 15000 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15001 | `	if( pArray == 0 ){` |
|      ! 0 | 15002 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15003 | `		return SXRET_OK;` |
|        - | 15004 | `	}` |
|       21 | 15005 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15006 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15007 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15008 | `		if( pEntry ){` |
|       15 | 15009 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15010 | `		}` |
|        8 | 15011 | `	}` |
|       21 | 15012 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15013 | `	return SXRET_OK;` |
|       11 | 15014 |  |
|        - | 15015 | `/*` |
|        - | 15016 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15017 | ` *  Default implementation of __autoload().` |
|        - | 15018 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15019 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15020 | ` * Parameters` |
|        - | 15021 | ` *  class` |
|        - | 15022 | ` *   The class name being searched.` |
|        - | 15023 | ` *  file_extensions` |
|        - | 15024 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15025 | ` */` |
|        2 | 15026 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15027 |  |
|        - | 15028 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15029 | `	SyBlob sPath;` |
|        - | 15030 | `	int nClass;` |
|        - | 15031 | `	sxi32 rc;` |
|        3 | 15032 | `	if( nArg < 1 ){` |
|      ! 0 | 15033 | `		return SXRET_OK;` |
|        - | 15034 | `	}` |
|        3 | 15035 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15036 | `	if( nClass < 1 ){` |
|      ! 0 | 15037 | `		return SXRET_OK;` |
|        - | 15038 | `	}` |
|        - | 15039 | `	/* Default extensions */` |
|        3 | 15040 | `	zExt = ".php,.inc";` |
|        3 | 15041 | `	if( nArg >= 2 ){` |
|        - | 15042 | `		int nExt;` |
|      ! 0 | 15043 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15044 | `		if( nExt < 1 ){` |
|      ! 0 | 15045 | `			zExt = ".php,.inc";` |
|      ! 0 | 15046 | `		}` |
|      ! 0 | 15047 | `	}` |
|        3 | 15048 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15049 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15050 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15051 | `	zCur = zExt;` |
|        7 | 15052 | `	while( zCur < zEnd ){` |
|        - | 15053 | `		const char *zComma;` |
|        - | 15054 | `		SyString sFile;` |
|        - | 15055 | `		int i;` |
|        - | 15056 | `		/* Find next comma or end */` |
|        5 | 15057 | `		zComma = zCur;` |
|       21 | 15058 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15059 | `			zComma++;` |
|        1 | 15060 | `		}` |
|        - | 15061 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15062 | `		SyBlobReset(&sPath);` |
|       69 | 15063 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15064 | `			char c = zClass[i];` |
|       65 | 15065 | `			if( c == '\\' ){` |
|      ! 0 | 15066 | `				c = '/';` |
|       65 | 15067 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15068 | `				c = c + ('a' - 'A');` |
|        6 | 15069 | `			}` |
|       65 | 15070 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15071 | `		}` |
|        - | 15072 | `		/* Append extension */` |
|        5 | 15073 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15074 | `		/* Try to include the file */` |
|        5 | 15075 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15076 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15077 | `		if( rc == SXRET_OK ){` |
|        - | 15078 | `			/* File included successfully */` |
|      ! 0 | 15079 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15080 | `			return SXRET_OK;` |
|        - | 15081 | `		}` |
|        - | 15082 | `		/* Move past the comma */` |
|        5 | 15083 | `		zCur = zComma;` |
|        5 | 15084 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15085 | `			zCur++;` |
|        1 | 15086 | `		}` |
|        1 | 15087 | `	}` |
|        3 | 15088 | `	SyBlobRelease(&sPath);` |
|        3 | 15089 | `	return SXRET_OK;` |
|        2 | 15090 |  |
|        - | 15091 | `/* Table of built-in VM functions. */` |
|        - | 15092 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15093 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15094 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15095 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15096 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15097 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15098 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15099 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15100 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15101 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15102 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15103 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15104 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15105 | `	    /* Constants management */` |
|        - | 15106 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15107 | `	{ "define",   vm_builtin_define               },` |
|        - | 15108 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15109 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15110 | `	   /* Class/Object functions */` |
|        - | 15111 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15112 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15113 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15114 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15115 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15116 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15117 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15118 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15119 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15120 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15121 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15122 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15123 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15124 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15125 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15126 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15127 | `	   /* SPL Autoloading */` |
|        - | 15128 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15129 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15130 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15131 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15132 | `	   /* Random numbers/strings generators */` |
|        - | 15133 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15134 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15135 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15136 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15137 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15138 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15139 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15140 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15141 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15142 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15143 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15144 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15145 | `	   /* Language constructs functions */` |
|        - | 15146 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15147 | `	{ "print", vm_builtin_print                   },` |
|        - | 15148 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15149 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15150 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15151 | `	  /* Variable handling functions */` |
|        - | 15152 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15153 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15154 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15155 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15156 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15157 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15158 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15159 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15160 | `	  /* Ouput control functions */` |
|        - | 15161 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15162 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15163 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15164 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15165 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15166 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15167 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15168 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15169 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15170 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15171 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15172 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15173 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15174 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15175 | `	  /* Assertion functions */` |
|        - | 15176 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15177 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15178 | `	  /* Error reporting functions */` |
|        - | 15179 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15180 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15181 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15182 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15183 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15184 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15185 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15186 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15187 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15188 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15189 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15190 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15191 | `	  /* Release info */` |
|        - | 15192 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15193 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15194 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15195 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15196 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15197 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15198 | `	  /* hashmap */` |
|        - | 15199 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15200 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15201 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15202 | `	  /* URL related function */` |
|        - | 15203 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15204 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15205 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15206 | `	   /* XML processing functions */` |
|        - | 15207 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15208 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15209 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15210 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15211 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15212 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15213 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15214 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15215 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15216 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15217 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15218 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15219 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15220 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15221 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15222 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15223 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15224 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15225 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15226 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15227 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15228 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15229 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15230 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15231 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15232 | `	   /* Command line processing */` |
|        - | 15233 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15234 | `	   /* JSON encoding/decoding */` |
|        - | 15235 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15236 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15237 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15238 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15239 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15240 | `	   /* Files/URI inclusion facility */` |
|        - | 15241 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15242 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15243 | `	{ "include",      vm_builtin_include          },` |
|        - | 15244 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15245 | `	{ "require",      vm_builtin_require          },` |
|        - | 15246 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15247 | `};` |
|        - | 15248 | `/*` |
|        - | 15249 | ` * Register the built-in VM functions defined above.` |
|        - | 15250 | ` */` |
|     2808 | 15251 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15252 |  |
|        - | 15253 | `	sxi32 rc;` |
|        - | 15254 | `	sxu32 n;` |
|   367850 | 15255 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15256 | `		/* Note that these special functions have access` |
|        - | 15257 | `		 * to the underlying virtual machine as their` |
|        - | 15258 | `		 * private data.` |
|        - | 15259 | `		 */` |
|   365042 | 15260 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   365042 | 15261 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15262 | `			return rc;` |
|        - | 15263 | `		}` |
|   182522 | 15264 | `	}` |
|     2810 | 15265 | `	return SXRET_OK;` |
|     1406 | 15266 |  |
|        - | 15267 | `/*` |
|        - | 15268 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15269 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15270 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15271 | ` */` |
|    96832 | 15272 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15273 |  |
|    96834 | 15274 | `	if( !iLoadable ){` |
|    94850 | 15275 | `		return pClass;` |
|        - | 15276 | `	}` |
|     1990 | 15277 | `	while(pClass){` |
|     1986 | 15278 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1982 | 15279 | `			return pClass;` |
|        - | 15280 | `		}` |
|        5 | 15281 | `		pClass = pClass->pNextName;` |
|        1 | 15282 | `	}` |
|        5 | 15283 | `	return 0;` |
|    48418 | 15284 |  |
|        - | 15285 | `/*` |
|        - | 15286 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15287 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15288 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15289 | ` * registered in the VM's class table.` |
|        - | 15290 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15291 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15292 | ` */` |
|       38 | 15293 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15294 |  |
|        - | 15295 | `	VmAutoloadCB *pEntry;` |
|        - | 15296 | `	ph7_value sArg,sResult;` |
|        - | 15297 | `	SyHashEntry *pHashEntry;` |
|        - | 15298 | `	ph7_class *pClass;` |
|        - | 15299 | `	sxu32 n,nEntry;` |
|       40 | 15300 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15301 | `	if( nEntry < 1 ){` |
|       26 | 15302 | `		return 0;` |
|        - | 15303 | `	}` |
|        - | 15304 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15305 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15306 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15307 | `	}` |
|        - | 15308 | `	/* Mark this class as being autoloaded */` |
|       14 | 15309 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15310 | `	/* Prepare the class name argument */` |
|       14 | 15311 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15312 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15313 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15314 | `	pClass = 0;` |
|       28 | 15315 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15316 | `		ph7_value *apArg[1];` |
|       24 | 15317 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15318 | `		if( pEntry == 0 ){` |
|      ! 0 | 15319 | `			continue;` |
|        - | 15320 | `		}` |
|       24 | 15321 | `		apArg[0] = &sArg;` |
|       24 | 15322 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15323 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15324 | `			continue;` |
|        - | 15325 | `		}` |
|        - | 15326 | `		/* Check if the class is now available */` |
|       24 | 15327 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15328 | `		if( pHashEntry ){` |
|       10 | 15329 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15330 | `			if( pClass ){` |
|       10 | 15331 | `				break;` |
|        - | 15332 | `			}` |
|      ! 0 | 15333 | `		}` |
|        9 | 15334 | `	}` |
|       14 | 15335 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15336 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15337 | `	/* Remove reentrancy guard */` |
|       14 | 15338 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15339 | `	return pClass;` |
|       21 | 15340 |  |
|        - | 15341 | `/*` |
|        - | 15342 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15343 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15344 | ` */` |
|       18 | 15345 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15346 |  |
|       20 | 15347 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15348 |  |
|        - | 15349 | `/*` |
|        - | 15350 | ` * Check if the given name refer to an installed class.` |
|        - | 15351 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15352 | ` */` |
|    96844 | 15353 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15354 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15355 | `	const char *zName,  /* Name of the target class */` |
|        - | 15356 | `	sxu32 nByte,        /* zName length */` |
|        - | 15357 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15358 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15359 | `						 */` |
|        - | 15360 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15361 | `	)` |
|        2 | 15362 |  |
|        - | 15363 | `	SyHashEntry *pEntry;` |
|        - | 15364 | `	ph7_class *pClass;` |
|    48422 | 15365 | `	SXUNUSED(iNest);` |
|        - | 15366 | `	/* Exact class lookup.` |
|        - | 15367 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15368 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    96846 | 15369 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    96846 | 15370 | `	if( pEntry == 0 ){` |
|        - | 15371 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15372 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15373 | `	}` |
|    96826 | 15374 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    96826 | 15375 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    48424 | 15376 |  |
|        - | 15377 | `/*` |
|        - | 15378 | ` * Reference Table Implementation` |
|        - | 15379 | ` * Status: stable <chm@symisc.net>` |
|        - | 15380 | ` * Intro` |
|        - | 15381 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15382 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15383 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15384 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15385 | ` *  Refer to the official for more information on this powerful` |
|        - | 15386 | ` *  extension.` |
|        - | 15387 | ` */` |
|        - | 15388 | `/*` |
|        - | 15389 | ` * Allocate a new reference entry.` |
|        - | 15390 | ` */` |
|  3169282 | 15391 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15392 |  |
|        - | 15393 | `	VmRefObj *pRef;` |
|        - | 15394 | `	/* Allocate a new instance */` |
|  3169284 | 15395 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3169284 | 15396 | `	if( pRef == 0 ){` |
|      ! 0 | 15397 | `		return 0;` |
|        - | 15398 | `	}` |
|        - | 15399 | `	/* Zero the structure */` |
|  3169284 | 15400 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15401 | `	/* Initialize fields */` |
|  3169284 | 15402 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3169284 | 15403 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3169284 | 15404 | `	pRef->nIdx = nIdx;` |
|  3169284 | 15405 | `	return pRef;` |
|  1584643 | 15406 |  |
|        - | 15407 | `/*` |
|        - | 15408 | ` * Default hash function used by the reference table` |
|        - | 15409 | ` * for lookup/insertion operations.` |
|        - | 15410 | ` */` |
| 17398145 | 15411 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15412 |  |
|        - | 15413 | `	/* Calculate the hash based on the memory object index */` |
| 17398147 | 15414 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15415 |  |
|        - | 15416 | `/*` |
|        - | 15417 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15418 | ` * in the reference table.` |
|        - | 15419 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15420 | ` * otherwise.` |
|        - | 15421 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15422 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15423 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15424 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15425 | ` * Refer to the official for more information on this powerful` |
|        - | 15426 | ` * extension.` |
|        - | 15427 | ` */` |
|  9449146 | 15428 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15429 |  |
|        - | 15430 | `	VmRefObj *pRef;` |
|        - | 15431 | `	sxu32 nBucket;` |
|        - | 15432 | `	/* Point to the appropriate bucket */` |
|  9449148 | 15433 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15434 | `	/* Perform the lookup */` |
|  9449148 | 15435 | `	pRef = pVm->apRefObj[nBucket];` |
| 20616349 | 15436 | `	for(;;){` |
| 41223324 | 15437 | `		if( pRef == 0 ){` |
|  3272456 | 15438 | `			break;` |
|        - | 15439 | `		}` |
| 37950870 | 15440 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15441 | `			/* Entry found */` |
|  6176694 | 15442 | `			return pRef;` |
|        - | 15443 | `		}` |
|        - | 15444 | `		/* Point to the next entry */` |
| 31774178 | 15445 | `		pRef = pRef->pNextCollide;` |
|        2 | 15446 | `	}` |
|        - | 15447 | `	/* No such entry,return NULL */` |
|  3272456 | 15448 | `	return 0;` |
|  4724575 | 15449 |  |
|        - | 15450 | `/*` |
|        - | 15451 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15452 | ` *` |
|        - | 15453 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15454 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15455 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15456 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15457 | ` * Refer to the official for more information on this powerful` |
|        - | 15458 | ` * extension.` |
|        - | 15459 | ` */` |
|  3169282 | 15460 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15461 |  |
|        - | 15462 | `	sxu32 nBucket;` |
|  3169284 | 15463 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15464 | `		VmRefObj **apNew;` |
|        - | 15465 | `		sxu32 nNew;` |
|        - | 15466 | `		/* Allocate a larger table */` |
|     4456 | 15467 | `		nNew = pVm->nRefSize << 1;` |
|     4456 | 15468 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4456 | 15469 | `		if( apNew ){` |
|     4456 | 15470 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15471 | `			sxu32 n;` |
|        - | 15472 | `			/* Zero the structure */` |
|     4456 | 15473 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15474 | `			/* Rehash all referenced entries */` |
|  2847842 | 15475 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15476 | `				/* Remove old collision links */` |
|  2843388 | 15477 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15478 | `				/* Point to the appropriate bucket */` |
|  2843388 | 15479 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15480 | `				/* Insert the entry  */` |
|  2843388 | 15481 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843388 | 15482 | `				if( apNew[nBucket] ){` |
|  2301116 | 15483 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15484 | `				}` |
|  2843388 | 15485 | `				apNew[nBucket] = pEntry;` |
|        - | 15486 | `				/* Point to the next entry */` |
|  2843388 | 15487 | `				pEntry = pEntry->pNext;` |
|  1421695 | 15488 | `			}` |
|        - | 15489 | `			/* Release the old table */` |
|     4456 | 15490 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15491 | `			/* Install the new one */` |
|     4456 | 15492 | `			pVm->apRefObj = apNew;` |
|     4456 | 15493 | `			pVm->nRefSize = nNew;` |
|     2227 | 15494 | `		}` |
|     2227 | 15495 | `	}` |
|        - | 15496 | `	/* Point to the appropriate bucket */` |
|  3169284 | 15497 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15498 | `	/* Insert the entry */` |
|  3169284 | 15499 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3169284 | 15500 | `	if( pVm->apRefObj[nBucket] ){` |
|  2590230 | 15501 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1295059 | 15502 | `	}` |
|  3169284 | 15503 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3169284 | 15504 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3169284 | 15505 | `	pVm->nRefUsed++;` |
|  3169284 | 15506 | `	return SXRET_OK;` |
|        2 | 15507 |  |
|        - | 15508 | `/*` |
|        - | 15509 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15510 | ` * the reference table.` |
|        - | 15511 | ` * This function is invoked when the user perform an unset` |
|        - | 15512 | ` * call [i.e: unset($var); ].` |
|        - | 15513 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15514 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15515 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15516 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15517 | ` * Refer to the official for more information on this powerful` |
|        - | 15518 | ` * extension.` |
|        - | 15519 | ` */` |
|  3128424 | 15520 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15521 |  |
|        - | 15522 | `	ph7_hashmap_node **apNode;` |
|        - | 15523 | `	SyHashEntry **apEntry;` |
|        - | 15524 | `	sxu32 n;` |
|        - | 15525 | `	/* Point to the reference table */` |
|  3128426 | 15526 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3128426 | 15527 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15528 | `	/* Unlink the entry from the reference table */` |
|  3237400 | 15529 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   108976 | 15530 | `		if( apEntry[n] ){` |
|   108926 | 15531 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    54462 | 15532 | `		}` |
|    54489 | 15533 | `	}` |
|  6148174 | 15534 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3019750 | 15535 | `		if( apNode[n] ){` |
|     6738 | 15536 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3368 | 15537 | `		}` |
|  1509876 | 15538 | `	}` |
|  3128426 | 15539 | `	if( pRef->pPrevCollide ){` |
|  1192095 | 15540 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   596179 | 15541 | `	}else{` |
|  1936333 | 15542 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15543 | `	}` |
|  3128426 | 15544 | `	if( pRef->pNextCollide ){` |
|  1777398 | 15545 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   888694 | 15546 | `	}` |
|  3128426 | 15547 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15548 | `	/* Release the node */` |
|  3128426 | 15549 | `	SySetRelease(&pRef->aReference);` |
|  3128426 | 15550 | `	SySetRelease(&pRef->aArrEntries);` |
|  3128426 | 15551 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3128426 | 15552 | `	pVm->nRefUsed--;` |
|  3128426 | 15553 | `	return SXRET_OK;` |
|        2 | 15554 |  |
|        - | 15555 | `/*` |
|        - | 15556 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15557 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15558 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15559 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15560 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15561 | ` * Refer to the official for more information on this powerful` |
|        - | 15562 | ` * extension.` |
|        - | 15563 | ` */` |
|  3204472 | 15564 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15565 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15566 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15567 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15568 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15569 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15570 | `	)` |
|        2 | 15571 |  |
|  3204474 | 15572 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15573 | `	VmRefObj *pRef;` |
|        - | 15574 | `	/* Check if the referenced object already exists */` |
|  3204474 | 15575 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3204474 | 15576 | `	if( pRef == 0 ){` |
|        - | 15577 | `		/* Create a new entry */` |
|  3169284 | 15578 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3169284 | 15579 | `		if( pRef == 0 ){` |
|      ! 0 | 15580 | `			return SXERR_MEM;` |
|        - | 15581 | `		}` |
|  3169284 | 15582 | `		pRef->iFlags = iFlags;` |
|        - | 15583 | `		/* Install the entry */` |
|  3169284 | 15584 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1584641 | 15585 | `	}` |
|  3204474 | 15586 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3204474 | 15587 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15588 | `		VmSlot sRef;` |
|        - | 15589 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15590 | `		 * be deleted when we leave this frame.` |
|        - | 15591 | `		 */` |
|   103270 | 15592 | `		sRef.nIdx = nIdx;` |
|   103270 | 15593 | `		sRef.pUserData = pEntry;` |
|   103270 | 15594 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15595 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15596 | `		}` |
|    51634 | 15597 | `	}` |
|  3204474 | 15598 | `	if( pEntry ){` |
|        - | 15599 | `		/* Address of the hash-entry */` |
|   138260 | 15600 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    69129 | 15601 | `	}` |
|  3204474 | 15602 | `	if( pMapEntry ){` |
|        - | 15603 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3058236 | 15604 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1529117 | 15605 | `	}` |
|  3204474 | 15606 | `	return SXRET_OK;` |
|  1602238 | 15607 |  |
|        - | 15608 | `/*` |
|        - | 15609 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15610 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15611 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15612 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15613 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15614 | ` * Refer to the official for more information on this powerful` |
|        - | 15615 | ` * extension.` |
|        - | 15616 | ` */` |
|  3116244 | 15617 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15618 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15619 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15620 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15621 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15622 | `	)` |
|        2 | 15623 |  |
|        - | 15624 | `	VmRefObj *pRef;` |
|        - | 15625 | `	sxu32 n;` |
|        - | 15626 | `	/* Check if the referenced object already exists */` |
|  3116246 | 15627 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3116246 | 15628 | `	if( pRef == 0 ){` |
|        - | 15629 | `		/* Not such entry */` |
|   103168 | 15630 | `		return SXERR_NOTFOUND;` |
|        - | 15631 | `	}` |
|        - | 15632 | `	/* Remove the desired entry */` |
|  3013080 | 15633 | `	if( pEntry ){` |
|        - | 15634 | `		SyHashEntry **apEntry;` |
|       62 | 15635 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 15636 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 15637 | `			if( apEntry[n] == pEntry ){` |
|        - | 15638 | `				/* Nullify the entry */` |
|       62 | 15639 | `				apEntry[n] = 0;` |
|        - | 15640 | `				/*` |
|        - | 15641 | `				 * NOTE:` |
|        - | 15642 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15643 | `				 * we avoid wasting spaces.` |
|        - | 15644 | `				 */` |
|       30 | 15645 | `			}` |
|       85 | 15646 | `		}` |
|       30 | 15647 | `	}` |
|  3013080 | 15648 | `	if( pMapEntry ){` |
|        - | 15649 | `		ph7_hashmap_node **apNode;` |
|  3013020 | 15650 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6026132 | 15651 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3013114 | 15652 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15653 | `				/* nullify the entry */` |
|  3013020 | 15654 | `				apNode[n] = 0;` |
|  1506509 | 15655 | `			}` |
|  1506558 | 15656 | `		}` |
|  1506509 | 15657 | `	}` |
|  3013080 | 15658 | `	return SXRET_OK;` |
|  1558124 | 15659 |  |
|        - | 15660 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15661 | `/*` |
|        - | 15662 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15663 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15664 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15665 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15666 | ` * For more information on how to register IO stream devices,please` |
|        - | 15667 | ` * refer to the official documentation.` |
|        - | 15668 | ` */` |
|    28472 | 15669 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15670 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15671 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15672 | `	int nByte              /* *pzDevice length*/` |
|        - | 15673 | `	)` |
|        2 | 15674 |  |
|        - | 15675 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15676 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15677 | `	SyString sDev,sCur;` |
|        - | 15678 | `	sxu32 n,nEntry;` |
|        - | 15679 | `	int rc;` |
|        - | 15680 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28474 | 15681 | `	zNext = zCur = zIn = *pzDevice;` |
|    28474 | 15682 | `	zEnd = &zIn[nByte];` |
|  1816552 | 15683 | `	while( zIn < zEnd ){` |
|  1788082 | 15684 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15685 | `			/* Got one */` |
|        3 | 15686 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15687 | `			break;` |
|        - | 15688 | `		}` |
|        - | 15689 | `		/* Advance the cursor */` |
|  1788080 | 15690 | `		zIn++;` |
|        2 | 15691 | `	}` |
|    28474 | 15692 | `	if( zIn >= zEnd ){` |
|        - | 15693 | `		/* No such scheme,return the default stream */` |
|    28472 | 15694 | `		return pVm->pDefStream;` |
|        - | 15695 | `	}` |
|        3 | 15696 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15697 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15698 | `	SyStringFullTrim(&sDev);` |
|        - | 15699 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15700 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15701 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15702 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15703 | `		pStream = apStream[n];` |
|        3 | 15704 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15705 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15706 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15707 | `		if( rc == 0 ){` |
|        - | 15708 | `			/* Stream device found */` |
|        3 | 15709 | `			*pzDevice = zNext;` |
|        3 | 15710 | `			return pStream;` |
|        - | 15711 | `		}` |
|      ! 0 | 15712 | `	}` |
|        - | 15713 | `	/* No such stream,return NULL */` |
|      ! 0 | 15714 | `	return 0;` |
|    14238 | 15715 |  |
|        - | 15716 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15717 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15718 |  |
