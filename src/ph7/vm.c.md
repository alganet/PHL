# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5220/7395 lines (70.59%)

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
|        - |     9 | `/*` |
|        - |    10 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    11 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    12 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    13 | ` * statements.` |
|        - |    14 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    15 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    16 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    17 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    18 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    19 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    20 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    21 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    22 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    23 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    24 | ` * and so on.` |
|        - |    25 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    26 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    27 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    28 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    29 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    30 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    31 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    32 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    33 | ` */` |
|        - |    34 | `/*` |
|        - |    35 | ` * Each active virtual machine frame is represented by an instance` |
|        - |    36 | ` * of the following structure.` |
|        - |    37 | ` * VM Frame hold local variables and other stuff related to function call.` |
|        - |    38 | ` */` |
|        - |    39 | `struct VmFrame` |
|        - |    40 |  |
|        - |    41 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|        - |    42 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|        - |    43 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|        - |    44 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|        - |    45 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|        - |    46 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|        - |    47 | `	SySet sArg;       /* Function arguments container */` |
|        - |    48 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|        - |    49 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|        - |    50 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|        - |    51 | `};` |
|        - |    52 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|        - |    53 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|        - |    54 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|        - |    55 | `/*` |
|        - |    56 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    57 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    58 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    59 | ` * a new memory object.` |
|        - |    60 | ` */` |
|        - |    61 | `typedef struct VmSlot VmSlot;` |
|        - |    62 | `struct VmSlot` |
|        - |    63 |  |
|        - |    64 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    65 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    66 | `};` |
|        - |    67 | `/*` |
|        - |    68 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    69 | ` * follwoing table.` |
|        - |    70 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    71 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    72 | ` * the reference implementation is consistent,solid and it's` |
|        - |    73 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    74 | ` * Refer to the official for more information on this powerful` |
|        - |    75 | ` * extension.` |
|        - |    76 | ` */` |
|        - |    77 | `struct VmRefObj` |
|        - |    78 |  |
|        - |    79 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |    80 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |    81 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |    82 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |    83 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |    84 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |    85 | `};` |
|        - |    86 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |    87 | `/*` |
|        - |    88 | ` * Output control buffer entry.` |
|        - |    89 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |    90 | ` */` |
|        - |    91 | `typedef struct VmObEntry VmObEntry;` |
|        - |    92 | `struct VmObEntry` |
|        - |    93 |  |
|        - |    94 | `	ph7_value sCallback; /* User defined callback */` |
|        - |    95 | `	SyBlob sOB;          /* Output buffer consumer */` |
|        - |    96 | `};` |
|        - |    97 | `/*` |
|        - |    98 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    99 | ` * is stored in an instance of the following structure.` |
|        - |   100 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   101 | ` */` |
|        - |   102 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   103 | `struct VmShutdownCB` |
|        - |   104 |  |
|        - |   105 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   106 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   107 | `	int nArg;             /* Total number of given arguments */` |
|        - |   108 | `};` |
|        - |   109 | `/* Uncaught exception code value */` |
|        - |   110 | `#define PH7_EXCEPTION -255` |
|        - |   111 |  |
|        - |   112 | `/*` |
|        - |   113 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   114 | ` */` |
|   496208 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   496210 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   496188 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   496180 |   123 | `	return FALSE;` |
|   248128 |   124 |  |
|        - |   125 | `/*` |
|        - |   126 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|        - |   127 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   128 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   129 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct SyhttpUri SyhttpUri;` |
|        - |   132 | `struct SyhttpUri` |
|        - |   133 |  |
|        - |   134 | `	SyString sHost;     /* Hostname or IP address */` |
|        - |   135 | `	SyString sPort;     /* Port number */` |
|        - |   136 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|        - |   137 | `	SyString sQuery;    /* Query part */` |
|        - |   138 | `	SyString sFragment; /* Fragment part */` |
|        - |   139 | `	SyString sScheme;   /* Scheme */` |
|        - |   140 | `	SyString sUser;     /* Username */` |
|        - |   141 | `	SyString sPass;     /* Password */` |
|        - |   142 | `	SyString sRaw;      /* Raw URI */` |
|        - |   143 | `};` |
|        - |   144 | `/*` |
|        - |   145 | ` * An instance of the following structure is used to record all MIME headers seen` |
|        - |   146 | ` * during a HTTP interaction.` |
|        - |   147 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   148 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   149 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   150 | ` */` |
|        - |   151 | `typedef struct SyhttpHeader SyhttpHeader;` |
|        - |   152 | `struct SyhttpHeader` |
|        - |   153 |  |
|        - |   154 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|        - |   155 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|        - |   156 | `};` |
|        - |   157 | `/*` |
|        - |   158 | ` * Supported HTTP methods.` |
|        - |   159 | ` */` |
|        - |   160 | `#define HTTP_METHOD_GET  1 /* GET */` |
|        - |   161 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|        - |   162 | `#define HTTP_METHOD_POST 3 /* POST */` |
|        - |   163 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|        - |   164 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|        - |   165 | `/*` |
|        - |   166 | ` * Supported HTTP protocol version.` |
|        - |   167 | ` */` |
|        - |   168 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|        - |   169 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|        - |   170 | `/*` |
|        - |   171 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   172 | ` * it can be expanded from the target PHP program.` |
|        - |   173 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   174 | ` * simple and work as follows:` |
|        - |   175 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   176 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   177 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   178 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   179 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   180 | ` * (Windows,Linux,...) and so on.` |
|        - |   181 | ` * Please refer to the official documentation for additional information.` |
|        - |   182 | ` */` |
|   196766 |   183 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   184 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   185 | `	const SyString *pName,  /* Constant name */` |
|        - |   186 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   187 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   188 | `	)` |
|        2 |   189 |  |
|        - |   190 | `	ph7_constant *pCons;` |
|        - |   191 | `	SyHashEntry *pEntry;` |
|        - |   192 | `	char *zDupName;` |
|        - |   193 | `	sxi32 rc;` |
|   196768 |   194 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   196768 |   195 | `	if( pEntry ){` |
|        - |   196 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   197 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   198 | `		pCons->xExpand = xExpand;` |
|        6 |   199 | `		pCons->pUserData = pUserData;` |
|        6 |   200 | `		return SXRET_OK;` |
|        - |   201 | `	}` |
|        - |   202 | `	/* Allocate a new constant instance */` |
|   196764 |   203 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   196764 |   204 | `	if( pCons == 0 ){` |
|      ! 0 |   205 | `		return 0;` |
|        - |   206 | `	}` |
|        - |   207 | `	/* Duplicate constant name */` |
|   196764 |   208 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   196764 |   209 | `	if( zDupName == 0 ){` |
|      ! 0 |   210 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   211 | `		return 0;` |
|        - |   212 | `	}` |
|        - |   213 | `	/* Install the constant */` |
|   196764 |   214 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   196764 |   215 | `	pCons->xExpand = xExpand;` |
|   196764 |   216 | `	pCons->pUserData = pUserData;` |
|   196764 |   217 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   196764 |   218 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   219 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   220 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   221 | `		return rc;` |
|        - |   222 | `	}` |
|        - |   223 | `	/* All done,constant can be invoked from PHP code */` |
|   196764 |   224 | `	return SXRET_OK;` |
|    98385 |   225 |  |
|        - |   226 | `/*` |
|        - |   227 | ` * Allocate a new foreign function instance.` |
|        - |   228 | ` * This function return SXRET_OK on success. Any other` |
|        - |   229 | ` * return value indicates failure.` |
|        - |   230 | ` * Please refer to the official documentation for an introduction to` |
|        - |   231 | ` * the foreign function mechanism.` |
|        - |   232 | ` */` |
|   423690 |   233 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   234 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   235 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   236 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   237 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   238 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   239 | `	)` |
|        2 |   240 |  |
|        - |   241 | `	ph7_user_func *pFunc;` |
|        - |   242 | `	char *zDup;` |
|        - |   243 | `	/* Allocate a new user function */` |
|   423692 |   244 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   423692 |   245 | `	if( pFunc == 0 ){` |
|      ! 0 |   246 | `		return SXERR_MEM;` |
|        - |   247 | `	}` |
|        - |   248 | `	/* Duplicate function name */` |
|   423692 |   249 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   423692 |   250 | `	if( zDup == 0 ){` |
|      ! 0 |   251 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   252 | `		return SXERR_MEM;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Zero the structure */` |
|   423692 |   255 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   256 | `	/* Initialize structure fields */` |
|   423692 |   257 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   423692 |   258 | `	pFunc->pVm   = pVm;` |
|   423692 |   259 | `	pFunc->xFunc = xFunc;` |
|   423692 |   260 | `	pFunc->pUserData = pUserData;` |
|   423692 |   261 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   262 | `	/* Write a pointer to the new function */` |
|   423692 |   263 | `	*ppOut = pFunc;` |
|   423692 |   264 | `	return SXRET_OK;` |
|   211847 |   265 |  |
|        - |   266 | `/*` |
|        - |   267 | ` * Install a foreign function and it's associated callback so that` |
|        - |   268 | ` * it can be invoked from the target PHP code.` |
|        - |   269 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   270 | ` * return value indicates failure.` |
|        - |   271 | ` * Please refer to the official documentation for an introduction to` |
|        - |   272 | ` * the foreign function mechanism.` |
|        - |   273 | ` */` |
|   424664 |   274 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   275 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   276 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   277 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   278 | `	void *pUserData           /* Foreign function private data */` |
|        - |   279 | `	)` |
|        2 |   280 |  |
|        - |   281 | `	ph7_user_func *pFunc;` |
|        - |   282 | `	SyHashEntry *pEntry;` |
|        - |   283 | `	sxi32 rc;` |
|        - |   284 | `	/* Overwrite any previously registered function with the same name */` |
|   424666 |   285 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   424666 |   286 | `	if( pEntry ){` |
|      976 |   287 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      976 |   288 | `		pFunc->pUserData = pUserData;` |
|      976 |   289 | `		pFunc->xFunc = xFunc;` |
|      976 |   290 | `		SySetReset(&pFunc->aAux);` |
|      976 |   291 | `		return SXRET_OK;` |
|        - |   292 | `	}` |
|        - |   293 | `	/* Create a new user function */` |
|   423692 |   294 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   423692 |   295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   296 | `		return rc;` |
|        - |   297 | `	}` |
|        - |   298 | `	/* Install the function in the corresponding hashtable */` |
|   423692 |   299 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   423692 |   300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   301 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   302 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   303 | `		return rc;` |
|        - |   304 | `	}` |
|        - |   305 | `	/* User function successfully installed */` |
|   423692 |   306 | `	return SXRET_OK;` |
|   212334 |   307 |  |
|        - |   308 | `/*` |
|        - |   309 | ` * Initialize a VM function.` |
|        - |   310 | ` */` |
|    52436 |   311 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   312 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   313 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   314 | `	const char *zName,  /* Function name */` |
|        - |   315 | `	sxu32 nByte,        /* zName length */` |
|        - |   316 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   317 | `	void *pUserData     /* Function private data */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	/* Zero the structure */` |
|    52438 |   321 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   322 | `	/* Initialize structure fields */` |
|        - |   323 | `	/* Arguments container */` |
|    52438 |   324 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   325 | `	/* Static variable container */` |
|    52438 |   326 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   327 | `	/* Bytecode container */` |
|    52438 |   328 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   329 | `    /* Preallocate some instruction slots */` |
|    52438 |   330 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   331 | `	/* Closure environment */` |
|    52438 |   332 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    52438 |   333 | `	pFunc->iFlags = iFlags;` |
|    52438 |   334 | `	pFunc->pUserData = pUserData;` |
|    52438 |   335 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    52438 |   336 | `	return SXRET_OK;` |
|        2 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   340 | ` */` |
|   140320 |   341 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   344 | `	SyString *pName     /* Function name */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	SyHashEntry *pEntry;` |
|        - |   348 | `	sxi32 rc;` |
|   140322 |   349 | `	if( pName == 0 ){` |
|        - |   350 | `		/* Use the built-in name */` |
|    16444 |   351 | `		pName = &pFunc->sName;` |
|     8221 |   352 | `	}` |
|        - |   353 | `	/* Check for duplicates (functions with the same name) first */` |
|   140322 |   354 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   140322 |   355 | `	if( pEntry ){` |
|    97966 |   356 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|    97966 |   357 | `		if( pLink != pFunc ){` |
|        - |   358 | `			/* Link */` |
|      185 |   359 | `			pFunc->pNextName = pLink;` |
|      185 |   360 | `			pEntry->pUserData = pFunc;` |
|       92 |   361 | `		}` |
|    97966 |   362 | `		return SXRET_OK;` |
|        - |   363 | `	}` |
|        - |   364 | `	/* First time seen */` |
|    42358 |   365 | `	pFunc->pNextName = 0;` |
|    42358 |   366 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    42358 |   367 | `	return rc;` |
|    70162 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   371 | ` */` |
|    12592 |   372 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   373 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   374 | `	ph7_class *pClass /* Target Class */` |
|        - |   375 | `	)` |
|        2 |   376 |  |
|    12594 |   377 | `	SyString *pName = &pClass->sName;` |
|        - |   378 | `	SyHashEntry *pEntry;` |
|        - |   379 | `	sxi32 rc;` |
|        - |   380 | `	/* Check for duplicates */` |
|    12594 |   381 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    12594 |   382 | `	if( pEntry ){` |
|       63 |   383 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   384 | `		/* Link entry with the same name */` |
|       63 |   385 | `		pClass->pNextName = pLink;` |
|       63 |   386 | `		pEntry->pUserData = pClass;` |
|       63 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|    12532 |   389 | `	pClass->pNextName = 0;` |
|        - |   390 | `	/* Perform a simple hashtable insertion */` |
|    12532 |   391 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    12532 |   392 | `	return rc;` |
|     6298 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Instruction builder interface.` |
|        - |   396 | ` */` |
|  1323678 |   397 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   398 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   399 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   400 | `	sxi32 iP1,    /* First operand */` |
|        - |   401 | `	sxu32 iP2,    /* Second operand */` |
|        - |   402 | `	void *p3,     /* Third operand */` |
|        - |   403 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   404 | `	)` |
|        2 |   405 |  |
|        - |   406 | `	VmInstr sInstr;` |
|        - |   407 | `	sxi32 rc;` |
|        - |   408 | `	/* Fill the VM instruction */` |
|  1323680 |   409 | `	sInstr.iOp = (sxu8)iOp;` |
|  1323680 |   410 | `	sInstr.iP1 = iP1;` |
|  1323680 |   411 | `	sInstr.iP2 = iP2;` |
|  1323680 |   412 | `	sInstr.p3  = p3;` |
|  1323680 |   413 | `	if( pIndex ){` |
|        - |   414 | `		/* Instruction index in the bytecode array */` |
|    79620 |   415 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    39809 |   416 | `	}` |
|        - |   417 | `	/* Finally,record the instruction */` |
|  1323680 |   418 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1323680 |   419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   420 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   421 | `		/* Fall throw */` |
|      ! 0 |   422 | `	}` |
|  1323680 |   423 | `	return rc;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Swap the current bytecode container with the given one.` |
|        - |   427 | ` */` |
|   127584 |   428 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   429 |  |
|   127586 |   430 | `	if( pContainer == 0 ){` |
|        - |   431 | `		/* Point to the default container */` |
|      ! 0 |   432 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   433 | `	}else{` |
|        - |   434 | `		/* Change container */` |
|   127586 |   435 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   436 | `	}` |
|   127586 |   437 | `	return SXRET_OK;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Return the current bytecode container.` |
|        - |   441 | ` */` |
|    63792 |   442 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   443 |  |
|    63794 |   444 | `	return pVm->pByteContainer;` |
|        2 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   448 | ` */` |
|    78312 |   449 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *pInstr;` |
|    78314 |   452 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    78314 |   453 | `	return pInstr;` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   457 | ` */` |
|   382912 |   458 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   459 |  |
|   382914 |   460 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Pop the last VM instruction.` |
|        - |   464 | ` */` |
|    74970 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|    74972 |   467 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   468 |  |
|        - |   469 | `/*` |
|        - |   470 | ` * Peek the last VM instruction.` |
|        - |   471 | ` */` |
|   200524 |   472 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   473 |  |
|   200526 |   474 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   475 |  |
|     2688 |   476 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *aInstr;` |
|        - |   479 | `	sxu32 n;` |
|     2690 |   480 | `	n = SySetUsed(pVm->pByteContainer);` |
|     2690 |   481 | `	if( n < 2 ){` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     2690 |   484 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     2690 |   485 | `	return &aInstr[n - 2];` |
|     1346 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Allocate a new virtual machine frame.` |
|        - |   489 | ` */` |
|     9050 |   490 | `static VmFrame * VmNewFrame(` |
|        - |   491 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   492 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	)` |
|        2 |   495 |  |
|        - |   496 | `	VmFrame *pFrame;` |
|        - |   497 | `	/* Allocate a new vm frame */` |
|     9052 |   498 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|     9052 |   499 | `	if( pFrame == 0 ){` |
|      ! 0 |   500 | `		return 0;` |
|        - |   501 | `	}` |
|        - |   502 | `	/* Zero the structure */` |
|     9052 |   503 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   504 | `	/* Initialize frame fields */` |
|     9052 |   505 | `	pFrame->pUserData = pUserData;` |
|     9052 |   506 | `	pFrame->pThis = pThis;` |
|     9052 |   507 | `	pFrame->pVm = pVm;` |
|     9052 |   508 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|     9052 |   509 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|     9052 |   510 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|     9052 |   511 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|     9052 |   512 | `	return pFrame;` |
|     4527 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Enter a VM frame.` |
|        - |   516 | ` */` |
|     9050 |   517 | `static sxi32 VmEnterFrame(` |
|        - |   518 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   519 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   522 | `	)` |
|        2 |   523 |  |
|        - |   524 | `	VmFrame *pFrame;` |
|        - |   525 | `	/* Allocate a new frame */` |
|     9052 |   526 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|     9052 |   527 | `	if( pFrame == 0 ){` |
|      ! 0 |   528 | `		return SXERR_MEM;` |
|        - |   529 | `	}` |
|        - |   530 | `	/* Link to the list of active VM frame */` |
|     9052 |   531 | `	pFrame->pParent = pVm->pFrame;` |
|     9052 |   532 | `	pVm->pFrame = pFrame;` |
|     9052 |   533 | `	if( ppFrame ){` |
|        - |   534 | `		/* Write a pointer to the new VM frame */` |
|     7818 |   535 | `		*ppFrame = pFrame;` |
|     3908 |   536 | `	}` |
|     9052 |   537 | `	return SXRET_OK;` |
|     4527 |   538 |  |
|        - |   539 | `/*` |
|        - |   540 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   541 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   542 | ` * information.` |
|        - |   543 | ` */` |
|       30 |   544 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   545 |  |
|        - |   546 | `	VmFrame *pTarget,*pFrame;` |
|       31 |   547 | `	SyHashEntry *pEntry = 0;` |
|        - |   548 | `	sxi32 rc;` |
|        - |   549 | `	/* Point to the upper frame */` |
|       31 |   550 | `	pFrame = pVm->pFrame;` |
|       31 |   551 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   552 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   553 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   554 | `	}` |
|       31 |   555 | `	pTarget = pFrame;` |
|       31 |   556 | `	pFrame = pTarget->pParent;` |
|       45 |   557 | `	while( pFrame ){` |
|       45 |   558 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   559 | `			/* Query the current frame */` |
|       31 |   560 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       31 |   561 | `			if( pEntry ){` |
|        - |   562 | `				/* Variable found */` |
|       31 |   563 | `				break;` |
|        - |   564 | `			}` |
|      ! 0 |   565 | `		}` |
|        - |   566 | `		/* Point to the upper frame */` |
|       15 |   567 | `		pFrame = pFrame->pParent;` |
|        1 |   568 | `	}` |
|       31 |   569 | `	if( pEntry == 0 ){` |
|        - |   570 | `		/* Inexistant variable */` |
|      ! 0 |   571 | `		return SXERR_NOTFOUND;` |
|        - |   572 | `	}` |
|        - |   573 | `	/* Link to the current frame */` |
|       31 |   574 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       31 |   575 | `	if( rc == SXRET_OK ){` |
|        - |   576 | `		sxu32 nIdx;` |
|       31 |   577 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |   578 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       15 |   579 | `	}` |
|       31 |   580 | `	return rc;` |
|       16 |   581 |  |
|        - |   582 | `/*` |
|        - |   583 | ` * Leave the top-most active frame.` |
|        - |   584 | ` */` |
|     7814 |   585 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   586 |  |
|     7816 |   587 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     7816 |   588 | `	if( pCurFrame ){` |
|        - |   589 | `		/* Unlink from the list of active VM frame */` |
|     7816 |   590 | `		pVm->pFrame = pCurFrame->pParent;` |
|     7816 |   591 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   592 | `			VmSlot  *aSlot;` |
|        - |   593 | `			sxu32 n;` |
|        - |   594 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     7798 |   595 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    58048 |   596 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   597 | `				/* Unset the local variable */` |
|    50252 |   598 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    25127 |   599 | `			}` |
|        - |   600 | `			/* Remove local reference */` |
|     7798 |   601 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    58082 |   602 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    50286 |   603 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    25144 |   604 | `			}` |
|     3898 |   605 | `		}` |
|        - |   606 | `		/* Release internal containers */` |
|     7816 |   607 | `		SyHashRelease(&pCurFrame->hVar);` |
|     7816 |   608 | `		SySetRelease(&pCurFrame->sArg);` |
|     7816 |   609 | `		SySetRelease(&pCurFrame->sLocal);` |
|     7816 |   610 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   611 | `		/* Release the whole structure */` |
|     7816 |   612 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     3907 |   613 | `	}` |
|     7816 |   614 |  |
|        - |   615 | `/*` |
|        - |   616 | ` * Compare two functions signature and return the comparison result.` |
|        - |   617 | ` */` |
|      818 |   618 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   619 |  |
|      819 |   620 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   621 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   622 | `	const char *zSin = pSecond->zString;` |
|      819 |   623 | `	const char *zFin = pFirst->zString;` |
|      819 |   624 | `	const char *zPtr = zFin;` |
|      409 |   625 | `	for(;;){` |
|      819 |   626 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   627 | `			break;` |
|        - |   628 | `		}` |
|      ! 0 |   629 | `		if( zFin[0] != zSin[0] ){` |
|        - |   630 | `			/* mismatch */` |
|      ! 0 |   631 | `			break;` |
|        - |   632 | `		}` |
|      ! 0 |   633 | `		zFin++;` |
|      ! 0 |   634 | `		zSin++;` |
|      ! 0 |   635 | `	}` |
|      819 |   636 | `	return (int)(zFin-zPtr);` |
|        1 |   637 |  |
|        - |   638 | `/*` |
|        - |   639 | ` * Select the appropriate VM function for the current call context.` |
|        - |   640 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   641 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   642 | ` * Refer to the official documentation for more information.` |
|        - |   643 | ` */` |
|      128 |   644 | `static ph7_vm_func * VmOverload(` |
|        - |   645 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   646 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   647 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   648 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   649 | `	)` |
|        1 |   650 |  |
|        - |   651 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   652 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   653 | `	ph7_vm_func *pLink;` |
|        - |   654 | `	SyString sArgSig;` |
|        - |   655 | `	SyBlob sSig;` |
|        - |   656 |  |
|      129 |   657 | `	pLink = pList;` |
|      129 |   658 | `	i = 0;` |
|        - |   659 | `	/* Put functions expecting the same number of passed arguments */` |
|     1073 |   660 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1011 |   661 | `		if( pLink == 0 ){` |
|       67 |   662 | `			break;` |
|        - |   663 | `		}` |
|      945 |   664 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   665 | `			/* Candidate for overloading */` |
|      883 |   666 | `			apSet[i++] = pLink;` |
|      441 |   667 | `		}` |
|        - |   668 | `		/* Point to the next entry */` |
|      945 |   669 | `		pLink = pLink->pNextName;` |
|        1 |   670 | `	}` |
|      129 |   671 | `	if( i < 1 ){` |
|        - |   672 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   673 | `		return pList;` |
|        - |   674 | `	}` |
|      129 |   675 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   676 | `		/* Return the only candidate */` |
|       27 |   677 | `		return apSet[0];` |
|        - |   678 | `	}` |
|        - |   679 | `	/* Calculate function signature */` |
|      103 |   680 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   681 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   682 | `		int c = 'n'; /* null */` |
|      253 |   683 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   684 | `			/* Hashmap */` |
|       45 |   685 | `			c = 'h';` |
|      231 |   686 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   687 | `			/* bool */` |
|      ! 0 |   688 | `			c = 'b';` |
|      209 |   689 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   690 | `			/* int */` |
|        5 |   691 | `			c = 'i';` |
|      207 |   692 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   693 | `			/* String */` |
|      105 |   694 | `			c = 's';` |
|      153 |   695 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   696 | `			/* Float */` |
|      ! 0 |   697 | `			c = 'f';` |
|      101 |   698 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   699 | `			/* Class instance */` |
|      ! 0 |   700 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   701 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   702 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   703 | `			c = -1;` |
|      ! 0 |   704 | `		}` |
|      253 |   705 | `		if( c > 0 ){` |
|      253 |   706 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   707 | `		}` |
|      127 |   708 | `	}` |
|      103 |   709 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   710 | `	iTarget = 0;` |
|      103 |   711 | `	iMax = -1;` |
|        - |   712 | `	/* Select the appropriate function */` |
|      921 |   713 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   714 | `		/* Compare the two signatures */` |
|      819 |   715 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   716 | `		if( iCur > iMax ){` |
|      103 |   717 | `			iMax = iCur;` |
|      103 |   718 | `			iTarget = j;` |
|       51 |   719 | `		}` |
|      410 |   720 | `	}` |
|      103 |   721 | `	SyBlobRelease(&sSig);` |
|        - |   722 | `	/* Appropriate function for the current call context */` |
|      103 |   723 | `	return apSet[iTarget];` |
|       65 |   724 |  |
|        - |   725 | `/* Forward declaration */` |
|        - |   726 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   727 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   728 | `/*` |
|        - |   729 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   730 | ` * it can be instanciated from the executed PHP script.` |
|        - |   731 | ` */` |
|    50984 |   732 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   733 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   734 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   735 | `	)` |
|        2 |   736 |  |
|        - |   737 | `	ph7_class_method *pMeth;` |
|        - |   738 | `	ph7_class_attr *pAttr;` |
|        - |   739 | `	SyHashEntry *pEntry;` |
|        - |   740 | `	sxi32 rc;` |
|        - |   741 | `	/* Reset the loop cursor */` |
|    50986 |   742 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   743 | `	/* Process only static and constant attribute */` |
|   153024 |   744 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   745 | `		/* Extract the current attribute */` |
|    76548 |   746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    76548 |   747 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   748 | `			ph7_value *pMemObj;` |
|        - |   749 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1286 |   750 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1286 |   751 | `			if( pMemObj == 0 ){` |
|      ! 0 |   752 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   753 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   754 | `					&pClass->sName,&pAttr->sName` |
|        - |   755 | `					);` |
|      ! 0 |   756 | `				return SXERR_MEM;` |
|        - |   757 | `			}` |
|     1286 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1286 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      642 |   761 | `			}` |
|        - |   762 | `			/* Record attribute index */` |
|     1286 |   763 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   764 | `			/* Install static attribute in the reference table */` |
|     1286 |   765 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      642 |   766 | `		}` |
|        2 |   767 | `	}` |
|        - |   768 | `	/* Install class methods */` |
|    50986 |   769 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   770 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   771 | `		 */` |
|    35302 |   772 | `		return SXRET_OK;` |
|        - |   773 | `	}` |
|        - |   774 | `	/* Create constructor alias if not yet done */` |
|    15686 |   775 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   776 | `		/* User constructor with the same base class name */` |
|      200 |   777 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      200 |   778 | `		if( pEntry ){` |
|      ! 0 |   779 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   780 | `			/* Create the alias */` |
|      ! 0 |   781 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   782 | `		}` |
|       99 |   783 | `	}` |
|        - |   784 | `	/* Install the methods now */` |
|    15686 |   785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   147412 |   786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   123886 |   787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   123886 |   788 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   123880 |   789 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   123880 |   790 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   791 | `				return rc;` |
|        - |   792 | `			}` |
|    61939 |   793 | `		}` |
|        2 |   794 | `	}` |
|        - |   795 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    15686 |   796 | `	pClass->bMounted = TRUE;` |
|    15686 |   797 | `	return SXRET_OK;` |
|    25494 |   798 |  |
|        - |   799 | `/*` |
|        - |   800 | ` * Allocate a private frame for attributes of the given` |
|        - |   801 | ` * class instance (Object in the PHP jargon).` |
|        - |   802 | ` */` |
|      574 |   803 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   804 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   805 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   806 | `	)` |
|        2 |   807 |  |
|      576 |   808 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   809 | `	ph7_class_attr *pAttr;` |
|        - |   810 | `	SyHashEntry *pEntry;` |
|        - |   811 | `	sxi32 rc;` |
|        - |   812 | `	/* Install class attribute in the private frame associated with this instance */` |
|      576 |   813 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1348 |   814 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   815 | `		VmClassAttr *pVmAttr;` |
|        - |   816 | `		/* Extract the current attribute */` |
|      774 |   817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      774 |   818 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      774 |   819 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   820 | `			return SXERR_MEM;` |
|        - |   821 | `		}` |
|      774 |   822 | `		pVmAttr->pAttr = pAttr;` |
|      774 |   823 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   824 | `			ph7_value *pMemObj;` |
|        - |   825 | `			/* Reserve a memory object for this attribute */` |
|      768 |   826 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      768 |   827 | `			if( pMemObj == 0 ){` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|      768 |   831 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|      768 |   832 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   833 | `				/* Initialize attribute default value (any complex expression) */` |
|      234 |   834 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      116 |   835 | `			}` |
|      768 |   836 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|      768 |   837 | `			if( rc != SXRET_OK ){` |
|        - |   838 | `				VmSlot sSlot;` |
|        - |   839 | `				/* Restore memory object */` |
|      ! 0 |   840 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   841 | `				sSlot.pUserData = 0;` |
|      ! 0 |   842 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   843 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   844 | `				return SXERR_MEM;` |
|        - |   845 | `			}` |
|        - |   846 | `			/* Install attribute in the reference table */` |
|      768 |   847 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      385 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|        8 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|      576 |   858 | `	return SXRET_OK;` |
|      289 |   859 |  |
|        - |   860 | `/* Forward declaration */` |
|        - |   861 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   862 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   863 | `/*` |
|        - |   864 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   865 | ` */` |
|        - |   866 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   867 | `/*` |
|        - |   868 | ` * Reserve a constant memory object.` |
|        - |   869 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   870 | ` */` |
|   152520 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   152522 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   148820 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    74409 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   152522 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   152522 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   152522 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   152522 |   888 | `	return pObj;` |
|    76262 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|    76556 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|    76558 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|    76558 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|    38278 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|    76558 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|    76558 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|    76558 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|    76558 |   911 | `	return pObj;` |
|    38280 |   912 |  |
|        - |   913 | `/* Forward declaration */` |
|        - |   914 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   915 | `/*` |
|        - |   916 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   917 | ` * directly as foreign functions.` |
|        - |   918 | ` */` |
|        - |   919 | `#define PH7_BUILTIN_LIB \` |
|        - |   920 | `	"class Exception { "\` |
|        - |   921 | `    "protected $message = 'Unknown exception';"\` |
|        - |   922 | `    "protected $code = 0;"\` |
|        - |   923 | `    "protected $file;"\` |
|        - |   924 | `    "protected $line;"\` |
|        - |   925 | `    "protected $trace;"\` |
|        - |   926 | `    "protected $previous;"\` |
|        - |   927 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   928 | `	"   if( isset($message) ){"\` |
|        - |   929 | `	"	  $this->message = $message;"\` |
|        - |   930 | `	"   }"\` |
|        - |   931 | `	"   $this->code = $code;"\` |
|        - |   932 | `	"   $this->file = __FILE__;"\` |
|        - |   933 | `	"   $this->line = __LINE__;"\` |
|        - |   934 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   935 | `	"   if( isset($previous) ){"\` |
|        - |   936 | `	"     $this->previous = $previous;"\` |
|        - |   937 | `	"   }"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getMessage(){"\` |
|        - |   940 | `	"   return $this->message;"\` |
|        - |   941 | `	"}"\` |
|        - |   942 | `	" public function getCode(){"\` |
|        - |   943 | `	"  return $this->code;"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function getFile(){"\` |
|        - |   946 | `	"  return $this->file;"\` |
|        - |   947 | `	"}"\` |
|        - |   948 | `	"public function getLine(){"\` |
|        - |   949 | `	"  return $this->line;"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"public function getTrace(){"\` |
|        - |   952 | `	"   return $this->trace;"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"public function getTraceAsString(){"\` |
|        - |   955 | `	"  return debug_string_backtrace();"\` |
|        - |   956 | `	"}"\` |
|        - |   957 | `	"public function getPrevious(){"\` |
|        - |   958 | `	"    return $this->previous;"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"public function __toString(){"\` |
|        - |   961 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   962 | `    "}"\` |
|        - |   963 | `	"}"\` |
|        - |   964 | `	"class Error extends Exception { }"\` |
|        - |   965 | `	"class TypeError extends Error { }"\` |
|        - |   966 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   967 | `	"class ErrorException extends Exception { "\` |
|        - |   968 | `	"protected $severity;"\` |
|        - |   969 | `	"public function __construct(string $message = null,"\` |
|        - |   970 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   971 | `	"   if( isset($message) ){"\` |
|        - |   972 | `	"	  $this->message = $message;"\` |
|        - |   973 | `	"   }"\` |
|        - |   974 | `	"   $this->severity = $severity;"\` |
|        - |   975 | `	"   $this->code = $code;"\` |
|        - |   976 | `	"   $this->file = $filename;"\` |
|        - |   977 | `	"   $this->line = $lineno;"\` |
|        - |   978 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   979 | `	"   if( isset($previous) ){"\` |
|        - |   980 | `	"     $this->previous = $previous;"\` |
|        - |   981 | `	"   }"\` |
|        - |   982 | `	"}"\` |
|        - |   983 | `	"public function getSeverity(){"\` |
|        - |   984 | `	"   return $this->severity;"\` |
|        - |   985 | `    "}"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"interface Iterator {"\` |
|        - |   988 | `	"public function current();"\` |
|        - |   989 | `	"public function key();"\` |
|        - |   990 | `	"public function next();"\` |
|        - |   991 | `	"public function rewind();"\` |
|        - |   992 | `	"public function valid();"\` |
|        - |   993 | `	"}"\` |
|        - |   994 | `	"interface IteratorAggregate {"\` |
|        - |   995 | `	"public function getIterator();"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"interface Serializable {"\` |
|        - |   998 | `	"public function serialize();"\` |
|        - |   999 | `	"public function unserialize(string $serialized);"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"/* Directory releated IO */"\` |
|        - |  1002 | `	"class Directory {"\` |
|        - |  1003 | `	"public $handle = null;"\` |
|        - |  1004 | `	"public $path  = null;"\` |
|        - |  1005 | `	"public function __construct(string $path)"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"   $this->handle = opendir($path);"\` |
|        - |  1008 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1009 | `	"      $this->path = $path;"\` |
|        - |  1010 | `	"   }"\` |
|        - |  1011 | `	"}"\` |
|        - |  1012 | `	"public function __destruct()"\` |
|        - |  1013 | `	"{"\` |
|        - |  1014 | `	"  if( $this->handle != null ){"\` |
|        - |  1015 | `	"       closedir($this->handle);"\` |
|        - |  1016 | `	"  }"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"public function read()"\` |
|        - |  1019 | `	"{"\` |
|        - |  1020 | `	"    return readdir($this->handle);"\` |
|        - |  1021 | `	"}"\` |
|        - |  1022 | `	"public function rewind()"\` |
|        - |  1023 | `	"{"\` |
|        - |  1024 | `	"    rewinddir($this->handle);"\` |
|        - |  1025 | `	"}"\` |
|        - |  1026 | `	"public function close()"\` |
|        - |  1027 | `	"{"\` |
|        - |  1028 | `	"    closedir($this->handle);"\` |
|        - |  1029 | `	"    $this->handle = null;"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"class stdClass{"\` |
|        - |  1033 | `	"  public $value;"\` |
|        - |  1034 | `	" /* Magic methods */"\` |
|        - |  1035 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1036 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1037 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1038 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1039 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"function dir(string $path){"\` |
|        - |  1042 | `	"   return new Directory($path);"\` |
|        - |  1043 | `	"}"\` |
|        - |  1044 | `	"function Dir(string $path){"\` |
|        - |  1045 | `	"   return new Directory($path);"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1048 | `    "{"\` |
|        - |  1049 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1050 | `	"  $aDir = array();"\` |
|        - |  1051 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1052 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1053 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1054 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1055 | `	"   }"\` |
|        - |  1056 | `	"  closedir($pHandle);"\` |
|        - |  1057 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1058 | `	"      rsort($aDir);"\` |
|        - |  1059 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1060 | `	"      sort($aDir);"\` |
|        - |  1061 | `	"  }"\` |
|        - |  1062 | `	"  return $aDir;"\` |
|        - |  1063 | `	"}"\` |
|        - |  1064 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1065 | `	"/* Open the target directory */"\` |
|        - |  1066 | `	"$zDir = dirname($pattern);"\` |
|        - |  1067 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1068 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1069 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1070 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1071 | `	"	return FALSE;"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"$pattern = basename($pattern);"\` |
|        - |  1074 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1075 | `	"/* Loop throw available entries */"\` |
|        - |  1076 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1077 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1078 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1079 | `	"	if( $rc ){"\` |
|        - |  1080 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1081 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1082 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1083 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1084 | `	"		  }"\` |
|        - |  1085 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1086 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1087 | `	"		 continue;"\` |
|        - |  1088 | `	"	   }"\` |
|        - |  1089 | `	"	   /* Add the entry */"\` |
|        - |  1090 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1091 | `	"	}"\` |
|        - |  1092 | `	" }"\` |
|        - |  1093 | `	"/* Close the handle */"\` |
|        - |  1094 | `	"closedir($pHandle);"\` |
|        - |  1095 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1096 | `	"  /* Sort the array */"\` |
|        - |  1097 | `	"  sort($pArray);"\` |
|        - |  1098 | `	"}"\` |
|        - |  1099 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1100 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1101 | `	"  $pArray[] = $pattern;"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"/* Return the created array */"\` |
|        - |  1104 | `	"return $pArray;"\` |
|        - |  1105 | `   "}"\` |
|        - |  1106 | `   "/* Creates a temporary file */"\` |
|        - |  1107 | `   "function tmpfile(){"\` |
|        - |  1108 | `   "  /* Extract the temp directory */"\` |
|        - |  1109 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1110 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1111 | `   "    /* Use the current dir */"\` |
|        - |  1112 | `   "    $zTempDir = '.';"\` |
|        - |  1113 | `   "  }"\` |
|        - |  1114 | `   "  /* Create the file */"\` |
|        - |  1115 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1116 | `   "  return $pHandle;"\` |
|        - |  1117 | `   "}"\` |
|        - |  1118 | `   "/* Creates a temporary filename */"\` |
|        - |  1119 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1120 | `   "{"\` |
|        - |  1121 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1122 | `   "}"\` |
|        - |  1123 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1124 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1125 | `   "/* Copy arguments */"\` |
|        - |  1126 | `   "$nArgs = func_num_args();"\` |
|        - |  1127 | `   "$pNew = array();"\` |
|        - |  1128 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1129 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1130 | `    "}"\` |
|        - |  1131 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1132 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1133 | `	"/* Erase */"\` |
|        - |  1134 | `	"array_erase($pArray);"\` |
|        - |  1135 | `	"/* Unshift */"\` |
|        - |  1136 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1137 | `	"return sizeof($pArray);"\` |
|        - |  1138 | `    "}"\` |
|        - |  1139 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1140 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1141 | `    "$arrays = func_get_args();"\` |
|        - |  1142 | `    "$narrays = count($arrays);"\` |
|        - |  1143 | `    "$ret = $arrays[0];"\` |
|        - |  1144 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1145 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1146 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1147 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1148 | `     "   $ret[] = $value;"\` |
|        - |  1149 | `     "  }else{"\` |
|        - |  1150 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1151 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1152 | `     " }else {"\` |
|        - |  1153 | `     "   $ret[$key] = $value;"\` |
|        - |  1154 | `     "  }"\` |
|        - |  1155 | `     " }"\` |
|        - |  1156 | `     " }"\` |
|        - |  1157 | `	 "}"\` |
|        - |  1158 | `	 " return $ret;"\` |
|        - |  1159 | `    "}"\` |
|        - |  1160 | `	"function max(){"\` |
|        - |  1161 | `    "  $pArgs = func_get_args();"\` |
|        - |  1162 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1163 | `	"  return null;"\` |
|        - |  1164 | `    " }"\` |
|        - |  1165 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1166 | `    " $pArg = $pArgs[0];"\` |
|        - |  1167 | `	" if( !is_array($pArg) ){"\` |
|        - |  1168 | `	"   return $pArg; "\` |
|        - |  1169 | `	" }"\` |
|        - |  1170 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1171 | `	"   return null;"\` |
|        - |  1172 | `	" }"\` |
|        - |  1173 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1174 | `	" reset($pArg);"\` |
|        - |  1175 | `	" $max = current($pArg);"\` |
|        - |  1176 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1177 | `	"   if( $val > $max ){"\` |
|        - |  1178 | `	"     $max = $val;"\` |
|        - |  1179 | `    " }"\` |
|        - |  1180 | `	" }"\` |
|        - |  1181 | `	" return $max;"\` |
|        - |  1182 | `    " }"\` |
|        - |  1183 | `    " $max = $pArgs[0];"\` |
|        - |  1184 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1185 | `    " $val = $pArgs[$i];"\` |
|        - |  1186 | `	"if( $val > $max ){"\` |
|        - |  1187 | `	" $max = $val;"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `    " }"\` |
|        - |  1190 | `	" return $max;"\` |
|        - |  1191 | `    "}"\` |
|        - |  1192 | `	"function min(){"\` |
|        - |  1193 | `    "  $pArgs = func_get_args();"\` |
|        - |  1194 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1195 | `	"  return null;"\` |
|        - |  1196 | `    " }"\` |
|        - |  1197 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1198 | `    " $pArg = $pArgs[0];"\` |
|        - |  1199 | `	" if( !is_array($pArg) ){"\` |
|        - |  1200 | `	"   return $pArg; "\` |
|        - |  1201 | `	" }"\` |
|        - |  1202 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1203 | `	"   return null;"\` |
|        - |  1204 | `	" }"\` |
|        - |  1205 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1206 | `	" reset($pArg);"\` |
|        - |  1207 | `	" $min = current($pArg);"\` |
|        - |  1208 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1209 | `	"   if( $val < $min ){"\` |
|        - |  1210 | `	"     $min = $val;"\` |
|        - |  1211 | `    " }"\` |
|        - |  1212 | `	" }"\` |
|        - |  1213 | `	" return $min;"\` |
|        - |  1214 | `    " }"\` |
|        - |  1215 | `    " $min = $pArgs[0];"\` |
|        - |  1216 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1217 | `    " $val = $pArgs[$i];"\` |
|        - |  1218 | `	"if( $val < $min ){"\` |
|        - |  1219 | `	" $min = $val;"\` |
|        - |  1220 | `	" }"\` |
|        - |  1221 | `    " }"\` |
|        - |  1222 | `	" return $min;"\` |
|        - |  1223 | `	"}"\` |
|        - |  1224 | `	"function fileowner(string $file){"\` |
|        - |  1225 | `    " $a = stat($file);"\` |
|        - |  1226 | `	" if( !is_array($a) ){"\` |
|        - |  1227 | `	"	return false;"\` |
|        - |  1228 | `	" }"\` |
|        - |  1229 | `	" return $a['uid'];"\` |
|        - |  1230 | `    "}"\` |
|        - |  1231 | `    "function filegroup(string $file){"\` |
|        - |  1232 | `	" $a = stat($file);"\` |
|        - |  1233 | `	" if( !is_array($a) ){"\` |
|        - |  1234 | `	"	return false;"\` |
|        - |  1235 | `	" }"\` |
|        - |  1236 | `	" return $a['gid'];"\` |
|        - |  1237 | `    "}"\` |
|        - |  1238 | `	 "function fileinode(string $file){"\` |
|        - |  1239 | `	" $a = stat($file);"\` |
|        - |  1240 | `	" if( !is_array($a) ){"\` |
|        - |  1241 | `	"	return false;"\` |
|        - |  1242 | `	" }"\` |
|        - |  1243 | `	" return $a['ino'];"\` |
|        - |  1244 | `    "}"` |
|        - |  1245 |  |
|        - |  1246 | `/*` |
|        - |  1247 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1248 | ` * start compiling the target PHP program.` |
|        - |  1249 | ` */` |
|     1234 |  1250 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1251 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1252 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1253 | `	 )` |
|        2 |  1254 |  |
|        - |  1255 | `	SyString sBuiltin;` |
|        - |  1256 | `	ph7_value *pObj;` |
|        - |  1257 | `	sxi32 rc;` |
|        - |  1258 | `	/* Zero the structure */` |
|     1236 |  1259 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1260 | `	/* Initialize VM fields */` |
|     1236 |  1261 | `	pVm->pEngine = &(*pEngine);` |
|     1236 |  1262 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1263 | `	/* Instructions containers */` |
|     1236 |  1264 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1236 |  1265 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1236 |  1266 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1267 | `	/* Object containers */` |
|     1236 |  1268 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1236 |  1269 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1270 | `	/* Virtual machine internal containers */` |
|     1236 |  1271 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1236 |  1272 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1236 |  1273 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1236 |  1274 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1236 |  1275 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1236 |  1276 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1236 |  1277 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1236 |  1278 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1236 |  1279 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1236 |  1280 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1236 |  1281 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1236 |  1282 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1236 |  1283 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1236 |  1284 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1236 |  1285 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1286 | `	/* Configuration containers */` |
|     1236 |  1287 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1236 |  1288 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1236 |  1289 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1236 |  1290 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1236 |  1291 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1292 | `	/* Error callbacks containers */` |
|     1236 |  1293 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1236 |  1294 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1236 |  1295 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1236 |  1296 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1236 |  1297 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1298 | `	/* Set a default recursion limit */` |
|        - |  1299 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1236 |  1300 | `	pVm->nMaxDepth = 32;` |
|        - |  1301 | `#else` |
|        - |  1302 | `	pVm->nMaxDepth = 16;` |
|        - |  1303 | `#endif` |
|        - |  1304 | `	/* Default assertion flags */` |
|     1236 |  1305 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1306 | `	/* JSON return status */` |
|     1236 |  1307 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1308 | `	/* PRNG context */` |
|     1236 |  1309 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1310 | `	/* Install the null constant */` |
|     1236 |  1311 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1236 |  1312 | `	if( pObj == 0 ){` |
|      ! 0 |  1313 | `		rc = SXERR_MEM;` |
|      ! 0 |  1314 | `		goto Err;` |
|        - |  1315 | `	}` |
|     1236 |  1316 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1317 | `	/* Install the boolean TRUE constant */` |
|     1236 |  1318 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1236 |  1319 | `	if( pObj == 0 ){` |
|      ! 0 |  1320 | `		rc = SXERR_MEM;` |
|      ! 0 |  1321 | `		goto Err;` |
|        - |  1322 | `	}` |
|     1236 |  1323 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1324 | `	/* Install the boolean FALSE constant */` |
|     1236 |  1325 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1236 |  1326 | `	if( pObj == 0 ){` |
|      ! 0 |  1327 | `		rc = SXERR_MEM;` |
|      ! 0 |  1328 | `		goto Err;` |
|        - |  1329 | `	}` |
|     1236 |  1330 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1331 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1332 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1333 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1236 |  1334 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1236 |  1335 | `	if( pObj == 0 ){` |
|      ! 0 |  1336 | `		rc = SXERR_MEM;` |
|      ! 0 |  1337 | `		goto Err;` |
|        - |  1338 | `	}` |
|     1236 |  1339 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1340 | `	/* Create the global frame */` |
|     1236 |  1341 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1236 |  1342 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1343 | `		goto Err;` |
|        - |  1344 | `	}` |
|        - |  1345 | `	/* Initialize the code generator */` |
|     1236 |  1346 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1236 |  1347 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1348 | `		goto Err;` |
|        - |  1349 | `	}` |
|        - |  1350 | `	/* VM correctly initialized,set the magic number */` |
|     1236 |  1351 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1236 |  1352 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1353 | `	/* Compile the built-in library */` |
|     1236 |  1354 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1355 | `	/* Reset the code generator */` |
|     1236 |  1356 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1236 |  1357 | `	return SXRET_OK;` |
|      ! 0 |  1358 | `Err:` |
|      ! 0 |  1359 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1360 | `	return rc;` |
|      619 |  1361 |  |
|        - |  1362 | `/*` |
|        - |  1363 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1364 | ` * routine which store the output in an internal blob.` |
|        - |  1365 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1366 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1367 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1368 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1369 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1370 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1371 | ` * to finish executing and extracting the output.` |
|        - |  1372 | ` */` |
|      ! 0 |  1373 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1374 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1375 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1376 | `	void *pUserData     /* User private data */` |
|        - |  1377 | `	)` |
|      ! 0 |  1378 |  |
|        - |  1379 | `	 sxi32 rc;` |
|        - |  1380 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1381 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1382 | `	 return rc;` |
|      ! 0 |  1383 |  |
|        - |  1384 | `#define VM_STACK_GUARD 16` |
|        - |  1385 | `/*` |
|        - |  1386 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1387 | ` * our compiled PHP program.` |
|        - |  1388 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1389 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1390 | ` */` |
|    19474 |  1391 | `static ph7_value * VmNewOperandStack(` |
|        - |  1392 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1393 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1394 | `	)` |
|        2 |  1395 |  |
|        - |  1396 | `	ph7_value *pStack;` |
|        - |  1397 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1398 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1399 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1400 | `  ** on the maximum stack depth required.` |
|        - |  1401 | `  **` |
|        - |  1402 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1403 | `  */` |
|    19476 |  1404 | `	nInstr += VM_STACK_GUARD;` |
|    19476 |  1405 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    19476 |  1406 | `	if( pStack == 0 ){` |
|      ! 0 |  1407 | `		return 0;` |
|        - |  1408 | `	}` |
|        - |  1409 | `	/* Initialize the operand stack */` |
|  1249456 |  1410 | `	while( nInstr > 0 ){` |
|  1229982 |  1411 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1229982 |  1412 | `		--nInstr;` |
|        2 |  1413 | `	}` |
|        - |  1414 | `	/* Ready for bytecode execution */` |
|    19476 |  1415 | `	return pStack;` |
|     9739 |  1416 |  |
|        - |  1417 | `/* Forward declaration */` |
|        - |  1418 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1419 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1420 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1421 | `/*` |
|        - |  1422 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1423 | ` * This routine gets called by the PH7 engine after` |
|        - |  1424 | ` * successful compilation of the target PHP program.` |
|        - |  1425 | ` */` |
|      974 |  1426 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1427 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1428 | `	)` |
|        2 |  1429 |  |
|        - |  1430 | `	SyHashEntry *pEntry;` |
|        - |  1431 | `	sxi32 rc;` |
|      976 |  1432 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1433 | `		/* Initialize your VM first */` |
|      ! 0 |  1434 | `		return SXERR_CORRUPT;` |
|        - |  1435 | `	}` |
|        - |  1436 | `	/* Mark the VM ready for byte-code execution */` |
|      976 |  1437 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1438 | `	/* Release the code generator now we have compiled our program */` |
|      976 |  1439 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1440 | `	/* Emit the DONE instruction */` |
|      976 |  1441 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      976 |  1442 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1443 | `		return SXERR_MEM;` |
|        - |  1444 | `	}` |
|        - |  1445 | `	/* Script return value */` |
|      976 |  1446 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1447 | `	/* Allocate a new operand stack */` |
|      976 |  1448 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      976 |  1449 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1450 | `		return SXERR_MEM;` |
|        - |  1451 | `	}` |
|        - |  1452 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1453 | `	 * private data. */` |
|      976 |  1454 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      976 |  1455 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1456 | `	/* Allocate the reference table */` |
|      976 |  1457 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      976 |  1458 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      976 |  1459 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1460 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1461 | `		return SXERR_MEM;` |
|        - |  1462 | `	}` |
|        - |  1463 | `	/* Zero the reference table */` |
|      976 |  1464 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1465 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      976 |  1466 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      976 |  1467 | `	if( rc != SXRET_OK ){` |
|        - |  1468 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1469 | `		return rc;` |
|        - |  1470 | `	}` |
|        - |  1471 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      976 |  1472 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      976 |  1473 | `	if( rc != SXRET_OK ){` |
|        - |  1474 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1475 | `		return rc;` |
|        - |  1476 | `	}` |
|        - |  1477 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      976 |  1478 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1479 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      976 |  1480 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1481 | `	/* Initialize and install static and constants class attributes */` |
|      976 |  1482 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    10738 |  1483 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     9764 |  1484 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     9764 |  1485 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1486 | `			return rc;` |
|        - |  1487 | `		}` |
|        2 |  1488 | `	}` |
|        - |  1489 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      976 |  1490 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1491 | `	/* VM is ready for bytecode execution */` |
|      976 |  1492 | `	return SXRET_OK;` |
|      489 |  1493 |  |
|        - |  1494 | `/*` |
|        - |  1495 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1496 | ` */` |
|      ! 0 |  1497 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1498 |  |
|      ! 0 |  1499 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1500 | `		return SXERR_CORRUPT;` |
|        - |  1501 | `	}` |
|        - |  1502 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1503 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1504 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1505 | `	/* Set the ready flag */` |
|      ! 0 |  1506 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1507 | `	return SXRET_OK;` |
|      ! 0 |  1508 |  |
|        - |  1509 | `/*` |
|        - |  1510 | ` * Release a Virtual Machine.` |
|        - |  1511 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1512 | ` */` |
|      966 |  1513 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1514 |  |
|        - |  1515 | `	/* Set the stale magic number */` |
|      968 |  1516 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1517 | `	/* Release the private memory subsystem */` |
|      968 |  1518 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      968 |  1519 | `	return SXRET_OK;` |
|        2 |  1520 |  |
|        - |  1521 | `/*` |
|        - |  1522 | ` * Initialize a foreign function call context.` |
|        - |  1523 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1524 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1525 | ` * functions.` |
|        - |  1526 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1527 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1528 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1529 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1530 | ` */` |
|   407832 |  1531 | `static sxi32 VmInitCallContext(` |
|        - |  1532 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1533 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1534 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1535 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1536 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1537 | `	)` |
|        2 |  1538 |  |
|   407834 |  1539 | `	pOut->pFunc = pFunc;` |
|   407834 |  1540 | `	pOut->pVm   = pVm;` |
|   407834 |  1541 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   407834 |  1542 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1543 | `	/* Assume a null return value */` |
|   407834 |  1544 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   407834 |  1545 | `	pOut->pRet = pRet;` |
|   407834 |  1546 | `	pOut->iFlags = iFlags;` |
|   407834 |  1547 | `	return SXRET_OK;` |
|        2 |  1548 |  |
|        - |  1549 | `/*` |
|        - |  1550 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1551 | ` * left behind.` |
|        - |  1552 | ` */` |
|   407832 |  1553 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1554 |  |
|        - |  1555 | `	sxu32 n;` |
|   407834 |  1556 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     4474 |  1557 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    12444 |  1558 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     7972 |  1559 | `			if( apObj[n] == 0 ){` |
|        - |  1560 | `				/* Already released */` |
|      250 |  1561 | `				continue;` |
|        - |  1562 | `			}` |
|     7724 |  1563 | `			PH7_MemObjRelease(apObj[n]);` |
|     7724 |  1564 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     3863 |  1565 | `		}` |
|     4474 |  1566 | `		SySetRelease(&pCtx->sVar);` |
|     2236 |  1567 | `	}` |
|   407834 |  1568 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1569 | `		ph7_aux_data *aAux;` |
|        - |  1570 | `		void *pChunk;` |
|        - |  1571 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1572 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1573 | `		 */` |
|        9 |  1574 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1575 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1576 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1577 | `			/* Release the chunk */` |
|       25 |  1578 | `			if( pChunk ){` |
|       25 |  1579 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1580 | `			}` |
|       13 |  1581 | `		}` |
|        9 |  1582 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1583 | `	}` |
|   407834 |  1584 |  |
|        - |  1585 | `/*` |
|        - |  1586 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1587 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1588 | ` */` |
|      248 |  1589 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1590 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1591 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1592 | `	)` |
|        2 |  1593 |  |
|      250 |  1594 | `	if( pValue == 0 ){` |
|        - |  1595 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1596 | `		return;` |
|        - |  1597 | `	}` |
|      250 |  1598 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1599 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1600 | `		sxu32 n;` |
|      936 |  1601 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1602 | `			if( apObj[n] == pValue ){` |
|      250 |  1603 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1604 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1605 | `				/* Mark as released */` |
|      250 |  1606 | `				apObj[n] = 0;` |
|      250 |  1607 | `				break;` |
|        - |  1608 | `			}` |
|      345 |  1609 | `		}` |
|      124 |  1610 | `	}` |
|      126 |  1611 |  |
|        - |  1612 | `/*` |
|        - |  1613 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1614 | ` */` |
|  2233356 |  1615 | `static void VmPopOperand(` |
|        - |  1616 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1617 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1618 | `	)` |
|        2 |  1619 |  |
|  2233358 |  1620 | `	ph7_value *pTos = *ppTos;` |
|  4776298 |  1621 | `	while( nPop > 0 ){` |
|  2542942 |  1622 | `		PH7_MemObjRelease(pTos);` |
|  2542942 |  1623 | `		pTos--;` |
|  2542942 |  1624 | `		nPop--;` |
|        2 |  1625 | `	}` |
|        - |  1626 | `	/* Top of the stack */` |
|  2233358 |  1627 | `	*ppTos = pTos;` |
|  2233358 |  1628 |  |
|        - |  1629 | `/*` |
|        - |  1630 | ` * Reserve a memory object.` |
|        - |  1631 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1632 | ` */` |
|   615240 |  1633 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1634 |  |
|   615242 |  1635 | `	ph7_value *pObj = 0;` |
|        - |  1636 | `	VmSlot *pSlot;` |
|        - |  1637 | `	sxu32 nIdx;` |
|        - |  1638 | `	/* Check for a free slot */` |
|   615242 |  1639 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|   615242 |  1640 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|   615242 |  1641 | `	if( pSlot ){` |
|   538686 |  1642 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   538686 |  1643 | `		nIdx = pSlot->nIdx;` |
|   269342 |  1644 | `	}` |
|   615242 |  1645 | `	if( pObj == 0 ){` |
|        - |  1646 | `		/* Reserve a new memory object */` |
|    76558 |  1647 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|    76558 |  1648 | `		if( pObj == 0 ){` |
|      ! 0 |  1649 | `			return 0;` |
|        - |  1650 | `		}` |
|    38278 |  1651 | `	}` |
|        - |  1652 | `	/* Set a null default value */` |
|   615242 |  1653 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|   615242 |  1654 | `	pObj->nIdx = nIdx;` |
|   615242 |  1655 | `	return pObj;` |
|   307622 |  1656 |  |
|        - |  1657 | `/*` |
|        - |  1658 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1659 | ` */` |
|    14352 |  1660 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1661 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1662 | `	const char *zKey,  /* Entry key */` |
|        - |  1663 | `	sxu32 nByte,       /* Key length */` |
|        - |  1664 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1665 | `	)` |
|        2 |  1666 |  |
|        - |  1667 | `	ph7_value sKey;` |
|        - |  1668 | `	sxi32 rc;` |
|    14354 |  1669 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    14354 |  1670 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1671 | `	/* Perform the insertion */` |
|    14354 |  1672 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    14354 |  1673 | `	PH7_MemObjRelease(&sKey);` |
|    14354 |  1674 | `	return rc;` |
|        2 |  1675 |  |
|        - |  1676 | `/*` |
|        - |  1677 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1678 | ` * Return a pointer to the variable value on success.` |
|        - |  1679 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1680 | ` */` |
|  2026686 |  1681 | `static ph7_value * VmExtractMemObj(` |
|        - |  1682 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1683 | `	const SyString *pName, /* Variable name */` |
|        - |  1684 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1685 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1686 | `	)` |
|        2 |  1687 |  |
|  2026688 |  1688 | `	int bNullify = FALSE;` |
|        - |  1689 | `	SyHashEntry *pEntry;` |
|        - |  1690 | `	VmFrame *pFrame;` |
|        - |  1691 | `	ph7_value *pObj;` |
|        - |  1692 | `	sxu32 nIdx;` |
|        - |  1693 | `	sxi32 rc;` |
|        - |  1694 | `	/* Point to the top active frame */` |
|  2026688 |  1695 | `	pFrame = pVm->pFrame;` |
|  2076948 |  1696 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1697 | `		/* Safely ignore the exception frame */` |
|    50261 |  1698 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1699 | `	}` |
|        - |  1700 | `	/* Perform the lookup */` |
|  2026688 |  1701 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1702 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1703 | `		pName = &sAnnon;` |
|        - |  1704 | `		/* Always nullify the object */` |
|      ! 0 |  1705 | `		bNullify = TRUE;` |
|      ! 0 |  1706 | `		bDup = FALSE;` |
|      ! 0 |  1707 | `	}` |
|        - |  1708 | `	/* Check the superglobals table first */` |
|  2026688 |  1709 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2026688 |  1710 | `	if( pEntry == 0 ){` |
|        - |  1711 | `		/* Query the top active frame */` |
|  2026652 |  1712 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2026652 |  1713 | `		if( pEntry == 0 ){` |
|    55304 |  1714 | `			char *zName = (char *)pName->zString;` |
|        - |  1715 | `			VmSlot sLocal;` |
|    55304 |  1716 | `			if( !bCreate ){` |
|        - |  1717 | `				/* Do not create the variable,return NULL instead */` |
|      466 |  1718 | `				return 0;` |
|        - |  1719 | `			}` |
|        - |  1720 | `			/* No such variable,automatically create a new one and install` |
|        - |  1721 | `			 * it in the current frame.` |
|        - |  1722 | `			 */` |
|    54840 |  1723 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    54840 |  1724 | `			if( pObj == 0 ){` |
|      ! 0 |  1725 | `				return 0;` |
|        - |  1726 | `			}` |
|    54840 |  1727 | `			nIdx = pObj->nIdx;` |
|    54840 |  1728 | `			if( bDup ){` |
|        - |  1729 | `				/* Duplicate name */` |
|      115 |  1730 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1731 | `				if( zName == 0 ){` |
|      ! 0 |  1732 | `					return 0;` |
|        - |  1733 | `				}` |
|       57 |  1734 | `			}` |
|        - |  1735 | `			/* Link to the top active VM frame */` |
|    54840 |  1736 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    54840 |  1737 | `			if( rc != SXRET_OK ){` |
|        - |  1738 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1739 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1740 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1741 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1742 | `				return 0;` |
|        - |  1743 | `			}` |
|    54840 |  1744 | `			if( pFrame->pParent != 0 ){` |
|        - |  1745 | `				/* Local variable */` |
|    50252 |  1746 | `				sLocal.nIdx = nIdx;` |
|    50252 |  1747 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    25127 |  1748 | `			}else{` |
|        - |  1749 | `				/* Register in the $GLOBALS array */` |
|     4590 |  1750 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1751 | `			}` |
|        - |  1752 | `			/* Install in the reference table */` |
|    54840 |  1753 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1754 | `			/* Save object index */` |
|    54840 |  1755 | `			pObj->nIdx = nIdx;` |
|    27421 |  1756 | `		}else{` |
|        - |  1757 | `			/* Extract variable contents */` |
|  1971350 |  1758 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  1971350 |  1759 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  1971350 |  1760 | `			if( bNullify && pObj ){` |
|      ! 0 |  1761 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1762 | `			}` |
|        - |  1763 | `		}` |
|  1013205 |  1764 | `	}else{` |
|        - |  1765 | `		/* Superglobal */` |
|       38 |  1766 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1767 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1768 | `	}` |
|  2026224 |  1769 | `	return pObj;` |
|  1013455 |  1770 |  |
|        - |  1771 | `/*` |
|        - |  1772 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1773 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1774 | ` */` |
|      992 |  1775 | `static ph7_value * VmExtractSuper(` |
|        - |  1776 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1777 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1778 | `	sxu32 nByte        /* zName length */` |
|        - |  1779 | `	)` |
|        2 |  1780 |  |
|        - |  1781 | `	SyHashEntry *pEntry;` |
|        - |  1782 | `	ph7_value *pValue;` |
|        - |  1783 | `	sxu32 nIdx;` |
|        - |  1784 | `	/* Query the superglobal table */` |
|      994 |  1785 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      994 |  1786 | `	if( pEntry == 0 ){` |
|        - |  1787 | `		/* No such entry */` |
|      ! 0 |  1788 | `		return 0;` |
|        - |  1789 | `	}` |
|        - |  1790 | `	/* Extract the superglobal index in the global object pool */` |
|      994 |  1791 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1792 | `	/* Extract the variable value  */` |
|      994 |  1793 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      994 |  1794 | `	return pValue;` |
|      498 |  1795 |  |
|        - |  1796 | `/*` |
|        - |  1797 | ` * Perform a raw hashmap insertion.` |
|        - |  1798 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1799 | ` */` |
|      990 |  1800 | `static sxi32 VmHashmapInsert(` |
|        - |  1801 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1802 | `	const char *zKey,   /* Entry key */` |
|        - |  1803 | `	int nKeylen,        /* zKey length*/` |
|        - |  1804 | `	const char *zData,  /* Entry data */` |
|        - |  1805 | `	int nLen            /* zData length */` |
|        - |  1806 | `	)` |
|        2 |  1807 |  |
|        - |  1808 | `	ph7_value sKey,sValue;` |
|        - |  1809 | `	sxi32 rc;` |
|      992 |  1810 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      992 |  1811 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|      992 |  1812 | `	if( zKey ){` |
|      978 |  1813 | `		if( nKeylen < 0 ){` |
|      978 |  1814 | `			nKeylen = (int)SyStrlen(zKey);` |
|      488 |  1815 | `		}` |
|      978 |  1816 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      488 |  1817 | `	}` |
|      992 |  1818 | `	if( zData ){` |
|      992 |  1819 | `		if( nLen < 0 ){` |
|        - |  1820 | `			/* Compute length automatically */` |
|      ! 0 |  1821 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1822 | `		}` |
|      992 |  1823 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      495 |  1824 | `	}` |
|        - |  1825 | `	/* Perform the insertion */` |
|      992 |  1826 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|      992 |  1827 | `	PH7_MemObjRelease(&sKey);` |
|      992 |  1828 | `	PH7_MemObjRelease(&sValue);` |
|      992 |  1829 | `	return rc;` |
|        2 |  1830 |  |
|        - |  1831 | `/* Forward declaration */` |
|        - |  1832 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1833 | `/*` |
|        - |  1834 | ` * Configure a working virtual machine instance.` |
|        - |  1835 | ` *` |
|        - |  1836 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1837 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1838 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1839 | ` * The second argument to this function is an integer configuration option` |
|        - |  1840 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1841 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1842 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1843 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1844 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1845 | ` */` |
|    15600 |  1846 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1847 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1848 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1849 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1850 | `	)` |
|        2 |  1851 |  |
|    15602 |  1852 | `	sxi32 rc = SXRET_OK;` |
|    15602 |  1853 | `	switch(nOp){` |
|      487 |  1854 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      976 |  1855 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      976 |  1856 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1857 | `		/* VM output consumer callback */` |
|        - |  1858 | `#ifdef UNTRUST` |
|        - |  1859 | `		if( xConsumer == 0 ){` |
|        - |  1860 | `			rc = SXERR_CORRUPT;` |
|        - |  1861 | `			break;` |
|        - |  1862 | `		}` |
|        - |  1863 | `#endif` |
|        - |  1864 | `		/* Install the output consumer */` |
|      976 |  1865 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      976 |  1866 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      976 |  1867 | `		break;` |
|        - |  1868 | `							   }` |
|      487 |  1869 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1870 | `		/* Import path */` |
|        - |  1871 | `		  const char *zPath;` |
|        - |  1872 | `		  SyString sPath;` |
|      976 |  1873 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1874 | `#if defined(UNTRUST)` |
|        - |  1875 | `		  if( zPath == 0 ){` |
|        - |  1876 | `			  rc = SXERR_EMPTY;` |
|        - |  1877 | `			  break;` |
|        - |  1878 | `		  }` |
|        - |  1879 | `#endif` |
|      976 |  1880 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1881 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1882 | `#ifdef __WINNT__` |
|        2 |  1883 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1884 | `#endif` |
|     1950 |  1885 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1886 | `		  /* Remove leading and trailing white spaces */` |
|      976 |  1887 | `		  SyStringFullTrim(&sPath);` |
|      976 |  1888 | `		  if( sPath.nByte > 0 ){` |
|        - |  1889 | `			  /* Store the path in the corresponding conatiner */` |
|      976 |  1890 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      487 |  1891 | `		  }` |
|      976 |  1892 | `		  break;` |
|        - |  1893 | `									 }` |
|      487 |  1894 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1895 | `		/* Run-Time Error report */` |
|      976 |  1896 | `		pVm->bErrReport = 1;` |
|      976 |  1897 | `		break;` |
|      ! 0 |  1898 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1899 | `		/* Recursion depth */` |
|      ! 0 |  1900 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1901 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1902 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1903 | `		}` |
|      ! 0 |  1904 | `		break;` |
|        - |  1905 | `									   }` |
|      ! 0 |  1906 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1907 | `		/* VM output length in bytes */` |
|      ! 0 |  1908 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1909 | `#ifdef UNTRUST` |
|        - |  1910 | `		if( pOut == 0 ){` |
|        - |  1911 | `			rc = SXERR_CORRUPT;` |
|        - |  1912 | `			break;` |
|        - |  1913 | `		}` |
|        - |  1914 | `#endif` |
|      ! 0 |  1915 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1916 | `		break;` |
|        - |  1917 | `							   }` |
|        - |  1918 |  |
|     4870 |  1919 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1920 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1921 | `		/* Create a new superglobal/global variable */` |
|     9742 |  1922 | `		const char *zName = va_arg(ap,const char *);` |
|     9742 |  1923 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1924 | `		SyHashEntry *pEntry;` |
|        - |  1925 | `		ph7_value *pObj;` |
|        - |  1926 | `		sxu32 nByte;` |
|        - |  1927 | `		sxu32 nIdx;` |
|        - |  1928 | `#ifdef UNTRUST` |
|        - |  1929 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1930 | `			rc = SXERR_CORRUPT;` |
|        - |  1931 | `			break;` |
|        - |  1932 | `		}` |
|        - |  1933 | `#endif` |
|     9742 |  1934 | `		nByte = SyStrlen(zName);` |
|     9742 |  1935 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1936 | `			/* Check if the superglobal is already installed */` |
|     9742 |  1937 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     4872 |  1938 | `		}else{` |
|        - |  1939 | `			/* Query the top active VM frame */` |
|      ! 0 |  1940 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1941 | `		}` |
|     9742 |  1942 | `		if( pEntry ){` |
|        - |  1943 | `			/* Variable already installed */` |
|      ! 0 |  1944 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1945 | `			/* Extract contents */` |
|      ! 0 |  1946 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1947 | `			if( pObj ){` |
|        - |  1948 | `				/* Overwrite old contents */` |
|      ! 0 |  1949 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1950 | `			}` |
|      ! 0 |  1951 | `		}else{` |
|        - |  1952 | `			/* Install a new variable */` |
|     9742 |  1953 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     9742 |  1954 | `			if( pObj == 0 ){` |
|      ! 0 |  1955 | `				rc = SXERR_MEM;` |
|      ! 0 |  1956 | `				break;` |
|        - |  1957 | `			}` |
|     9742 |  1958 | `			nIdx = pObj->nIdx;` |
|        - |  1959 | `			/* Copy value */` |
|     9742 |  1960 | `			PH7_MemObjStore(pValue,pObj);` |
|     9742 |  1961 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1962 | `				/* Install the superglobal */` |
|     9742 |  1963 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     4872 |  1964 | `			}else{` |
|        - |  1965 | `				/* Install in the current frame */` |
|      ! 0 |  1966 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1967 | `			}` |
|     9742 |  1968 | `			if( rc == SXRET_OK ){` |
|        - |  1969 | `				SyHashEntry *pRef;` |
|     9742 |  1970 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     9742 |  1971 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     4872 |  1972 | `				}else{` |
|      ! 0 |  1973 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1974 | `				}` |
|        - |  1975 | `				/* Install in the reference table */` |
|     9742 |  1976 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     9742 |  1977 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1978 | `					/* Register in the $GLOBALS array */` |
|     9742 |  1979 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     4870 |  1980 | `				}` |
|     4870 |  1981 | `			}` |
|        - |  1982 | `		}` |
|     9742 |  1983 | `		break;` |
|        - |  1984 | `									}` |
|      488 |  1985 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1986 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1987 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1988 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1989 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1990 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1991 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|      978 |  1992 | `		const char *zKey   = va_arg(ap,const char *);` |
|      978 |  1993 | `		const char *zValue = va_arg(ap,const char *);` |
|      978 |  1994 | `		int nLen = va_arg(ap,int);` |
|        - |  1995 | `		ph7_hashmap *pMap;` |
|        - |  1996 | `		ph7_value *pValue;` |
|      978 |  1997 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1998 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1999 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|      977 |  2000 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2001 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2002 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      976 |  2003 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2004 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2005 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|      976 |  2006 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2007 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2008 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      976 |  2009 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2010 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2011 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|      976 |  2012 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2013 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2014 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2015 | `		}else{` |
|        - |  2016 | `			/* Extract the $_SERVER superglobal */` |
|      976 |  2017 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2018 | `		}` |
|      978 |  2019 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2020 | `			/* No such entry */` |
|      ! 0 |  2021 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2022 | `			break;` |
|        - |  2023 | `		}` |
|        - |  2024 | `		/* Point to the hashmap */` |
|      978 |  2025 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2026 | `		/* Perform the insertion */` |
|      978 |  2027 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|      978 |  2028 | `		break;` |
|        - |  2029 | `								   }` |
|        7 |  2030 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2031 | `		/* Script arguments */` |
|       16 |  2032 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2033 | `		ph7_hashmap *pMap;` |
|        - |  2034 | `		ph7_value *pValue;` |
|        - |  2035 | `		sxu32 n;` |
|       16 |  2036 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2037 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2038 | `			break;` |
|        - |  2039 | `		}` |
|        - |  2040 | `		/* Extract the $argv array */` |
|       16 |  2041 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       16 |  2042 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2043 | `			/* No such entry */` |
|      ! 0 |  2044 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2045 | `			break;` |
|        - |  2046 | `		}` |
|        - |  2047 | `		/* Point to the hashmap */` |
|       16 |  2048 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2049 | `		/* Perform the insertion */` |
|       16 |  2050 | `		n = (sxu32)SyStrlen(zValue);` |
|       16 |  2051 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       16 |  2052 | `		if( rc == SXRET_OK ){` |
|       16 |  2053 | `			if( pMap->nEntry > 1 ){` |
|        - |  2054 | `				/* Append space separator first */` |
|       10 |  2055 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        4 |  2056 | `			}` |
|       16 |  2057 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|        7 |  2058 | `		}` |
|       16 |  2059 | `		break;` |
|        - |  2060 | `								  }` |
|      ! 0 |  2061 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2062 | `		/* error_log() consumer */` |
|      ! 0 |  2063 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2064 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2065 | `		break;` |
|        - |  2066 | `										}` |
|      ! 0 |  2067 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2068 | `		/* Script return value */` |
|      ! 0 |  2069 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2070 | `#ifdef UNTRUST` |
|        - |  2071 | `		if( ppValue == 0 ){` |
|        - |  2072 | `			rc = SXERR_CORRUPT;` |
|        - |  2073 | `			break;` |
|        - |  2074 | `		}` |
|        - |  2075 | `#endif` |
|      ! 0 |  2076 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2077 | `		break;` |
|        - |  2078 | `								   }` |
|      974 |  2079 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2080 | `		/* Register an IO stream device */` |
|     1950 |  2081 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2082 | `		/* Make sure we are dealing with a valid IO stream */` |
|     2922 |  2083 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     1950 |  2084 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2085 | `				/* Invalid stream */` |
|      ! 0 |  2086 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2087 | `				break;` |
|        - |  2088 | `		}` |
|     1950 |  2089 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2090 | `			/* Make the 'file://' stream the defaut stream device */` |
|      976 |  2091 | `			pVm->pDefStream = pStream;` |
|      487 |  2092 | `		}` |
|        - |  2093 | `		/* Insert in the appropriate container */` |
|     1950 |  2094 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     1950 |  2095 | `		break;` |
|        - |  2096 | `								  }` |
|      ! 0 |  2097 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2098 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2099 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2100 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2101 | `#ifdef UNTRUST` |
|        - |  2102 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2103 | `			rc = SXERR_CORRUPT;` |
|        - |  2104 | `			break;` |
|        - |  2105 | `		}` |
|        - |  2106 | `#endif` |
|      ! 0 |  2107 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2108 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2109 | `		break;` |
|        - |  2110 | `									   }` |
|      ! 0 |  2111 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2112 | `		/* Raw HTTP request*/` |
|      ! 0 |  2113 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2114 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2115 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2116 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2117 | `			break;` |
|        - |  2118 | `		}` |
|      ! 0 |  2119 | `		if( nByte < 0 ){` |
|        - |  2120 | `			/* Compute length automatically */` |
|      ! 0 |  2121 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2122 | `		}` |
|        - |  2123 | `		/* Process the request */` |
|      ! 0 |  2124 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2125 | `		break;` |
|        - |  2126 | `									}` |
|      ! 0 |  2127 | `	default:` |
|        - |  2128 | `		/* Unknown configuration option */` |
|      ! 0 |  2129 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2130 | `		break;` |
|        - |  2131 | `	}` |
|    15602 |  2132 | `	return rc;` |
|        2 |  2133 |  |
|        - |  2134 | `/* Forward declaration */` |
|        - |  2135 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2136 | `/*` |
|        - |  2137 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2138 | ` * format.` |
|        - |  2139 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2140 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2141 | ` * (STDOUT).` |
|        - |  2142 | ` */` |
|        2 |  2143 | `static sxi32 VmByteCodeDump(` |
|        - |  2144 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2145 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2146 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2147 | `	)` |
|        1 |  2148 |  |
|        - |  2149 | `	static const char zDump[] = {` |
|        - |  2150 | `		"====================================================\n"` |
|        - |  2151 | `		"PH7 VM Dump\n"` |
|        - |  2152 | `		"====================================================\n"` |
|        - |  2153 | `	};` |
|        - |  2154 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2155 | `	sxi32 rc = SXRET_OK;` |
|        - |  2156 | `	sxu32 n;` |
|        - |  2157 | `	/* Point to the PH7 instructions */` |
|        3 |  2158 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2159 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2160 | `	n = 0;` |
|        3 |  2161 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2162 | `	/* Dump instructions */` |
|        6 |  2163 | `	for(;;){` |
|       13 |  2164 | `		if( pInstr >= pEnd ){` |
|        - |  2165 | `			/* No more instructions */` |
|        3 |  2166 | `			break;` |
|        - |  2167 | `		}` |
|        - |  2168 | `		/* Format and call the consumer callback */` |
|       16 |  2169 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2170 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2171 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2172 | `		if( rc != SXRET_OK ){` |
|        - |  2173 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2174 | `			return rc;` |
|        - |  2175 | `		}` |
|       11 |  2176 | `		++n;` |
|       11 |  2177 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2178 | `	}` |
|        3 |  2179 | `	return rc;` |
|        2 |  2180 |  |
|        - |  2181 | `/* Forward declaration */` |
|        - |  2182 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2183 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2184 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2185 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2186 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2187 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2188 | `/*` |
|        - |  2189 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2190 | ` * consumer callback.` |
|        - |  2191 | ` */` |
|      100 |  2192 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        2 |  2193 |  |
|      102 |  2194 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      102 |  2195 | `	sxi32 rc = SXRET_OK;` |
|        - |  2196 | `	/* Append a new line */` |
|        - |  2197 | `#ifdef __WINNT__` |
|        2 |  2198 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2199 | `#else` |
|      100 |  2200 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2201 | `#endif` |
|        - |  2202 | `	/* Invoke the output consumer callback */` |
|      102 |  2203 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      102 |  2204 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2205 | `		/* Increment output length */` |
|      101 |  2206 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|       50 |  2207 | `	}` |
|      102 |  2208 | `	return rc;` |
|        2 |  2209 |  |
|        - |  2210 | `/*` |
|        - |  2211 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2212 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2213 | ` * information.` |
|        - |  2214 | ` */` |
|       86 |  2215 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)` |
|        2 |  2216 |  |
|       88 |  2217 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2218 | `		ph7_value apArg[4];` |
|        - |  2219 | `		ph7_value *apArgPtr[4];` |
|        - |  2220 | `		ph7_value sResult;` |
|        - |  2221 | `		SyString sErr;` |
|        - |  2222 | `		/* Prepare arguments */` |
|        9 |  2223 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        9 |  2224 | `		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));` |
|        9 |  2225 | `		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|        9 |  2226 | `		if( pFile ){` |
|        9 |  2227 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|        9 |  2228 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|        5 |  2229 | `		}else{` |
|      ! 0 |  2230 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2231 | `		}` |
|        9 |  2232 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|        9 |  2233 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2234 | `		/* Set up pointer array */` |
|        9 |  2235 | `		apArgPtr[0] = &apArg[0];` |
|        9 |  2236 | `		apArgPtr[1] = &apArg[1];` |
|        9 |  2237 | `		apArgPtr[2] = &apArg[2];` |
|        9 |  2238 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2239 | `		/* Call the handler */` |
|        9 |  2240 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2241 | `		/* Check return value */` |
|        9 |  2242 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2243 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2244 | `		}` |
|        - |  2245 | `		/* Release */` |
|        9 |  2246 | `		PH7_MemObjRelease(&apArg[0]);` |
|        9 |  2247 | `		PH7_MemObjRelease(&apArg[1]);` |
|        9 |  2248 | `		PH7_MemObjRelease(&apArg[2]);` |
|        9 |  2249 | `		PH7_MemObjRelease(&apArg[3]);` |
|        9 |  2250 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2251 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2252 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|        9 |  2253 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2254 | `	}` |
|        - |  2255 | `	/* No handler, always call error handler */` |
|       80 |  2256 | `	return TRUE;` |
|       45 |  2257 |  |
|       62 |  2258 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2259 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2260 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2261 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2262 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2263 | `	)` |
|        2 |  2264 |  |
|       64 |  2265 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2266 | `	SyString *pFile;` |
|        - |  2267 | `	char *zErr;` |
|       64 |  2268 | `	sxi32 rc = SXRET_OK;` |
|       64 |  2269 | `	if( !pVm->bErrReport ){` |
|        - |  2270 | `		/* Don't bother reporting errors */` |
|        3 |  2271 | `		return SXRET_OK;` |
|        - |  2272 | `	}` |
|        - |  2273 | `	/* Reset the working buffer */` |
|       62 |  2274 | `	SyBlobReset(pWorker);` |
|        - |  2275 | `	/* Peek the processed file if available */` |
|       62 |  2276 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       62 |  2277 | `	if( pFile ){` |
|        - |  2278 | `		/* Append file name */` |
|       62 |  2279 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       62 |  2280 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       30 |  2281 | `	}` |
|       62 |  2282 | `	zErr = "Error: ";` |
|       62 |  2283 | `	switch(iErr){` |
|       27 |  2284 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|       14 |  2285 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|       11 |  2286 | `	default:` |
|       23 |  2287 | `		iErr = PH7_CTX_ERR;` |
|       22 |  2288 | `		break;` |
|        - |  2289 | `	}` |
|       62 |  2290 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       62 |  2291 | `	if( pFuncName ){` |
|        - |  2292 | `		/* Append function name first */` |
|       29 |  2293 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2294 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2295 | `	}` |
|       62 |  2296 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2297 | `	/* Check for user error handler */` |
|       62 |  2298 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){` |
|       53 |  2299 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2300 | `	}` |
|       62 |  2301 | `	return rc;` |
|       33 |  2302 |  |
|        - |  2303 | `/*` |
|        - |  2304 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2305 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2306 | ` * information.` |
|        - |  2307 | ` */` |
|       26 |  2308 | `static sxi32 VmThrowErrorAp(` |
|        - |  2309 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2310 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2311 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2312 | `	const char *zFormat, /* Format message */` |
|        - |  2313 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2314 | `	)` |
|        2 |  2315 |  |
|       28 |  2316 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2317 | `	SyBlob sMsg;` |
|        - |  2318 | `	SyString *pFile;` |
|        - |  2319 | `	char *zErr;` |
|       28 |  2320 | `	sxi32 rc = SXRET_OK;` |
|       28 |  2321 | `	if( !pVm->bErrReport ){` |
|        - |  2322 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2323 | `		return SXRET_OK;` |
|        - |  2324 | `	}` |
|        - |  2325 | `	/* Reset the working buffer */` |
|       28 |  2326 | `	SyBlobReset(pWorker);` |
|        - |  2327 | `	/* Peek the processed file if available */` |
|       28 |  2328 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       28 |  2329 | `	if( pFile ){` |
|        - |  2330 | `		/* Append file name */` |
|       28 |  2331 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       28 |  2332 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       13 |  2333 | `	}` |
|       28 |  2334 | `	zErr = "Error: ";` |
|       28 |  2335 | `	switch(iErr){` |
|       10 |  2336 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|        7 |  2337 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|        6 |  2338 | `	default:` |
|       13 |  2339 | `		iErr = PH7_CTX_ERR;` |
|       12 |  2340 | `		break;` |
|        - |  2341 | `	}` |
|       28 |  2342 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       28 |  2343 | `	if( pFuncName ){` |
|        - |  2344 | `		/* Append function name first */` |
|       14 |  2345 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       14 |  2346 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|        6 |  2347 | `	}` |
|        - |  2348 | `	/* Format the raw message */` |
|       28 |  2349 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       28 |  2350 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2351 | `	/* Check if a user error handler is installed */` |
|       28 |  2352 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){` |
|        - |  2353 | `		/* No handler or handler returned TRUE, normal processing */` |
|       28 |  2354 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       28 |  2355 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2356 | `	}` |
|       28 |  2357 | `	SyBlobRelease(&sMsg);` |
|       28 |  2358 | `	return rc;` |
|       15 |  2359 |  |
|        - |  2360 | `/*` |
|        - |  2361 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2362 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2363 | ` * information.` |
|        - |  2364 | ` * ------------------------------------` |
|        - |  2365 | ` * Simple boring wrapper function.` |
|        - |  2366 | ` * ------------------------------------` |
|        - |  2367 | ` */` |
|       14 |  2368 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2369 |  |
|        - |  2370 | `	va_list ap;` |
|        - |  2371 | `	sxi32 rc;` |
|       15 |  2372 | `	va_start(ap,zFormat);` |
|       15 |  2373 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2374 | `	va_end(ap);` |
|       15 |  2375 | `	return rc;` |
|        1 |  2376 |  |
|        - |  2377 | `/*` |
|        - |  2378 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2379 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2380 | ` * information.` |
|        - |  2381 | ` * ------------------------------------` |
|        - |  2382 | ` * Simple boring wrapper function.` |
|        - |  2383 | ` * ------------------------------------` |
|        - |  2384 | ` */` |
|       12 |  2385 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2386 |  |
|        - |  2387 | `	sxi32 rc;` |
|       14 |  2388 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       14 |  2389 | `	return rc;` |
|        2 |  2390 |  |
|        - |  2391 | `/*` |
|        - |  2392 | ` * Resolve function context from the current frame.` |
|        - |  2393 | ` */` |
|       44 |  2394 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2395 |  |
|        - |  2396 | `	VmFrame *pFrame;` |
|        - |  2397 | `	ph7_vm_func *pFunc;` |
|       45 |  2398 | `	*pzFuncName = 0;` |
|       45 |  2399 | `	*pnFuncLen = 0;` |
|       45 |  2400 | `	pFrame = pVm->pFrame;` |
|       45 |  2401 | `	if( pFrame == 0 ){` |
|      ! 0 |  2402 | `		return;` |
|        - |  2403 | `	}` |
|       45 |  2404 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2405 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2406 | `	}` |
|       45 |  2407 | `	if( pFrame->pParent == 0 ){` |
|       45 |  2408 | `		return;` |
|        - |  2409 | `	}` |
|      ! 0 |  2410 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  2411 | `	if( pFunc == 0 ){` |
|      ! 0 |  2412 | `		return;` |
|        - |  2413 | `	}` |
|      ! 0 |  2414 | `	*pzFuncName = pFunc->sName.zString;` |
|      ! 0 |  2415 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|       23 |  2416 |  |
|        - |  2417 | `/*` |
|        - |  2418 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2419 | ` */` |
|       22 |  2420 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2421 |  |
|        - |  2422 | `	SyBlob sOut;` |
|        - |  2423 | `	SyString *pFile;` |
|       23 |  2424 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2425 | `		return PH7_OK;` |
|        - |  2426 | `	}` |
|       23 |  2427 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2428 | `		zClass = "Exception";` |
|      ! 0 |  2429 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2430 | `	}` |
|       23 |  2431 | `	if( zMsg == 0 ){` |
|      ! 0 |  2432 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2433 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2434 | `	}` |
|       23 |  2435 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|       23 |  2436 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       11 |  2437 | `	}` |
|       23 |  2438 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       23 |  2439 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|       23 |  2440 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|       23 |  2441 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|       23 |  2442 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|       23 |  2443 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|       23 |  2444 | `	if( pFile ){` |
|       23 |  2445 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|       23 |  2446 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       23 |  2447 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|       11 |  2448 | `	}` |
|       23 |  2449 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       23 |  2450 | `	if( pFile ){` |
|       23 |  2451 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|       23 |  2452 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       23 |  2453 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2454 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2455 | `		}else{` |
|       23 |  2456 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2457 | `		}` |
|       11 |  2458 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2459 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2460 | `	}else{` |
|      ! 0 |  2461 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2462 | `	}` |
|       23 |  2463 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|       23 |  2464 | `	if( pFile ){` |
|       23 |  2465 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|       23 |  2466 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|       23 |  2467 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|       23 |  2468 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|       11 |  2469 | `	}` |
|       23 |  2470 | `	VmCallErrorHandler(pVm,&sOut);` |
|       23 |  2471 | `	SyBlobRelease(&sOut);` |
|       23 |  2472 | `	return PH7_ABORT;` |
|       12 |  2473 |  |
|        - |  2474 | `/*` |
|        - |  2475 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2476 | ` */` |
|       22 |  2477 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2478 |  |
|        - |  2479 | `	ph7_vm *pVm;` |
|        - |  2480 | `	ph7_class *pClass;` |
|        - |  2481 | `	ph7_class_instance *pThis;` |
|        - |  2482 | `	ph7_class_method *pCons;` |
|        - |  2483 | `	ph7_value sArg;` |
|        - |  2484 | `	ph7_value *apArg[1];` |
|        - |  2485 | `	SyBlob sMsg;` |
|        - |  2486 | `	SyString sMsgStr;` |
|        - |  2487 | `	VmFrame *pFrame;` |
|        - |  2488 | `	va_list ap;` |
|        - |  2489 | `	sxi32 rc;` |
|        - |  2490 |  |
|       24 |  2491 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2492 | `		return PH7_ABORT;` |
|        - |  2493 | `	}` |
|       24 |  2494 | `	pVm = pCtx->pVm;` |
|       24 |  2495 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2496 | `		zClass = "Error";` |
|      ! 0 |  2497 | `	}` |
|       24 |  2498 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|       24 |  2499 | `	if( pClass == 0 ){` |
|      ! 0 |  2500 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2501 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2502 | `			zClass` |
|        - |  2503 | `			);` |
|        - |  2504 | `	}` |
|       24 |  2505 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       24 |  2506 | `	if( pThis == 0 ){` |
|      ! 0 |  2507 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2508 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2509 | `			);` |
|        - |  2510 | `	}` |
|        - |  2511 |  |
|       24 |  2512 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       24 |  2513 | `	va_start(ap,zFormat);` |
|       24 |  2514 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       24 |  2515 | `	va_end(ap);` |
|        - |  2516 |  |
|       24 |  2517 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       24 |  2518 | `	if( pCons ){` |
|       24 |  2519 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       24 |  2520 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       24 |  2521 | `		apArg[0] = &sArg;` |
|       24 |  2522 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       24 |  2523 | `		PH7_MemObjRelease(&sArg);` |
|       11 |  2524 | `	}` |
|       24 |  2525 | `	SyBlobRelease(&sMsg);` |
|        - |  2526 |  |
|       24 |  2527 | `	pFrame = pVm->pFrame;` |
|       24 |  2528 | `	if( pFrame ){` |
|       26 |  2529 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2530 | `			pFrame = pFrame->pParent;` |
|        1 |  2531 | `		}` |
|       24 |  2532 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       11 |  2533 | `	}` |
|       24 |  2534 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  2535 | `	PH7_ClassInstanceUnref(pThis);` |
|       24 |  2536 | `	if( rc == SXERR_ABORT ){` |
|       21 |  2537 | `		return PH7_ABORT;` |
|        - |  2538 | `	}` |
|        3 |  2539 | `	return PH7_EXCEPTION;` |
|       13 |  2540 |  |
|        - |  2541 | `/*` |
|        - |  2542 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2543 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2544 | ` */` |
|      ! 0 |  2545 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2546 |  |
|        - |  2547 | `	ph7_vm *pVm;` |
|        - |  2548 | `	SyBlob sMsg;` |
|      ! 0 |  2549 | `	const char *zFuncName = 0;` |
|      ! 0 |  2550 | `	int nFuncLen = 0;` |
|        - |  2551 | `	va_list ap;` |
|        - |  2552 | `	sxi32 rc;` |
|        - |  2553 |  |
|      ! 0 |  2554 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2555 | `		return PH7_OK;` |
|        - |  2556 | `	}` |
|      ! 0 |  2557 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2558 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2559 | `		zClass = "Error";` |
|      ! 0 |  2560 | `	}` |
|        - |  2561 |  |
|      ! 0 |  2562 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2563 |  |
|      ! 0 |  2564 | `	va_start(ap,zFormat);` |
|      ! 0 |  2565 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2566 | `	va_end(ap);` |
|        - |  2567 |  |
|      ! 0 |  2568 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2569 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2570 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2571 | `	}` |
|      ! 0 |  2572 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2573 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2574 | `	}` |
|      ! 0 |  2575 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2576 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2577 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2578 | `	return rc;` |
|      ! 0 |  2579 |  |
|        - |  2580 | `/*` |
|        - |  2581 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2582 | ` *` |
|        - |  2583 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2584 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2585 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2586 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2587 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2588 | ` * then the program execution is halted.` |
|        - |  2589 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2590 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2591 | ` * or to reset the VM to it's initial state.` |
|        - |  2592 | ` */` |
|    19474 |  2593 | `static sxi32 VmByteCodeExec(` |
|        - |  2594 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2595 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2596 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2597 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2598 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2599 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2600 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2601 | `	)` |
|        2 |  2602 |  |
|        - |  2603 | `	VmInstr *pInstr;` |
|        - |  2604 | `	ph7_value *pTos;` |
|        - |  2605 | `	SySet aArg;` |
|        - |  2606 | `	sxi32 pc;` |
|        - |  2607 | `	sxi32 rc;` |
|        - |  2608 | `	/* Argument container */` |
|    19476 |  2609 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    19476 |  2610 | `	if( nTos < 0 ){` |
|    18932 |  2611 | `		pTos = &pStack[-1];` |
|     9467 |  2612 | `	}else{` |
|      546 |  2613 | `		pTos = &pStack[nTos];` |
|        - |  2614 | `	}` |
|    19476 |  2615 | `	pc = 0;` |
|        - |  2616 | `	/* Execute as much as we can */` |
|  3345203 |  2617 | `	for(;;){` |
|        - |  2618 | `		/* Fetch the instruction to execute */` |
|  6689704 |  2619 | `		pInstr = &aInstr[pc];` |
|  6689704 |  2620 | `		rc = SXRET_OK;` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2623 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2624 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2625 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2626 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2627 | ` */` |
|  6689704 |  2628 | `		switch(pInstr->iOp){` |
|        - |  2629 | `/*` |
|        - |  2630 | ` * DONE: P1 * *` |
|        - |  2631 | ` *` |
|        - |  2632 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2633 | ` * and return immediately.` |
|        - |  2634 | ` */` |
|     9719 |  2635 | `case PH7_OP_DONE:` |
|    19440 |  2636 | `	if( pInstr->iP1 ){` |
|        - |  2637 | `#ifdef UNTRUST` |
|        - |  2638 | `		if( pTos < pStack ){` |
|        - |  2639 | `			goto Abort;` |
|        - |  2640 | `		}` |
|        - |  2641 | `#endif` |
|    10360 |  2642 | `		if( pLastRef ){` |
|     7392 |  2643 | `			*pLastRef = pTos->nIdx;` |
|     3695 |  2644 | `		}` |
|    10360 |  2645 | `		if( pResult ){` |
|        - |  2646 | `			/* Execution result */` |
|    10052 |  2647 | `			PH7_MemObjStore(pTos,pResult);` |
|     5025 |  2648 | `		}` |
|    10360 |  2649 | `		VmPopOperand(&pTos,1);` |
|    14261 |  2650 | `	}else if( pLastRef ){` |
|        - |  2651 | `		/* Nothing referenced */` |
|      394 |  2652 | `		*pLastRef = SXU32_HIGH;` |
|      196 |  2653 | `	}` |
|    19440 |  2654 | `	goto Done;` |
|        - |  2655 | `/*` |
|        - |  2656 | ` * HALT: P1 * *` |
|        - |  2657 | ` *` |
|        - |  2658 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2659 | ` * and abort immediately.` |
|        - |  2660 | ` */` |
|        4 |  2661 | `case PH7_OP_HALT:` |
|        9 |  2662 | `	if( pInstr->iP1 ){` |
|        - |  2663 | `#ifdef UNTRUST` |
|        - |  2664 | `		if( pTos < pStack ){` |
|        - |  2665 | `			goto Abort;` |
|        - |  2666 | `		}` |
|        - |  2667 | `#endif` |
|        9 |  2668 | `		if( pLastRef ){` |
|      ! 0 |  2669 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2670 | `		}` |
|        9 |  2671 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2672 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2673 | `				/* Output the exit message */` |
|        7 |  2674 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2675 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2676 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2677 | `					/* Increment output length */` |
|        5 |  2678 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2679 | `				}` |
|        3 |  2680 | `			}` |
|        7 |  2681 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2682 | `			/* Record exit status */` |
|        5 |  2683 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2684 | `		}` |
|        9 |  2685 | `		VmPopOperand(&pTos,1);` |
|        4 |  2686 | `	}else if( pLastRef ){` |
|        - |  2687 | `		/* Nothing referenced */` |
|      ! 0 |  2688 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2689 | `	}` |
|        - |  2690 | `	/* Check if we're in an included file context */` |
|        9 |  2691 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2692 | `		/* Terminate the entire process */` |
|        9 |  2693 | `		exit(pVm->iExitStatus);` |
|        - |  2694 | `	}` |
|      ! 0 |  2695 | `	goto Abort;` |
|        - |  2696 | `/*` |
|        - |  2697 | ` * JMP: * P2 *` |
|        - |  2698 | ` *` |
|        - |  2699 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2700 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2701 | ` */` |
|   151183 |  2702 | `case PH7_OP_JMP:` |
|   302412 |  2703 | `	pc = pInstr->iP2 - 1;` |
|   302412 |  2704 | `	break;` |
|        - |  2705 | `/*` |
|        - |  2706 | ` * JZ: P1 P2 *` |
|        - |  2707 | ` *` |
|        - |  2708 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2709 | ` * entry in the stack if P1 is zero.` |
|        - |  2710 | ` */` |
|   334722 |  2711 | `case PH7_OP_JZ:` |
|        - |  2712 | `#ifdef UNTRUST` |
|        - |  2713 | `	if( pTos < pStack ){` |
|        - |  2714 | `		goto Abort;` |
|        - |  2715 | `	}` |
|        - |  2716 | `#endif` |
|        - |  2717 | `	/* Get a boolean value */` |
|   669534 |  2718 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2719 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2720 | `	}` |
|   669534 |  2721 | `	if( !pTos->x.iVal ){` |
|        - |  2722 | `		/* Take the jump */` |
|   319098 |  2723 | `		pc = pInstr->iP2 - 1;` |
|   159548 |  2724 | `	}` |
|   669534 |  2725 | `	if( !pInstr->iP1 ){` |
|   522568 |  2726 | `		VmPopOperand(&pTos,1);` |
|   261305 |  2727 | `	}` |
|   669534 |  2728 | `	break;` |
|        - |  2729 | `/*` |
|        - |  2730 | ` * JNZ: P1 P2 *` |
|        - |  2731 | ` *` |
|        - |  2732 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2733 | ` * entry in the stack if P1 is zero.` |
|        - |  2734 | ` */` |
|    28463 |  2735 | `case PH7_OP_JNZ:` |
|        - |  2736 | `#ifdef UNTRUST` |
|        - |  2737 | `	if( pTos < pStack ){` |
|        - |  2738 | `		goto Abort;` |
|        - |  2739 | `	}` |
|        - |  2740 | `#endif` |
|        - |  2741 | `	/* Get a boolean value */` |
|    56928 |  2742 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2743 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2744 | `	}` |
|    56928 |  2745 | `	if( pTos->x.iVal ){` |
|        - |  2746 | `		/* Take the jump */` |
|     3102 |  2747 | `		pc = pInstr->iP2 - 1;` |
|     1550 |  2748 | `	}` |
|    56928 |  2749 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2750 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2751 | `	}` |
|    56928 |  2752 | `	break;` |
|        - |  2753 | `/*` |
|        - |  2754 | ` * NOOP: * * *` |
|        - |  2755 | ` *` |
|        - |  2756 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2757 | ` * destination.` |
|        - |  2758 | ` */` |
|      ! 0 |  2759 | `case PH7_OP_NOOP:` |
|      ! 0 |  2760 | `	break;` |
|        - |  2761 | `/*` |
|        - |  2762 | ` * POP: P1 * *` |
|        - |  2763 | ` *` |
|        - |  2764 | ` * Pop P1 elements from the operand stack.` |
|        - |  2765 | ` */` |
|   270798 |  2766 | `case PH7_OP_POP: {` |
|   541642 |  2767 | `	sxi32 n = pInstr->iP1;` |
|   541642 |  2768 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2769 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2770 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2771 | `	}` |
|   541642 |  2772 | `	VmPopOperand(&pTos,n);` |
|   541642 |  2773 | `	break;` |
|        - |  2774 | `				 }` |
|        - |  2775 | `/*` |
|        - |  2776 | ` * CVT_INT: * * *` |
|        - |  2777 | ` *` |
|        - |  2778 | ` * Force the top of the stack to be an integer.` |
|        - |  2779 | ` */` |
|       29 |  2780 | `case PH7_OP_CVT_INT:` |
|        - |  2781 | `#ifdef UNTRUST` |
|        - |  2782 | `	if( pTos < pStack ){` |
|        - |  2783 | `		goto Abort;` |
|        - |  2784 | `	}` |
|        - |  2785 | `#endif` |
|       60 |  2786 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2787 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2788 | `	}` |
|        - |  2789 | `	/* Invalidate any prior representation */` |
|       60 |  2790 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       60 |  2791 | `	break;` |
|        - |  2792 | `/*` |
|        - |  2793 | ` * CVT_REAL: * * *` |
|        - |  2794 | ` *` |
|        - |  2795 | ` * Force the top of the stack to be a real.` |
|        - |  2796 | ` */` |
|        4 |  2797 | `case PH7_OP_CVT_REAL:` |
|        - |  2798 | `#ifdef UNTRUST` |
|        - |  2799 | `	if( pTos < pStack ){` |
|        - |  2800 | `		goto Abort;` |
|        - |  2801 | `	}` |
|        - |  2802 | `#endif` |
|        9 |  2803 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2804 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2805 | `	}` |
|        - |  2806 | `	/* Invalidate any prior representation */` |
|        9 |  2807 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2808 | `	break;` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * CVT_STR: * * *` |
|        - |  2811 | ` *` |
|        - |  2812 | ` * Force the top of the stack to be a string.` |
|        - |  2813 | ` */` |
|      136 |  2814 | `case PH7_OP_CVT_STR:` |
|        - |  2815 | `#ifdef UNTRUST` |
|        - |  2816 | `	if( pTos < pStack ){` |
|        - |  2817 | `		goto Abort;` |
|        - |  2818 | `	}` |
|        - |  2819 | `#endif` |
|      274 |  2820 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2821 | `		PH7_MemObjToString(pTos);` |
|      136 |  2822 | `	}` |
|      274 |  2823 | `	break;` |
|        - |  2824 | `/*` |
|        - |  2825 | ` * CVT_BOOL: * * *` |
|        - |  2826 | ` *` |
|        - |  2827 | ` * Force the top of the stack to be a boolean.` |
|        - |  2828 | ` */` |
|        5 |  2829 | `case PH7_OP_CVT_BOOL:` |
|        - |  2830 | `#ifdef UNTRUST` |
|        - |  2831 | `	if( pTos < pStack ){` |
|        - |  2832 | `		goto Abort;` |
|        - |  2833 | `	}` |
|        - |  2834 | `#endif` |
|       11 |  2835 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2836 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2837 | `	}` |
|       11 |  2838 | `	break;` |
|        - |  2839 | `/*` |
|        - |  2840 | ` * CVT_NULL: * * *` |
|        - |  2841 | ` *` |
|        - |  2842 | ` * Nullify the top of the stack.` |
|        - |  2843 | ` */` |
|        3 |  2844 | `case PH7_OP_CVT_NULL:` |
|        - |  2845 | `#ifdef UNTRUST` |
|        - |  2846 | `	if( pTos < pStack ){` |
|        - |  2847 | `		goto Abort;` |
|        - |  2848 | `	}` |
|        - |  2849 | `#endif` |
|        7 |  2850 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * CVT_NUMC: * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2856 | ` */` |
|      ! 0 |  2857 | `case PH7_OP_CVT_NUMC:` |
|        - |  2858 | `#ifdef UNTRUST` |
|        - |  2859 | `	if( pTos < pStack ){` |
|        - |  2860 | `		goto Abort;` |
|        - |  2861 | `	}` |
|        - |  2862 | `#endif` |
|        - |  2863 | `	/* Force a numeric cast */` |
|      ! 0 |  2864 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2865 | `	break;` |
|        - |  2866 | `/*` |
|        - |  2867 | ` * CVT_ARRAY: * * *` |
|        - |  2868 | ` *` |
|        - |  2869 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2870 | ` */` |
|       10 |  2871 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2872 | `#ifdef UNTRUST` |
|        - |  2873 | `	if( pTos < pStack ){` |
|        - |  2874 | `		goto Abort;` |
|        - |  2875 | `	}` |
|        - |  2876 | `#endif` |
|        - |  2877 | `	/* Force a hashmap cast */` |
|       21 |  2878 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2879 | `	if( rc != SXRET_OK ){` |
|        - |  2880 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2881 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2882 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2883 | `	}` |
|       21 |  2884 | `	break;` |
|        - |  2885 | `/*` |
|        - |  2886 | ` * CVT_OBJ: * * *` |
|        - |  2887 | ` *` |
|        - |  2888 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2889 | ` */` |
|        8 |  2890 | `case PH7_OP_CVT_OBJ:` |
|        - |  2891 | `#ifdef UNTRUST` |
|        - |  2892 | `	if( pTos < pStack ){` |
|        - |  2893 | `		goto Abort;` |
|        - |  2894 | `	}` |
|        - |  2895 | `#endif` |
|       17 |  2896 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2897 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2898 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2899 | `	}` |
|       17 |  2900 | `	break;` |
|        - |  2901 | `/*` |
|        - |  2902 | ` * ERR_CTRL * * *` |
|        - |  2903 | ` *` |
|        - |  2904 | ` * Error control operator.` |
|        - |  2905 | ` */` |
|     7758 |  2906 | `case PH7_OP_ERR_CTRL:` |
|        - |  2907 | `	/*` |
|        - |  2908 | `	 * TICKET 1433-038:` |
|        - |  2909 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2910 | `	 * use the public API,to control error output.` |
|        - |  2911 | `	 */` |
|    15516 |  2912 | `	break;` |
|        - |  2913 | `/*` |
|        - |  2914 | ` * IS_A * * *` |
|        - |  2915 | ` *` |
|        - |  2916 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2917 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2918 | ` * holding a class name or an object).` |
|        - |  2919 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2920 | ` */` |
|       11 |  2921 | `case PH7_OP_IS_A:{` |
|       23 |  2922 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2923 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2924 | `#ifdef UNTRUST` |
|        - |  2925 | `	if( pNos < pStack ){` |
|        - |  2926 | `		goto Abort;` |
|        - |  2927 | `	}` |
|        - |  2928 | `#endif` |
|       23 |  2929 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2930 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2931 | `		ph7_class *pClass = 0;` |
|        - |  2932 | `		/* Extract the target class */` |
|       21 |  2933 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2934 | `			/* Instance already loaded */` |
|      ! 0 |  2935 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2936 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2937 | `			/* Perform the query */` |
|       31 |  2938 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2939 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2940 | `		}` |
|       21 |  2941 | `		if( pClass ){` |
|        - |  2942 | `			/* Perform the query */` |
|       21 |  2943 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2944 | `		}` |
|       10 |  2945 | `	}` |
|        - |  2946 | `	/* Push result */` |
|       23 |  2947 | `	VmPopOperand(&pTos,1);` |
|       23 |  2948 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2949 | `	pTos->x.iVal = iRes;` |
|       23 |  2950 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2951 | `	break;` |
|        - |  2952 | `				 }` |
|        - |  2953 |  |
|        - |  2954 | `/*` |
|        - |  2955 | ` * LOADC P1 P2 *` |
|        - |  2956 | ` *` |
|        - |  2957 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2958 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2959 | ` */` |
|   606970 |  2960 | `case PH7_OP_LOADC: {` |
|        - |  2961 | `	ph7_value *pObj;` |
|        - |  2962 | `	/* Reserve a room */` |
|  1213986 |  2963 | `	pTos++;` |
|  1213986 |  2964 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1213986 |  2965 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2966 | `			SyHashEntry *pEntry;` |
|        - |  2967 | `			/* Candidate for expansion via user defined callbacks */` |
|    11990 |  2968 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    11990 |  2969 | `			if( pEntry ){` |
|    10942 |  2970 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2971 | `				/* Set a NULL default value */` |
|    10942 |  2972 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    10942 |  2973 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2974 | `				/* Invoke the callback and deal with the expanded value */` |
|    10942 |  2975 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2976 | `				/* Mark as constant */` |
|    10942 |  2977 | `				pTos->nIdx = SXU32_HIGH;` |
|    10942 |  2978 | `				break;` |
|        - |  2979 | `			}` |
|      524 |  2980 | `		}` |
|  1203046 |  2981 | `		PH7_MemObjLoad(pObj,pTos);` |
|   601546 |  2982 | `	}else{` |
|        - |  2983 | `		/* Set a NULL value */` |
|      ! 0 |  2984 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2985 | `	}` |
|        - |  2986 | `	/* Mark as constant */` |
|  1203046 |  2987 | `	pTos->nIdx = SXU32_HIGH;` |
|  1203046 |  2988 | `	break;` |
|        - |  2989 | `				  }` |
|        - |  2990 | `/*` |
|        - |  2991 | ` * LOAD: P1 * P3` |
|        - |  2992 | ` *` |
|        - |  2993 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2994 | ` * from the P3 operand.` |
|        - |  2995 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2996 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2997 | ` */` |
|   871708 |  2998 | `case PH7_OP_LOAD:{` |
|        - |  2999 | `	ph7_value *pObj;` |
|        - |  3000 | `	SyString sName;` |
|  1743638 |  3001 | `	if( pInstr->p3 == 0 ){` |
|        - |  3002 | `		/* Take the variable name from the top of the stack */` |
|        - |  3003 | `#ifdef UNTRUST` |
|        - |  3004 | `		if( pTos < pStack ){` |
|        - |  3005 | `			goto Abort;` |
|        - |  3006 | `		}` |
|        - |  3007 | `#endif` |
|        - |  3008 | `		/* Force a string cast */` |
|       25 |  3009 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3010 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3011 | `		}` |
|       25 |  3012 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       13 |  3013 | `	}else{` |
|  1743614 |  3014 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3015 | `		/* Reserve a room for the target object */` |
|  1743614 |  3016 | `		pTos++;` |
|        - |  3017 | `	}` |
|        - |  3018 | `	/* Extract the requested memory object */` |
|  1743638 |  3019 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  1743638 |  3020 | `	if( pObj == 0 ){` |
|      456 |  3021 | `		if( pInstr->iP1 ){` |
|        - |  3022 | `			/* Variable not found,load NULL */` |
|      456 |  3023 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3024 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3025 | `			}else{` |
|      456 |  3026 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3027 | `			}` |
|      456 |  3028 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|   871937 |  3029 | `			break;` |
|      ! 0 |  3030 | `		}else{` |
|        - |  3031 | `			/* Fatal error */` |
|      ! 0 |  3032 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3033 | `			goto Abort;` |
|        - |  3034 | `		}` |
|        - |  3035 | `	}` |
|        - |  3036 | `	/* Load variable contents */` |
|  1743184 |  3037 | `	PH7_MemObjLoad(pObj,pTos);` |
|  1743184 |  3038 | `	pTos->nIdx = pObj->nIdx;` |
|  1743184 |  3039 | `	break;` |
|        - |  3040 | `				   }` |
|        - |  3041 | `/*` |
|        - |  3042 | ` * LOAD_MAP P1 * *` |
|        - |  3043 | ` *` |
|        - |  3044 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3045 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3046 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3047 | ` */` |
|    13099 |  3048 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3049 | `	ph7_hashmap *pMap;` |
|        - |  3050 | `	/* Allocate a new hashmap instance */` |
|    26200 |  3051 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    26200 |  3052 | `	if( pMap == 0 ){` |
|      ! 0 |  3053 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3054 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3055 | `		goto Abort;` |
|        - |  3056 | `	}` |
|    26200 |  3057 | `	if( pInstr->iP1 > 0 ){` |
|     1414 |  3058 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3059 | `		/* Perform the insertion */` |
|     3994 |  3060 | `		while( pEntry < pTos ){` |
|     2582 |  3061 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3062 | `				/* Insertion by reference */` |
|      142 |  3063 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3064 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3065 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3066 | `					);` |
|       48 |  3067 | `			}else{` |
|        - |  3068 | `				/* Standard insertion */` |
|     3731 |  3069 | `				PH7_HashmapInsert(pMap,` |
|     2486 |  3070 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1243 |  3071 | `					&pEntry[1]` |
|        - |  3072 | `				);` |
|        - |  3073 | `			}` |
|        - |  3074 | `			/* Next pair on the stack */` |
|     2582 |  3075 | `			pEntry += 2;` |
|        2 |  3076 | `		}` |
|        - |  3077 | `		/* Pop P1 elements */` |
|     1414 |  3078 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      706 |  3079 | `	}` |
|        - |  3080 | `	/* Push the hashmap */` |
|    26200 |  3081 | `	pTos++;` |
|    26200 |  3082 | `	pTos->nIdx = SXU32_HIGH;` |
|    26200 |  3083 | `	pTos->x.pOther = pMap;` |
|    26200 |  3084 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    26200 |  3085 | `	break;` |
|        - |  3086 | `					  }` |
|        - |  3087 | `/*` |
|        - |  3088 | ` * LOAD_LIST: P1 * *` |
|        - |  3089 | ` *` |
|        - |  3090 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3091 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3092 | ` * Caveats:` |
|        - |  3093 | ` *  This implementation support only a single nesting level.` |
|        - |  3094 | ` */` |
|       17 |  3095 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3096 | `	ph7_value *pEntry;` |
|       35 |  3097 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3098 | `		/* Empty list,break immediately */` |
|      ! 0 |  3099 | `		break;` |
|        - |  3100 | `	}` |
|       35 |  3101 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3102 | `#ifdef UNTRUST` |
|        - |  3103 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3104 | `		goto Abort;` |
|        - |  3105 | `	}` |
|        - |  3106 | `#endif` |
|       35 |  3107 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3108 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3109 | `		ph7_hashmap_node *pNode;` |
|        - |  3110 | `		ph7_value sKey,*pObj;` |
|        - |  3111 | `		/* Start Copying */` |
|       31 |  3112 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3113 | `		while( pEntry <= pTos ){` |
|       69 |  3114 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3115 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3116 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3117 | `					if( rc == SXRET_OK ){` |
|        - |  3118 | `						/* Store node value */` |
|       65 |  3119 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3120 | `					}else{` |
|        - |  3121 | `						/* Nullify the variable */` |
|      ! 0 |  3122 | `						PH7_MemObjRelease(pObj);` |
|        - |  3123 | `					}` |
|       32 |  3124 | `				}` |
|       32 |  3125 | `			}` |
|       69 |  3126 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3127 | `			pEntry++;` |
|        1 |  3128 | `		}` |
|       15 |  3129 | `	}` |
|       35 |  3130 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3131 | `	break;` |
|        - |  3132 | `					   }` |
|        - |  3133 | `/*` |
|        - |  3134 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3135 | ` *` |
|        - |  3136 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3137 | ` * from the stack.` |
|        - |  3138 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3139 | ` * instead.` |
|        - |  3140 | ` */` |
|   123262 |  3141 | `case PH7_OP_LOAD_IDX: {` |
|   246570 |  3142 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   246570 |  3143 | `	ph7_hashmap *pMap = 0;` |
|        - |  3144 | `	ph7_value *pIdx;` |
|   246570 |  3145 | `	pIdx = 0;` |
|   246570 |  3146 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3147 | `		if( !pInstr->iP2){` |
|        - |  3148 | `			/* No available index,load NULL */` |
|      ! 0 |  3149 | `			if( pTos >= pStack ){` |
|      ! 0 |  3150 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3151 | `			}else{` |
|        - |  3152 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3153 | `				pTos++;` |
|      ! 0 |  3154 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3155 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3156 | `			}` |
|        - |  3157 | `			/* Emit a notice */` |
|      ! 0 |  3158 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3159 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3160 | `			break;` |
|        - |  3161 | `		}` |
|      ! 0 |  3162 | `	}else{` |
|   246570 |  3163 | `		pIdx = pTos;` |
|   246570 |  3164 | `		pTos--;` |
|        - |  3165 | `	}` |
|   246570 |  3166 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3167 | `		/* String access */` |
|   181406 |  3168 | `		if( pIdx ){` |
|        - |  3169 | `			sxu32 nOfft;` |
|   181406 |  3170 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3171 | `				/* Force an int cast */` |
|      ! 0 |  3172 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3173 | `			}` |
|   181406 |  3174 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   181406 |  3175 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3176 | `				/* Invalid offset,load null */` |
|      ! 0 |  3177 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3178 | `			}else{` |
|   181406 |  3179 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   181406 |  3180 | `				int c = zData[nOfft];` |
|   181406 |  3181 | `				PH7_MemObjRelease(pTos);` |
|   181406 |  3182 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   181406 |  3183 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3184 | `			}` |
|    90726 |  3185 | `		}else{` |
|        - |  3186 | `			/* No available index,load NULL */` |
|      ! 0 |  3187 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3188 | `		}` |
|   181406 |  3189 | `		break;` |
|        - |  3190 | `	}` |
|    65166 |  3191 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3192 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3193 | `			ph7_value *pObj;` |
|      ! 0 |  3194 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3195 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3196 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3197 | `			}` |
|      ! 0 |  3198 | `		}` |
|      ! 0 |  3199 | `	}` |
|    65166 |  3200 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    65166 |  3201 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3202 | `		/* Point to the hashmap */` |
|    65166 |  3203 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    65166 |  3204 | `		if( pIdx ){` |
|        - |  3205 | `			/* Load the desired entry */` |
|    65166 |  3206 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    32582 |  3207 | `		}` |
|    65166 |  3208 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3209 | `			/* Create a new empty entry */` |
|      ! 0 |  3210 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3211 | `			if( rc == SXRET_OK ){` |
|        - |  3212 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3213 | `				pNode = pMap->pLast;` |
|      ! 0 |  3214 | `			}` |
|      ! 0 |  3215 | `		}` |
|    32582 |  3216 | `	}` |
|    65166 |  3217 | `	if( pIdx ){` |
|    65166 |  3218 | `		PH7_MemObjRelease(pIdx);` |
|    32582 |  3219 | `	}` |
|    65166 |  3220 | `	if( rc == SXRET_OK ){` |
|        - |  3221 | `		/* Load entry contents */` |
|    31536 |  3222 | `		if( pMap->iRef < 2 ){` |
|        - |  3223 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3224 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3225 | `			 */` |
|      ! 0 |  3226 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3227 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|      ! 0 |  3228 | `		}else{` |
|    31536 |  3229 | `			pTos->nIdx = pNode->nValIdx;` |
|    31536 |  3230 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    31536 |  3231 | `			PH7_HashmapUnref(pMap);` |
|        - |  3232 | `		}` |
|    15769 |  3233 | `	}else{` |
|        - |  3234 | `		/* No such entry,load NULL */` |
|    33632 |  3235 | `		PH7_MemObjRelease(pTos);` |
|    33632 |  3236 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3237 | `	}` |
|    65166 |  3238 | `	break;` |
|        - |  3239 | `					  }` |
|        - |  3240 | `/*` |
|        - |  3241 | ` * LOAD_CLOSURE * * P3` |
|        - |  3242 | ` *` |
|        - |  3243 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3244 | ` * name in the stack.` |
|        - |  3245 | ` */` |
|        2 |  3246 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3247 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3248 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3249 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3250 | `		ph7_vm_func *pClosure;` |
|        - |  3251 | `		char *zName;` |
|        - |  3252 | `		sxu32 mLen;` |
|        - |  3253 | `		sxu32 n;` |
|        - |  3254 | `		/* Create a new VM function */` |
|        5 |  3255 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3256 | `		/* Generate an unique closure name */` |
|        5 |  3257 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3258 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3259 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3260 | `			goto Abort;` |
|        - |  3261 | `		}` |
|        5 |  3262 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3263 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3264 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3265 | `		}` |
|        - |  3266 | `		/* Zero the stucture */` |
|        5 |  3267 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3268 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3269 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3270 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3271 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3272 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3273 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3274 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3275 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3276 | `		/* Register the closure */` |
|        5 |  3277 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3278 | `		/* Set up closure environment */` |
|        5 |  3279 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3280 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3281 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3282 | `			ph7_value *pValue;` |
|        9 |  3283 | `			pEnv = &aEnv[n];` |
|        9 |  3284 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3285 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3286 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3287 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3288 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3289 | `				/* Pass by reference */` |
|      ! 0 |  3290 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3291 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3292 | `					);` |
|      ! 0 |  3293 | `			}` |
|        - |  3294 | `			/* Standard pass by value */` |
|        9 |  3295 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3296 | `			if( pValue ){` |
|        - |  3297 | `				/* Copy imported value */` |
|        5 |  3298 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3299 | `			}` |
|        - |  3300 | `			/* Insert the imported variable */` |
|        9 |  3301 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3302 | `		}` |
|        - |  3303 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3304 | `		pTos++;` |
|        5 |  3305 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3306 | `	}` |
|        5 |  3307 | `	break;` |
|        - |  3308 | `						 }` |
|        - |  3309 | `/*` |
|        - |  3310 | ` * STORE * P2 P3` |
|        - |  3311 | ` *` |
|        - |  3312 | ` * Perform a store (Assignment) operation.` |
|        - |  3313 | ` */` |
|    78740 |  3314 | `case PH7_OP_STORE: {` |
|        - |  3315 | `	ph7_value *pObj;` |
|        - |  3316 | `	SyString sName;` |
|        - |  3317 | `#ifdef UNTRUST` |
|        - |  3318 | `	if( pTos < pStack ){` |
|        - |  3319 | `		goto Abort;` |
|        - |  3320 | `	}` |
|        - |  3321 | `#endif` |
|   157482 |  3322 | `	if( pInstr->iP2 ){` |
|        - |  3323 | `		sxu32 nIdx;` |
|        - |  3324 | `		/* Member store operation */` |
|      578 |  3325 | `		nIdx = pTos->nIdx;` |
|      578 |  3326 | `		VmPopOperand(&pTos,1);` |
|      578 |  3327 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3328 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3329 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3330 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3331 | `		}else{` |
|        - |  3332 | `			/* Point to the desired memory object */` |
|      574 |  3333 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      574 |  3334 | `			if( pObj ){` |
|        - |  3335 | `				/* Perform the store operation */` |
|      574 |  3336 | `				PH7_MemObjStore(pTos,pObj);` |
|      286 |  3337 | `			}` |
|        - |  3338 | `		}` |
|    79030 |  3339 | `		break;` |
|   156906 |  3340 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3341 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3342 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3343 | `			/* Force a string cast */` |
|      ! 0 |  3344 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3345 | `		}` |
|        7 |  3346 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3347 | `		pTos--;` |
|        - |  3348 | `#ifdef UNTRUST` |
|        - |  3349 | `		if( pTos < pStack  ){` |
|        - |  3350 | `			goto Abort;` |
|        - |  3351 | `		}` |
|        - |  3352 | `#endif` |
|        4 |  3353 | `	}else{` |
|   156900 |  3354 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3355 | `	}` |
|        - |  3356 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   156906 |  3357 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   156906 |  3358 | `	if( pObj == 0 ){` |
|      ! 0 |  3359 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3360 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3361 | `		goto Abort;` |
|        - |  3362 | `	}` |
|   156906 |  3363 | `	if( !pInstr->p3 ){` |
|        7 |  3364 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3365 | `	}` |
|        - |  3366 | `	/* Perform the store operation */` |
|   156906 |  3367 | `	PH7_MemObjStore(pTos,pObj);` |
|   156906 |  3368 | `	break;` |
|        - |  3369 | `				   }` |
|        - |  3370 | `/*` |
|        - |  3371 | ` * STORE_IDX:   P1 * P3` |
|        - |  3372 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3373 | ` *` |
|        - |  3374 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3375 | ` */` |
|    68369 |  3376 | `case PH7_OP_STORE_IDX:` |
|        - |  3377 | `case PH7_OP_STORE_IDX_REF: {` |
|   136740 |  3378 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3379 | `	ph7_value *pKey;` |
|        - |  3380 | `	sxu32 nIdx;` |
|   136740 |  3381 | `	if( pInstr->iP1 ){` |
|        - |  3382 | `		/* Key is next on stack */` |
|    50776 |  3383 | `		pKey = pTos;` |
|    50776 |  3384 | `		pTos--;` |
|    25389 |  3385 | `	}else{` |
|    85966 |  3386 | `		pKey = 0;` |
|        - |  3387 | `	}` |
|   136740 |  3388 | `	nIdx = pTos->nIdx;` |
|   136740 |  3389 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3390 | `		/* Hashmap already loaded */` |
|   136688 |  3391 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   136688 |  3392 | `		if( pMap->iRef < 2 ){` |
|        - |  3393 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3394 | `			pMap->iRef = 2;` |
|      ! 0 |  3395 | `		}` |
|    68345 |  3396 | `	}else{` |
|        - |  3397 | `		ph7_value *pObj;` |
|       53 |  3398 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3399 | `		if( pObj == 0 ){` |
|      ! 0 |  3400 | `			if( pKey ){` |
|      ! 0 |  3401 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3402 | `			}` |
|      ! 0 |  3403 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3404 | `			break;` |
|        - |  3405 | `		}` |
|        - |  3406 | `		/* Phase#1: Load the array */` |
|       53 |  3407 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3408 | `			VmPopOperand(&pTos,1);` |
|       53 |  3409 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3410 | `				/* Force a string cast */` |
|      ! 0 |  3411 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3412 | `			}` |
|       53 |  3413 | `			if( pKey == 0 ){` |
|        - |  3414 | `				/* Append string */` |
|        3 |  3415 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3416 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3417 | `				}` |
|        2 |  3418 | `			}else{` |
|        - |  3419 | `				sxu32 nOfft;` |
|       51 |  3420 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3421 | `					/* Force an int cast */` |
|       51 |  3422 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3423 | `				}` |
|       51 |  3424 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3425 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3426 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3427 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3428 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3429 | `				}else{` |
|      ! 0 |  3430 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3431 | `						/* Perform an append operation */` |
|      ! 0 |  3432 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3433 | `					}` |
|        - |  3434 | `				}` |
|        - |  3435 | `			}` |
|       53 |  3436 | `			if( pKey ){` |
|       51 |  3437 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3438 | `			}` |
|       53 |  3439 | `			break;` |
|      ! 0 |  3440 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3441 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3442 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3443 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3444 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3445 | `				goto Abort;` |
|        - |  3446 | `			}` |
|      ! 0 |  3447 | `		}` |
|      ! 0 |  3448 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3449 | `	}` |
|   136688 |  3450 | `	VmPopOperand(&pTos,1);` |
|        - |  3451 | `	/* Phase#2: Perform the insertion */` |
|   136688 |  3452 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3453 | `		/* Insertion by reference */` |
|       13 |  3454 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3455 | `	}else{` |
|   136676 |  3456 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3457 | `	}` |
|   136688 |  3458 | `	if( pKey ){` |
|    50726 |  3459 | `		PH7_MemObjRelease(pKey);` |
|    25362 |  3460 | `	}` |
|   136688 |  3461 | `	break;` |
|        - |  3462 | `					   }` |
|        - |  3463 | `/*` |
|        - |  3464 | ` * INCR: P1 * *` |
|        - |  3465 | ` *` |
|        - |  3466 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3467 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3468 | ` * the stack and increment after that.` |
|        - |  3469 | ` */` |
|    95054 |  3470 | `case PH7_OP_INCR:` |
|        - |  3471 | `#ifdef UNTRUST` |
|        - |  3472 | `	if( pTos < pStack ){` |
|        - |  3473 | `		goto Abort;` |
|        - |  3474 | `	}` |
|        - |  3475 | `#endif` |
|   190154 |  3476 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   190154 |  3477 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3478 | `			ph7_value *pObj;` |
|   190154 |  3479 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3480 | `				/* Force a numeric cast */` |
|   190154 |  3481 | `				PH7_MemObjToNumeric(pObj);` |
|   190154 |  3482 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3483 | `					pObj->rVal++;` |
|        - |  3484 | `					/* Try to get an integer representation */` |
|      ! 0 |  3485 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3486 | `				}else{` |
|   190154 |  3487 | `					pObj->x.iVal++;` |
|   190154 |  3488 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3489 | `				}` |
|   190154 |  3490 | `				if( pInstr->iP1 ){` |
|        - |  3491 | `					/* Pre-icrement */` |
|       55 |  3492 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3493 | `				}` |
|    95098 |  3494 | `			}` |
|    95100 |  3495 | `		}else{` |
|      ! 0 |  3496 | `			if( pInstr->iP1 ){` |
|        - |  3497 | `				/* Force a numeric cast */` |
|      ! 0 |  3498 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3499 | `				/* Pre-increment */` |
|      ! 0 |  3500 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3501 | `					pTos->rVal++;` |
|        - |  3502 | `					/* Try to get an integer representation */` |
|      ! 0 |  3503 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3504 | `				}else{` |
|      ! 0 |  3505 | `					pTos->x.iVal++;` |
|      ! 0 |  3506 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3507 | `				}` |
|      ! 0 |  3508 | `			}` |
|        - |  3509 | `		}` |
|    95098 |  3510 | `	}` |
|   190154 |  3511 | `	break;` |
|        - |  3512 | `/*` |
|        - |  3513 | ` * DECR: P1 * *` |
|        - |  3514 | ` *` |
|        - |  3515 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3516 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3517 | ` * and decrement after that.` |
|        - |  3518 | ` */` |
|        2 |  3519 | `case PH7_OP_DECR:` |
|        - |  3520 | `#ifdef UNTRUST` |
|        - |  3521 | `	if( pTos < pStack ){` |
|        - |  3522 | `		goto Abort;` |
|        - |  3523 | `	}` |
|        - |  3524 | `#endif` |
|        5 |  3525 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3526 | `		/* Force a numeric cast */` |
|        5 |  3527 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3528 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3529 | `			ph7_value *pObj;` |
|        5 |  3530 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3531 | `				/* Force a numeric cast */` |
|        5 |  3532 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3533 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3534 | `					pObj->rVal--;` |
|        - |  3535 | `					/* Try to get an integer representation */` |
|      ! 0 |  3536 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3537 | `				}else{` |
|        5 |  3538 | `					pObj->x.iVal--;` |
|        5 |  3539 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3540 | `				}` |
|        5 |  3541 | `				if( pInstr->iP1 ){` |
|        - |  3542 | `					/* Pre-icrement */` |
|      ! 0 |  3543 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3544 | `				}` |
|        2 |  3545 | `			}` |
|        3 |  3546 | `		}else{` |
|      ! 0 |  3547 | `			if( pInstr->iP1 ){` |
|        - |  3548 | `				/* Pre-increment */` |
|      ! 0 |  3549 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3550 | `					pTos->rVal--;` |
|        - |  3551 | `					/* Try to get an integer representation */` |
|      ! 0 |  3552 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3553 | `				}else{` |
|      ! 0 |  3554 | `					pTos->x.iVal--;` |
|      ! 0 |  3555 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3556 | `				}` |
|      ! 0 |  3557 | `			}` |
|        - |  3558 | `		}` |
|        2 |  3559 | `	}` |
|        5 |  3560 | `	break;` |
|        - |  3561 | `/*` |
|        - |  3562 | ` * UMINUS: * * *` |
|        - |  3563 | ` *` |
|        - |  3564 | ` * Perform a unary minus operation.` |
|        - |  3565 | ` */` |
|    17095 |  3566 | `case PH7_OP_UMINUS:` |
|        - |  3567 | `#ifdef UNTRUST` |
|        - |  3568 | `	if( pTos < pStack ){` |
|        - |  3569 | `		goto Abort;` |
|        - |  3570 | `	}` |
|        - |  3571 | `#endif` |
|        - |  3572 | `	/* Force a numeric (integer,real or both) cast */` |
|    34192 |  3573 | `	PH7_MemObjToNumeric(pTos);` |
|    34192 |  3574 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       19 |  3575 | `		pTos->rVal = -pTos->rVal;` |
|        9 |  3576 | `	}` |
|    34192 |  3577 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    34174 |  3578 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    17086 |  3579 | `	}` |
|    34192 |  3580 | `	break;` |
|        - |  3581 | `/*` |
|        - |  3582 | ` * UPLUS: * * *` |
|        - |  3583 | ` *` |
|        - |  3584 | ` * Perform a unary plus operation.` |
|        - |  3585 | ` */` |
|       16 |  3586 | `case PH7_OP_UPLUS:` |
|        - |  3587 | `#ifdef UNTRUST` |
|        - |  3588 | `	if( pTos < pStack ){` |
|        - |  3589 | `		goto Abort;` |
|        - |  3590 | `	}` |
|        - |  3591 | `#endif` |
|        - |  3592 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3593 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3594 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3595 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3596 | `	}` |
|       33 |  3597 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3598 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3599 | `	}` |
|       33 |  3600 | `	break;` |
|        - |  3601 | `/*` |
|        - |  3602 | ` * OP_LNOT: * * *` |
|        - |  3603 | ` *` |
|        - |  3604 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3605 | ` * with its complement.` |
|        - |  3606 | ` */` |
|    27218 |  3607 | `case PH7_OP_LNOT:` |
|        - |  3608 | `#ifdef UNTRUST` |
|        - |  3609 | `	if( pTos < pStack ){` |
|        - |  3610 | `		goto Abort;` |
|        - |  3611 | `	}` |
|        - |  3612 | `#endif` |
|        - |  3613 | `	/* Force a boolean cast */` |
|    54482 |  3614 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3615 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3616 | `	}` |
|    54482 |  3617 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    54482 |  3618 | `	break;` |
|        - |  3619 | `/*` |
|        - |  3620 | ` * OP_BITNOT: * * *` |
|        - |  3621 | ` *` |
|        - |  3622 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3623 | ` * with its ones-complement.` |
|        - |  3624 | ` */` |
|        3 |  3625 | `case PH7_OP_BITNOT:` |
|        - |  3626 | `#ifdef UNTRUST` |
|        - |  3627 | `	if( pTos < pStack ){` |
|        - |  3628 | `		goto Abort;` |
|        - |  3629 | `	}` |
|        - |  3630 | `#endif` |
|        - |  3631 | `	/* Force an integer cast */` |
|        7 |  3632 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3633 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3634 | `	}` |
|        7 |  3635 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3636 | `	break;` |
|        - |  3637 | `/* OP_MUL * * *` |
|        - |  3638 | ` * OP_MUL_STORE * * *` |
|        - |  3639 | ` *` |
|        - |  3640 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3641 | ` * and push the result back onto the stack.` |
|        - |  3642 | ` */` |
|     1231 |  3643 | `case PH7_OP_MUL:` |
|        - |  3644 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3645 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3646 | `	/* Force the operand to be numeric */` |
|        - |  3647 | `#ifdef UNTRUST` |
|        - |  3648 | `	if( pNos < pStack ){` |
|        - |  3649 | `		goto Abort;` |
|        - |  3650 | `	}` |
|        - |  3651 | `#endif` |
|     2464 |  3652 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3653 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3654 | `	/* Perform the requested operation */` |
|     2464 |  3655 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3656 | `		/* Floating point arithemic */` |
|        - |  3657 | `		ph7_real a,b,r;` |
|       17 |  3658 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3659 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3660 | `		}` |
|       17 |  3661 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3662 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3663 | `		}` |
|       17 |  3664 | `		a = pNos->rVal;` |
|       17 |  3665 | `		b = pTos->rVal;` |
|       17 |  3666 | `		r = a * b;` |
|        - |  3667 | `		/* Push the result */` |
|       17 |  3668 | `		pNos->rVal = r;` |
|       17 |  3669 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3670 | `		/* Try to get an integer representation */` |
|       17 |  3671 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3672 | `	}else{` |
|        - |  3673 | `		/* Integer arithmetic */` |
|        - |  3674 | `		sxi64 a,b,r;` |
|     2448 |  3675 | `		a = pNos->x.iVal;` |
|     2448 |  3676 | `		b = pTos->x.iVal;` |
|     2448 |  3677 | `		r = a * b;` |
|        - |  3678 | `		/* Push the result */` |
|     2448 |  3679 | `		pNos->x.iVal = r;` |
|     2448 |  3680 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3681 | `	}` |
|     2464 |  3682 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3683 | `		ph7_value *pObj;` |
|       19 |  3684 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3685 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3686 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3687 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3688 | `		}` |
|        9 |  3689 | `	}` |
|     2464 |  3690 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3691 | `	break;` |
|        - |  3692 | `				 }` |
|        - |  3693 | `/* OP_ADD * * *` |
|        - |  3694 | ` *` |
|        - |  3695 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3696 | ` * and push the result back onto the stack.` |
|        - |  3697 | ` */` |
|      424 |  3698 | `case PH7_OP_ADD:{` |
|      850 |  3699 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3700 | `#ifdef UNTRUST` |
|        - |  3701 | `	if( pNos < pStack ){` |
|        - |  3702 | `		goto Abort;` |
|        - |  3703 | `	}` |
|        - |  3704 | `#endif` |
|        - |  3705 | `	/* Perform the addition */` |
|      850 |  3706 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      850 |  3707 | `	VmPopOperand(&pTos,1);` |
|      850 |  3708 | `	break;` |
|        - |  3709 | `				}` |
|        - |  3710 | `/*` |
|        - |  3711 | ` * OP_ADD_STORE * * *` |
|        - |  3712 | ` *` |
|        - |  3713 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3714 | ` * and push the result back onto the stack.` |
|        - |  3715 | ` */` |
|      481 |  3716 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3717 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3718 | `	ph7_value *pObj;` |
|        - |  3719 | `	sxu32 nIdx;` |
|        - |  3720 | `#ifdef UNTRUST` |
|        - |  3721 | `	if( pNos < pStack ){` |
|        - |  3722 | `		goto Abort;` |
|        - |  3723 | `	}` |
|        - |  3724 | `#endif` |
|        - |  3725 | `	/* Perform the addition */` |
|      963 |  3726 | `	nIdx = pTos->nIdx;` |
|      963 |  3727 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3728 | `	/* Peform the store operation */` |
|      963 |  3729 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3730 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3731 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3732 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3733 | `	}` |
|        - |  3734 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3735 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3736 | `	VmPopOperand(&pTos,1);` |
|      963 |  3737 | `	break;` |
|        - |  3738 | `				}` |
|        - |  3739 | `/* OP_SUB * * *` |
|        - |  3740 | ` *` |
|        - |  3741 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3742 | ` * first (what was next on the stack) from the second (the` |
|        - |  3743 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3744 | ` */` |
|      280 |  3745 | `case PH7_OP_SUB: {` |
|      561 |  3746 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3747 | `#ifdef UNTRUST` |
|        - |  3748 | `	if( pNos < pStack ){` |
|        - |  3749 | `		goto Abort;` |
|        - |  3750 | `	}` |
|        - |  3751 | `#endif` |
|      561 |  3752 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3753 | `		/* Floating point arithemic */` |
|        - |  3754 | `		ph7_real a,b,r;` |
|       73 |  3755 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3756 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3757 | `		}` |
|       73 |  3758 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3759 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3760 | `		}` |
|       73 |  3761 | `		a = pNos->rVal;` |
|       73 |  3762 | `		b = pTos->rVal;` |
|       73 |  3763 | `		r = a - b;` |
|        - |  3764 | `		/* Push the result */` |
|       73 |  3765 | `		pNos->rVal = r;` |
|       73 |  3766 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3767 | `		/* Try to get an integer representation */` |
|       73 |  3768 | `		PH7_MemObjTryInteger(pNos);` |
|       37 |  3769 | `	}else{` |
|        - |  3770 | `		/* Integer arithmetic */` |
|        - |  3771 | `		sxi64 a,b,r;` |
|      489 |  3772 | `		a = pNos->x.iVal;` |
|      489 |  3773 | `		b = pTos->x.iVal;` |
|      489 |  3774 | `		r = a - b;` |
|        - |  3775 | `		/* Push the result */` |
|      489 |  3776 | `		pNos->x.iVal = r;` |
|      489 |  3777 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3778 | `	}` |
|      561 |  3779 | `	VmPopOperand(&pTos,1);` |
|      561 |  3780 | `	break;` |
|        - |  3781 | `				 }` |
|        - |  3782 | `/* OP_SUB_STORE * * *` |
|        - |  3783 | ` *` |
|        - |  3784 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3785 | ` * first (what was next on the stack) from the second (the` |
|        - |  3786 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3787 | ` */` |
|        1 |  3788 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3789 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3790 | `	ph7_value *pObj;` |
|        - |  3791 | `#ifdef UNTRUST` |
|        - |  3792 | `	if( pNos < pStack ){` |
|        - |  3793 | `		goto Abort;` |
|        - |  3794 | `	}` |
|        - |  3795 | `#endif` |
|        3 |  3796 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3797 | `		/* Floating point arithemic */` |
|        - |  3798 | `		ph7_real a,b,r;` |
|      ! 0 |  3799 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3800 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3801 | `		}` |
|      ! 0 |  3802 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3803 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3804 | `		}` |
|      ! 0 |  3805 | `		a = pTos->rVal;` |
|      ! 0 |  3806 | `		b = pNos->rVal;` |
|      ! 0 |  3807 | `		r = a - b;` |
|        - |  3808 | `		/* Push the result */` |
|      ! 0 |  3809 | `		pNos->rVal = r;` |
|      ! 0 |  3810 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3811 | `		/* Try to get an integer representation */` |
|      ! 0 |  3812 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3813 | `	}else{` |
|        - |  3814 | `		/* Integer arithmetic */` |
|        - |  3815 | `		sxi64 a,b,r;` |
|        3 |  3816 | `		a = pTos->x.iVal;` |
|        3 |  3817 | `		b = pNos->x.iVal;` |
|        3 |  3818 | `		r = a - b;` |
|        - |  3819 | `		/* Push the result */` |
|        3 |  3820 | `		pNos->x.iVal = r;` |
|        3 |  3821 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3822 | `	}` |
|        3 |  3823 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3824 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3825 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3826 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3827 | `	}` |
|        3 |  3828 | `	VmPopOperand(&pTos,1);` |
|        3 |  3829 | `	break;` |
|        - |  3830 | `				 }` |
|        - |  3831 |  |
|        - |  3832 | `/*` |
|        - |  3833 | ` * OP_MOD * * *` |
|        - |  3834 | ` *` |
|        - |  3835 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3836 | ` * first (what was next on the stack) from the second (the` |
|        - |  3837 | ` * top of the stack) and push the remainder after division` |
|        - |  3838 | ` * onto the stack.` |
|        - |  3839 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3840 | ` */` |
|      296 |  3841 | `case PH7_OP_MOD:{` |
|      594 |  3842 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3843 | `	sxi64 a,b,r;` |
|        - |  3844 | `#ifdef UNTRUST` |
|        - |  3845 | `	if( pNos < pStack ){` |
|        - |  3846 | `		goto Abort;` |
|        - |  3847 | `	}` |
|        - |  3848 | `#endif` |
|        - |  3849 | `	/* Force the operands to be integer */` |
|      594 |  3850 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3851 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3852 | `	}` |
|      594 |  3853 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3854 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3855 | `	}` |
|        - |  3856 | `	/* Perform the requested operation */` |
|      594 |  3857 | `	a = pNos->x.iVal;` |
|      594 |  3858 | `	b = pTos->x.iVal;` |
|      594 |  3859 | `	if( b == 0 ){` |
|        3 |  3860 | `		r = 0;` |
|        3 |  3861 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3862 | `		/* goto Abort; */` |
|        2 |  3863 | `	}else{` |
|      591 |  3864 | `		r = a%b;` |
|        - |  3865 | `	}` |
|        - |  3866 | `	/* Push the result */` |
|      594 |  3867 | `	pNos->x.iVal = r;` |
|      594 |  3868 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3869 | `	VmPopOperand(&pTos,1);` |
|      594 |  3870 | `	break;` |
|        - |  3871 | `				}` |
|        - |  3872 | `/*` |
|        - |  3873 | ` * OP_MOD_STORE * * *` |
|        - |  3874 | ` *` |
|        - |  3875 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3876 | ` * first (what was next on the stack) from the second (the` |
|        - |  3877 | ` * top of the stack) and push the remainder after division` |
|        - |  3878 | ` * onto the stack.` |
|        - |  3879 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3880 | ` */` |
|        1 |  3881 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3882 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3883 | `	ph7_value *pObj;` |
|        - |  3884 | `	sxi64 a,b,r;` |
|        - |  3885 | `#ifdef UNTRUST` |
|        - |  3886 | `	if( pNos < pStack ){` |
|        - |  3887 | `		goto Abort;` |
|        - |  3888 | `	}` |
|        - |  3889 | `#endif` |
|        - |  3890 | `	/* Force the operands to be integer */` |
|        3 |  3891 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3892 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3893 | `	}` |
|        3 |  3894 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3895 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3896 | `	}` |
|        - |  3897 | `	/* Perform the requested operation */` |
|        3 |  3898 | `	a = pTos->x.iVal;` |
|        3 |  3899 | `	b = pNos->x.iVal;` |
|        3 |  3900 | `	if( b == 0 ){` |
|      ! 0 |  3901 | `		r = 0;` |
|      ! 0 |  3902 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3903 | `		/* goto Abort; */` |
|      ! 0 |  3904 | `	}else{` |
|        3 |  3905 | `		r = a%b;` |
|        - |  3906 | `	}` |
|        - |  3907 | `	/* Push the result */` |
|        3 |  3908 | `	pNos->x.iVal = r;` |
|        3 |  3909 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3910 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3911 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3912 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3913 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3914 | `	}` |
|        3 |  3915 | `	VmPopOperand(&pTos,1);` |
|        3 |  3916 | `	break;` |
|        - |  3917 | `				}` |
|        - |  3918 | `/*` |
|        - |  3919 | ` * OP_DIV * * *` |
|        - |  3920 | ` *` |
|        - |  3921 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3922 | ` * first (what was next on the stack) from the second (the` |
|        - |  3923 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3924 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3925 | ` */` |
|       28 |  3926 | `case PH7_OP_DIV:{` |
|       58 |  3927 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3928 | `	ph7_real a,b,r;` |
|        - |  3929 | `#ifdef UNTRUST` |
|        - |  3930 | `	if( pNos < pStack ){` |
|        - |  3931 | `		goto Abort;` |
|        - |  3932 | `	}` |
|        - |  3933 | `#endif` |
|        - |  3934 | `	/* Force the operands to be real */` |
|       58 |  3935 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3936 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3937 | `	}` |
|       58 |  3938 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3939 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3940 | `	}` |
|        - |  3941 | `	/* Perform the requested operation */` |
|       58 |  3942 | `	a = pNos->rVal;` |
|       58 |  3943 | `	b = pTos->rVal;` |
|       58 |  3944 | `	if( b == 0 ){` |
|        - |  3945 | `		/* Division by zero */` |
|        3 |  3946 | `		pNos->rVal = 0;` |
|        3 |  3947 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3948 | `		/* goto Abort; */` |
|        2 |  3949 | `	}else{` |
|       55 |  3950 | `		r = a/b;` |
|        - |  3951 | `		/* Push the result */` |
|       55 |  3952 | `		pNos->rVal = r;` |
|       55 |  3953 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3954 | `		/* Try to get an integer representation */` |
|       55 |  3955 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3956 | `	}` |
|       58 |  3957 | `	VmPopOperand(&pTos,1);` |
|       58 |  3958 | `	break;` |
|        - |  3959 | `				}` |
|        - |  3960 | `/*` |
|        - |  3961 | ` * OP_DIV_STORE * * *` |
|        - |  3962 | ` *` |
|        - |  3963 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3964 | ` * first (what was next on the stack) from the second (the` |
|        - |  3965 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3966 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3967 | ` */` |
|        1 |  3968 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3969 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3970 | `	ph7_value *pObj;` |
|        - |  3971 | `	ph7_real a,b,r;` |
|        - |  3972 | `#ifdef UNTRUST` |
|        - |  3973 | `	if( pNos < pStack ){` |
|        - |  3974 | `		goto Abort;` |
|        - |  3975 | `	}` |
|        - |  3976 | `#endif` |
|        - |  3977 | `	/* Force the operands to be real */` |
|        3 |  3978 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3979 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3980 | `	}` |
|        3 |  3981 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3982 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3983 | `	}` |
|        - |  3984 | `	/* Perform the requested operation */` |
|        3 |  3985 | `	a = pTos->rVal;` |
|        3 |  3986 | `	b = pNos->rVal;` |
|        3 |  3987 | `	if( b == 0 ){` |
|        - |  3988 | `		/* Division by zero */` |
|      ! 0 |  3989 | `		r = 0;` |
|      ! 0 |  3990 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3991 | `		/* goto Abort; */` |
|      ! 0 |  3992 | `	}else{` |
|        3 |  3993 | `		r = a/b;` |
|        - |  3994 | `		/* Push the result */` |
|        3 |  3995 | `		pNos->rVal = r;` |
|        3 |  3996 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3997 | `		/* Try to get an integer representation */` |
|        3 |  3998 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3999 | `	}` |
|        3 |  4000 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4001 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4002 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4003 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4004 | `	}` |
|        3 |  4005 | `	VmPopOperand(&pTos,1);` |
|        3 |  4006 | `	break;` |
|        - |  4007 | `				}` |
|        - |  4008 | `/* OP_BAND * * *` |
|        - |  4009 | ` *` |
|        - |  4010 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4011 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4012 | ` * two elements.` |
|        - |  4013 | `*/` |
|        - |  4014 | `/* OP_BOR * * *` |
|        - |  4015 | ` *` |
|        - |  4016 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4017 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4018 | ` * two elements.` |
|        - |  4019 | ` */` |
|        - |  4020 | `/* OP_BXOR * * *` |
|        - |  4021 | ` *` |
|        - |  4022 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4023 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4024 | ` * two elements.` |
|        - |  4025 | ` */` |
|       19 |  4026 | `case PH7_OP_BAND:` |
|        - |  4027 | `case PH7_OP_BOR:` |
|        - |  4028 | `case PH7_OP_BXOR:{` |
|       39 |  4029 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4030 | `	sxi64 a,b,r;` |
|        - |  4031 | `#ifdef UNTRUST` |
|        - |  4032 | `	if( pNos < pStack ){` |
|        - |  4033 | `		goto Abort;` |
|        - |  4034 | `	}` |
|        - |  4035 | `#endif` |
|        - |  4036 | `	/* Force the operands to be integer */` |
|       39 |  4037 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4038 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4039 | `	}` |
|       39 |  4040 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4041 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4042 | `	}` |
|        - |  4043 | `	/* Perform the requested operation */` |
|       39 |  4044 | `	a = pNos->x.iVal;` |
|       39 |  4045 | `	b = pTos->x.iVal;` |
|       39 |  4046 | `	switch(pInstr->iOp){` |
|        6 |  4047 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4048 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4049 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4050 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4051 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4052 | `	case PH7_OP_BAND:` |
|       15 |  4053 | `	default:          r = a&b; break;` |
|        - |  4054 | `	}` |
|        - |  4055 | `	/* Push the result */` |
|       39 |  4056 | `	pNos->x.iVal = r;` |
|       39 |  4057 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4058 | `	VmPopOperand(&pTos,1);` |
|       39 |  4059 | `	break;` |
|        - |  4060 | `				 }` |
|        - |  4061 | `/* OP_BAND_STORE * * *` |
|        - |  4062 | ` *` |
|        - |  4063 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4064 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4065 | ` * two elements.` |
|        - |  4066 | `*/` |
|        - |  4067 | `/* OP_BOR_STORE * * *` |
|        - |  4068 | ` *` |
|        - |  4069 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4070 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4071 | ` * two elements.` |
|        - |  4072 | ` */` |
|        - |  4073 | `/* OP_BXOR_STORE * * *` |
|        - |  4074 | ` *` |
|        - |  4075 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4076 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4077 | ` * two elements.` |
|        - |  4078 | ` */` |
|        7 |  4079 | `case PH7_OP_BAND_STORE:` |
|        - |  4080 | `case PH7_OP_BOR_STORE:` |
|        - |  4081 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4082 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4083 | `	ph7_value *pObj;` |
|        - |  4084 | `	sxi64 a,b,r;` |
|        - |  4085 | `#ifdef UNTRUST` |
|        - |  4086 | `	if( pNos < pStack ){` |
|        - |  4087 | `		goto Abort;` |
|        - |  4088 | `	}` |
|        - |  4089 | `#endif` |
|        - |  4090 | `	/* Force the operands to be integer */` |
|       15 |  4091 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4092 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4093 | `	}` |
|       15 |  4094 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4095 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4096 | `	}` |
|        - |  4097 | `	/* Perform the requested operation */` |
|       15 |  4098 | `	a = pTos->x.iVal;` |
|       15 |  4099 | `	b = pNos->x.iVal;` |
|       15 |  4100 | `	switch(pInstr->iOp){` |
|        2 |  4101 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4102 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4103 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4104 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4105 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4106 | `	case PH7_OP_BAND:` |
|        5 |  4107 | `	default:          r = a&b; break;` |
|        - |  4108 | `	}` |
|        - |  4109 | `	/* Push the result */` |
|       15 |  4110 | `	pNos->x.iVal = r;` |
|       15 |  4111 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4112 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4113 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4114 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4115 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4116 | `	}` |
|       15 |  4117 | `	VmPopOperand(&pTos,1);` |
|       15 |  4118 | `	break;` |
|        - |  4119 | `				 }` |
|        - |  4120 | `/* OP_SHL * * *` |
|        - |  4121 | ` *` |
|        - |  4122 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4123 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4124 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4125 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4126 | ` */` |
|        - |  4127 | `/* OP_SHR * * *` |
|        - |  4128 | ` *` |
|        - |  4129 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4130 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4131 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4132 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4133 | ` */` |
|        9 |  4134 | `case PH7_OP_SHL:` |
|        - |  4135 | `case PH7_OP_SHR: {` |
|       19 |  4136 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4137 | `	sxi64 a,r;` |
|        - |  4138 | `	sxi32 b;` |
|        - |  4139 | `#ifdef UNTRUST` |
|        - |  4140 | `	if( pNos < pStack ){` |
|        - |  4141 | `		goto Abort;` |
|        - |  4142 | `	}` |
|        - |  4143 | `#endif` |
|        - |  4144 | `	/* Force the operands to be integer */` |
|       19 |  4145 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4146 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4147 | `	}` |
|       19 |  4148 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4149 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4150 | `	}` |
|        - |  4151 | `	/* Perform the requested operation */` |
|       19 |  4152 | `	a = pNos->x.iVal;` |
|       19 |  4153 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4154 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4155 | `		r = a << b;` |
|        6 |  4156 | `	}else{` |
|        9 |  4157 | `		r = a >> b;` |
|        - |  4158 | `	}` |
|        - |  4159 | `	/* Push the result */` |
|       19 |  4160 | `	pNos->x.iVal = r;` |
|       19 |  4161 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4162 | `	VmPopOperand(&pTos,1);` |
|       19 |  4163 | `	break;` |
|        - |  4164 | `				 }` |
|        - |  4165 | `/*  OP_SHL_STORE * * *` |
|        - |  4166 | ` *` |
|        - |  4167 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4168 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4169 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4170 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4171 | ` */` |
|        - |  4172 | `/* OP_SHR_STORE * * *` |
|        - |  4173 | ` *` |
|        - |  4174 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4175 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4176 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4177 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4178 | ` */` |
|        7 |  4179 | `case PH7_OP_SHL_STORE:` |
|        - |  4180 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4181 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4182 | `	ph7_value *pObj;` |
|        - |  4183 | `	sxi64 a,r;` |
|        - |  4184 | `	sxi32 b;` |
|        - |  4185 | `#ifdef UNTRUST` |
|        - |  4186 | `	if( pNos < pStack ){` |
|        - |  4187 | `		goto Abort;` |
|        - |  4188 | `	}` |
|        - |  4189 | `#endif` |
|        - |  4190 | `	/* Force the operands to be integer */` |
|       15 |  4191 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4192 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4193 | `	}` |
|       15 |  4194 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4195 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4196 | `	}` |
|        - |  4197 | `	/* Perform the requested operation */` |
|       15 |  4198 | `	a = pTos->x.iVal;` |
|       15 |  4199 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4200 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4201 | `		r = a << b;` |
|        4 |  4202 | `	}else{` |
|        9 |  4203 | `		r = a >> b;` |
|        - |  4204 | `	}` |
|        - |  4205 | `	/* Push the result */` |
|       15 |  4206 | `	pNos->x.iVal = r;` |
|       15 |  4207 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4208 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4209 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4210 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4211 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4212 | `	}` |
|       15 |  4213 | `	VmPopOperand(&pTos,1);` |
|       15 |  4214 | `	break;` |
|        - |  4215 | `				 }` |
|        - |  4216 | `/* CAT:  P1 * *` |
|        - |  4217 | ` *` |
|        - |  4218 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4219 | ` * back.` |
|        - |  4220 | ` */` |
|    51924 |  4221 | `case PH7_OP_CAT:{` |
|        - |  4222 | `	ph7_value *pNos,*pCur;` |
|   103850 |  4223 | `	if( pInstr->iP1 < 1 ){` |
|    74138 |  4224 | `		pNos = &pTos[-1];` |
|    37070 |  4225 | `	}else{` |
|    29714 |  4226 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4227 | `	}` |
|        - |  4228 | `#ifdef UNTRUST` |
|        - |  4229 | `	if( pNos < pStack ){` |
|        - |  4230 | `		goto Abort;` |
|        - |  4231 | `	}` |
|        - |  4232 | `#endif` |
|        - |  4233 | `	/* Force a string cast */` |
|   103850 |  4234 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      534 |  4235 | `		PH7_MemObjToString(pNos);` |
|      266 |  4236 | `	}` |
|   103850 |  4237 | `	pCur = &pNos[1];` |
|   218006 |  4238 | `	while( pCur <= pTos ){` |
|   114158 |  4239 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    53224 |  4240 | `			PH7_MemObjToString(pCur);` |
|    26611 |  4241 | `		}` |
|        - |  4242 | `		/* Perform the concatenation */` |
|   114158 |  4243 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   114120 |  4244 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    57059 |  4245 | `		}` |
|   114158 |  4246 | `		SyBlobRelease(&pCur->sBlob);` |
|   114158 |  4247 | `		pCur++;` |
|        2 |  4248 | `	}` |
|   103850 |  4249 | `	pTos = pNos;` |
|   103850 |  4250 | `	break;` |
|        - |  4251 | `				}` |
|        - |  4252 | `/*  CAT_STORE: * * *` |
|        - |  4253 | ` *` |
|        - |  4254 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4255 | ` * back.` |
|        - |  4256 | ` */` |
|     1244 |  4257 | `case PH7_OP_CAT_STORE:{` |
|     2489 |  4258 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4259 | `	ph7_value *pObj;` |
|        - |  4260 | `#ifdef UNTRUST` |
|        - |  4261 | `	if( pNos < pStack ){` |
|        - |  4262 | `		goto Abort;` |
|        - |  4263 | `	}` |
|        - |  4264 | `#endif` |
|     2489 |  4265 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4266 | `		/* Force a string cast */` |
|      ! 0 |  4267 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4268 | `	}` |
|     2489 |  4269 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4270 | `		/* Force a string cast */` |
|      ! 0 |  4271 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4272 | `	}` |
|        - |  4273 | `	/* Perform the concatenation (Reverse order) */` |
|     2489 |  4274 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     2489 |  4275 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     1244 |  4276 | `	}` |
|        - |  4277 | `	/* Perform the store operation */` |
|     2489 |  4278 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4279 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     2489 |  4280 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     2489 |  4281 | `		PH7_MemObjStore(pTos,pObj);` |
|     1244 |  4282 | `	}` |
|     2489 |  4283 | `	PH7_MemObjStore(pTos,pNos);` |
|     2489 |  4284 | `	VmPopOperand(&pTos,1);` |
|     2489 |  4285 | `	break;` |
|        - |  4286 | `				}` |
|        - |  4287 | `/* OP_AND: * * *` |
|        - |  4288 | ` *` |
|        - |  4289 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4290 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4291 | ` * stack.` |
|        - |  4292 | ` */` |
|        - |  4293 | `/* OP_OR: * * *` |
|        - |  4294 | ` *` |
|        - |  4295 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4296 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4297 | ` * stack.` |
|        - |  4298 | ` */` |
|    57103 |  4299 | `case PH7_OP_LAND:` |
|        - |  4300 | `case PH7_OP_LOR: {` |
|   114252 |  4301 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4302 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4303 | `#ifdef UNTRUST` |
|        - |  4304 | `	if( pNos < pStack ){` |
|        - |  4305 | `		goto Abort;` |
|        - |  4306 | `	}` |
|        - |  4307 | `#endif` |
|        - |  4308 | `	/* Force a boolean cast */` |
|   114252 |  4309 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4310 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4311 | `	}` |
|   114252 |  4312 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4313 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4314 | `	}` |
|   114252 |  4315 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   114252 |  4316 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   114252 |  4317 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4318 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    60426 |  4319 | `		v1 = and_logic[v1*3+v2];` |
|    30236 |  4320 | `	}else{` |
|        - |  4321 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    53828 |  4322 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4323 | `	}` |
|   114252 |  4324 | `	if( v1 == 2 ){` |
|      ! 0 |  4325 | `		v1 = 1;` |
|      ! 0 |  4326 | `	}` |
|   114252 |  4327 | `	VmPopOperand(&pTos,1);` |
|   114252 |  4328 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   114252 |  4329 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   114252 |  4330 | `	break;` |
|        - |  4331 | `				 }` |
|        - |  4332 | `/* OP_LXOR: * * *` |
|        - |  4333 | ` *` |
|        - |  4334 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4335 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4336 | ` * stack.` |
|        - |  4337 | ` * According to the PHP language reference manual:` |
|        - |  4338 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4339 | ` *  TRUE,but not both.` |
|        - |  4340 | ` */` |
|        5 |  4341 | `case PH7_OP_LXOR:{` |
|       11 |  4342 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4343 | `	sxi32 v = 0;` |
|        - |  4344 | `#ifdef UNTRUST` |
|        - |  4345 | `	if( pNos < pStack ){` |
|        - |  4346 | `		goto Abort;` |
|        - |  4347 | `	}` |
|        - |  4348 | `#endif` |
|        - |  4349 | `	/* Force a boolean cast */` |
|       11 |  4350 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4351 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4352 | `	}` |
|       11 |  4353 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4354 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4355 | `	}` |
|       11 |  4356 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4357 | `		v = 1;` |
|        3 |  4358 | `	}` |
|       11 |  4359 | `	VmPopOperand(&pTos,1);` |
|       11 |  4360 | `	pTos->x.iVal = v;` |
|       11 |  4361 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4362 | `	break;` |
|        - |  4363 | `				 }` |
|        - |  4364 | `/* OP_EQ P1 P2 P3` |
|        - |  4365 | ` *` |
|        - |  4366 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4367 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4368 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4369 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4370 | ` */` |
|        - |  4371 | `/* OP_NEQ P1 P2 P3` |
|        - |  4372 | ` *` |
|        - |  4373 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4374 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4375 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4376 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4377 | ` */` |
|     3051 |  4378 | `case PH7_OP_EQ:` |
|        - |  4379 | `case PH7_OP_NEQ: {` |
|     6104 |  4380 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4381 | `	/* Perform the comparison and act accordingly */` |
|        - |  4382 | `#ifdef UNTRUST` |
|        - |  4383 | `	if( pNos < pStack ){` |
|        - |  4384 | `		goto Abort;` |
|        - |  4385 | `	}` |
|        - |  4386 | `#endif` |
|     6104 |  4387 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6104 |  4388 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4389 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     6099 |  4390 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     6072 |  4391 | `		rc = rc == 0;` |
|     3037 |  4392 | `	}else{` |
|       24 |  4393 | `		rc = rc != 0;` |
|        - |  4394 | `	}` |
|     6104 |  4395 | `	VmPopOperand(&pTos,1);` |
|     6104 |  4396 | `	if( !pInstr->iP2 ){` |
|        - |  4397 | `		/* Push comparison result without taking the jump */` |
|     6104 |  4398 | `		PH7_MemObjRelease(pTos);` |
|     6104 |  4399 | `		pTos->x.iVal = rc;` |
|        - |  4400 | `		/* Invalidate any prior representation */` |
|     6104 |  4401 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3053 |  4402 | `	}else{` |
|      ! 0 |  4403 | `		if( rc ){` |
|        - |  4404 | `			/* Jump to the desired location */` |
|      ! 0 |  4405 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4406 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4407 | `		}` |
|        - |  4408 | `	}` |
|     6104 |  4409 | `	break;` |
|        - |  4410 | `				 }` |
|        - |  4411 | `/* OP_TEQ P1 P2 *` |
|        - |  4412 | ` *` |
|        - |  4413 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4414 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4415 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4416 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4417 | ` */` |
|    84089 |  4418 | `case PH7_OP_TEQ: {` |
|   168180 |  4419 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4420 | `	/* Perform the comparison and act accordingly */` |
|        - |  4421 | `#ifdef UNTRUST` |
|        - |  4422 | `	if( pNos < pStack ){` |
|        - |  4423 | `		goto Abort;` |
|        - |  4424 | `	}` |
|        - |  4425 | `#endif` |
|   168180 |  4426 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   168180 |  4427 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4428 | `		rc = 0;` |
|        2 |  4429 | `	}else{` |
|   168178 |  4430 | `		rc = rc == 0;` |
|        - |  4431 | `	}` |
|   168180 |  4432 | `	VmPopOperand(&pTos,1);` |
|   168180 |  4433 | `	if( !pInstr->iP2 ){` |
|        - |  4434 | `		/* Push comparison result without taking the jump */` |
|   168180 |  4435 | `		PH7_MemObjRelease(pTos);` |
|   168180 |  4436 | `		pTos->x.iVal = rc;` |
|        - |  4437 | `		/* Invalidate any prior representation */` |
|   168180 |  4438 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    84091 |  4439 | `	}else{` |
|      ! 0 |  4440 | `		if( rc ){` |
|        - |  4441 | `			/* Jump to the desired location */` |
|      ! 0 |  4442 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4443 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4444 | `		}` |
|        - |  4445 | `	}` |
|   168180 |  4446 | `	break;` |
|        - |  4447 | `				 }` |
|        - |  4448 | `/* OP_TNE P1 P2 *` |
|        - |  4449 | ` *` |
|        - |  4450 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4451 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4452 | ` * instruction.` |
|        - |  4453 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4454 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4455 | ` *` |
|        - |  4456 | ` */` |
|    66209 |  4457 | `case PH7_OP_TNE: {` |
|   132420 |  4458 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4459 | `	/* Perform the comparison and act accordingly */` |
|        - |  4460 | `#ifdef UNTRUST` |
|        - |  4461 | `	if( pNos < pStack ){` |
|        - |  4462 | `		goto Abort;` |
|        - |  4463 | `	}` |
|        - |  4464 | `#endif` |
|   132420 |  4465 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   132420 |  4466 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4467 | `		rc = 1;` |
|        2 |  4468 | `	}else{` |
|   132418 |  4469 | `		rc = rc != 0;` |
|        - |  4470 | `	}` |
|   132420 |  4471 | `	VmPopOperand(&pTos,1);` |
|   132420 |  4472 | `	if( !pInstr->iP2 ){` |
|        - |  4473 | `		/* Push comparison result without taking the jump */` |
|   132420 |  4474 | `		PH7_MemObjRelease(pTos);` |
|   132420 |  4475 | `		pTos->x.iVal = rc;` |
|        - |  4476 | `		/* Invalidate any prior representation */` |
|   132420 |  4477 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    66211 |  4478 | `	}else{` |
|      ! 0 |  4479 | `		if( rc ){` |
|        - |  4480 | `			/* Jump to the desired location */` |
|      ! 0 |  4481 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4482 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4483 | `		}` |
|        - |  4484 | `	}` |
|   132420 |  4485 | `	break;` |
|        - |  4486 | `				 }` |
|        - |  4487 | `/* OP_LT P1 P2 P3` |
|        - |  4488 | ` *` |
|        - |  4489 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4490 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4491 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4492 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4493 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4494 | ` *` |
|        - |  4495 | ` */` |
|        - |  4496 | `/* OP_LE P1 P2 P3` |
|        - |  4497 | ` *` |
|        - |  4498 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4499 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4500 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4501 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4502 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4503 | ` *` |
|        - |  4504 | ` */` |
|    70104 |  4505 | `case PH7_OP_LT:` |
|        - |  4506 | `case PH7_OP_LE: {` |
|   140254 |  4507 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4508 | `	/* Perform the comparison and act accordingly */` |
|        - |  4509 | `#ifdef UNTRUST` |
|        - |  4510 | `	if( pNos < pStack ){` |
|        - |  4511 | `		goto Abort;` |
|        - |  4512 | `	}` |
|        - |  4513 | `#endif` |
|   140254 |  4514 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   140254 |  4515 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4516 | `		rc = 0;` |
|   140250 |  4517 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4518 | `		rc = rc < 1;` |
|      198 |  4519 | `	}else{` |
|   139852 |  4520 | `		rc = rc < 0;` |
|        - |  4521 | `	}` |
|   140254 |  4522 | `	VmPopOperand(&pTos,1);` |
|   140254 |  4523 | `	if( !pInstr->iP2 ){` |
|        - |  4524 | `		/* Push comparison result without taking the jump */` |
|   140254 |  4525 | `		PH7_MemObjRelease(pTos);` |
|   140254 |  4526 | `		pTos->x.iVal = rc;` |
|        - |  4527 | `		/* Invalidate any prior representation */` |
|   140254 |  4528 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    70150 |  4529 | `	}else{` |
|      ! 0 |  4530 | `		if( rc ){` |
|        - |  4531 | `			/* Jump to the desired location */` |
|      ! 0 |  4532 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4533 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4534 | `		}` |
|        - |  4535 | `	}` |
|   140254 |  4536 | `	break;` |
|        - |  4537 | `				}` |
|        - |  4538 | `/* OP_GT P1 P2 P3` |
|        - |  4539 | ` *` |
|        - |  4540 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4541 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4542 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4543 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4544 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4545 | ` *` |
|        - |  4546 | ` */` |
|        - |  4547 | `/* OP_GE P1 P2 P3` |
|        - |  4548 | ` *` |
|        - |  4549 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4550 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4551 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4552 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4553 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4554 | ` *` |
|        - |  4555 | ` */` |
|    24629 |  4556 | `case PH7_OP_GT:` |
|        - |  4557 | `case PH7_OP_GE: {` |
|    49260 |  4558 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4559 | `	/* Perform the comparison and act accordingly */` |
|        - |  4560 | `#ifdef UNTRUST` |
|        - |  4561 | `	if( pNos < pStack ){` |
|        - |  4562 | `		goto Abort;` |
|        - |  4563 | `	}` |
|        - |  4564 | `#endif` |
|    49260 |  4565 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    49260 |  4566 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4567 | `		rc = 0;` |
|    49256 |  4568 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    49104 |  4569 | `		rc = rc >= 0;` |
|    24553 |  4570 | `	}else{` |
|      150 |  4571 | `		rc = rc > 0;` |
|        - |  4572 | `	}` |
|    49260 |  4573 | `	VmPopOperand(&pTos,1);` |
|    49260 |  4574 | `	if( !pInstr->iP2 ){` |
|        - |  4575 | `		/* Push comparison result without taking the jump */` |
|    49260 |  4576 | `		PH7_MemObjRelease(pTos);` |
|    49260 |  4577 | `		pTos->x.iVal = rc;` |
|        - |  4578 | `		/* Invalidate any prior representation */` |
|    49260 |  4579 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    24631 |  4580 | `	}else{` |
|      ! 0 |  4581 | `		if( rc ){` |
|        - |  4582 | `			/* Jump to the desired location */` |
|      ! 0 |  4583 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4584 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4585 | `		}` |
|        - |  4586 | `	}` |
|    49260 |  4587 | `	break;` |
|        - |  4588 | `				}` |
|        - |  4589 | `/* OP_SEQ P1 P2 *` |
|        - |  4590 | ` * Strict string comparison.` |
|        - |  4591 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4592 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4593 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4594 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4595 | ` * use PH7_OP_EQ.` |
|        - |  4596 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4597 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4598 | ` */` |
|        - |  4599 | `/* OP_SNE P1 P2 *` |
|        - |  4600 | ` * Strict string comparison.` |
|        - |  4601 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4602 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4603 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4604 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4605 | ` * use PH7_OP_EQ.` |
|        - |  4606 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4607 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4608 | ` */` |
|       18 |  4609 | `case PH7_OP_SEQ:` |
|        - |  4610 | `case PH7_OP_SNE: {` |
|       38 |  4611 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4612 | `	SyString s1,s2;` |
|        - |  4613 | `	/* Perform the comparison and act accordingly */` |
|        - |  4614 | `#ifdef UNTRUST` |
|        - |  4615 | `	if( pNos < pStack ){` |
|        - |  4616 | `		goto Abort;` |
|        - |  4617 | `	}` |
|        - |  4618 | `#endif` |
|        - |  4619 | `	/* Force a string cast */` |
|       38 |  4620 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4621 | `		PH7_MemObjToString(pTos);` |
|        2 |  4622 | `	}` |
|       38 |  4623 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4624 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4625 | `	}` |
|       38 |  4626 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4627 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4628 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4629 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4630 | `		rc = rc != 0;` |
|      ! 0 |  4631 | `	}else{` |
|       38 |  4632 | `		rc = rc == 0;` |
|        - |  4633 | `	}` |
|       38 |  4634 | `	VmPopOperand(&pTos,1);` |
|       38 |  4635 | `	if( !pInstr->iP2 ){` |
|        - |  4636 | `		/* Push comparison result without taking the jump */` |
|       38 |  4637 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4638 | `		pTos->x.iVal = rc;` |
|        - |  4639 | `		/* Invalidate any prior representation */` |
|       38 |  4640 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4641 | `	}else{` |
|      ! 0 |  4642 | `		if( rc ){` |
|        - |  4643 | `			/* Jump to the desired location */` |
|      ! 0 |  4644 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4645 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4646 | `		}` |
|        - |  4647 | `	}` |
|       38 |  4648 | `	break;` |
|        - |  4649 | `				 }` |
|        - |  4650 | `/*` |
|        - |  4651 | ` * OP_LOAD_REF * * *` |
|        - |  4652 | ` * Push the index of a referenced object on the stack.` |
|        - |  4653 | ` */` |
|       57 |  4654 | `case PH7_OP_LOAD_REF: {` |
|        - |  4655 | `	sxu32 nIdx;` |
|        - |  4656 | `#ifdef UNTRUST` |
|        - |  4657 | `	if( pTos < pStack ){` |
|        - |  4658 | `		goto Abort;` |
|        - |  4659 | `	}` |
|        - |  4660 | `#endif` |
|        - |  4661 | `	/* Extract memory object index */` |
|      115 |  4662 | `	nIdx = pTos->nIdx;` |
|      115 |  4663 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4664 | `		/* Nullify the object */` |
|       95 |  4665 | `		PH7_MemObjRelease(pTos);` |
|        - |  4666 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4667 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4668 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4669 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4670 | `	}` |
|      115 |  4671 | `	break;` |
|        - |  4672 | `					  }` |
|        - |  4673 | `/*` |
|        - |  4674 | ` * OP_STORE_REF * * P3` |
|        - |  4675 | ` * Perform an assignment operation by reference.` |
|        - |  4676 | ` */` |
|       14 |  4677 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4678 | `	 SyString sName = { 0 , 0 };` |
|        - |  4679 | `	 VmFrame *pFrameLocal;` |
|        - |  4680 | `	SyHashEntry *pEntry;` |
|        - |  4681 | `	sxu32 nIdx;` |
|        - |  4682 | `#ifdef UNTRUST` |
|        - |  4683 | `	if( pTos < pStack ){` |
|        - |  4684 | `		goto Abort;` |
|        - |  4685 | `	}` |
|        - |  4686 | `#endif` |
|       30 |  4687 | `	if( pInstr->p3 == 0 ){` |
|        - |  4688 | `		char *zName;` |
|        - |  4689 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4690 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4691 | `			/* Force a string cast */` |
|      ! 0 |  4692 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4693 | `		}` |
|      ! 0 |  4694 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4695 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4696 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4697 | `			if( zName ){` |
|      ! 0 |  4698 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4699 | `			}` |
|      ! 0 |  4700 | `		}` |
|      ! 0 |  4701 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4702 | `		pTos--;` |
|      ! 0 |  4703 | `	}else{` |
|       30 |  4704 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4705 | `	}` |
|       30 |  4706 | `	nIdx = pTos->nIdx;` |
|       30 |  4707 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4708 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4709 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4710 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4711 | `		}else{` |
|        - |  4712 | `			ph7_value *pObj;` |
|        - |  4713 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4714 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4715 | `			if( pObj == 0 ){` |
|      ! 0 |  4716 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4717 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4718 | `				goto Abort;` |
|        - |  4719 | `			}` |
|        - |  4720 | `			/* Perform the store operation */` |
|      ! 0 |  4721 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4722 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4723 | `		}` |
|       30 |  4724 | `	}else if( sName.nByte > 0){` |
|       30 |  4725 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4726 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4727 | `		}else{` |
|       30 |  4728 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4729 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4730 | `				/* Safely ignore the exception frame */` |
|       21 |  4731 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4732 | `			}` |
|        - |  4733 | `			/* Query the local frame */` |
|       30 |  4734 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4735 | `			if( pEntry ){` |
|      ! 0 |  4736 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4737 | `			}else{` |
|       30 |  4738 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4739 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4740 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4741 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4742 | `				}` |
|       30 |  4743 | `				if( rc == SXRET_OK ){` |
|       30 |  4744 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4745 | `				}` |
|        - |  4746 | `			}` |
|        - |  4747 | `		}` |
|       14 |  4748 | `	}` |
|       30 |  4749 | `	break;` |
|        - |  4750 | `				 }` |
|        - |  4751 | `/*` |
|        - |  4752 | ` * OP_UPLINK P1 * *` |
|        - |  4753 | ` * Link a variable to the top active VM frame.` |
|        - |  4754 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4755 | ` */` |
|       14 |  4756 | `case PH7_OP_UPLINK: {` |
|       29 |  4757 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4758 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4759 | `		SyString sName;` |
|        - |  4760 | `		/* Perform the link */` |
|       59 |  4761 | `		while( pLink <= pTos ){` |
|       31 |  4762 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4763 | `				/* Force a string cast */` |
|      ! 0 |  4764 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4765 | `			}` |
|       31 |  4766 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4767 | `			if( sName.nByte > 0 ){` |
|       31 |  4768 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4769 | `			}` |
|       31 |  4770 | `			pLink++;` |
|        1 |  4771 | `		}` |
|       14 |  4772 | `	}` |
|       29 |  4773 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4774 | `	break;` |
|        - |  4775 | `					}` |
|        - |  4776 | `/*` |
|        - |  4777 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4778 | ` * Push an exception in the corresponding container so that` |
|        - |  4779 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4780 | ` */` |
|       10 |  4781 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4782 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4783 | `	VmFrame *pFrameLocal;` |
|       22 |  4784 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4785 | `	/* Create the exception frame */` |
|       22 |  4786 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4787 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4788 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4789 | `		goto Abort;` |
|        - |  4790 | `	}` |
|        - |  4791 | `	/* Mark the special frame */` |
|       22 |  4792 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4793 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4794 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4795 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4796 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4797 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4798 | `	}` |
|       22 |  4799 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4800 | `	break;` |
|        - |  4801 | `							}` |
|        - |  4802 | `/*` |
|        - |  4803 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4804 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4805 | ` */` |
|        9 |  4806 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4807 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4808 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4809 | `		ph7_exception **apException;` |
|        - |  4810 | `		/* Pop the loaded exception */` |
|        7 |  4811 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4812 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4813 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4814 | `		}` |
|        3 |  4815 | `	}` |
|       20 |  4816 | `	pException->pFrame = 0;` |
|        - |  4817 | `	/* Leave the exception frame */` |
|       20 |  4818 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4819 | `	break;` |
|        - |  4820 | `							}` |
|        - |  4821 |  |
|        - |  4822 | `/*` |
|        - |  4823 | ` * OP_THROW * P2 *` |
|        - |  4824 | ` * Throw an user exception.` |
|        - |  4825 | ` */` |
|        8 |  4826 | `case PH7_OP_THROW: {` |
|       18 |  4827 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4828 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4829 | `#ifdef UNTRUST` |
|        - |  4830 | `	if( pTos < pStack ){` |
|        - |  4831 | `		goto Abort;` |
|        - |  4832 | `	}` |
|        - |  4833 | `#endif` |
|       24 |  4834 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4835 | `		/* Safely ignore the exception frame */` |
|        8 |  4836 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4837 | `	}` |
|        - |  4838 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4839 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4840 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4841 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4842 | `		ph7_class *pException;` |
|        - |  4843 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4844 | `		 */` |
|       18 |  4845 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4846 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4847 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4848 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4849 | `			if( rc == SXERR_ABORT ){` |
|        - |  4850 | `				/* Abort processing immediately */` |
|      ! 0 |  4851 | `				goto Abort;` |
|        - |  4852 | `			}` |
|      ! 0 |  4853 | `		}else{` |
|        - |  4854 | `			/* Throw the exception */` |
|       18 |  4855 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4856 | `			if( rc == SXERR_ABORT ){` |
|        - |  4857 | `				/* Abort processing immediately */` |
|        3 |  4858 | `				goto Abort;` |
|        - |  4859 | `			}` |
|        - |  4860 | `		}` |
|        9 |  4861 | `	}else{` |
|        - |  4862 | `		/* Expecting a class instance */` |
|      ! 0 |  4863 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4864 | `		if( rc == SXERR_ABORT ){` |
|        - |  4865 | `			/* Abort processing immediately */` |
|      ! 0 |  4866 | `			goto Abort;` |
|        - |  4867 | `		}` |
|        - |  4868 | `	}` |
|        - |  4869 | `	/* Pop the top entry */` |
|       16 |  4870 | `	VmPopOperand(&pTos,1);` |
|        - |  4871 | `	/* Perform an unconditional jump */` |
|       16 |  4872 | `	pc = nJump - 1;` |
|       16 |  4873 | `	break;` |
|        - |  4874 | `				   }` |
|        - |  4875 | `/*` |
|        - |  4876 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4877 | ` * Prepare a foreach step.` |
|        - |  4878 | ` */` |
|     3471 |  4879 | `case PH7_OP_FOREACH_INIT: {` |
|     6944 |  4880 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4881 | `	void *pName;` |
|        - |  4882 | `#ifdef UNTRUST` |
|        - |  4883 | `	if( pTos < pStack ){` |
|        - |  4884 | `		goto Abort;` |
|        - |  4885 | `	}` |
|        - |  4886 | `#endif` |
|     6944 |  4887 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4888 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4889 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4890 | `			/* Force a string cast */` |
|      ! 0 |  4891 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4892 | `		}` |
|        - |  4893 | `		/* Duplicate name */` |
|      ! 0 |  4894 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4895 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4896 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4897 | `		}` |
|      ! 0 |  4898 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4899 | `	}` |
|     6944 |  4900 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4901 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4902 | `			/* Force a string cast */` |
|      ! 0 |  4903 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4904 | `		}` |
|        - |  4905 | `		/* Duplicate name */` |
|      ! 0 |  4906 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4907 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4908 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4909 | `		}` |
|      ! 0 |  4910 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4911 | `	}` |
|        - |  4912 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     6944 |  4913 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4914 | `		/* Jump out of the loop */` |
|      ! 0 |  4915 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4916 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4917 | `		}` |
|      ! 0 |  4918 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4919 | `	}else{` |
|        - |  4920 | `		ph7_foreach_step *pStep;` |
|     6944 |  4921 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     6944 |  4922 | `		if( pStep == 0 ){` |
|      ! 0 |  4923 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4924 | `			/* Jump out of the loop */` |
|      ! 0 |  4925 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4926 | `		}else{` |
|        - |  4927 | `			/* Zero the structure */` |
|     6944 |  4928 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4929 | `			/* Prepare the step */` |
|     6944 |  4930 | `			pStep->iFlags = pInfo->iFlags;` |
|     6944 |  4931 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     6936 |  4932 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4933 | `				/* Reset the internal loop cursor */` |
|     6936 |  4934 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4935 | `				/* Mark the step */` |
|     6936 |  4936 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     6936 |  4937 | `				pStep->xIter.pMap = pMap;` |
|     6936 |  4938 | `				pMap->iRef++;` |
|     3469 |  4939 | `			}else{` |
|        9 |  4940 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4941 | `				/* Reset the loop cursor */` |
|        9 |  4942 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4943 | `				/* Mark the step */` |
|        9 |  4944 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4945 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4946 | `				pThis->iRef++;` |
|        - |  4947 | `			}` |
|        - |  4948 | `		}` |
|     6944 |  4949 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4950 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4951 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4952 | `			/* Jump out of the loop */` |
|      ! 0 |  4953 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4954 | `		}` |
|        - |  4955 | `	}` |
|     6944 |  4956 | `	VmPopOperand(&pTos,1);` |
|     6944 |  4957 | `	break;` |
|        - |  4958 | `						  }` |
|        - |  4959 | `/*` |
|        - |  4960 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4961 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4962 | ` */` |
|    58633 |  4963 | `case PH7_OP_FOREACH_STEP: {` |
|   117268 |  4964 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4965 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4966 | `	ph7_value *pValue;` |
|        - |  4967 | `	VmFrame *pFrameLocal;` |
|        - |  4968 | `	/* Peek the last step */` |
|   117268 |  4969 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   117268 |  4970 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   117268 |  4971 | `	pFrameLocal = pVm->pFrame;` |
|   122300 |  4972 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4973 | `		/* Safely ignore the exception frame */` |
|     5033 |  4974 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4975 | `	}` |
|   117268 |  4976 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   117244 |  4977 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4978 | `		ph7_hashmap_node *pNode;` |
|        - |  4979 | `		/* Extract the current node value */` |
|   117244 |  4980 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   117244 |  4981 | `		if( pNode == 0 ){` |
|        - |  4982 | `			/* No more entry to process */` |
|     6936 |  4983 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     6936 |  4984 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4985 | `				/* Break the reference with the last element */` |
|        5 |  4986 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4987 | `			}` |
|        - |  4988 | `			/* Automatically reset the loop cursor */` |
|     6936 |  4989 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4990 | `			/* Cleanup the mess left behind */` |
|     6936 |  4991 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     6936 |  4992 | `			SySetPop(&pInfo->aStep);` |
|     6936 |  4993 | `			PH7_HashmapUnref(pMap);` |
|     3469 |  4994 | `		}else{` |
|   110310 |  4995 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      135 |  4996 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      135 |  4997 | `				if( pKey ){` |
|      135 |  4998 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       67 |  4999 | `				}` |
|       67 |  5000 | `			}` |
|   110310 |  5001 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5002 | `				SyHashEntry *pEntry;` |
|        - |  5003 | `				/* Pass by reference */` |
|       13 |  5004 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5005 | `				if( pEntry ){` |
|       13 |  5006 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5007 | `				}else{` |
|      ! 0 |  5008 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5009 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5010 | `				}` |
|        7 |  5011 | `			}else{` |
|        - |  5012 | `				/* Make a copy of the entry value */` |
|   110298 |  5013 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   110298 |  5014 | `				if( pValue ){` |
|   110298 |  5015 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    55148 |  5016 | `				}` |
|        - |  5017 | `			}` |
|        - |  5018 | `		}` |
|    58623 |  5019 | `	}else{` |
|       25 |  5020 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5021 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5022 | `		SyHashEntry *pEntry;` |
|        - |  5023 | `		/* Point to the next attribute */` |
|       29 |  5024 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5025 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5026 | `			/* Check access permission */` |
|       31 |  5027 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5028 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5029 | `					break; /* Access is granted */` |
|        - |  5030 | `			}` |
|        1 |  5031 | `		}` |
|       25 |  5032 | `		if( pEntry == 0 ){` |
|        - |  5033 | `			/* Clean up the mess left behind */` |
|        9 |  5034 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5035 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5036 | `				/* Break the reference with the last element */` |
|        3 |  5037 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5038 | `			}` |
|        9 |  5039 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5040 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5041 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5042 | `		}else{` |
|       17 |  5043 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5044 | `			ph7_value *pAttrValue;` |
|       17 |  5045 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5046 | `				/* Fill with the current attribute name */` |
|       17 |  5047 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5048 | `				if( pKey ){` |
|       17 |  5049 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5050 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5051 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5052 | `				}` |
|        8 |  5053 | `			}` |
|        - |  5054 | `			/* Extract attribute value */` |
|       17 |  5055 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5056 | `			if( pAttrValue ){` |
|       17 |  5057 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5058 | `					/* Pass by reference */` |
|        3 |  5059 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5060 | `					if( pEntry ){` |
|        3 |  5061 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5062 | `					}else{` |
|      ! 0 |  5063 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5064 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5065 | `					}` |
|        2 |  5066 | `				}else{` |
|        - |  5067 | `					/* Make a copy of the attribute value */` |
|       15 |  5068 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5069 | `					if( pValue ){` |
|       15 |  5070 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5071 | `					}` |
|        - |  5072 | `				}` |
|        8 |  5073 | `			}` |
|        - |  5074 | `		}` |
|        - |  5075 | `	}` |
|   117268 |  5076 | `	break;` |
|        - |  5077 | `						  }` |
|        - |  5078 | `/*` |
|        - |  5079 | ` * OP_MEMBER P1 P2` |
|        - |  5080 | ` * Load class attribute/method on the stack.` |
|        - |  5081 | ` */` |
|      480 |  5082 | `case PH7_OP_MEMBER: {` |
|        - |  5083 | `	ph7_class_instance *pThis;` |
|        - |  5084 | `	ph7_value *pNos;` |
|        - |  5085 | `	SyString sName;` |
|      962 |  5086 | `	if( !pInstr->iP1 ){` |
|      904 |  5087 | `		pNos = &pTos[-1];` |
|        - |  5088 | `#ifdef UNTRUST` |
|        - |  5089 | `		if( pNos < pStack ){` |
|        - |  5090 | `			goto Abort;` |
|        - |  5091 | `		}` |
|        - |  5092 | `#endif` |
|      904 |  5093 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5094 | `			ph7_class *pClass;` |
|        - |  5095 | `			/* Class already instantiated */` |
|      904 |  5096 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5097 | `			/* Point to the instantiated class */` |
|      904 |  5098 | `			pClass = pThis->pClass;` |
|        - |  5099 | `			/* Extract attribute name first */` |
|      904 |  5100 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      904 |  5101 | `			if( pInstr->iP2 ){` |
|        - |  5102 | `				/* Method call */` |
|      120 |  5103 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5104 | `				if( sName.nByte > 0 ){` |
|        - |  5105 | `					/* Extract the target method */` |
|      120 |  5106 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5107 | `				}` |
|      120 |  5108 | `				if( pMeth == 0 ){` |
|      ! 0 |  5109 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5110 | `						&pClass->sName,&sName` |
|        - |  5111 | `						);` |
|        - |  5112 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5113 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5114 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5115 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5116 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5117 | `				}else{` |
|        - |  5118 | `					/* Push method name on the stack */` |
|      120 |  5119 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5120 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5121 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5122 | `				}` |
|      120 |  5123 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5124 | `			}else{` |
|        - |  5125 | `				/* Attribute access */` |
|      786 |  5126 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5127 | `				SyHashEntry *pEntry;` |
|        - |  5128 | `				/* Extract the target attribute */` |
|      786 |  5129 | `				if( sName.nByte > 0 ){` |
|      786 |  5130 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|      786 |  5131 | `					if( pEntry ){` |
|        - |  5132 | `						/* Point to the attribute value */` |
|      784 |  5133 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      391 |  5134 | `					}` |
|      392 |  5135 | `				}` |
|      786 |  5136 | `				if( pObjAttr == 0 ){` |
|        - |  5137 | `					/* No such attribute,load null */` |
|        4 |  5138 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5139 | `						&pClass->sName,&sName);` |
|        - |  5140 | `					/* Call the __get magic method if available */` |
|        3 |  5141 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5142 | `				}` |
|      786 |  5143 | `				VmPopOperand(&pTos,1);` |
|        - |  5144 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5145 | `				 * This is due to the following case:` |
|        - |  5146 | `				 *     (new TestClass())->foo;` |
|        - |  5147 | `				 */` |
|      786 |  5148 | `				pThis->iRef++;` |
|      786 |  5149 | `				PH7_MemObjRelease(pTos);` |
|      786 |  5150 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|      786 |  5151 | `				if( pObjAttr ){` |
|      784 |  5152 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5153 | `					/* Check attribute access */` |
|      784 |  5154 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5155 | `						/* Load attribute */` |
|      784 |  5156 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      784 |  5157 | `						if( pValue ){` |
|      784 |  5158 | `							if( pThis->iRef < 2 ){` |
|        - |  5159 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5160 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5161 | `								 */` |
|        3 |  5162 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5163 | `							}else{` |
|        - |  5164 | `								/* Simple load */` |
|      782 |  5165 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5166 | `							}` |
|      784 |  5167 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      782 |  5168 | `								if( pThis->iRef > 1 ){` |
|        - |  5169 | `									/* Load attribute index */` |
|      780 |  5170 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      389 |  5171 | `								}` |
|      390 |  5172 | `							}` |
|      391 |  5173 | `						}` |
|      391 |  5174 | `					}` |
|      391 |  5175 | `				}` |
|        - |  5176 | `				/* Safely unreference the object */` |
|      786 |  5177 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5178 | `			}` |
|      453 |  5179 | `		}else{` |
|      ! 0 |  5180 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5181 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5182 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5183 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5184 | `		}` |
|      453 |  5185 | `	}else{` |
|        - |  5186 | `		/* Static member access using class name */` |
|       59 |  5187 | `		pNos = pTos;` |
|       59 |  5188 | `		pThis = 0;` |
|       59 |  5189 | `		if( !pInstr->p3 ){` |
|       57 |  5190 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5191 | `			pNos--;` |
|        - |  5192 | `#ifdef UNTRUST` |
|        - |  5193 | `			if( pNos < pStack ){` |
|        - |  5194 | `				goto Abort;` |
|        - |  5195 | `			}` |
|        - |  5196 | `#endif` |
|       29 |  5197 | `		}else{` |
|        - |  5198 | `			/* Attribute name already computed */` |
|        3 |  5199 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5200 | `		}` |
|       59 |  5201 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5202 | `			ph7_class *pClass = 0;` |
|       59 |  5203 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5204 | `				/* Class already instantiated */` |
|      ! 0 |  5205 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5206 | `				pClass = pThis->pClass;` |
|      ! 0 |  5207 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5208 | `			}else{` |
|        - |  5209 | `				/* Try to extract the target class */` |
|       59 |  5210 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5211 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5212 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5213 | `				}` |
|        - |  5214 | `			}` |
|       59 |  5215 | `			if( pClass == 0 ){` |
|        - |  5216 | `				/* Undefined class */` |
|      ! 0 |  5217 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5218 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5219 | `					);` |
|      ! 0 |  5220 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5221 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5222 | `				}` |
|      ! 0 |  5223 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5224 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5225 | `			}else{` |
|       59 |  5226 | `				if( pInstr->iP2 ){` |
|        - |  5227 | `					/* Method call */` |
|       25 |  5228 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5229 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5230 | `						/* Extract the target method */` |
|       25 |  5231 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5232 | `					}` |
|       25 |  5233 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5234 | `						if( pMeth ){` |
|      ! 0 |  5235 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5236 | `								&pClass->sName,&sName` |
|        - |  5237 | `								);` |
|      ! 0 |  5238 | `						}else{` |
|      ! 0 |  5239 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5240 | `								&pClass->sName,&sName` |
|        - |  5241 | `								);` |
|        - |  5242 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5243 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5244 | `						}` |
|        - |  5245 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5246 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5247 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5248 | `						}` |
|      ! 0 |  5249 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5250 | `					}else{` |
|        - |  5251 | `						/* Push method name on the stack */` |
|       25 |  5252 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5253 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5254 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5255 | `					}` |
|       25 |  5256 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5257 | `				}else{` |
|        - |  5258 | `					/* Attribute access */` |
|       35 |  5259 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5260 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5261 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5262 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5263 | `						/* ::class returns the fully qualified class name */` |
|        - |  5264 | `						/* Pop the attribute name from the stack */` |
|       27 |  5265 | `						if( !pInstr->p3 ){` |
|       27 |  5266 | `							VmPopOperand(&pTos,1);` |
|       13 |  5267 | `						}` |
|       27 |  5268 | `						PH7_MemObjRelease(pTos);` |
|        - |  5269 | `						/* Load the class name */` |
|       27 |  5270 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5271 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5272 | `					}else{` |
|        - |  5273 | `						/* Extract the target attribute */` |
|        9 |  5274 | `						if( sName.nByte > 0 ){` |
|        9 |  5275 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5276 | `						}` |
|        9 |  5277 | `						if( pAttr == 0 ){` |
|        - |  5278 | `							/* No such attribute,load null */` |
|      ! 0 |  5279 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5280 | `								&pClass->sName,&sName);` |
|        - |  5281 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5282 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5283 | `						}` |
|        - |  5284 | `						/* Pop the attribute name from the stack */` |
|        9 |  5285 | `						if( !pInstr->p3 ){` |
|        7 |  5286 | `							VmPopOperand(&pTos,1);` |
|        3 |  5287 | `						}` |
|        9 |  5288 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5289 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5290 | `						if( pAttr ){` |
|        9 |  5291 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5292 | `								/* Access to a non static attribute */` |
|      ! 0 |  5293 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5294 | `									&pClass->sName,&pAttr->sName` |
|        - |  5295 | `									);` |
|      ! 0 |  5296 | `							}else{` |
|        - |  5297 | `								ph7_value *pValue;` |
|        - |  5298 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5299 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5300 | `									/* Load the desired attribute */` |
|        9 |  5301 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5302 | `									if( pValue ){` |
|        9 |  5303 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5304 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5305 | `											/* Load index number */` |
|        3 |  5306 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5307 | `										}` |
|        4 |  5308 | `									}` |
|        4 |  5309 | `								}` |
|        - |  5310 | `							}` |
|        4 |  5311 | `						}` |
|        - |  5312 | `					}` |
|        - |  5313 | `				}` |
|       59 |  5314 | `				if( pThis ){` |
|        - |  5315 | `					/* Safely unreference the object */` |
|      ! 0 |  5316 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5317 | `				}` |
|        - |  5318 | `			}` |
|       30 |  5319 | `		}else{` |
|        - |  5320 | `			/* Pop operands */` |
|      ! 0 |  5321 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5322 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5323 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5324 | `			}` |
|      ! 0 |  5325 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5326 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5327 | `		}` |
|        - |  5328 | `	}` |
|      962 |  5329 | `	break;` |
|        - |  5330 | `					}` |
|        - |  5331 | `/*` |
|        - |  5332 | ` * OP_NEW P1 * * *` |
|        - |  5333 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5334 | ` */` |
|      247 |  5335 | `case PH7_OP_NEW: {` |
|      496 |  5336 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      496 |  5337 | `	ph7_class *pClass = 0;` |
|        - |  5338 | `	ph7_class_instance *pNew;` |
|      496 |  5339 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5340 | `		/* Try to extract the desired class */` |
|      743 |  5341 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      494 |  5342 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      247 |  5343 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5344 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5345 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5346 | `	}` |
|      496 |  5347 | `	if( pClass == 0 ){` |
|        - |  5348 | `		/* No such class */` |
|      ! 0 |  5349 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5350 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5351 | `			);` |
|      ! 0 |  5352 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5353 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5354 | `			/* Pop given arguments */` |
|      ! 0 |  5355 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5356 | `		}` |
|      ! 0 |  5357 | `	}else{` |
|        - |  5358 | `		ph7_class_method *pCons;` |
|        - |  5359 | `		/* Create a new class instance */` |
|      496 |  5360 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      496 |  5361 | `		if( pNew == 0 ){` |
|      ! 0 |  5362 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5363 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5364 | `				&pClass->sName` |
|        - |  5365 | `			);` |
|      ! 0 |  5366 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5367 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5368 | `				/* Pop given arguments */` |
|      ! 0 |  5369 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5370 | `			}` |
|      ! 0 |  5371 | `			break;` |
|        - |  5372 | `		}` |
|        - |  5373 | `		/* Check if a constructor is available */` |
|      496 |  5374 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      496 |  5375 | `		if( pCons == 0 ){` |
|      444 |  5376 | `			SyString *pName = &pClass->sName;` |
|        - |  5377 | `			/* Check for a constructor with the same base class name */` |
|      444 |  5378 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      221 |  5379 | `		}` |
|      496 |  5380 | `		if( pCons ){` |
|        - |  5381 | `			/* Call the class constructor */` |
|       54 |  5382 | `			SySetReset(&aArg);` |
|       96 |  5383 | `			while( pArg < pTos ){` |
|       44 |  5384 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5385 | `				pArg++;` |
|        2 |  5386 | `			}` |
|       54 |  5387 | `			if( pVm->bErrReport ){` |
|        - |  5388 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5389 | `				sxu32 n;` |
|       12 |  5390 | `				n = SySetUsed(&aArg);` |
|        - |  5391 | `				/* Emit a notice for missing arguments */` |
|       28 |  5392 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       18 |  5393 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       18 |  5394 | `					if( pFuncArg ){` |
|       18 |  5395 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5396 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5397 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5398 | `						}` |
|        8 |  5399 | `					}` |
|       18 |  5400 | `					n++;` |
|        2 |  5401 | `				}` |
|        5 |  5402 | `			}` |
|       54 |  5403 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5404 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5405 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5406 | `				pNew->iRef = 1;` |
|      ! 0 |  5407 | `			}` |
|       26 |  5408 | `		}` |
|      496 |  5409 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5410 | `			/* Pop given arguments */` |
|       38 |  5411 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5412 | `		}` |
|      496 |  5413 | `		PH7_MemObjRelease(pTos);` |
|      496 |  5414 | `		pTos->x.pOther = pNew;` |
|      496 |  5415 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5416 | `	}` |
|      496 |  5417 | `	break;` |
|        - |  5418 | `				 }` |
|        - |  5419 | `/*` |
|        - |  5420 | ` * OP_CLONE * * *` |
|        - |  5421 | ` * Perfome a clone operation.` |
|        - |  5422 | ` */` |
|       23 |  5423 | `case PH7_OP_CLONE: {` |
|        - |  5424 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5425 | `#ifdef UNTRUST` |
|        - |  5426 | `	if( pTos < pStack ){` |
|        - |  5427 | `		goto Abort;` |
|        - |  5428 | `	}` |
|        - |  5429 | `#endif` |
|        - |  5430 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5431 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5432 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5433 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5434 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5435 | `		break;` |
|        - |  5436 | `	}` |
|        - |  5437 | `	/* Point to the source */` |
|       44 |  5438 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5439 | `	/* Perform the clone operation */` |
|       44 |  5440 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5441 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5442 | `	if( pClone == 0 ){` |
|      ! 0 |  5443 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5444 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5445 | `	}else{` |
|        - |  5446 | `		/* Load the cloned object */` |
|       44 |  5447 | `		pTos->x.pOther = pClone;` |
|       44 |  5448 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5449 | `	}` |
|       44 |  5450 | `	break;` |
|        - |  5451 | `				   }` |
|        - |  5452 | `/*` |
|        - |  5453 | ` * OP_SWITCH * * P3` |
|        - |  5454 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5455 | ` */` |
|       16 |  5456 | `case PH7_OP_SWITCH: {` |
|       34 |  5457 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5458 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5459 | `	ph7_value sValue,sCaseValue;` |
|        - |  5460 | `	sxu32 n,nEntry;` |
|        - |  5461 | `#ifdef UNTRUST` |
|        - |  5462 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5463 | `		goto Abort;` |
|        - |  5464 | `	}` |
|        - |  5465 | `#endif` |
|        - |  5466 | `	/* Point to the case table  */` |
|       34 |  5467 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       34 |  5468 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5469 | `	/* Select the appropriate case block to execute */` |
|       34 |  5470 | `	PH7_MemObjInit(pVm,&sValue);` |
|       34 |  5471 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       68 |  5472 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       68 |  5473 | `		pCase = &aCase[n];` |
|       68 |  5474 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5475 | `		/* Execute the case expression first */` |
|       68 |  5476 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5477 | `		/* Compare the two expression */` |
|       68 |  5478 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       68 |  5479 | `		PH7_MemObjRelease(&sValue);` |
|       68 |  5480 | `		PH7_MemObjRelease(&sCaseValue);` |
|       68 |  5481 | `		if( rc == 0 ){` |
|        - |  5482 | `			/* Value match,jump to this block */` |
|       34 |  5483 | `			pc = pCase->nStart - 1;` |
|       34 |  5484 | `			break;` |
|        - |  5485 | `		}` |
|       19 |  5486 | `	}` |
|       34 |  5487 | `	VmPopOperand(&pTos,1);` |
|       34 |  5488 | `	if( n >= nEntry ){` |
|        - |  5489 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5490 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5491 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5492 | `		}else{` |
|        - |  5493 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5494 | `			pc = pSwitch->nOut - 1;` |
|        - |  5495 | `		}` |
|      ! 0 |  5496 | `	}` |
|       34 |  5497 | `	break;` |
|        - |  5498 | `					}` |
|        - |  5499 | `/*` |
|        - |  5500 | ` * OP_CALL P1 * *` |
|        - |  5501 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5502 | ` *  function on the stack.` |
|        - |  5503 | ` */` |
|   207789 |  5504 | `case PH7_OP_CALL: {` |
|   415624 |  5505 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5506 | `	SyHashEntry *pEntry;` |
|        - |  5507 | `	SyString sName;` |
|        - |  5508 | `	/* Extract function name */` |
|   415624 |  5509 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5510 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5511 | `			ph7_value sResult;` |
|      ! 0 |  5512 | `			SySetReset(&aArg);` |
|      ! 0 |  5513 | `			while( pArg < pTos ){` |
|      ! 0 |  5514 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5515 | `				pArg++;` |
|      ! 0 |  5516 | `			}` |
|      ! 0 |  5517 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5518 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5519 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5520 | `			SySetReset(&aArg);` |
|        - |  5521 | `			/* Pop given arguments */` |
|      ! 0 |  5522 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5523 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5524 | `			}` |
|        - |  5525 | `			/* Copy result */` |
|      ! 0 |  5526 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5527 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5528 | `		}else{` |
|        3 |  5529 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5530 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5531 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5532 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5533 | `			}else{` |
|        - |  5534 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5535 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5536 | `			}` |
|        - |  5537 | `			/* Pop given arguments */` |
|        3 |  5538 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5539 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5540 | `			}` |
|        - |  5541 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5542 | `			PH7_MemObjRelease(pTos);` |
|        - |  5543 | `		}` |
|   207778 |  5544 | `		break;` |
|        - |  5545 | `	}` |
|   415622 |  5546 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5547 | `	/* Check for a compiled function first */` |
|   415622 |  5548 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   415622 |  5549 | `	if( pEntry ){` |
|        - |  5550 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5551 | `		ph7_class_instance *pThis;` |
|        - |  5552 | `		ph7_value *pFrameStack;` |
|        - |  5553 | `		ph7_vm_func *pVmFunc;` |
|        - |  5554 | `		ph7_class *pSelf;` |
|        - |  5555 | `		VmFrame *pFrame;` |
|        - |  5556 | `		ph7_value *pObj;` |
|        - |  5557 | `		VmSlot sArg;` |
|        - |  5558 | `		sxu32 n;` |
|        - |  5559 | `		/* initialize fields */` |
|     7786 |  5560 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     7786 |  5561 | `		pThis = 0;` |
|     7786 |  5562 | `		pSelf = 0;` |
|     7786 |  5563 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5564 | `			ph7_class_method *pMeth;` |
|        - |  5565 | `			/* Class method call */` |
|      390 |  5566 | `			ph7_value *pTarget = &pTos[-1];` |
|      390 |  5567 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5568 | `				/* Extract the 'this' pointer */` |
|      390 |  5569 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5570 | `					/* Instance already loaded */` |
|      360 |  5571 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      360 |  5572 | `					pThis->iRef++;` |
|      360 |  5573 | `					pSelf = pThis->pClass;` |
|      179 |  5574 | `				}` |
|      390 |  5575 | `				if( pSelf == 0 ){` |
|       31 |  5576 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5577 | `						/* "Late Static Binding" class name */` |
|       37 |  5578 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5579 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5580 | `					}` |
|       31 |  5581 | `					if( pSelf == 0 ){` |
|        7 |  5582 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5583 | `					}` |
|       15 |  5584 | `				}` |
|      390 |  5585 | `				if( pThis == 0  ){` |
|       31 |  5586 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5587 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5588 | `						/* Safely ignore the exception frame */` |
|        3 |  5589 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5590 | `					}` |
|       31 |  5591 | `					if( pFrameLocal->pParent ){` |
|        - |  5592 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5593 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5594 | `						if( pThis ){` |
|       13 |  5595 | `							pThis->iRef++;` |
|        6 |  5596 | `						}` |
|        9 |  5597 | `					}` |
|       15 |  5598 | `				}` |
|      390 |  5599 | `				VmPopOperand(&pTos,1);` |
|      390 |  5600 | `				PH7_MemObjRelease(pTos);` |
|        - |  5601 | `				/* Synchronize pointers */` |
|      390 |  5602 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5603 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5604 | `				 * user have already computed the random generated unique class method name` |
|        - |  5605 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5606 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5607 | `				 */` |
|      390 |  5608 | `				while( pArg < pStack ){` |
|      ! 0 |  5609 | `					pArg++;` |
|      ! 0 |  5610 | `				}` |
|      390 |  5611 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5612 | `					/* Check if the call is allowed */` |
|      390 |  5613 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      390 |  5614 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5615 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5616 | `							/* Pop given arguments */` |
|      ! 0 |  5617 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5618 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5619 | `							}` |
|        - |  5620 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5621 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5622 | `							break;` |
|        - |  5623 | `						}` |
|        2 |  5624 | `					}` |
|      194 |  5625 | `				}` |
|      194 |  5626 | `			}` |
|      194 |  5627 | `		}` |
|        - |  5628 | `		/* Check The recursion limit */` |
|     7786 |  5629 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5630 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5631 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5632 | `				&pVmFunc->sName);` |
|        - |  5633 | `			/* Pop given arguments */` |
|        3 |  5634 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5635 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5636 | `			}` |
|        - |  5637 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5638 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5639 | `			break;` |
|        - |  5640 | `		}` |
|     7784 |  5641 | `		if( pVmFunc->pNextName ){` |
|        - |  5642 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      129 |  5643 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       64 |  5644 | `		}` |
|        - |  5645 | `		/* Extract the formal argument set */` |
|     7784 |  5646 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5647 | `		/* Create a new VM frame  */` |
|     7784 |  5648 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     7784 |  5649 | `		if( rc != SXRET_OK ){` |
|        - |  5650 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5651 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5652 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5653 | `				&pVmFunc->sName);` |
|        - |  5654 | `			/* Pop given arguments */` |
|      ! 0 |  5655 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5656 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5657 | `			}` |
|        - |  5658 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5659 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5660 | `			break;` |
|        - |  5661 | `		}` |
|     7784 |  5662 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5663 | `			/* Install the '$this' variable */` |
|        - |  5664 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      370 |  5665 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      370 |  5666 | `			if( pObj ){` |
|        - |  5667 | `				/* Reflect the change */` |
|      370 |  5668 | `				pObj->x.pOther = pThis;` |
|      370 |  5669 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      184 |  5670 | `			}` |
|      184 |  5671 | `		}` |
|     7784 |  5672 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5673 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5674 | `			/* Install static variables */` |
|      ! 0 |  5675 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5676 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5677 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5678 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5679 | `					/* Initialize the static variables */` |
|      ! 0 |  5680 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5681 | `					if( pObj ){` |
|        - |  5682 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5683 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5684 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5685 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5686 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5687 | `						}` |
|      ! 0 |  5688 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5689 | `					}else{` |
|      ! 0 |  5690 | `						continue;` |
|        - |  5691 | `					}` |
|      ! 0 |  5692 | `				}` |
|        - |  5693 | `				/* Install in the current frame */` |
|      ! 0 |  5694 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5695 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5696 | `			}` |
|      ! 0 |  5697 | `		}` |
|        - |  5698 | `		/* Push arguments in the local frame */` |
|     7784 |  5699 | `		n = 0;` |
|    22254 |  5700 | `		while( pArg < pTos ){` |
|    14472 |  5701 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    14372 |  5702 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5703 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5704 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5705 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5706 | `						goto Abort;` |
|        - |  5707 | `					}` |
|      ! 0 |  5708 | `				}` |
|        - |  5709 | `				/* Make sure the given arguments are of the correct type */` |
|    14372 |  5710 | `				if( aFormalArg[n].nType > 0 ){` |
|      986 |  5711 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5712 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5713 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5714 | `						ph7_class *pClass;` |
|        - |  5715 | `						/* Try to extract the desired class */` |
|      ! 0 |  5716 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5717 | `						if( pClass ){` |
|      ! 0 |  5718 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5719 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5720 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5721 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5722 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5723 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5724 | `								}` |
|      ! 0 |  5725 | `							}else{` |
|        - |  5726 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5727 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5728 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5729 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5730 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5731 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5732 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5733 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5734 | `								}` |
|        - |  5735 | `							}` |
|      ! 0 |  5736 | `						}` |
|      986 |  5737 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5738 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5739 | `						/* Cast to the desired type */` |
|      ! 0 |  5740 | `						xCast(pArg);` |
|      ! 0 |  5741 | `					}` |
|      492 |  5742 | `				}` |
|    14372 |  5743 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5744 | `					/* Pass by reference */` |
|       25 |  5745 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5746 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5747 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5748 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5749 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5750 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5751 | `						}` |
|        - |  5752 | `						/* Switch to pass by value */` |
|      ! 0 |  5753 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5754 | `					}else{` |
|        - |  5755 | `						SyHashEntry *pRefEntry;` |
|        - |  5756 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5757 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5758 | `						if( pRefEntry == 0 ){` |
|       37 |  5759 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5760 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5761 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5762 | `							sArg.pUserData = 0;` |
|       25 |  5763 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5764 | `						}` |
|       25 |  5765 | `						pObj = 0;` |
|        - |  5766 | `					}` |
|       13 |  5767 | `				}else{` |
|        - |  5768 | `					/* Pass by value,make a copy of the given argument */` |
|    14348 |  5769 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5770 | `				}` |
|     7187 |  5771 | `			}else{` |
|        - |  5772 | `				char zName[32];` |
|        - |  5773 | `				SyString sArgName;` |
|        - |  5774 | `				/* Set a dummy name */` |
|      101 |  5775 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5776 | `				sArgName.zString = zName;` |
|        - |  5777 | `				/* Annonymous argument */` |
|      101 |  5778 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5779 | `			}` |
|    14472 |  5780 | `			if( pObj ){` |
|    14448 |  5781 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5782 | `				/* Insert argument index  */` |
|    14448 |  5783 | `				sArg.nIdx = pObj->nIdx;` |
|    14448 |  5784 | `				sArg.pUserData = 0;` |
|    14448 |  5785 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     7223 |  5786 | `			}` |
|    14472 |  5787 | `			PH7_MemObjRelease(pArg);` |
|    14472 |  5788 | `			pArg++;` |
|    14472 |  5789 | `			++n;` |
|        2 |  5790 | `		}` |
|        - |  5791 | `		/* Set up closure environment */` |
|     7784 |  5792 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5793 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5794 | `			ph7_value *pValue;` |
|        - |  5795 | `			sxu32 iEnv;` |
|        9 |  5796 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5797 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5798 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5799 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5800 | `					/* Do not install null value */` |
|        9 |  5801 | `					continue;` |
|        - |  5802 | `				}` |
|        9 |  5803 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5804 | `				if( pValue == 0 ){` |
|      ! 0 |  5805 | `					continue;` |
|        - |  5806 | `				}` |
|        - |  5807 | `				/* Invalidate any prior representation */` |
|        9 |  5808 | `				PH7_MemObjRelease(pValue);` |
|        - |  5809 | `				/* Duplicate bound variable value */` |
|        9 |  5810 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5811 | `			}` |
|        4 |  5812 | `		}` |
|        - |  5813 | `		/* Process default values */` |
|     8616 |  5814 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|      834 |  5815 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      824 |  5816 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      824 |  5817 | `				if( pObj ){` |
|        - |  5818 | `					/* Evaluate the default value and extract it's result */` |
|      824 |  5819 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|      824 |  5820 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5821 | `						goto Abort;` |
|        - |  5822 | `					}` |
|        - |  5823 | `					/* Insert argument index */` |
|      824 |  5824 | `					sArg.nIdx = pObj->nIdx;` |
|      824 |  5825 | `					sArg.pUserData = 0;` |
|      824 |  5826 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5827 | `					/* Make sure the default argument is of the correct type */` |
|      824 |  5828 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5829 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5830 | `						/* Cast to the desired type */` |
|      ! 0 |  5831 | `						xCast(pObj);` |
|      ! 0 |  5832 | `					}` |
|      411 |  5833 | `				}` |
|      411 |  5834 | `			}` |
|      834 |  5835 | `			++n;` |
|        2 |  5836 | `		}` |
|        - |  5837 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5838 | `		 * does not return anything.` |
|        - |  5839 | `		 */` |
|     7784 |  5840 | `		PH7_MemObjRelease(pTos);` |
|     7784 |  5841 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5842 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     7784 |  5843 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     7784 |  5844 | `		if( pFrameStack == 0 ){` |
|        - |  5845 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5846 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5847 | `				&pVmFunc->sName);` |
|      ! 0 |  5848 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5849 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5850 | `			}` |
|      ! 0 |  5851 | `			break;` |
|        - |  5852 | `		}` |
|     7784 |  5853 | `		if( pSelf ){` |
|        - |  5854 | `			/* Push class name */` |
|      388 |  5855 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      193 |  5856 | `		}` |
|        - |  5857 | `		/* Increment nesting level */` |
|     7784 |  5858 | `		pVm->nRecursionDepth++;` |
|        - |  5859 | `		/* Execute function body */` |
|     7784 |  5860 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5861 | `		/* Decrement nesting level */` |
|     7784 |  5862 | `		pVm->nRecursionDepth--;` |
|     7784 |  5863 | `		if( pSelf ){` |
|        - |  5864 | `			/* Pop class name */` |
|      388 |  5865 | `			(void)SySetPop(&pVm->aSelf);` |
|      193 |  5866 | `		}` |
|        - |  5867 | `		/* Cleanup the mess left behind */` |
|     7784 |  5868 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5869 | `			/* Return by reference,reflect that */` |
|        9 |  5870 | `			if( n != SXU32_HIGH ){` |
|        9 |  5871 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5872 | `				sxu32 i;` |
|        - |  5873 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5874 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5875 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5876 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5877 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5878 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5879 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5880 | `								&pVmFunc->sName);` |
|      ! 0 |  5881 | `						}` |
|      ! 0 |  5882 | `						n = SXU32_HIGH;` |
|      ! 0 |  5883 | `						break;` |
|        - |  5884 | `					}` |
|        3 |  5885 | `				}` |
|        5 |  5886 | `			}else{` |
|      ! 0 |  5887 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5888 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5889 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5890 | `						&pVmFunc->sName);` |
|      ! 0 |  5891 | `				}` |
|        - |  5892 | `			}` |
|        9 |  5893 | `			pTos->nIdx = n;` |
|        4 |  5894 | `		}` |
|        - |  5895 | `		/* Cleanup the mess left behind */` |
|     7784 |  5896 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5897 | `			/* An exception was throw in this frame */` |
|        7 |  5898 | `			pFrame = pFrame->pParent;` |
|        7 |  5899 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5900 | `				/* Pop the resutlt */` |
|        5 |  5901 | `				VmPopOperand(&pTos,1);` |
|        - |  5902 | `				/* Jump to this destination */` |
|        5 |  5903 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5904 | `				rc = PH7_OK;` |
|        3 |  5905 | `			}else{` |
|        3 |  5906 | `				if( pFrame->pParent ){` |
|        3 |  5907 | `					rc = PH7_EXCEPTION;` |
|        2 |  5908 | `				}else{` |
|        - |  5909 | `					/* Continue normal execution */` |
|      ! 0 |  5910 | `					rc = PH7_OK;` |
|        - |  5911 | `				}` |
|        - |  5912 | `			}` |
|        3 |  5913 | `		}` |
|        - |  5914 | `		/* Free the operand stack */` |
|     7784 |  5915 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5916 | `		/* Leave the frame */` |
|     7784 |  5917 | `		VmLeaveFrame(&(*pVm));` |
|     7784 |  5918 | `		if( rc == PH7_ABORT ){` |
|        - |  5919 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5920 | `			goto Abort;` |
|     7784 |  5921 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5922 | `			goto Exception;` |
|        - |  5923 | `		}` |
|     3892 |  5924 | `	}else{` |
|        - |  5925 | `		ph7_user_func *pFunc;` |
|        - |  5926 | `		ph7_context sCtx;` |
|        - |  5927 | `		ph7_value sRet;` |
|        - |  5928 | `		/* Look for an installed foreign function */` |
|   407838 |  5929 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   407838 |  5930 | `		if( pEntry == 0 ){` |
|        - |  5931 | `			/* Call to undefined function */` |
|        5 |  5932 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5933 | `			/* Pop given arguments */` |
|        5 |  5934 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5935 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5936 | `			}` |
|        - |  5937 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5938 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5939 | `			break;` |
|        - |  5940 | `		}` |
|   407834 |  5941 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5942 | `		/* Start collecting function arguments */` |
|   407834 |  5943 | `		SySetReset(&aArg);` |
|  1106820 |  5944 | `		while( pArg < pTos ){` |
|   698988 |  5945 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   698988 |  5946 | `			pArg++;` |
|        2 |  5947 | `		}` |
|        - |  5948 | `		/* Assume a null return value */` |
|   407834 |  5949 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5950 | `		/* Init the call context */` |
|   407834 |  5951 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5952 | `		/* Call the foreign function */` |
|   407834 |  5953 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5954 | `		/* Release the call context */` |
|   407834 |  5955 | `		VmReleaseCallContext(&sCtx);` |
|   407834 |  5956 | `		if( rc == PH7_ABORT ){` |
|       23 |  5957 | `			goto Abort;` |
|   407812 |  5958 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5959 | `			goto Exception;` |
|        - |  5960 | `		}` |
|   407810 |  5961 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5962 | `			/* Pop function name and arguments */` |
|   393182 |  5963 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   196612 |  5964 | `		}` |
|        - |  5965 | `		/* Save foreign function return value */` |
|   407810 |  5966 | `		PH7_MemObjStore(&sRet,pTos);` |
|   407810 |  5967 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5968 | `	}` |
|   415590 |  5969 | `	break;` |
|        - |  5970 | `				  }` |
|        - |  5971 | `/*` |
|        - |  5972 | ` * OP_CONSUME: P1 * *` |
|        - |  5973 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5974 | ` */` |
|     8159 |  5975 | `case PH7_OP_CONSUME: {` |
|    16320 |  5976 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    16320 |  5977 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5978 |  |
|    16320 |  5979 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    16320 |  5980 | `	pCur = pOut;` |
|        - |  5981 | `	/* Start the consume process  */` |
|    32668 |  5982 | `	while( pOut <= pTos ){` |
|        - |  5983 | `		/* Force a string cast */` |
|    16350 |  5984 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       56 |  5985 | `			PH7_MemObjToString(pOut);` |
|       27 |  5986 | `		}` |
|    16350 |  5987 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5988 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5989 | `			/* Invoke the output consumer callback */` |
|     8872 |  5990 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     8872 |  5991 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5992 | `				/* Increment output length */` |
|     3364 |  5993 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1681 |  5994 | `			}` |
|     8872 |  5995 | `			SyBlobRelease(&pOut->sBlob);` |
|     8872 |  5996 | `			if( rc == SXERR_ABORT ){` |
|        - |  5997 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5998 | `				goto Abort;` |
|        - |  5999 | `			}` |
|     4435 |  6000 | `		}` |
|    16350 |  6001 | `		pOut++;` |
|        2 |  6002 | `	}` |
|    16320 |  6003 | `	pTos = &pCur[-1];` |
|    16318 |  6004 | `	break;` |
|        - |  6005 | `					 }` |
|        - |  6006 |  |
|        - |  6007 | `		} /* Switch() */` |
|  6670230 |  6008 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6009 | `	} /* For(;;) */` |
|     9719 |  6010 | `Done:` |
|    19440 |  6011 | `	SySetRelease(&aArg);` |
|    19440 |  6012 | `	return SXRET_OK;` |
|       12 |  6013 | `Abort:` |
|       25 |  6014 | `	SySetRelease(&aArg);` |
|       69 |  6015 | `	while( pTos >= pStack ){` |
|       45 |  6016 | `		PH7_MemObjRelease(pTos);` |
|       45 |  6017 | `		pTos--;` |
|        1 |  6018 | `	}` |
|       25 |  6019 | `	return PH7_ABORT;` |
|        2 |  6020 | `Exception:` |
|        5 |  6021 | `	SySetRelease(&aArg);` |
|        9 |  6022 | `	while( pTos >= pStack ){` |
|        5 |  6023 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6024 | `		pTos--;` |
|        1 |  6025 | `	}` |
|        5 |  6026 | `	return PH7_EXCEPTION;` |
|     9735 |  6027 |  |
|        - |  6028 | `/*` |
|        - |  6029 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6030 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6031 | ` * See block-comment on that function for additional information.` |
|        - |  6032 | ` */` |
|    10174 |  6033 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6034 |  |
|        - |  6035 | `	ph7_value *pStack;` |
|        - |  6036 | `	sxi32 rc;` |
|        - |  6037 | `	/* Allocate a new operand stack */` |
|    10176 |  6038 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    10176 |  6039 | `	if( pStack == 0 ){` |
|      ! 0 |  6040 | `		return SXERR_MEM;` |
|        - |  6041 | `	}` |
|        - |  6042 | `	/* Execute the program */` |
|    10176 |  6043 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6044 | `	/* Free the operand stack */` |
|    10176 |  6045 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6046 | `	/* Execution result */` |
|    10176 |  6047 | `	return rc;` |
|     5089 |  6048 |  |
|        - |  6049 | `/*` |
|        - |  6050 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6051 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6052 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6053 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6054 | ` * execution ends.` |
|        - |  6055 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6056 | ` * additional information.` |
|        - |  6057 | ` */` |
|      966 |  6058 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6059 |  |
|        - |  6060 | `	VmShutdownCB *pEntry;` |
|        - |  6061 | `	ph7_value *apArg[10];` |
|        - |  6062 | `	sxu32 n,nEntry;` |
|        - |  6063 | `	int i;` |
|        - |  6064 | `	/* Point to the stack of registered callbacks */` |
|      968 |  6065 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    10628 |  6066 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     9662 |  6067 | `		apArg[i] = 0;` |
|     4832 |  6068 | `	}` |
|      970 |  6069 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6070 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6071 | `		if( pEntry ){` |
|        - |  6072 | `			/* Prepare callback arguments if any */` |
|        3 |  6073 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6074 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6075 | `					break;` |
|        - |  6076 | `				}` |
|      ! 0 |  6077 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6078 | `			}` |
|        - |  6079 | `			/* Invoke the callback */` |
|        3 |  6080 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6081 | `			/*` |
|        - |  6082 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6083 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6084 | `			 */` |
|        3 |  6085 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6086 | `			if( pEntry ){` |
|        3 |  6087 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6088 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6089 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6090 | `				}` |
|        1 |  6091 | `			}` |
|        1 |  6092 | `		}` |
|        2 |  6093 | `	}` |
|      968 |  6094 | `	SySetReset(&pVm->aShutdown);` |
|      968 |  6095 |  |
|        - |  6096 | `/*` |
|        - |  6097 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6098 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6099 | ` * See block-comment on that function for additional information.` |
|        - |  6100 | ` */` |
|      974 |  6101 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6102 |  |
|        - |  6103 | `	/* Make sure we are ready to execute this program */` |
|      976 |  6104 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6105 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6106 | `	}` |
|        - |  6107 | `	/* Set the execution magic number  */` |
|      976 |  6108 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6109 | `	/* Execute the program */` |
|      976 |  6110 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6111 | `	/* Invoke any shutdown callbacks */` |
|      972 |  6112 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6113 | `	/*` |
|        - |  6114 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6115 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6116 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6117 | `	 */` |
|      972 |  6118 | `	return SXRET_OK;` |
|      489 |  6119 |  |
|        - |  6120 | `/*` |
|        - |  6121 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6122 | ` * the desired message.` |
|        - |  6123 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6124 | ` * in 'api.c' for additional information.` |
|        - |  6125 | ` */` |
|      378 |  6126 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6127 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6128 | `	SyString *pString /* Message to output */` |
|        - |  6129 | `	)` |
|        2 |  6130 |  |
|      380 |  6131 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      380 |  6132 | `	sxi32 rc = SXRET_OK;` |
|        - |  6133 | `	/* Call the output consumer */` |
|      380 |  6134 | `	if( pString->nByte > 0 ){` |
|      380 |  6135 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      380 |  6136 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6137 | `			/* Increment output length */` |
|       17 |  6138 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6139 | `		}` |
|      189 |  6140 | `	}` |
|      380 |  6141 | `	return rc;` |
|        2 |  6142 |  |
|        - |  6143 | `/*` |
|        - |  6144 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6145 | ` * callback to consume the formatted message.` |
|        - |  6146 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6147 | ` * in 'api.c' for additional information.` |
|        - |  6148 | ` */` |
|        2 |  6149 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6150 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6151 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6152 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6153 | `	)` |
|        1 |  6154 |  |
|        3 |  6155 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6156 | `	sxi32 rc = SXRET_OK;` |
|        - |  6157 | `	SyBlob sWorker;` |
|        - |  6158 | `	/* Format the message and call the output consumer */` |
|        3 |  6159 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6160 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6161 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6162 | `		/* Consume the formatted message */` |
|        3 |  6163 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6164 | `	}` |
|        3 |  6165 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6166 | `		/* Increment output length */` |
|      ! 0 |  6167 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6168 | `	}` |
|        - |  6169 | `	/* Release the working buffer */` |
|        3 |  6170 | `	SyBlobRelease(&sWorker);` |
|        3 |  6171 | `	return rc;` |
|        1 |  6172 |  |
|        - |  6173 | `/*` |
|        - |  6174 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6175 | ` * This function never fail and always return a pointer` |
|        - |  6176 | ` * to a null terminated string.` |
|        - |  6177 | ` */` |
|       10 |  6178 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6179 |  |
|       11 |  6180 | `	const char *zOp = "Unknown     ";` |
|       11 |  6181 | `	switch(nOp){` |
|        3 |  6182 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6185 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6190 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6229 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6230 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6243 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6246 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6247 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6248 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6252 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6254 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6255 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6256 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6257 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6259 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6260 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6261 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6262 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6263 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6264 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6265 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6266 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6267 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6268 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6269 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6270 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6271 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6272 | `	default:` |
|      ! 0 |  6273 | `		break;` |
|        - |  6274 | `	}` |
|       11 |  6275 | `	return zOp;` |
|        1 |  6276 |  |
|        - |  6277 | `/*` |
|        - |  6278 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6279 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6280 | ` * is responsible of consuming the generated dump.` |
|        - |  6281 | ` */` |
|        2 |  6282 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6283 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6284 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6285 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6286 | `	)` |
|        1 |  6287 |  |
|        - |  6288 | `	sxi32 rc;` |
|        3 |  6289 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6290 | `	return rc;` |
|        1 |  6291 |  |
|        - |  6292 | `/*` |
|        - |  6293 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6294 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6295 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6296 | ` * in 'compile.c' for additional information.` |
|        - |  6297 | ` */` |
|        8 |  6298 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6299 |  |
|        9 |  6300 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6301 | `	/* Evaluate and expand constant value */` |
|        9 |  6302 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6303 |  |
|        - |  6304 | `/*` |
|        - |  6305 | ` * Section:` |
|        - |  6306 | ` *  Function handling functions.` |
|        - |  6307 | ` * Status:` |
|        - |  6308 | ` *    Stable.` |
|        - |  6309 | ` */` |
|        - |  6310 | `/*` |
|        - |  6311 | ` * int func_num_args(void)` |
|        - |  6312 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6313 | ` * Parameters` |
|        - |  6314 | ` *   None.` |
|        - |  6315 | ` * Return` |
|        - |  6316 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6317 | ` *  or -1 if called from the globe scope.` |
|        - |  6318 | ` */` |
|      754 |  6319 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6320 |  |
|        - |  6321 | `	VmFrame *pFrame;` |
|        - |  6322 | `	ph7_vm *pVm;` |
|        - |  6323 | `	/* Point to the target VM */` |
|      756 |  6324 | `	pVm = pCtx->pVm;` |
|        - |  6325 | `	/* Current frame */` |
|      756 |  6326 | `	pFrame = pVm->pFrame;` |
|      756 |  6327 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6328 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6329 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6330 | `	}` |
|      756 |  6331 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6332 | `		SXUNUSED(nArg);` |
|      ! 0 |  6333 | `		SXUNUSED(apArg);` |
|        - |  6334 | `		/* Global frame,return -1 */` |
|      ! 0 |  6335 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6336 | `		return SXRET_OK;` |
|        - |  6337 | `	}` |
|        - |  6338 | `	/* Total number of arguments passed to the enclosing function */` |
|      756 |  6339 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      756 |  6340 | `	ph7_result_int(pCtx,nArg);` |
|      756 |  6341 | `	return SXRET_OK;` |
|      379 |  6342 |  |
|        - |  6343 | `/*` |
|        - |  6344 | ` * value func_get_arg(int $arg_num)` |
|        - |  6345 | ` *   Return an item from the argument list.` |
|        - |  6346 | ` * Parameters` |
|        - |  6347 | ` *  Argument number(index start from zero).` |
|        - |  6348 | ` * Return` |
|        - |  6349 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6350 | ` */` |
|        6 |  6351 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6352 |  |
|        8 |  6353 | `	ph7_value *pObj = 0;` |
|        8 |  6354 | `	VmSlot *pSlot = 0;` |
|        - |  6355 | `	VmFrame *pFrame;` |
|        - |  6356 | `	ph7_vm *pVm;` |
|        - |  6357 | `	/* Point to the target VM */` |
|        8 |  6358 | `	pVm = pCtx->pVm;` |
|        - |  6359 | `	/* Current frame */` |
|        8 |  6360 | `	pFrame = pVm->pFrame;` |
|        8 |  6361 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6362 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6363 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6364 | `	}` |
|        8 |  6365 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6366 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6367 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6368 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6369 | `		return SXRET_OK;` |
|        - |  6370 | `	}` |
|        - |  6371 | `	/* Extract the desired index */` |
|        5 |  6372 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6373 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6374 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6375 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6376 | `		return SXRET_OK;` |
|        - |  6377 | `	}` |
|        - |  6378 | `	/* Extract the desired argument */` |
|        5 |  6379 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6380 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6381 | `			/* Return the desired argument */` |
|        5 |  6382 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6383 | `		}else{` |
|        - |  6384 | `			/* No such argument,return false */` |
|      ! 0 |  6385 | `			ph7_result_bool(pCtx,0);` |
|        - |  6386 | `		}` |
|        3 |  6387 | `	}else{` |
|        - |  6388 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6389 | `		ph7_result_bool(pCtx,0);` |
|        - |  6390 | `	}` |
|        5 |  6391 | `	return SXRET_OK;` |
|        5 |  6392 |  |
|        - |  6393 | `/*` |
|        - |  6394 | ` * array func_get_args_byref(void)` |
|        - |  6395 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6396 | ` * Parameters` |
|        - |  6397 | ` *  None.` |
|        - |  6398 | ` * Return` |
|        - |  6399 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6400 | ` *  member of the current user-defined function's argument list.` |
|        - |  6401 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6402 | ` * NOTE:` |
|        - |  6403 | ` *  Arguments are returned to the array by reference.` |
|        - |  6404 | ` */` |
|        2 |  6405 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6406 |  |
|        - |  6407 | `	ph7_value *pArray;` |
|        - |  6408 | `	VmFrame *pFrame;` |
|        - |  6409 | `	VmSlot *aSlot;` |
|        - |  6410 | `	sxu32 n;` |
|        - |  6411 | `	/* Point to the current frame */` |
|        3 |  6412 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6413 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6414 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6415 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6416 | `	}` |
|        3 |  6417 | `	if( pFrame->pParent == 0 ){` |
|        - |  6418 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6419 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6420 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6421 | `		return SXRET_OK;` |
|        - |  6422 | `	}` |
|        - |  6423 | `	/* Create a new array */` |
|        3 |  6424 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6425 | `	if( pArray == 0 ){` |
|      ! 0 |  6426 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6427 | `		SXUNUSED(apArg);` |
|      ! 0 |  6428 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6429 | `		return SXRET_OK;` |
|        - |  6430 | `	}` |
|        - |  6431 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6432 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6433 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6434 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6435 | `	}` |
|        - |  6436 | `	/* Return the freshly created array */` |
|        3 |  6437 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6438 | `	return SXRET_OK;` |
|        2 |  6439 |  |
|        - |  6440 | `/*` |
|        - |  6441 | ` * array func_get_args(void)` |
|        - |  6442 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6443 | ` * Parameters` |
|        - |  6444 | ` *  None.` |
|        - |  6445 | ` * Return` |
|        - |  6446 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6447 | ` *  member of the current user-defined function's argument list.` |
|        - |  6448 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6449 | ` */` |
|       46 |  6450 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6451 |  |
|       47 |  6452 | `	ph7_value *pObj = 0;` |
|        - |  6453 | `	ph7_value *pArray;` |
|        - |  6454 | `	VmFrame *pFrame;` |
|        - |  6455 | `	VmSlot *aSlot;` |
|        - |  6456 | `	sxu32 n;` |
|        - |  6457 | `	/* Point to the current frame */` |
|       47 |  6458 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6459 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6460 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6461 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6462 | `	}` |
|       47 |  6463 | `	if( pFrame->pParent == 0 ){` |
|        - |  6464 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6465 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6466 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6467 | `		return SXRET_OK;` |
|        - |  6468 | `	}` |
|        - |  6469 | `	/* Create a new array */` |
|       47 |  6470 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6471 | `	if( pArray == 0 ){` |
|      ! 0 |  6472 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6473 | `		SXUNUSED(apArg);` |
|      ! 0 |  6474 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6475 | `		return SXRET_OK;` |
|        - |  6476 | `	}` |
|        - |  6477 | `	/* Start filling the array with the given arguments */` |
|       47 |  6478 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6479 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6480 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6481 | `		if( pObj ){` |
|       97 |  6482 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6483 | `		}` |
|       49 |  6484 | `	}` |
|        - |  6485 | `	/* Return the freshly created array */` |
|       47 |  6486 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6487 | `	return SXRET_OK;` |
|       24 |  6488 |  |
|        - |  6489 | `/*` |
|        - |  6490 | ` * bool function_exists(string $name)` |
|        - |  6491 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6492 | ` * Parameters` |
|        - |  6493 | ` *  The name of the desired function.` |
|        - |  6494 | ` * Return` |
|        - |  6495 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6496 | ` */` |
|     1696 |  6497 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6498 |  |
|        - |  6499 | `	const char *zName;` |
|        - |  6500 | `	ph7_vm *pVm;` |
|        - |  6501 | `	int nLen;` |
|        - |  6502 | `	int res;` |
|     1698 |  6503 | `	if( nArg < 1 ){` |
|        - |  6504 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6505 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6506 | `		return SXRET_OK;` |
|        - |  6507 | `	}` |
|        - |  6508 | `	/* Point to the target VM */` |
|     1698 |  6509 | `	pVm = pCtx->pVm;` |
|        - |  6510 | `	/* Extract the function name */` |
|     1698 |  6511 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6512 | `	/* Assume the function is not defined */` |
|     1698 |  6513 | `	res = 0;` |
|        - |  6514 | `	/* Perform the lookup */` |
|     2544 |  6515 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1692 |  6516 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6517 | `			/* Function is defined */` |
|      212 |  6518 | `			res = 1;` |
|      105 |  6519 | `	}` |
|     1698 |  6520 | `	ph7_result_bool(pCtx,res);` |
|     1698 |  6521 | `	return SXRET_OK;` |
|      850 |  6522 |  |
|        - |  6523 | `/* Forward declaration */` |
|        - |  6524 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6525 | `/*` |
|        - |  6526 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6527 | ` * [i.e: Whether it is callable or not].` |
|        - |  6528 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6529 | ` */` |
|    11434 |  6530 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6531 |  |
|    11436 |  6532 | `	int res = 0;` |
|    11436 |  6533 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6534 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6535 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6536 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6537 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6538 | `		if( pMethod && CallInvoke ){` |
|        - |  6539 | `			ph7_value sResult;` |
|        - |  6540 | `			sxi32 rc;` |
|        - |  6541 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6542 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6543 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6544 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6545 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6546 | `			}` |
|      ! 0 |  6547 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6548 | `		}` |
|    11436 |  6549 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  6550 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        7 |  6551 | `		if( pMap->nEntry > 1 ){` |
|        - |  6552 | `			ph7_class *pClass;` |
|        - |  6553 | `			ph7_value *pV;` |
|        - |  6554 | `			/* Extract the target class */` |
|        7 |  6555 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6556 | `			if( pV ){` |
|        7 |  6557 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|        7 |  6558 | `				if( pClass ){` |
|        - |  6559 | `					ph7_class_method *pMethod;` |
|        - |  6560 | `					/* Extract the target method */` |
|        7 |  6561 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6562 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6563 | `						/* Perform the lookup */` |
|        7 |  6564 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6565 | `						if( pMethod ){` |
|        - |  6566 | `							/* Method is callable */` |
|        5 |  6567 | `							res = 1;` |
|        2 |  6568 | `						}` |
|        3 |  6569 | `					}` |
|        3 |  6570 | `				}` |
|        3 |  6571 | `			}` |
|        4 |  6572 | `		}` |
|    11433 |  6573 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6574 | `		const char *zName;` |
|        - |  6575 | `		int nLen;` |
|        - |  6576 | `		/* Extract the name */` |
|     2804 |  6577 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6578 | `		/* Perform the lookup */` |
|     2807 |  6579 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|        6 |  6580 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6581 | `				/* Function is callable */` |
|     2800 |  6582 | `				res = 1;` |
|     1399 |  6583 | `		}` |
|     1401 |  6584 | `	}` |
|    11436 |  6585 | `	return res;` |
|        2 |  6586 |  |
|        - |  6587 | `/*` |
|        - |  6588 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6589 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6590 | ` * Parameters` |
|        - |  6591 | ` * $name` |
|        - |  6592 | ` *    The callback function to check` |
|        - |  6593 | ` * $syntax_only` |
|        - |  6594 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6595 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6596 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6597 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6598 | ` *    a string.` |
|        - |  6599 | ` * Return` |
|        - |  6600 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6601 | ` */` |
|       14 |  6602 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6603 |  |
|        - |  6604 | `	ph7_vm *pVm;` |
|        - |  6605 | `	int res;` |
|       15 |  6606 | `	if( nArg < 1 ){` |
|        - |  6607 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6608 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6609 | `		return SXRET_OK;` |
|        - |  6610 | `	}` |
|        - |  6611 | `	/* Point to the target VM */` |
|       15 |  6612 | `	pVm = pCtx->pVm;` |
|        - |  6613 | `	/* Perform the requested operation */` |
|       15 |  6614 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6615 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6616 | `	return SXRET_OK;` |
|        8 |  6617 |  |
|        - |  6618 | `/*` |
|        - |  6619 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6620 | ` * defined below.` |
|        - |  6621 | ` */` |
|     1040 |  6622 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6623 |  |
|     1041 |  6624 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6625 | `	ph7_value sName;` |
|        - |  6626 | `	sxi32 rc;` |
|        - |  6627 | `	/* Prepare the function name for insertion */` |
|     1041 |  6628 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1041 |  6629 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6630 | `	/* Perform the insertion */` |
|     1041 |  6631 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1041 |  6632 | `	PH7_MemObjRelease(&sName);` |
|     1041 |  6633 | `	return rc;` |
|        1 |  6634 |  |
|        - |  6635 | `/*` |
|        - |  6636 | ` * array get_defined_functions(void)` |
|        - |  6637 | ` *  Returns an array of all defined functions.` |
|        - |  6638 | ` * Parameter` |
|        - |  6639 | ` *  None.` |
|        - |  6640 | ` * Return` |
|        - |  6641 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6642 | ` *  both built-in (internal) and user-defined.` |
|        - |  6643 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6644 | ` *  defined ones using $arr["user"].` |
|        - |  6645 | ` * Note:` |
|        - |  6646 | ` *  NULL is returned on failure.` |
|        - |  6647 | ` */` |
|        2 |  6648 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6649 |  |
|        - |  6650 | `	ph7_value *pArray,*pEntry;` |
|        - |  6651 | `	/* NOTE:` |
|        - |  6652 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6653 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6654 | `	 */` |
|        3 |  6655 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6656 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6657 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6658 | `		SXUNUSED(apArg);` |
|        - |  6659 | `		/* Return NULL */` |
|      ! 0 |  6660 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6661 | `		return SXRET_OK;` |
|        - |  6662 | `	}` |
|        3 |  6663 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6664 | `	if( pEntry == 0 ){` |
|        - |  6665 | `		/* Return NULL */` |
|      ! 0 |  6666 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6667 | `		return SXRET_OK;` |
|        - |  6668 | `	}` |
|        - |  6669 | `	/* Fill with the appropriate information */` |
|        3 |  6670 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6671 | `	/* Create the 'internal' index */` |
|        3 |  6672 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6673 | `	/* Create the user-func array */` |
|        3 |  6674 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6675 | `	if( pEntry == 0 ){` |
|        - |  6676 | `		/* Return NULL */` |
|      ! 0 |  6677 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6678 | `		return SXRET_OK;` |
|        - |  6679 | `	}` |
|        - |  6680 | `	/* Fill with the appropriate information */` |
|        3 |  6681 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6682 | `	/* Create the 'user' index */` |
|        3 |  6683 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6684 | `	/* Return the multi-dimensional array */` |
|        3 |  6685 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6686 | `	return SXRET_OK;` |
|        2 |  6687 |  |
|        - |  6688 | `/*` |
|        - |  6689 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6690 | ` *  Register a function for execution on shutdown.` |
|        - |  6691 | ` * Note` |
|        - |  6692 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6693 | ` *  be called in the same order as they were registered.` |
|        - |  6694 | ` * Parameters` |
|        - |  6695 | ` *  $callback` |
|        - |  6696 | ` *   The shutdown callback to register.` |
|        - |  6697 | ` * $param` |
|        - |  6698 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6699 | ` * Return` |
|        - |  6700 | ` *  Nothing.` |
|        - |  6701 | ` */` |
|        2 |  6702 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6703 |  |
|        - |  6704 | `	VmShutdownCB sEntry;` |
|        - |  6705 | `	int i,j;` |
|        3 |  6706 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6707 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6708 | `		return PH7_OK;` |
|        - |  6709 | `	}` |
|        - |  6710 | `	/* Zero the Entry */` |
|        3 |  6711 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6712 | `	/* Initialize fields */` |
|        3 |  6713 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6714 | `	/* Save the callback name for later invocation name */` |
|        3 |  6715 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6716 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6717 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6718 | `	}` |
|        - |  6719 | `	/* Copy arguments */` |
|        3 |  6720 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6721 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6722 | `			/* Limit reached */` |
|      ! 0 |  6723 | `			break;` |
|        - |  6724 | `		}` |
|      ! 0 |  6725 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6726 | `	}` |
|        3 |  6727 | `	sEntry.nArg = j;` |
|        - |  6728 | `	/* Install the callback */` |
|        3 |  6729 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6730 | `	return PH7_OK;` |
|        2 |  6731 |  |
|        - |  6732 | `/*` |
|        - |  6733 | ` * Section:` |
|        - |  6734 | ` *  Class handling functions.` |
|        - |  6735 | ` * Status:` |
|        - |  6736 | ` *    Stable.` |
|        - |  6737 | ` */` |
|        - |  6738 | `/*` |
|        - |  6739 | ` * Extract the top active class. NULL is returned` |
|        - |  6740 | ` * if the class stack is empty.` |
|        - |  6741 | ` */` |
|       64 |  6742 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6743 |  |
|       66 |  6744 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6745 | `	ph7_class **apClass;` |
|       66 |  6746 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6747 | `		/* Empty stack,return NULL */` |
|       15 |  6748 | `		return 0;` |
|        - |  6749 | `	}` |
|        - |  6750 | `	/* Peek the last entry */` |
|       52 |  6751 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|       52 |  6752 | `	return apClass[pSet->nUsed - 1];` |
|       34 |  6753 |  |
|        - |  6754 | `/*` |
|        - |  6755 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6756 | ` *   Get the class that declared the currently executing method.` |
|        - |  6757 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6758 | ` *` |
|        - |  6759 | ` * Parameters` |
|        - |  6760 | ` *   pVm: Target VM` |
|        - |  6761 | ` *` |
|        - |  6762 | ` * Return` |
|        - |  6763 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6764 | ` *   - Not executing within a class method` |
|        - |  6765 | ` *` |
|        - |  6766 | ` * Note` |
|        - |  6767 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6768 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6769 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6770 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6771 | ` *   declaring class.` |
|        - |  6772 | ` */` |
|       18 |  6773 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6774 |  |
|       19 |  6775 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6776 | `	ph7_vm_func *pVmFunc;` |
|        - |  6777 |  |
|        - |  6778 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6779 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6780 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6781 | `	}` |
|        - |  6782 |  |
|        - |  6783 | `	/* Check if we're in a method context */` |
|       19 |  6784 | `	if( pFrame->pParent ){` |
|       15 |  6785 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6786 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6787 | `			/* Return the declaring class */` |
|       15 |  6788 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6789 | `		}` |
|      ! 0 |  6790 | `	}` |
|        - |  6791 |  |
|        5 |  6792 | `	return 0;` |
|       10 |  6793 |  |
|        - |  6794 |  |
|        - |  6795 | `/*` |
|        - |  6796 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6797 | ` *   Returns the name of the class of an object` |
|        - |  6798 | ` * Parameters` |
|        - |  6799 | ` *  object` |
|        - |  6800 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6801 | ` * Return` |
|        - |  6802 | ` *  The name of the class of which object is an instance.` |
|        - |  6803 | ` *  Returns FALSE if object is not an object.` |
|        - |  6804 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6805 | ` */` |
|       18 |  6806 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6807 |  |
|        - |  6808 | `	ph7_class *pClass;` |
|        - |  6809 | `	SyString *pName;` |
|       20 |  6810 | `	if( nArg < 1 ){` |
|        - |  6811 | `		/* Check if we are inside a class */` |
|      ! 0 |  6812 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6813 | `		if( pClass ){` |
|        - |  6814 | `			/* Point to the class name */` |
|      ! 0 |  6815 | `			pName = &pClass->sName;` |
|      ! 0 |  6816 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6817 | `		}else{` |
|        - |  6818 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6819 | `			ph7_result_bool(pCtx,0);` |
|        - |  6820 | `		}` |
|      ! 0 |  6821 | `	}else{` |
|        - |  6822 | `		/* Extract the target class */` |
|       20 |  6823 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6824 | `		if( pClass ){` |
|       18 |  6825 | `			pName = &pClass->sName;` |
|        - |  6826 | `			/* Return the class name */` |
|       18 |  6827 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6828 | `		}else{` |
|        - |  6829 | `			/* Not a class instance,return FALSE */` |
|        3 |  6830 | `			ph7_result_bool(pCtx,0);` |
|        - |  6831 | `		}` |
|        - |  6832 | `	}` |
|       20 |  6833 | `	return PH7_OK;` |
|        2 |  6834 |  |
|        - |  6835 | `/*` |
|        - |  6836 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6837 | ` *   Returns the name of the parent class of an object` |
|        - |  6838 | ` * Parameters` |
|        - |  6839 | ` *  object` |
|        - |  6840 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6841 | ` * Return` |
|        - |  6842 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6843 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6844 | ` *  not have a parent.` |
|        - |  6845 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6846 | ` */` |
|        8 |  6847 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6848 |  |
|        - |  6849 | `	ph7_class *pClass;` |
|        - |  6850 | `	SyString *pName;` |
|        9 |  6851 | `	if( nArg < 1 ){` |
|        - |  6852 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6853 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6854 | `		if( pClass && pClass->pBase ){` |
|        - |  6855 | `			/* Point to the class name */` |
|        3 |  6856 | `			pName = &pClass->pBase->sName;` |
|        3 |  6857 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6858 | `		}else{` |
|        - |  6859 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6860 | `			ph7_result_bool(pCtx,0);` |
|        - |  6861 | `		}` |
|        2 |  6862 | `	}else{` |
|        - |  6863 | `		/* Extract the target class */` |
|        7 |  6864 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6865 | `		if( pClass ){` |
|        7 |  6866 | `			if( pClass->pBase ){` |
|        5 |  6867 | `				pName = &pClass->pBase->sName;` |
|        - |  6868 | `				/* Return the parent class name */` |
|        5 |  6869 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6870 | `			}else{` |
|        - |  6871 | `				/* Object does not have a parent class */` |
|        3 |  6872 | `				ph7_result_bool(pCtx,0);` |
|        - |  6873 | `			}` |
|        4 |  6874 | `		}else{` |
|        - |  6875 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6876 | `			ph7_result_bool(pCtx,0);` |
|        - |  6877 | `		}` |
|        - |  6878 | `	}` |
|        9 |  6879 | `	return PH7_OK;` |
|        1 |  6880 |  |
|        - |  6881 | `/*` |
|        - |  6882 | ` * string get_called_class(void)` |
|        - |  6883 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6884 | ` * Parameters` |
|        - |  6885 | ` *  None.` |
|        - |  6886 | ` * Return` |
|        - |  6887 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6888 | ` */` |
|        4 |  6889 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6890 |  |
|        - |  6891 | `	ph7_class *pClass;` |
|        - |  6892 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6893 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6894 | `	if( pClass ){` |
|        - |  6895 | `		SyString *pName;` |
|        - |  6896 | `		/* Point to the class name */` |
|        5 |  6897 | `		pName = &pClass->sName;` |
|        5 |  6898 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6899 | `	}else{` |
|      ! 0 |  6900 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6901 | `		SXUNUSED(apArg);` |
|        - |  6902 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6903 | `		ph7_result_bool(pCtx,0);` |
|        - |  6904 | `	}` |
|        5 |  6905 | `	return PH7_OK;` |
|        1 |  6906 |  |
|        - |  6907 | `/*` |
|        - |  6908 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6909 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6910 | ` * string which hold the class name.` |
|        - |  6911 | ` */` |
|       78 |  6912 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6913 |  |
|       80 |  6914 | `	ph7_class *pClass = 0;` |
|       80 |  6915 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6916 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6917 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       59 |  6918 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6919 | `		const char *zClass;` |
|        - |  6920 | `		int nLen;` |
|        - |  6921 | `		/* Extract class name */` |
|       35 |  6922 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       35 |  6923 | `		if( nLen > 0 ){` |
|        - |  6924 | `			SyHashEntry *pEntry;` |
|        - |  6925 | `			/* Perform a lookup */` |
|       35 |  6926 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       35 |  6927 | `			if( pEntry ){` |
|        - |  6928 | `				/* Point to the desired class */` |
|       31 |  6929 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6930 | `			}` |
|       17 |  6931 | `		}` |
|       17 |  6932 | `	}` |
|       80 |  6933 | `	return pClass;` |
|        2 |  6934 |  |
|        - |  6935 | `/*` |
|        - |  6936 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6937 | ` *   Checks if the object or class has a property.` |
|        - |  6938 | ` * Parameters` |
|        - |  6939 | ` *  class` |
|        - |  6940 | ` *   The class name or an object of the class to test for` |
|        - |  6941 | ` * property` |
|        - |  6942 | ` *  The name of the property` |
|        - |  6943 | ` * Return` |
|        - |  6944 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6945 | ` */` |
|       12 |  6946 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6947 |  |
|       13 |  6948 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6949 | `	if( nArg > 1 ){` |
|        - |  6950 | `		ph7_class *pClass;` |
|       13 |  6951 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6952 | `		if( pClass ){` |
|        - |  6953 | `			const char *zName;` |
|        - |  6954 | `			int nLen;` |
|        - |  6955 | `			/* Extract attribute name */` |
|       13 |  6956 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6957 | `			if( nLen > 0 ){` |
|        - |  6958 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6959 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6960 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6961 | `						/* property exists,flag that */` |
|       11 |  6962 | `						res = 1;` |
|        5 |  6963 | `				}` |
|        6 |  6964 | `			}` |
|        6 |  6965 | `		}` |
|        6 |  6966 | `	}` |
|       13 |  6967 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6968 | `	return PH7_OK;` |
|        1 |  6969 |  |
|        - |  6970 | `/*` |
|        - |  6971 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6972 | ` *   Checks if the given method is a class member.` |
|        - |  6973 | ` * Parameters` |
|        - |  6974 | ` *  class` |
|        - |  6975 | ` *   The class name or an object of the class to test for` |
|        - |  6976 | ` * property` |
|        - |  6977 | ` *  The name of the method` |
|        - |  6978 | ` * Return` |
|        - |  6979 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6980 | ` */` |
|        4 |  6981 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6982 |  |
|        5 |  6983 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6984 | `	if( nArg > 1 ){` |
|        - |  6985 | `		ph7_class *pClass;` |
|        5 |  6986 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6987 | `		if( pClass ){` |
|        - |  6988 | `			const char *zName;` |
|        - |  6989 | `			int nLen;` |
|        - |  6990 | `			/* Extract method name */` |
|        5 |  6991 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6992 | `			if( nLen > 0 ){` |
|        - |  6993 | `				/* Perform the lookup in the method table */` |
|        5 |  6994 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6995 | `					/* method exists,flag that */` |
|        3 |  6996 | `					res = 1;` |
|        1 |  6997 | `				}` |
|        2 |  6998 | `			}` |
|        2 |  6999 | `		}` |
|        2 |  7000 | `	}` |
|        5 |  7001 | `	ph7_result_bool(pCtx,res);` |
|        5 |  7002 | `	return PH7_OK;` |
|        1 |  7003 |  |
|        - |  7004 | `/*` |
|        - |  7005 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7006 | ` *   Checks if the class has been defined.` |
|        - |  7007 | ` * Parameters` |
|        - |  7008 | ` *  class_name` |
|        - |  7009 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7010 | ` *   unlinke the standard PHP engine.` |
|        - |  7011 | ` *  autoload` |
|        - |  7012 | ` *   Whether or not to call __autoload by default.` |
|        - |  7013 | ` * Return` |
|        - |  7014 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7015 | ` */` |
|       12 |  7016 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7017 |  |
|       14 |  7018 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  7019 | `	if( nArg > 0 ){` |
|        - |  7020 | `		const char *zName;` |
|        - |  7021 | `		int nLen;` |
|        - |  7022 | `		/* Extract given name */` |
|       14 |  7023 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7024 | `		/* Perform a hashlookup */` |
|       14 |  7025 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7026 | `			/* class is available */` |
|       10 |  7027 | `			res = 1;` |
|        4 |  7028 | `		}` |
|        6 |  7029 | `	}` |
|       14 |  7030 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7031 | `	return PH7_OK;` |
|        2 |  7032 |  |
|        - |  7033 | `/*` |
|        - |  7034 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7035 | ` *   Checks if the interface has been defined.` |
|        - |  7036 | ` * Parameters` |
|        - |  7037 | ` *  class_name` |
|        - |  7038 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7039 | ` *   unlinke the standard PHP engine.` |
|        - |  7040 | ` *  autoload` |
|        - |  7041 | ` *   Whether or not to call __autoload by default.` |
|        - |  7042 | ` * Return` |
|        - |  7043 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7044 | ` */` |
|        6 |  7045 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7046 |  |
|        7 |  7047 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7048 | `	if( nArg > 0 ){` |
|        7 |  7049 | `		SyHashEntry *pEntry = 0;` |
|        - |  7050 | `		const char *zName;` |
|        - |  7051 | `		int nLen;` |
|        - |  7052 | `		/* Extract given name */` |
|        7 |  7053 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7054 | `		/* Perform a hashlookup */` |
|        7 |  7055 | `		if( nLen > 0 ){` |
|        7 |  7056 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7057 | `		}` |
|        7 |  7058 | `		if( pEntry ){` |
|        5 |  7059 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7060 | `			while( pClass ){` |
|        5 |  7061 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7062 | `					/* interface is available */` |
|        5 |  7063 | `					res = 1;` |
|        5 |  7064 | `					break;` |
|        - |  7065 | `				}` |
|        - |  7066 | `				/* Next with the same name */` |
|      ! 0 |  7067 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7068 | `			}` |
|        2 |  7069 | `		}` |
|        3 |  7070 | `	}` |
|        7 |  7071 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7072 | `	return PH7_OK;` |
|        1 |  7073 |  |
|        - |  7074 | `/*` |
|        - |  7075 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7076 | ` *   Creates an alias for a class.` |
|        - |  7077 | ` * Parameters` |
|        - |  7078 | ` *  original` |
|        - |  7079 | ` *    The original class.` |
|        - |  7080 | ` *  alias` |
|        - |  7081 | ` *   The alias name for the class.` |
|        - |  7082 | ` * Return` |
|        - |  7083 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7084 | ` */` |
|        2 |  7085 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7086 |  |
|        - |  7087 | `	const char *zOld,*zNew;` |
|        - |  7088 | `	int nOldLen,nNewLen;` |
|        - |  7089 | `	SyHashEntry *pEntry;` |
|        - |  7090 | `	ph7_class *pClass;` |
|        - |  7091 | `	char *zDup;` |
|        - |  7092 | `	sxi32 rc;` |
|        3 |  7093 | `	if( nArg < 2 ){` |
|        - |  7094 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7095 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7096 | `		return PH7_OK;` |
|        - |  7097 | `	}` |
|        - |  7098 | `	/* Extract old class name */` |
|        3 |  7099 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7100 | `	/* Extract alias name */` |
|        3 |  7101 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7102 | `	if( nNewLen < 1 ){` |
|        - |  7103 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7104 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7105 | `		return PH7_OK;` |
|        - |  7106 | `	}` |
|        - |  7107 | `	/* Perform a hash lookup */` |
|        3 |  7108 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7109 | `	if( pEntry ==  0 ){` |
|        - |  7110 | `		/* No such class,return FALSE */` |
|      ! 0 |  7111 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7112 | `		return PH7_OK;` |
|        - |  7113 | `	}` |
|        - |  7114 | `	/* Point to the class */` |
|        3 |  7115 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7116 | `	/* Duplicate alias name */` |
|        3 |  7117 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7118 | `	if( zDup == 0 ){` |
|        - |  7119 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7120 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7121 | `		return PH7_OK;` |
|        - |  7122 | `	}` |
|        - |  7123 | `	/* Create the alias */` |
|        3 |  7124 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7125 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7126 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7127 | `	}` |
|        3 |  7128 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7129 | `	return PH7_OK;` |
|        2 |  7130 |  |
|        - |  7131 | `/*` |
|        - |  7132 | ` * array get_declared_classes(void)` |
|        - |  7133 | ` *   Returns an array with the name of the defined classes` |
|        - |  7134 | ` * Parameters` |
|        - |  7135 | ` *  None` |
|        - |  7136 | ` * Return` |
|        - |  7137 | ` *   Returns an array of the names of the declared classes` |
|        - |  7138 | ` *   in the current script.` |
|        - |  7139 | ` * Note:` |
|        - |  7140 | ` *   NULL is returned on failure.` |
|        - |  7141 | ` */` |
|        2 |  7142 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7143 |  |
|        - |  7144 | `	ph7_value *pName,*pArray;` |
|        - |  7145 | `	SyHashEntry *pEntry;` |
|        - |  7146 | `	/* Create a new array first */` |
|        3 |  7147 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7148 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7149 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7150 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7151 | `		SXUNUSED(apArg);` |
|        - |  7152 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7153 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7154 | `		return PH7_OK;` |
|        - |  7155 | `	}` |
|        - |  7156 | `	/* Fill the array with the defined classes */` |
|        3 |  7157 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       50 |  7158 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       47 |  7159 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7160 | `		/* Do not register classes defined as interfaces */` |
|       47 |  7161 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       41 |  7162 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7163 | `			/* insert class name */` |
|       41 |  7164 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7165 | `			/* Reset the cursor */` |
|       41 |  7166 | `			ph7_value_reset_string_cursor(pName);` |
|       20 |  7167 | `		}` |
|        1 |  7168 | `	}` |
|        - |  7169 | `	/* Return the created array */` |
|        3 |  7170 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7171 | `	return PH7_OK;` |
|        2 |  7172 |  |
|        - |  7173 | `/*` |
|        - |  7174 | ` * array get_declared_interfaces(void)` |
|        - |  7175 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7176 | ` * Parameters` |
|        - |  7177 | ` *  None` |
|        - |  7178 | ` * Return` |
|        - |  7179 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7180 | ` *   in the current script.` |
|        - |  7181 | ` * Note:` |
|        - |  7182 | ` *   NULL is returned on failure.` |
|        - |  7183 | ` */` |
|        2 |  7184 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7185 |  |
|        - |  7186 | `	ph7_value *pName,*pArray;` |
|        - |  7187 | `	SyHashEntry *pEntry;` |
|        - |  7188 | `	/* Create a new array first */` |
|        3 |  7189 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7190 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7191 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7192 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7193 | `		SXUNUSED(apArg);` |
|        - |  7194 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7195 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7196 | `		return PH7_OK;` |
|        - |  7197 | `	}` |
|        - |  7198 | `	/* Fill the array with the defined classes */` |
|        3 |  7199 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7200 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7201 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7202 | `		/* Register classes defined as interfaces only */` |
|       49 |  7203 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7204 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7205 | `			/* insert interface name */` |
|        9 |  7206 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7207 | `			/* Reset the cursor */` |
|        9 |  7208 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7209 | `		}` |
|        1 |  7210 | `	}` |
|        - |  7211 | `	/* Return the created array */` |
|        3 |  7212 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7213 | `	return PH7_OK;` |
|        2 |  7214 |  |
|        - |  7215 | `/*` |
|        - |  7216 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7217 | ` *   Returns an array with the name of the class methods` |
|        - |  7218 | ` * Parameters` |
|        - |  7219 | ` *  class_name` |
|        - |  7220 | ` *  The class name or class instance` |
|        - |  7221 | ` * Return` |
|        - |  7222 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7223 | ` *  In case of an error, it returns NULL.` |
|        - |  7224 | ` * Note:` |
|        - |  7225 | ` *   NULL is returned on failure.` |
|        - |  7226 | ` */` |
|        6 |  7227 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7228 |  |
|        - |  7229 | `	ph7_value *pName,*pArray;` |
|        - |  7230 | `	SyHashEntry *pEntry;` |
|        - |  7231 | `	ph7_class *pClass;` |
|        - |  7232 | `	/* Extract the target class first */` |
|        7 |  7233 | `	pClass = 0;` |
|        7 |  7234 | `	if( nArg > 0 ){` |
|        7 |  7235 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7236 | `	}` |
|        7 |  7237 | `	if( pClass == 0 ){` |
|        - |  7238 | `		/* No such class,return NULL */` |
|        3 |  7239 | `		ph7_result_null(pCtx);` |
|        3 |  7240 | `		return PH7_OK;` |
|        - |  7241 | `	}` |
|        - |  7242 | `	/* Create a new array  */` |
|        5 |  7243 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7244 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7245 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7246 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7247 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7248 | `		return PH7_OK;` |
|        - |  7249 | `	}` |
|        - |  7250 | `	/* Fill the array with the defined methods */` |
|        5 |  7251 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7252 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7253 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7254 | `		/* Insert method name */` |
|       13 |  7255 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7256 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7257 | `		/* Reset the cursor */` |
|       13 |  7258 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7259 | `	}` |
|        - |  7260 | `	/* Return the created array */` |
|        5 |  7261 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7262 | `	/*` |
|        - |  7263 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7264 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7265 | `	 */` |
|        5 |  7266 | `	return PH7_OK;` |
|        4 |  7267 |  |
|        - |  7268 | `/*` |
|        - |  7269 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7270 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7271 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7272 | ` */` |
|      824 |  7273 | `static int VmClassMemberAccess(` |
|        - |  7274 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7275 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7276 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7277 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7278 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7279 | `	)` |
|        2 |  7280 |  |
|      826 |  7281 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      262 |  7282 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7283 | `		ph7_vm_func *pVmFunc;` |
|      266 |  7284 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7285 | `			/* Safely ignore the exception frame */` |
|        5 |  7286 | `			pFrame = pFrame->pParent;` |
|        1 |  7287 | `		}` |
|      262 |  7288 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      262 |  7289 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7290 | `			goto dis; /* Access is forbidden */` |
|        - |  7291 | `		}` |
|      254 |  7292 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7293 | `			/* Must be the same instance */` |
|        7 |  7294 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7295 | `				goto dis; /* Access is forbidden */` |
|        - |  7296 | `			}` |
|        4 |  7297 | `		}else{` |
|        - |  7298 | `			/* Protected */` |
|      248 |  7299 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7300 | `			/* Must be a derived class */` |
|      248 |  7301 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7302 | `				goto dis; /* Access is forbidden */` |
|        - |  7303 | `			}` |
|        - |  7304 | `		}` |
|      126 |  7305 | `	}` |
|      818 |  7306 | `	return 1; /* Access is granted */` |
|        4 |  7307 | `dis:` |
|        9 |  7308 | `	if( bLog ){` |
|      ! 0 |  7309 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7310 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7311 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7312 | `	}` |
|        9 |  7313 | `	return 0; /* Access is forbidden */` |
|      414 |  7314 |  |
|        - |  7315 | `/*` |
|        - |  7316 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7317 | ` *   Get the default properties of the class` |
|        - |  7318 | ` * Parameters` |
|        - |  7319 | ` *  class_name` |
|        - |  7320 | ` *   The class name or class instance` |
|        - |  7321 | ` * Return` |
|        - |  7322 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7323 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7324 | ` *  of varname => value.` |
|        - |  7325 | ` * Note:` |
|        - |  7326 | ` *   NULL is returned on failure.` |
|        - |  7327 | ` */` |
|        2 |  7328 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7329 |  |
|        - |  7330 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7331 | `	SyHashEntry *pEntry;` |
|        - |  7332 | `	ph7_class *pClass;` |
|        - |  7333 | `	/* Extract the target class first */` |
|        3 |  7334 | `	pClass = 0;` |
|        3 |  7335 | `	if( nArg > 0 ){` |
|        3 |  7336 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7337 | `	}` |
|        3 |  7338 | `	if( pClass == 0 ){` |
|        - |  7339 | `		/* No such class,return NULL */` |
|      ! 0 |  7340 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7341 | `		return PH7_OK;` |
|        - |  7342 | `	}` |
|        - |  7343 | `	/* Create a new array  */` |
|        3 |  7344 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7345 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7346 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7347 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7348 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7349 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7350 | `		return PH7_OK;` |
|        - |  7351 | `	}` |
|        - |  7352 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7353 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7354 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7355 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7356 | `		/* Check if the access is allowed */` |
|        5 |  7357 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7358 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7359 | `			ph7_value *pValue = 0;` |
|        5 |  7360 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7361 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7362 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7363 | `			}else{` |
|      ! 0 |  7364 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7365 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7366 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7367 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7368 | `					pValue = &sValue;` |
|      ! 0 |  7369 | `				}` |
|        - |  7370 | `			}` |
|        - |  7371 | `			/* Fill in the array */` |
|        5 |  7372 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7373 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7374 | `			/* Reset the cursor */` |
|        5 |  7375 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7376 | `		}` |
|        1 |  7377 | `	}` |
|        3 |  7378 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7379 | `	/* Return the created array */` |
|        3 |  7380 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7381 | `	/*` |
|        - |  7382 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7383 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7384 | `	 */` |
|        3 |  7385 | `	return PH7_OK;` |
|        2 |  7386 |  |
|        - |  7387 | `/*` |
|        - |  7388 | ` * array get_object_vars(object $this)` |
|        - |  7389 | ` *   Gets the properties of the given object` |
|        - |  7390 | ` * Parameters` |
|        - |  7391 | ` *  this` |
|        - |  7392 | ` *   A class instance` |
|        - |  7393 | ` * Return` |
|        - |  7394 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7395 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7396 | ` *  it will be returned with a NULL value.` |
|        - |  7397 | ` * Note:` |
|        - |  7398 | ` *   NULL is returned on failure.` |
|        - |  7399 | ` */` |
|        2 |  7400 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7401 |  |
|        3 |  7402 | `	ph7_class_instance *pThis = 0;` |
|        - |  7403 | `	ph7_value *pName,*pArray;` |
|        - |  7404 | `	SyHashEntry *pEntry;` |
|        3 |  7405 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7406 | `		/* Extract the target instance */` |
|        3 |  7407 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7408 | `	}` |
|        3 |  7409 | `	if( pThis == 0 ){` |
|        - |  7410 | `		/* No such instance,return NULL */` |
|      ! 0 |  7411 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7412 | `		return PH7_OK;` |
|        - |  7413 | `	}` |
|        - |  7414 | `	/* Create a new array  */` |
|        3 |  7415 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7416 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7417 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7418 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7419 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7420 | `		return PH7_OK;` |
|        - |  7421 | `	}` |
|        - |  7422 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7423 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7424 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7425 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7426 | `		SyString *pAttrName;` |
|        7 |  7427 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7428 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7429 | `			continue;` |
|        - |  7430 | `		}` |
|        7 |  7431 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7432 | `		/* Check if the access is allowed */` |
|        7 |  7433 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7434 | `			ph7_value *pValue = 0;` |
|        - |  7435 | `			/* Extract attribute */` |
|        3 |  7436 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7437 | `			if( pValue ){` |
|        - |  7438 | `				/* Insert attribute name in the array */` |
|        3 |  7439 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7440 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7441 | `			}` |
|        - |  7442 | `			/* Reset the cursor */` |
|        3 |  7443 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7444 | `		}` |
|        1 |  7445 | `	}` |
|        - |  7446 | `	/* Return the created array */` |
|        3 |  7447 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7448 | `	/*` |
|        - |  7449 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7450 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7451 | `	 */` |
|        3 |  7452 | `	return PH7_OK;` |
|        2 |  7453 |  |
|        - |  7454 | `/*` |
|        - |  7455 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7456 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7457 | ` */` |
|      362 |  7458 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7459 |  |
|        - |  7460 | `	ph7_class **apInterface;` |
|        - |  7461 | `	sxu32 n;` |
|      364 |  7462 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7463 | `		/* Empty interface container */` |
|      362 |  7464 | `		return FALSE;` |
|        - |  7465 | `	}` |
|        - |  7466 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7467 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7468 | `	/* Perform the lookup */` |
|        3 |  7469 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7470 | `		if( apInterface[n] == pClass ){` |
|        3 |  7471 | `			return TRUE;` |
|        - |  7472 | `		}` |
|      ! 0 |  7473 | `	}` |
|      ! 0 |  7474 | `	return FALSE;` |
|      183 |  7475 |  |
|        - |  7476 | `/*` |
|        - |  7477 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7478 | ` * is an instance of the main class (second argument).` |
|        - |  7479 | ` * Otherwise FALSE is returned.` |
|        - |  7480 | ` */` |
|      298 |  7481 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7482 |  |
|        - |  7483 | `	ph7_class *pParent;` |
|        - |  7484 | `	sxi32 rc;` |
|      300 |  7485 | `	if( pThis == pClass ){` |
|        - |  7486 | `		/* Instance of the same class */` |
|      140 |  7487 | `		return TRUE;` |
|        - |  7488 | `	}` |
|        - |  7489 | `	/* Check implemented interfaces */` |
|      162 |  7490 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|      162 |  7491 | `	if( rc ){` |
|        3 |  7492 | `		return TRUE;` |
|        - |  7493 | `	}` |
|        - |  7494 | `	/* Check parent classes */` |
|      160 |  7495 | `	pParent = pThis->pBase;` |
|      362 |  7496 | `	while( pParent ){` |
|      360 |  7497 | `		if( pParent == pClass ){` |
|        - |  7498 | `			/* Same instance */` |
|      158 |  7499 | `			return TRUE;` |
|        - |  7500 | `		}` |
|        - |  7501 | `		/* Check the implemented interfaces */` |
|      204 |  7502 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|      204 |  7503 | `		if( rc ){` |
|      ! 0 |  7504 | `			return TRUE;` |
|        - |  7505 | `		}` |
|        - |  7506 | `		/* Point to the parent class */` |
|      204 |  7507 | `		pParent = pParent->pBase;` |
|        2 |  7508 | `	}` |
|        - |  7509 | `	/* Not an instance of the the given class */` |
|        3 |  7510 | `	return FALSE;` |
|      151 |  7511 |  |
|        - |  7512 | `/*` |
|        - |  7513 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7514 | ` * is a subclass of the main class (second argument).` |
|        - |  7515 | ` * Otherwise FALSE is returned.` |
|        - |  7516 | ` */` |
|        4 |  7517 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7518 |  |
|        5 |  7519 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7520 | `	SyHashEntry *pEntry;` |
|        - |  7521 | `	SyString *pName;` |
|        - |  7522 | `	sxi32 rc;` |
|        5 |  7523 | `	while( pClass ){` |
|        5 |  7524 | `		pName = &pClass->sName;` |
|        - |  7525 | `		/* Query the derived hashtable */` |
|        5 |  7526 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7527 | `		if( pEntry ){` |
|        5 |  7528 | `			return TRUE;` |
|        - |  7529 | `		}` |
|      ! 0 |  7530 | `		pClass = pClass->pBase;` |
|      ! 0 |  7531 | `	}` |
|      ! 0 |  7532 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7533 | `	if( rc ){` |
|      ! 0 |  7534 | `		return TRUE;` |
|        - |  7535 | `	}` |
|        - |  7536 | `	/* Not a subclass */` |
|      ! 0 |  7537 | `	return FALSE;` |
|        3 |  7538 |  |
|        - |  7539 | `/*` |
|        - |  7540 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7541 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7542 | ` * Parameters` |
|        - |  7543 | ` *  object` |
|        - |  7544 | ` *   The tested object` |
|        - |  7545 | ` * class_name` |
|        - |  7546 | ` *  The class name` |
|        - |  7547 | ` * Return` |
|        - |  7548 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7549 | ` *   parents, FALSE otherwise.` |
|        - |  7550 | ` */` |
|        2 |  7551 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7552 |  |
|        3 |  7553 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7554 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7555 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7556 | `		ph7_class *pClass;` |
|        - |  7557 | `		/* Extract the given class */` |
|        3 |  7558 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7559 | `		if( pClass ){` |
|        - |  7560 | `			/* Perform the query */` |
|        3 |  7561 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7562 | `		}` |
|        1 |  7563 | `	}` |
|        - |  7564 | `	/* Query result */` |
|        3 |  7565 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7566 | `	return PH7_OK;` |
|        1 |  7567 |  |
|        - |  7568 | `/*` |
|        - |  7569 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7570 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7571 | ` * Parameters` |
|        - |  7572 | ` *  object` |
|        - |  7573 | ` *   The tested object` |
|        - |  7574 | ` * class_name` |
|        - |  7575 | ` *  The class name` |
|        - |  7576 | ` * Return` |
|        - |  7577 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7578 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7579 | ` */` |
|        6 |  7580 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7581 |  |
|        7 |  7582 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7583 | `	if( nArg > 1 ){` |
|        - |  7584 | `		ph7_class *pClass,*pMain;` |
|        - |  7585 | `		/* Extract the given classes */` |
|        7 |  7586 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7587 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7588 | `		if( pClass && pMain ){` |
|        - |  7589 | `			/* Perform the query */` |
|        5 |  7590 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7591 | `		}` |
|        3 |  7592 | `	}` |
|        - |  7593 | `	/* Query result */` |
|        7 |  7594 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7595 | `	return PH7_OK;` |
|        1 |  7596 |  |
|        - |  7597 | `/*` |
|        - |  7598 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7599 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7600 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7601 | ` * return value indicates failure.` |
|        - |  7602 | ` */` |
|      246 |  7603 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7604 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7605 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7606 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7607 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7608 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7609 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7610 | `	)` |
|        2 |  7611 |  |
|        - |  7612 | `	ph7_value *aStack;` |
|        - |  7613 | `	VmInstr aInstr[2];` |
|        - |  7614 | `	int iCursor;` |
|        - |  7615 | `	int i;` |
|        - |  7616 | `	/* Create a new operand stack */` |
|      248 |  7617 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      248 |  7618 | `	if( aStack == 0 ){` |
|      ! 0 |  7619 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7620 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7621 | `		return SXERR_MEM;` |
|        - |  7622 | `	}` |
|        - |  7623 | `	/* Fill the operand stack with the given arguments */` |
|      342 |  7624 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       96 |  7625 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7626 | `		/*` |
|        - |  7627 | `		 * Symisc eXtension:` |
|        - |  7628 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7629 | `		 */` |
|       96 |  7630 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|       49 |  7631 | `	}` |
|      248 |  7632 | `	iCursor = nArg + 1;` |
|      248 |  7633 | `	if( pThis ){` |
|        - |  7634 | `		/*` |
|        - |  7635 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7636 | `		 */` |
|      242 |  7637 | `		pThis->iRef++; /* Increment reference count */` |
|      242 |  7638 | `		aStack[i].x.pOther = pThis;` |
|      242 |  7639 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      120 |  7640 | `	}` |
|      248 |  7641 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      248 |  7642 | `	i++;` |
|        - |  7643 | `	/* Push method name */` |
|      248 |  7644 | `	SyBlobReset(&aStack[i].sBlob);` |
|      248 |  7645 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      248 |  7646 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      248 |  7647 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7648 | `	/* Emit the CALL istruction */` |
|      248 |  7649 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      248 |  7650 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      248 |  7651 | `	aInstr[0].iP2 = 0;` |
|      248 |  7652 | `	aInstr[0].p3  = 0;` |
|        - |  7653 | `	/* Emit the DONE instruction */` |
|      248 |  7654 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      248 |  7655 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      248 |  7656 | `	aInstr[1].iP2 = 0;` |
|      248 |  7657 | `	aInstr[1].p3  = 0;` |
|        - |  7658 | `	/* Execute the method body (if available) */` |
|      248 |  7659 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7660 | `	/* Clean up the mess left behind */` |
|      248 |  7661 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      248 |  7662 | `	return PH7_OK;` |
|      125 |  7663 |  |
|        - |  7664 | `/*` |
|        - |  7665 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7666 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7667 | ` * in the apArg[] array.` |
|        - |  7668 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7669 | ` * return value indicates failure.` |
|        - |  7670 | ` */` |
|      330 |  7671 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7672 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7673 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7674 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7675 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7676 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7677 | `	)` |
|        2 |  7678 |  |
|        - |  7679 | `	ph7_value *aStack;` |
|        - |  7680 | `	VmInstr aInstr[2];` |
|        - |  7681 | `	int i;` |
|      332 |  7682 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7683 | `		/* Don't bother processing,it's invalid anyway */` |
|       23 |  7684 | `		if( pResult ){` |
|        - |  7685 | `			/* Assume a null return value */` |
|      ! 0 |  7686 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7687 | `		}` |
|       23 |  7688 | `		return SXERR_INVALID;` |
|        - |  7689 | `	}` |
|      310 |  7690 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7691 | `		/* Class method */` |
|       11 |  7692 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7693 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7694 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7695 | `		ph7_class *pClass = 0;` |
|        - |  7696 | `		ph7_value *pValue;` |
|        - |  7697 | `		sxi32 rc;` |
|       11 |  7698 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7699 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7700 | `			if( pResult ){` |
|        - |  7701 | `				/* Assume a null return value */` |
|      ! 0 |  7702 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7703 | `			}` |
|      ! 0 |  7704 | `			return SXRET_OK;` |
|        - |  7705 | `		}` |
|        - |  7706 | `		/* Extract the class name or an instance of it */` |
|       11 |  7707 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7708 | `		if( pValue ){` |
|       11 |  7709 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7710 | `		}` |
|       11 |  7711 | `		if( pClass == 0 ){` |
|        - |  7712 | `			/* No such class,return NULL */` |
|      ! 0 |  7713 | `			if( pResult ){` |
|      ! 0 |  7714 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7715 | `			}` |
|      ! 0 |  7716 | `			return SXRET_OK;` |
|        - |  7717 | `		}` |
|       11 |  7718 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7719 | `			/* Point to the class instance */` |
|        5 |  7720 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7721 | `		}` |
|        - |  7722 | `		/* Try to extract the method */` |
|       11 |  7723 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7724 | `		if( pValue ){` |
|       11 |  7725 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7726 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7727 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7728 | `			}` |
|        5 |  7729 | `		}` |
|       11 |  7730 | `		if( pMethod == 0 ){` |
|        - |  7731 | `			/* No such method,return NULL */` |
|      ! 0 |  7732 | `			if( pResult ){` |
|      ! 0 |  7733 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7734 | `			}` |
|      ! 0 |  7735 | `			return SXRET_OK;` |
|        - |  7736 | `		}` |
|        - |  7737 | `		/* Call the class method */` |
|       11 |  7738 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7739 | `		return rc;` |
|        - |  7740 | `	}` |
|        - |  7741 | `	/* Create a new operand stack */` |
|      300 |  7742 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      300 |  7743 | `	if( aStack == 0 ){` |
|      ! 0 |  7744 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7745 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7746 | `		if( pResult ){` |
|        - |  7747 | `			/* Assume a null return value */` |
|      ! 0 |  7748 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7749 | `		}` |
|      ! 0 |  7750 | `		return SXERR_MEM;` |
|        - |  7751 | `	}` |
|        - |  7752 | `	/* Fill the operand stack with the given arguments */` |
|      928 |  7753 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      629 |  7754 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7755 | `		/*` |
|        - |  7756 | `		 * Symisc eXtension:` |
|        - |  7757 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7758 | `		 */` |
|      629 |  7759 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      315 |  7760 | `	}` |
|        - |  7761 | `	/* Push the function name */` |
|      300 |  7762 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      300 |  7763 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7764 | `	/* Emit the CALL istruction */` |
|      300 |  7765 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      300 |  7766 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      300 |  7767 | `	aInstr[0].iP2 = 0;` |
|      300 |  7768 | `	aInstr[0].p3  = 0;` |
|        - |  7769 | `	/* Emit the DONE instruction */` |
|      300 |  7770 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      300 |  7771 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      300 |  7772 | `	aInstr[1].iP2 = 0;` |
|      300 |  7773 | `	aInstr[1].p3  = 0;` |
|        - |  7774 | `	/* Execute the function body (if available) */` |
|      300 |  7775 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7776 | `	/* Clean up the mess left behind */` |
|      300 |  7777 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      300 |  7778 | `	return PH7_OK;` |
|      167 |  7779 |  |
|        - |  7780 | `/*` |
|        - |  7781 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7782 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7783 | ` * parameter.` |
|        - |  7784 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7785 | ` * return value indicates failure.` |
|        - |  7786 | ` */` |
|      190 |  7787 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7788 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7789 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7790 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7791 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7792 | `	)` |
|        1 |  7793 |  |
|        - |  7794 | `	ph7_value *pArg;` |
|        - |  7795 | `	SySet aArg;` |
|        - |  7796 | `	va_list ap;` |
|        - |  7797 | `	sxi32 rc;` |
|      191 |  7798 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7799 | `	/* Copy arguments one after one */` |
|      191 |  7800 | `	va_start(ap,pResult);` |
|      319 |  7801 | `	for(;;){` |
|      639 |  7802 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7803 | `		if( pArg == 0 ){` |
|      191 |  7804 | `			break;` |
|        - |  7805 | `		}` |
|      449 |  7806 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7807 | `	}` |
|        - |  7808 | `	/* Call the core routine */` |
|      191 |  7809 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7810 | `	/* Cleanup */` |
|      191 |  7811 | `	SySetRelease(&aArg);` |
|      191 |  7812 | `	return rc;` |
|        1 |  7813 |  |
|        - |  7814 | `/*` |
|        - |  7815 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7816 | ` *  Call the callback given by the first parameter.` |
|        - |  7817 | ` * Parameter` |
|        - |  7818 | ` *  $callback` |
|        - |  7819 | ` *   The callable to be called.` |
|        - |  7820 | ` *  ...` |
|        - |  7821 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7822 | ` * Return` |
|        - |  7823 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7824 | ` */` |
|       14 |  7825 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7826 |  |
|        - |  7827 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7828 | `	sxi32 rc;` |
|       15 |  7829 | `	if( nArg < 1 ){` |
|        - |  7830 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7831 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7832 | `		return PH7_OK;` |
|        - |  7833 | `	}` |
|       15 |  7834 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7835 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7836 | `	/* Try to invoke the callback */` |
|       15 |  7837 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7838 | `	if( rc != SXRET_OK ){` |
|        - |  7839 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7840 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7841 | `	}else{` |
|        - |  7842 | `		/* Callback result */` |
|       15 |  7843 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7844 | `	}` |
|       15 |  7845 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7846 | `	return PH7_OK;` |
|        8 |  7847 |  |
|        - |  7848 | `/*` |
|        - |  7849 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7850 | ` *  Call a callback with an array of parameters.` |
|        - |  7851 | ` * Parameter` |
|        - |  7852 | ` *  $callback` |
|        - |  7853 | ` *   The callable to be called.` |
|        - |  7854 | ` * $param_arr` |
|        - |  7855 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7856 | ` * Return` |
|        - |  7857 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7858 | ` */` |
|       10 |  7859 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7860 |  |
|        - |  7861 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7862 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7863 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7864 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7865 | `	sxi32 rc;` |
|        - |  7866 | `	sxu32 n;` |
|       11 |  7867 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7868 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7869 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7870 | `		return PH7_OK;` |
|        - |  7871 | `	}` |
|       11 |  7872 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7873 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7874 | `	/* Initialize the arguments container */` |
|       11 |  7875 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7876 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7877 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7878 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7879 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7880 | `		/* Extract node value */` |
|       13 |  7881 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7882 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7883 | `		}` |
|        - |  7884 | `		/* Point to the next entry */` |
|       13 |  7885 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7886 | `	}` |
|        - |  7887 | `	/* Try to invoke the callback */` |
|       11 |  7888 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7889 | `	if( rc != SXRET_OK ){` |
|        - |  7890 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7891 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7892 | `	}else{` |
|        - |  7893 | `		/* Callback result */` |
|       11 |  7894 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7895 | `	}` |
|        - |  7896 | `	/* Cleanup the mess left behind */` |
|       11 |  7897 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7898 | `	SySetRelease(&aArg);` |
|       11 |  7899 | `	return PH7_OK;` |
|        6 |  7900 |  |
|        - |  7901 | `/*` |
|        - |  7902 | ` * bool defined(string $name)` |
|        - |  7903 | ` *  Checks whether a given named constant exists.` |
|        - |  7904 | ` * Parameter:` |
|        - |  7905 | ` *  Name of the desired constant.` |
|        - |  7906 | ` * Return` |
|        - |  7907 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7908 | ` */` |
|       14 |  7909 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7910 |  |
|        - |  7911 | `	const char *zName;` |
|       16 |  7912 | `	int nLen = 0;` |
|       16 |  7913 | `	int res = 0;` |
|       16 |  7914 | `	if( nArg < 1 ){` |
|        - |  7915 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7916 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7917 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7918 | `		return SXRET_OK;` |
|        - |  7919 | `	}` |
|        - |  7920 | `	/* Extract constant name */` |
|       16 |  7921 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7922 | `	/* Perform the lookup */` |
|       16 |  7923 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7924 | `		/* Already defined */` |
|       10 |  7925 | `		res = 1;` |
|        4 |  7926 | `	}` |
|       16 |  7927 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7928 | `	return SXRET_OK;` |
|        9 |  7929 |  |
|        - |  7930 | `/*` |
|        - |  7931 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7932 | ` * below.` |
|        - |  7933 | ` */` |
|        8 |  7934 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7935 |  |
|       10 |  7936 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7937 | `	/* Expand constant value */` |
|       10 |  7938 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7939 |  |
|        - |  7940 | `/*` |
|        - |  7941 | ` * bool define(string $constant_name,expression value)` |
|        - |  7942 | ` *  Defines a named constant at runtime.` |
|        - |  7943 | ` * Parameter:` |
|        - |  7944 | ` *  $constant_name` |
|        - |  7945 | ` *   The name of the constant` |
|        - |  7946 | ` *  $value` |
|        - |  7947 | ` *   Constant value` |
|        - |  7948 | ` * Return:` |
|        - |  7949 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7950 | ` */` |
|       10 |  7951 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7952 |  |
|        - |  7953 | `	const char *zName;  /* Constant name */` |
|        - |  7954 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7955 | `	int nLen = 0;       /* Name length */` |
|        - |  7956 | `	sxi32 rc;` |
|       12 |  7957 | `	if( nArg < 2 ){` |
|        - |  7958 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7959 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7960 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7961 | `		return SXRET_OK;` |
|        - |  7962 | `	}` |
|       12 |  7963 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7964 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7965 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7966 | `		return SXRET_OK;` |
|        - |  7967 | `	}` |
|        - |  7968 | `	/* Extract constant name */` |
|       12 |  7969 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7970 | `	if( nLen < 1 ){` |
|      ! 0 |  7971 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7972 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7973 | `		return SXRET_OK;` |
|        - |  7974 | `	}` |
|        - |  7975 | `	/* Duplicate constant value */` |
|       12 |  7976 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7977 | `	if( pValue == 0 ){` |
|      ! 0 |  7978 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7979 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7980 | `		return SXRET_OK;` |
|        - |  7981 | `	}` |
|        - |  7982 | `	/* Initialize the memory object */` |
|       12 |  7983 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7984 | `	/* Register the constant */` |
|       12 |  7985 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7986 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7987 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7988 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7989 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7990 | `		return SXRET_OK;` |
|        - |  7991 | `	}` |
|        - |  7992 | `	/* Duplicate constant value */` |
|       12 |  7993 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7994 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7995 | `		/* Lower case the constant name */` |
|      ! 0 |  7996 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7997 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7998 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7999 | `				/* UTF-8 stream */` |
|      ! 0 |  8000 | `				zCur++;` |
|      ! 0 |  8001 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8002 | `					zCur++;` |
|      ! 0 |  8003 | `				}` |
|      ! 0 |  8004 | `				continue;` |
|        - |  8005 | `			}` |
|      ! 0 |  8006 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8007 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8008 | `				zCur[0] = (char)c;` |
|      ! 0 |  8009 | `			}` |
|      ! 0 |  8010 | `			zCur++;` |
|      ! 0 |  8011 | `		}` |
|        - |  8012 | `		/* Finally,register the constant */` |
|      ! 0 |  8013 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8014 | `	}` |
|        - |  8015 | `	/* All done,return TRUE */` |
|       12 |  8016 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8017 | `	return SXRET_OK;` |
|        7 |  8018 |  |
|        - |  8019 | `/*` |
|        - |  8020 | ` * value constant(string $name)` |
|        - |  8021 | ` *  Returns the value of a constant` |
|        - |  8022 | ` * Parameter` |
|        - |  8023 | ` *  $name` |
|        - |  8024 | ` *    Name of the constant.` |
|        - |  8025 | ` * Return` |
|        - |  8026 | ` *  Constant value or NULL if not defined.` |
|        - |  8027 | ` */` |
|        8 |  8028 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8029 |  |
|        - |  8030 | `	SyHashEntry *pEntry;` |
|        - |  8031 | `	ph7_constant *pCons;` |
|        - |  8032 | `	const char *zName; /* Constant name */` |
|        - |  8033 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8034 | `	int nLen;` |
|       10 |  8035 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8036 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8037 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8038 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8039 | `		return SXRET_OK;` |
|        - |  8040 | `	}` |
|        - |  8041 | `	/* Extract the constant name */` |
|       10 |  8042 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8043 | `	/* Perform the query */` |
|       10 |  8044 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8045 | `	if( pEntry == 0 ){` |
|        3 |  8046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8047 | `		ph7_result_null(pCtx);` |
|        3 |  8048 | `		return SXRET_OK;` |
|        - |  8049 | `	}` |
|        8 |  8050 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8051 | `	/* Point to the structure that describe the constant */` |
|        8 |  8052 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8053 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8054 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8055 | `	/* Return that value */` |
|        8 |  8056 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8057 | `	/* Cleanup */` |
|        8 |  8058 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8059 | `	return SXRET_OK;` |
|        6 |  8060 |  |
|        - |  8061 | `/*` |
|        - |  8062 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8063 | ` * defined below.` |
|        - |  8064 | ` */` |
|      414 |  8065 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8066 |  |
|      415 |  8067 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8068 | `	ph7_value sName;` |
|        - |  8069 | `	sxi32 rc;` |
|        - |  8070 | `	/* Prepare the constant name for insertion */` |
|      415 |  8071 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8072 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8073 | `	/* Perform the insertion */` |
|      415 |  8074 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8075 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8076 | `	return rc;` |
|        1 |  8077 |  |
|        - |  8078 | `/*` |
|        - |  8079 | ` * array get_defined_constants(void)` |
|        - |  8080 | ` *  Returns an associative array with the names of all defined` |
|        - |  8081 | ` *  constants.` |
|        - |  8082 | ` * Parameters` |
|        - |  8083 | ` *  NONE.` |
|        - |  8084 | ` * Returns` |
|        - |  8085 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8086 | ` */` |
|        2 |  8087 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8088 |  |
|        - |  8089 | `	ph7_value *pArray;` |
|        - |  8090 | `	/* Create the array first*/` |
|        3 |  8091 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8092 | `	if( pArray == 0 ){` |
|      ! 0 |  8093 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8094 | `		SXUNUSED(apArg);` |
|        - |  8095 | `		/* Return NULL */` |
|      ! 0 |  8096 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8097 | `		return SXRET_OK;` |
|        - |  8098 | `	}` |
|        - |  8099 | `	/* Fill the array with the defined constants */` |
|        3 |  8100 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8101 | `	/* Return the created array */` |
|        3 |  8102 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8103 | `	return SXRET_OK;` |
|        2 |  8104 |  |
|        - |  8105 | `/*` |
|        - |  8106 | ` * Section:` |
|        - |  8107 | ` *  Output Control (OB) functions.` |
|        - |  8108 | ` * Status:` |
|        - |  8109 | ` *    Stable.` |
|        - |  8110 | ` */` |
|        - |  8111 | `/* Forward declaration */` |
|        - |  8112 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8113 | `/*` |
|        - |  8114 | ` * void ob_clean(void)` |
|        - |  8115 | ` *  This function discards the contents of the output buffer.` |
|        - |  8116 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8117 | ` * Parameter` |
|        - |  8118 | ` *  None` |
|        - |  8119 | ` * Return` |
|        - |  8120 | ` *  No value is returned.` |
|        - |  8121 | ` */` |
|        2 |  8122 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8123 |  |
|        3 |  8124 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8125 | `	VmObEntry *pOb;` |
|        1 |  8126 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8127 | `	SXUNUSED(apArg);` |
|        - |  8128 | `	/* Peek the top most OB */` |
|        3 |  8129 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8130 | `	if( pOb ){` |
|        3 |  8131 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8132 | `	}` |
|        3 |  8133 | `	return PH7_OK;` |
|        1 |  8134 |  |
|        - |  8135 | `/*` |
|        - |  8136 | ` * bool ob_end_clean(void)` |
|        - |  8137 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8138 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8139 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8140 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8141 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8142 | ` * Parameter` |
|        - |  8143 | ` *  None` |
|        - |  8144 | ` * Return` |
|        - |  8145 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8146 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8147 | ` * (possible for special buffer)` |
|        - |  8148 | ` */` |
|     2634 |  8149 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8150 |  |
|     2636 |  8151 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8152 | `	VmObEntry *pOb;` |
|        - |  8153 | `	/* Pop the top most OB */` |
|     2636 |  8154 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2636 |  8155 | `	if( pOb == 0){` |
|        - |  8156 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8157 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8158 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8159 | `		SXUNUSED(apArg);` |
|      ! 0 |  8160 | `	}else{` |
|        - |  8161 | `		/* Release */` |
|     2636 |  8162 | `		VmObRestore(pVm,pOb);` |
|        - |  8163 | `		/* Return true */` |
|     2636 |  8164 | `		ph7_result_bool(pCtx,1);` |
|        - |  8165 | `	}` |
|     2636 |  8166 | `	return PH7_OK;` |
|        2 |  8167 |  |
|        - |  8168 | `/*` |
|        - |  8169 | ` * string ob_get_contents(void)` |
|        - |  8170 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8171 | ` * Parameter` |
|        - |  8172 | ` *  None` |
|        - |  8173 | ` * Return` |
|        - |  8174 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8175 | ` */` |
|        6 |  8176 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8177 |  |
|        7 |  8178 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8179 | `	VmObEntry *pOb;` |
|        - |  8180 | `	/* Peek the top most OB */` |
|        7 |  8181 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8182 | `	if( pOb == 0 ){` |
|        - |  8183 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8184 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8185 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8186 | `		SXUNUSED(apArg);` |
|      ! 0 |  8187 | `	}else{` |
|        - |  8188 | `		/* Return contents */` |
|        7 |  8189 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8190 | `	}` |
|        7 |  8191 | `	return PH7_OK;` |
|        1 |  8192 |  |
|        - |  8193 | `/*` |
|        - |  8194 | ` * string ob_get_clean(void)` |
|        - |  8195 | ` * string ob_get_flush(void)` |
|        - |  8196 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8197 | ` * Parameter` |
|        - |  8198 | ` *  None` |
|        - |  8199 | ` * Return` |
|        - |  8200 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8201 | ` */` |
|     3882 |  8202 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8203 |  |
|     3884 |  8204 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8205 | `	VmObEntry *pOb;` |
|        - |  8206 | `	/* Pop the top most OB */` |
|     3884 |  8207 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3884 |  8208 | `	if( pOb == 0 ){` |
|        - |  8209 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8210 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8211 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8212 | `		SXUNUSED(apArg);` |
|      ! 0 |  8213 | `	}else{` |
|        - |  8214 | `		/* Return contents */` |
|     3884 |  8215 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8216 | `		/* Release */` |
|     3884 |  8217 | `		VmObRestore(pVm,pOb);` |
|        - |  8218 | `	}` |
|     3884 |  8219 | `	return PH7_OK;` |
|        2 |  8220 |  |
|        - |  8221 | `/*` |
|        - |  8222 | ` * int ob_get_length(void)` |
|        - |  8223 | ` *  Return the length of the output buffer.` |
|        - |  8224 | ` * Parameter` |
|        - |  8225 | ` *  None` |
|        - |  8226 | ` * Return` |
|        - |  8227 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8228 | ` */` |
|        2 |  8229 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8230 |  |
|        3 |  8231 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8232 | `	VmObEntry *pOb;` |
|        - |  8233 | `	/* Peek the top most OB */` |
|        3 |  8234 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8235 | `	if( pOb == 0 ){` |
|        - |  8236 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8237 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8238 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8239 | `		SXUNUSED(apArg);` |
|      ! 0 |  8240 | `	}else{` |
|        - |  8241 | `		/* Return OB length */` |
|        3 |  8242 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8243 | `	}` |
|        3 |  8244 | `	return PH7_OK;` |
|        1 |  8245 |  |
|        - |  8246 | `/*` |
|        - |  8247 | ` * int ob_get_level(void)` |
|        - |  8248 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8249 | ` * Parameter` |
|        - |  8250 | ` *  None` |
|        - |  8251 | ` * Return` |
|        - |  8252 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8253 | ` */` |
|        6 |  8254 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8255 |  |
|        7 |  8256 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8257 | `	int iNest;` |
|        3 |  8258 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8259 | `	SXUNUSED(apArg);` |
|        - |  8260 | `	/* Nesting level */` |
|        7 |  8261 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8262 | `	/* Return the nesting value */` |
|        7 |  8263 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8264 | `	return PH7_OK;` |
|        1 |  8265 |  |
|        - |  8266 | `/*` |
|        - |  8267 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8268 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8269 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8270 | ` */` |
|     5874 |  8271 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8272 |  |
|     5876 |  8273 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8274 | `	VmObEntry *pEntry;` |
|        - |  8275 | `	ph7_value sResult;` |
|        - |  8276 | `	/* Peek the top most entry */` |
|     5876 |  8277 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     5876 |  8278 | `	if( pEntry == 0 ){` |
|        - |  8279 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8280 | `		return PH7_OK;` |
|        - |  8281 | `	}` |
|     5876 |  8282 | `	PH7_MemObjInit(pVm,&sResult);` |
|     5876 |  8283 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8284 | `		ph7_value sArg,*apArg[2];` |
|        - |  8285 | `		/* Fill the first argument */` |
|      ! 0 |  8286 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8287 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8288 | `		apArg[0] = &sArg;` |
|        - |  8289 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8290 | `		pVm->nObDepth++;` |
|      ! 0 |  8291 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8292 | `		pVm->nObDepth--;` |
|      ! 0 |  8293 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8294 | `			/* Extract the function result */` |
|      ! 0 |  8295 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8296 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8297 | `		}` |
|      ! 0 |  8298 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8299 | `	}` |
|     5876 |  8300 | `	if( nDataLen > 0 ){` |
|        - |  8301 | `		/* Redirect the VM output to the internal buffer */` |
|     5876 |  8302 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     2937 |  8303 | `	}` |
|        - |  8304 | `	/* Release */` |
|     5876 |  8305 | `	PH7_MemObjRelease(&sResult);` |
|     5876 |  8306 | `	return PH7_OK;` |
|     2939 |  8307 |  |
|        - |  8308 | `/*` |
|        - |  8309 | ` * Restore the default consumer.` |
|        - |  8310 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8311 | ` * information.` |
|        - |  8312 | ` */` |
|     6518 |  8313 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8314 |  |
|     6520 |  8315 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6520 |  8316 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8317 | `		/* No more stackable OB */` |
|     6502 |  8318 | `		pCons->xConsumer = pCons->xDef;` |
|     6502 |  8319 | `		pCons->pUserData = pCons->pDefData;` |
|     3250 |  8320 | `	}` |
|        - |  8321 | `	/* Release OB data */` |
|     6520 |  8322 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6520 |  8323 | `	SyBlobRelease(&pEntry->sOB);` |
|     6520 |  8324 |  |
|        - |  8325 | `/*` |
|        - |  8326 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8327 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8328 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8329 | ` *  buffer.` |
|        - |  8330 | ` * Parameter` |
|        - |  8331 | ` *  $output_callback` |
|        - |  8332 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8333 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8334 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8335 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8336 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8337 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8338 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8339 | ` *   will return FALSE.` |
|        - |  8340 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8341 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8342 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8343 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8344 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8345 | ` * Return` |
|        - |  8346 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8347 | ` */` |
|     6518 |  8348 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8349 |  |
|     6520 |  8350 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8351 | `	VmObEntry sOb;` |
|        - |  8352 | `	sxi32 rc;` |
|        - |  8353 | `	/* Initialize the OB entry */` |
|     6520 |  8354 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6520 |  8355 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6520 |  8356 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8357 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8358 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8359 | `	}` |
|        - |  8360 | `	/* Push in the stack */` |
|     6520 |  8361 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6520 |  8362 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8363 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8364 | `	}else{` |
|     6520 |  8365 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8366 | `		/* Substitute the default VM consumer */` |
|     6520 |  8367 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6502 |  8368 | `			pCons->xDef = pCons->xConsumer;` |
|     6502 |  8369 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8370 | `			/* Install the new consumer */` |
|     6502 |  8371 | `			pCons->xConsumer = VmObConsumer;` |
|     6502 |  8372 | `			pCons->pUserData = pVm;` |
|     3250 |  8373 | `		}` |
|        - |  8374 | `	}` |
|     6520 |  8375 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6520 |  8376 | `	return PH7_OK;` |
|        2 |  8377 |  |
|        - |  8378 | `/*` |
|        - |  8379 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8380 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8381 | ` * information.` |
|        - |  8382 | ` */` |
|        4 |  8383 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8384 |  |
|        5 |  8385 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8386 | `	sxi32 rc;` |
|        - |  8387 | `	/* Flush contents */` |
|        5 |  8388 | `	rc = PH7_OK;` |
|        5 |  8389 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8390 | `		/* Call the VM output consumer */` |
|        5 |  8391 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8392 | `		/* Increment VM output counter */` |
|        5 |  8393 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8394 | `		if( rc != PH7_ABORT ){` |
|        5 |  8395 | `			rc = PH7_OK;` |
|        2 |  8396 | `		}` |
|        2 |  8397 | `	}` |
|        5 |  8398 | `	if( bRelease ){` |
|        3 |  8399 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8400 | `	}else{` |
|        - |  8401 | `		/* Reset the blob */` |
|        3 |  8402 | `		SyBlobReset(pBlob);` |
|        - |  8403 | `	}` |
|        5 |  8404 | `	return rc;` |
|        1 |  8405 |  |
|        - |  8406 | `/*` |
|        - |  8407 | ` * void ob_flush(void)` |
|        - |  8408 | ` * void flush(void)` |
|        - |  8409 | ` *  Flush (send) the output buffer.` |
|        - |  8410 | ` * Parameter` |
|        - |  8411 | ` *  None` |
|        - |  8412 | ` * Return` |
|        - |  8413 | ` *  No return value.` |
|        - |  8414 | ` */` |
|        2 |  8415 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8416 |  |
|        3 |  8417 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8418 | `	VmObEntry *pOb;` |
|        - |  8419 | `	sxi32 rc;` |
|        - |  8420 | `	/* Peek the top most OB entry */` |
|        3 |  8421 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8422 | `	if( pOb == 0 ){` |
|        - |  8423 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8424 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8425 | `		SXUNUSED(apArg);` |
|      ! 0 |  8426 | `		return PH7_OK;` |
|        - |  8427 | `	}` |
|        - |  8428 | `	/* Flush contents */` |
|        3 |  8429 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8430 | `	return rc;` |
|        2 |  8431 |  |
|        - |  8432 | `/*` |
|        - |  8433 | ` * bool ob_end_flush(void)` |
|        - |  8434 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8435 | ` * Parameter` |
|        - |  8436 | ` *  None` |
|        - |  8437 | ` * Return` |
|        - |  8438 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8439 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8440 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8441 | ` */` |
|        2 |  8442 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8443 |  |
|        3 |  8444 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8445 | `	VmObEntry *pOb;` |
|        - |  8446 | `	sxi32 rc;` |
|        - |  8447 | `	/* Pop the top most OB entry */` |
|        3 |  8448 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8449 | `	if( pOb == 0 ){` |
|        - |  8450 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8451 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8452 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8453 | `		SXUNUSED(apArg);` |
|      ! 0 |  8454 | `		return PH7_OK;` |
|        - |  8455 | `	}` |
|        - |  8456 | `	/* Flush contents */` |
|        3 |  8457 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8458 | `	/* Return true */` |
|        3 |  8459 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8460 | `	return rc;` |
|        2 |  8461 |  |
|        - |  8462 | `/*` |
|        - |  8463 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8464 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8465 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8466 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8467 | ` * Parameter` |
|        - |  8468 | ` *  $flag` |
|        - |  8469 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8470 | ` * Return` |
|        - |  8471 | ` *   Nothing` |
|        - |  8472 | ` */` |
|        4 |  8473 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8474 |  |
|        - |  8475 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8476 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8477 | `	 */` |
|        2 |  8478 | `	SXUNUSED(pCtx);` |
|        2 |  8479 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8480 | `	SXUNUSED(apArg);` |
|        5 |  8481 | `	return PH7_OK;` |
|        1 |  8482 |  |
|        - |  8483 | `/*` |
|        - |  8484 | ` * array ob_list_handlers(void)` |
|        - |  8485 | ` *  Lists all output handlers in use.` |
|        - |  8486 | ` * Parameter` |
|        - |  8487 | ` *  None` |
|        - |  8488 | ` * Return` |
|        - |  8489 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8490 | ` */` |
|        2 |  8491 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8492 |  |
|        3 |  8493 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8494 | `	ph7_value *pArray;` |
|        - |  8495 | `	VmObEntry *aEntry;` |
|        - |  8496 | `	ph7_value sVal;` |
|        - |  8497 | `	sxu32 n;` |
|        3 |  8498 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8499 | `		/* Empty stack,return null */` |
|      ! 0 |  8500 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8501 | `		return PH7_OK;` |
|        - |  8502 | `	}` |
|        - |  8503 | `	/* Create a new array */` |
|        3 |  8504 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8505 | `	if( pArray == 0 ){` |
|        - |  8506 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8507 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8508 | `		SXUNUSED(apArg);` |
|      ! 0 |  8509 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8510 | `		return PH7_OK;` |
|        - |  8511 | `	}` |
|        3 |  8512 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8513 | `	/* Point to the installed OB entries */` |
|        3 |  8514 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8515 | `	/* Perform the requested operation */` |
|        5 |  8516 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8517 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8518 | `		/* Extract handler name */` |
|        3 |  8519 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8520 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8521 | `			/* Callback,dup it's name */` |
|      ! 0 |  8522 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8523 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8524 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8525 | `		}else{` |
|        3 |  8526 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8527 | `		}` |
|        3 |  8528 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8529 | `		/* Perform the insertion */` |
|        3 |  8530 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8531 | `	}` |
|        3 |  8532 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8533 | `	/* Return the freshly created array */` |
|        3 |  8534 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8535 | `	return PH7_OK;` |
|        2 |  8536 |  |
|        - |  8537 | `/*` |
|        - |  8538 | ` * Section:` |
|        - |  8539 | ` *  Random numbers/string generators.` |
|        - |  8540 | ` * Status:` |
|        - |  8541 | ` *    Stable.` |
|        - |  8542 | ` */` |
|        - |  8543 | `/*` |
|        - |  8544 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8545 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8546 | ` * used by te SQLite3 library.` |
|        - |  8547 | ` */` |
|     1048 |  8548 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8549 |  |
|        - |  8550 | `	sxu32 iNum;` |
|     1050 |  8551 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1050 |  8552 | `	return iNum;` |
|        2 |  8553 |  |
|        - |  8554 | `/*` |
|        - |  8555 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8556 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8557 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8558 | ` * by te SQLite3 library.` |
|        - |  8559 | ` */` |
|    36138 |  8560 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8561 |  |
|        - |  8562 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8563 | `	int i;` |
|        - |  8564 | `	/* Generate a binary string first */` |
|    36140 |  8565 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8566 | `	/* Turn the binary string into english based alphabet */` |
|   397692 |  8567 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   361554 |  8568 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   180778 |  8569 | `	 }` |
|    36140 |  8570 |  |
|        - |  8571 | `/*` |
|        - |  8572 | ` * int rand()` |
|        - |  8573 | ` * int mt_rand()` |
|        - |  8574 | ` * int rand(int $min,int $max)` |
|        - |  8575 | ` * int mt_rand(int $min,int $max)` |
|        - |  8576 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8577 | ` * Parameter` |
|        - |  8578 | ` *  $min` |
|        - |  8579 | ` *    The lowest value to return (default: 0)` |
|        - |  8580 | ` *  $max` |
|        - |  8581 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8582 | ` * Return` |
|        - |  8583 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8584 | ` * Note:` |
|        - |  8585 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8586 | ` *  by te SQLite3 library.` |
|        - |  8587 | ` */` |
|       20 |  8588 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8589 |  |
|        - |  8590 | `	sxu32 iNum;` |
|        - |  8591 | `	/* Generate the random number */` |
|       21 |  8592 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8593 | `	if( nArg > 1 ){` |
|        - |  8594 | `		sxu32 iMin,iMax;` |
|        3 |  8595 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8596 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8597 | `		if( iMin < iMax ){` |
|        3 |  8598 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8599 | `			if( iDiv > 0 ){` |
|        3 |  8600 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8601 | `			}` |
|        1 |  8602 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8603 | `			iNum %= iMax;` |
|      ! 0 |  8604 | `		}` |
|        1 |  8605 | `	}` |
|        - |  8606 | `	/* Return the number */` |
|       21 |  8607 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8608 | `	return SXRET_OK;` |
|        1 |  8609 |  |
|        - |  8610 | `/*` |
|        - |  8611 | ` * int getrandmax(void)` |
|        - |  8612 | ` * int mt_getrandmax(void)` |
|        - |  8613 | ` * int rc4_getrandmax(void)` |
|        - |  8614 | ` *   Show largest possible random value` |
|        - |  8615 | ` * Return` |
|        - |  8616 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8617 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8618 | ` * Note:` |
|        - |  8619 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8620 | ` *  by te SQLite3 library.` |
|        - |  8621 | ` */` |
|        4 |  8622 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8623 |  |
|        2 |  8624 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8625 | `	SXUNUSED(apArg);` |
|        5 |  8626 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8627 | `	return SXRET_OK;` |
|        1 |  8628 |  |
|        - |  8629 | `/*` |
|        - |  8630 | ` * string rand_str()` |
|        - |  8631 | ` * string rand_str(int $len)` |
|        - |  8632 | ` *  Generate a random string (English alphabet).` |
|        - |  8633 | ` * Parameter` |
|        - |  8634 | ` *  $len` |
|        - |  8635 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8636 | ` * Return` |
|        - |  8637 | ` *   A pseudo random string.` |
|        - |  8638 | ` * Note:` |
|        - |  8639 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8640 | ` *  by te SQLite3 library.` |
|        - |  8641 | ` *  This function is a symisc extension.` |
|        - |  8642 | ` */` |
|      122 |  8643 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8644 |  |
|        - |  8645 | `	char zString[1024];` |
|      124 |  8646 | `	int iLen = 0x10;` |
|      124 |  8647 | `	if( nArg > 0 ){` |
|        - |  8648 | `		/* Get the desired length */` |
|      124 |  8649 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      124 |  8650 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8651 | `			/* Default length */` |
|        3 |  8652 | `			iLen = 0x10;` |
|        1 |  8653 | `		}` |
|       61 |  8654 | `	}` |
|        - |  8655 | `	/* Generate the random string */` |
|      124 |  8656 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8657 | `	/* Return the generated string */` |
|      124 |  8658 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      124 |  8659 | `	return SXRET_OK;` |
|        2 |  8660 |  |
|        - |  8661 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8662 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8663 | `/* Unique ID private data */` |
|        - |  8664 | `struct unique_id_data` |
|        - |  8665 |  |
|        - |  8666 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8667 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8668 | `};` |
|        - |  8669 | `/*` |
|        - |  8670 | ` * Binary to hex consumer callback.` |
|        - |  8671 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8672 | ` * defined below.` |
|        - |  8673 | ` */` |
|      192 |  8674 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8675 |  |
|      193 |  8676 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8677 | `	sxu32 nBuflen;` |
|        - |  8678 | `	/* Extract result buffer length */` |
|      193 |  8679 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8680 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8681 | `			/*` |
|        - |  8682 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8683 | `			 * string will be 13 characters long` |
|        - |  8684 | `			 */` |
|       25 |  8685 | `		return SXERR_ABORT;` |
|        - |  8686 | `	}` |
|      169 |  8687 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8688 | `		return SXERR_ABORT;` |
|        - |  8689 | `	}` |
|        - |  8690 | `	/* Safely Consume the hex stream */` |
|      169 |  8691 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8692 | `	return SXRET_OK;` |
|       97 |  8693 |  |
|        - |  8694 | `/*` |
|        - |  8695 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8696 | ` *  Generate a unique ID` |
|        - |  8697 | ` * Parameter` |
|        - |  8698 | ` * $prefix` |
|        - |  8699 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8700 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8701 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8702 | ` * $more_entropy` |
|        - |  8703 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8704 | ` *  that the result will be unique.` |
|        - |  8705 | ` * Return` |
|        - |  8706 | ` *  Returns the unique identifier, as a string.` |
|        - |  8707 | ` */` |
|       24 |  8708 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8709 |  |
|        - |  8710 | `	struct unique_id_data sUniq;` |
|        - |  8711 | `	unsigned char zDigest[20];` |
|       25 |  8712 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8713 | `	const char *zPrefix;` |
|        - |  8714 | `	SHA1Context sCtx;` |
|        - |  8715 | `	char zRandom[7];` |
|        - |  8716 | `	int nPrefix;` |
|        - |  8717 | `	int entropy;` |
|        - |  8718 | `	/* Generate a random string first */` |
|       25 |  8719 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8720 | `	/* Initialize fields */` |
|       25 |  8721 | `	zPrefix = 0;` |
|       25 |  8722 | `	nPrefix = 0;` |
|       25 |  8723 | `	entropy = 0;` |
|       25 |  8724 | `	if( nArg > 0 ){` |
|        - |  8725 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8726 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8727 | `		if( nArg > 1 ){` |
|      ! 0 |  8728 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8729 | `		}` |
|      ! 0 |  8730 | `	}` |
|       25 |  8731 | `	SHA1Init(&sCtx);` |
|        - |  8732 | `	/* Generate the random ID */` |
|       25 |  8733 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8734 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8735 | `	}` |
|        - |  8736 | `	/* Append the random ID */` |
|       25 |  8737 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8738 | `	/* Append the random string */` |
|       25 |  8739 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8740 | `	/* Increment the number */` |
|       25 |  8741 | `	pVm->unique_id++;` |
|       25 |  8742 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8743 | `	/* Hexify the digest */` |
|       25 |  8744 | `	sUniq.pCtx = pCtx;` |
|       25 |  8745 | `	sUniq.entropy = entropy;` |
|       25 |  8746 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8747 | `	/* All done */` |
|       25 |  8748 | `	return PH7_OK;` |
|        1 |  8749 |  |
|        - |  8750 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8751 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8752 | `/*` |
|        - |  8753 | ` * Section:` |
|        - |  8754 | ` *  Language construct implementation as foreign functions.` |
|        - |  8755 | ` * Status:` |
|        - |  8756 | ` *    Stable.` |
|        - |  8757 | ` */` |
|        - |  8758 | `/*` |
|        - |  8759 | ` * void echo($string...)` |
|        - |  8760 | ` *  Output one or more messages.` |
|        - |  8761 | ` * Parameters` |
|        - |  8762 | ` *  $string` |
|        - |  8763 | ` *   Message to output.` |
|        - |  8764 | ` * Return` |
|        - |  8765 | ` *  NULL.` |
|        - |  8766 | ` */` |
|      ! 0 |  8767 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8768 |  |
|        - |  8769 | `	const char *zData;` |
|      ! 0 |  8770 | `	int nDataLen = 0;` |
|        - |  8771 | `	ph7_vm *pVm;` |
|        - |  8772 | `	int i,rc;` |
|        - |  8773 | `	/* Point to the target VM */` |
|      ! 0 |  8774 | `	pVm = pCtx->pVm;` |
|        - |  8775 | `	/* Output */` |
|      ! 0 |  8776 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8777 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8778 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8779 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8780 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8781 | `				/* Increment output length */` |
|      ! 0 |  8782 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8783 | `			}` |
|      ! 0 |  8784 | `			if( rc == SXERR_ABORT ){` |
|        - |  8785 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8786 | `				return PH7_ABORT;` |
|        - |  8787 | `			}` |
|      ! 0 |  8788 | `		}` |
|      ! 0 |  8789 | `	}` |
|      ! 0 |  8790 | `	return SXRET_OK;` |
|      ! 0 |  8791 |  |
|        - |  8792 | `/*` |
|        - |  8793 | ` * int print($string...)` |
|        - |  8794 | ` *  Output one or more messages.` |
|        - |  8795 | ` * Parameters` |
|        - |  8796 | ` *  $string` |
|        - |  8797 | ` *   Message to output.` |
|        - |  8798 | ` * Return` |
|        - |  8799 | ` *  1 always.` |
|        - |  8800 | ` */` |
|        2 |  8801 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8802 |  |
|        - |  8803 | `	const char *zData;` |
|        3 |  8804 | `	int nDataLen = 0;` |
|        - |  8805 | `	ph7_vm *pVm;` |
|        - |  8806 | `	int i,rc;` |
|        - |  8807 | `	/* Point to the target VM */` |
|        3 |  8808 | `	pVm = pCtx->pVm;` |
|        - |  8809 | `	/* Output */` |
|        5 |  8810 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8811 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8812 | `		if( nDataLen > 0 ){` |
|        3 |  8813 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8814 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8815 | `				/* Increment output length */` |
|        3 |  8816 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8817 | `			}` |
|        3 |  8818 | `			if( rc == SXERR_ABORT ){` |
|        - |  8819 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8820 | `				return PH7_ABORT;` |
|        - |  8821 | `			}` |
|        1 |  8822 | `		}` |
|        2 |  8823 | `	}` |
|        - |  8824 | `	/* Return 1 */` |
|        3 |  8825 | `	ph7_result_int(pCtx,1);` |
|        3 |  8826 | `	return SXRET_OK;` |
|        2 |  8827 |  |
|        - |  8828 | `/*` |
|        - |  8829 | ` * void exit(string $msg)` |
|        - |  8830 | ` * void exit(int $status)` |
|        - |  8831 | ` * void die(string $ms)` |
|        - |  8832 | ` * void die(int $status)` |
|        - |  8833 | ` *   Output a message and terminate program execution.` |
|        - |  8834 | ` * Parameter` |
|        - |  8835 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8836 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8837 | ` *  and not printed` |
|        - |  8838 | ` * Return` |
|        - |  8839 | ` *  NULL` |
|        - |  8840 | ` */` |
|      ! 0 |  8841 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8842 |  |
|      ! 0 |  8843 | `	if( nArg > 0 ){` |
|      ! 0 |  8844 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8845 | `			const char *zData;` |
|      ! 0 |  8846 | `			int iLen = 0;` |
|        - |  8847 | `			/* Print exit message */` |
|      ! 0 |  8848 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8849 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8850 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8851 | `			sxi32 iExitStatus;` |
|        - |  8852 | `			/* Record exit status code */` |
|      ! 0 |  8853 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8854 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8855 | `		}` |
|      ! 0 |  8856 | `	}` |
|        - |  8857 | `	/* Check if we are in an included file */` |
|      ! 0 |  8858 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8859 | `		/* Exit the entire process */` |
|      ! 0 |  8860 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8861 | `	}` |
|        - |  8862 | `	/* Abort processing immediately */` |
|      ! 0 |  8863 | `	return PH7_ABORT;` |
|      ! 0 |  8864 |  |
|        - |  8865 | `/*` |
|        - |  8866 | ` * bool isset($var,...)` |
|        - |  8867 | ` *  Finds out whether a variable is set.` |
|        - |  8868 | ` * Parameters` |
|        - |  8869 | ` *  One or more variable to check.` |
|        - |  8870 | ` * Return` |
|        - |  8871 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8872 | ` */` |
|    51072 |  8873 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8874 |  |
|        - |  8875 | `	ph7_value *pObj;` |
|    51074 |  8876 | `	int res = 0;` |
|        - |  8877 | `	int i;` |
|    51074 |  8878 | `	if( nArg < 1 ){` |
|        - |  8879 | `		/* Missing arguments,return false */` |
|      ! 0 |  8880 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8881 | `		return SXRET_OK;` |
|        - |  8882 | `	}` |
|        - |  8883 | `	/* Iterate over available arguments */` |
|    68746 |  8884 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    51074 |  8885 | `		pObj = apArg[i];` |
|    51074 |  8886 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    33358 |  8887 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8888 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8889 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8890 | `			}` |
|    16678 |  8891 | `		}` |
|    51074 |  8892 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    51074 |  8893 | `		if( !res ){` |
|        - |  8894 | `			/* Variable not set,return FALSE */` |
|    33402 |  8895 | `			ph7_result_bool(pCtx,0);` |
|    33402 |  8896 | `			return SXRET_OK;` |
|        - |  8897 | `		}` |
|     8838 |  8898 | `	}` |
|        - |  8899 | `	/* All given variable are set,return TRUE */` |
|    17674 |  8900 | `	ph7_result_bool(pCtx,1);` |
|    17674 |  8901 | `	return SXRET_OK;` |
|    25538 |  8902 |  |
|        - |  8903 | `/*` |
|        - |  8904 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8905 | ` * frame,the reference table and discard it's contents.` |
|        - |  8906 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8907 | ` */` |
|   598334 |  8908 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8909 |  |
|        - |  8910 | `	ph7_value *pObj;` |
|        - |  8911 | `	VmRefObj *pRef;` |
|   598336 |  8912 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|   598336 |  8913 | `	if( pObj ){` |
|        - |  8914 | `		/* Release the object */` |
|   598336 |  8915 | `		PH7_MemObjRelease(pObj);` |
|   299167 |  8916 | `	}` |
|        - |  8917 | `	/* Remove old reference links */` |
|   598336 |  8918 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|   598336 |  8919 | `	if( pRef ){` |
|   598316 |  8920 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8921 | `		/* Unlink from the reference table */` |
|   598316 |  8922 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|   598316 |  8923 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8924 | `			VmSlot sFree;` |
|        - |  8925 | `			/* Restore to the free list */` |
|   598310 |  8926 | `			sFree.nIdx = nObjIdx;` |
|   598310 |  8927 | `			sFree.pUserData = 0;` |
|   598310 |  8928 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|   299154 |  8929 | `		}` |
|   299157 |  8930 | `	}` |
|   598336 |  8931 | `	return SXRET_OK;` |
|        2 |  8932 |  |
|        - |  8933 | `/*` |
|        - |  8934 | ` * void unset($var,...)` |
|        - |  8935 | ` *   Unset one or more given variable.` |
|        - |  8936 | ` * Parameters` |
|        - |  8937 | ` *  One or more variable to unset.` |
|        - |  8938 | ` * Return` |
|        - |  8939 | ` *  Nothing.` |
|        - |  8940 | ` */` |
|     2636 |  8941 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8942 |  |
|        - |  8943 | `	ph7_value *pObj;` |
|        - |  8944 | `	ph7_vm *pVm;` |
|        - |  8945 | `	int i;` |
|        - |  8946 | `	/* Point to the target VM */` |
|     2638 |  8947 | `	pVm = pCtx->pVm;` |
|        - |  8948 | `	/* Iterate and unset */` |
|     8118 |  8949 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5482 |  8950 | `		pObj = apArg[i];` |
|     5482 |  8951 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      700 |  8952 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8953 | `				/* Throw an error */` |
|      ! 0 |  8954 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8955 | `			}` |
|      351 |  8956 | `		}else{` |
|     4783 |  8957 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8958 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     4783 |  8959 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     4777 |  8960 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2388 |  8961 | `			}` |
|        - |  8962 | `		}` |
|     2742 |  8963 | `	}` |
|     2638 |  8964 | `	return SXRET_OK;` |
|        2 |  8965 |  |
|        - |  8966 | `/*` |
|        - |  8967 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8968 | ` */` |
|      108 |  8969 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8970 |  |
|      109 |  8971 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8972 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8973 | `	ph7_value *pObj;` |
|        - |  8974 | `	sxu32 nIdx;` |
|        - |  8975 | `	/* Extract the memory object */` |
|      109 |  8976 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8977 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8978 | `	if( pObj ){` |
|      109 |  8979 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8980 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8981 | `				SyString sName;` |
|        - |  8982 | `				ph7_value sKey;` |
|        - |  8983 | `				/* Perform the insertion */` |
|      107 |  8984 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  8985 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  8986 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  8987 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  8988 | `			}` |
|       53 |  8989 | `		}` |
|       54 |  8990 | `	}` |
|      109 |  8991 | `	return SXRET_OK;` |
|        1 |  8992 |  |
|        - |  8993 | `/*` |
|        - |  8994 | ` * array get_defined_vars(void)` |
|        - |  8995 | ` *  Returns an array of all defined variables.` |
|        - |  8996 | ` * Parameter` |
|        - |  8997 | ` *  None` |
|        - |  8998 | ` * Return` |
|        - |  8999 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9000 | ` */` |
|        2 |  9001 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9002 |  |
|        3 |  9003 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9004 | `	ph7_value *pArray;` |
|        - |  9005 | `	/* Create a new array */` |
|        3 |  9006 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9007 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9008 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9009 | `		SXUNUSED(apArg);` |
|        - |  9010 | `		/* Return NULL */` |
|      ! 0 |  9011 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9012 | `		return SXRET_OK;` |
|        - |  9013 | `	}` |
|        - |  9014 | `	/* Superglobals first */` |
|        3 |  9015 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9016 | `	/* Then variable defined in the current frame */` |
|        3 |  9017 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9018 | `	/* Finally,return the created array */` |
|        3 |  9019 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9020 | `	return SXRET_OK;` |
|        2 |  9021 |  |
|        - |  9022 | `/*` |
|        - |  9023 | ` * bool gettype($var)` |
|        - |  9024 | ` *  Get the type of a variable` |
|        - |  9025 | ` * Parameters` |
|        - |  9026 | ` *   $var` |
|        - |  9027 | ` *    The variable being type checked.` |
|        - |  9028 | ` * Return` |
|        - |  9029 | ` *   String representation of the given variable type.` |
|        - |  9030 | ` */` |
|       30 |  9031 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9032 |  |
|       31 |  9033 | `	const char *zType = "Empty";` |
|       31 |  9034 | `	if( nArg > 0 ){` |
|       31 |  9035 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  9036 | `	}` |
|        - |  9037 | `	/* Return the variable type */` |
|       31 |  9038 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       31 |  9039 | `	return SXRET_OK;` |
|        1 |  9040 |  |
|        - |  9041 | `/*` |
|        - |  9042 | ` * string get_resource_type(resource $handle)` |
|        - |  9043 | ` *  This function gets the type of the given resource.` |
|        - |  9044 | ` * Parameters` |
|        - |  9045 | ` *  $handle` |
|        - |  9046 | ` *  The evaluated resource handle.` |
|        - |  9047 | ` * Return` |
|        - |  9048 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9049 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9050 | ` *  the return value will be the string Unknown.` |
|        - |  9051 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9052 | ` *  is not a resource.` |
|        - |  9053 | ` */` |
|        2 |  9054 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9055 |  |
|        3 |  9056 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9057 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9058 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9059 | `		return PH7_OK;` |
|        - |  9060 | `	}` |
|        3 |  9061 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9062 | `	return SXRET_OK;` |
|        2 |  9063 |  |
|        - |  9064 | `/*` |
|        - |  9065 | ` * void var_dump(expression,....)` |
|        - |  9066 | ` *   var_dump � Dumps information about a variable` |
|        - |  9067 | ` * Parameters` |
|        - |  9068 | ` *   One or more expression to dump.` |
|        - |  9069 | ` * Returns` |
|        - |  9070 | ` *  Nothing.` |
|        - |  9071 | ` */` |
|      246 |  9072 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9073 |  |
|        - |  9074 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9075 | `	int i;` |
|      248 |  9076 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9077 | `	/* Dump one or more expressions */` |
|      500 |  9078 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      254 |  9079 | `		ph7_value *pObj = apArg[i];` |
|        - |  9080 | `		/* Reset the working buffer */` |
|      254 |  9081 | `		SyBlobReset(&sDump);` |
|        - |  9082 | `		/* Dump the given expression */` |
|      254 |  9083 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9084 | `		/* Output */` |
|      254 |  9085 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      254 |  9086 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      126 |  9087 | `		}` |
|      128 |  9088 | `	}` |
|        - |  9089 | `	/* Release the working buffer */` |
|      248 |  9090 | `	SyBlobRelease(&sDump);` |
|      248 |  9091 | `	return SXRET_OK;` |
|        2 |  9092 |  |
|        - |  9093 | `/*` |
|        - |  9094 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9095 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9096 | ` * Parameters` |
|        - |  9097 | ` *   expression: Expression to dump` |
|        - |  9098 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9099 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9100 | ` *            print_r() will return the information rather than print it.` |
|        - |  9101 | ` * Return` |
|        - |  9102 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9103 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9104 | ` */` |
|       16 |  9105 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9106 |  |
|       17 |  9107 | `	int ret_string = 0;` |
|        - |  9108 | `	SyBlob sDump;` |
|       17 |  9109 | `	if( nArg < 1 ){` |
|        - |  9110 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9111 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9112 | `		return SXRET_OK;` |
|        - |  9113 | `	}` |
|       17 |  9114 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9115 | `	if ( nArg > 1 ){` |
|        - |  9116 | `		/* Where to redirect output */` |
|       11 |  9117 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9118 | `	}` |
|        - |  9119 | `	/* Generate dump */` |
|       17 |  9120 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9121 | `	if( !ret_string ){` |
|        - |  9122 | `		/* Output dump */` |
|        7 |  9123 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9124 | `		/* Return true */` |
|        7 |  9125 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9126 | `	}else{` |
|        - |  9127 | `		/* Generated dump as return value */` |
|       11 |  9128 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9129 | `	}` |
|        - |  9130 | `	/* Release the working buffer */` |
|       17 |  9131 | `	SyBlobRelease(&sDump);` |
|       17 |  9132 | `	return SXRET_OK;` |
|        9 |  9133 |  |
|        - |  9134 | `/*` |
|        - |  9135 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9136 | ` * Same job as print_r. (see coment above)` |
|        - |  9137 | ` */` |
|        2 |  9138 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9139 |  |
|        3 |  9140 | `	int ret_string = 0;` |
|        - |  9141 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9142 | `	if( nArg < 1 ){` |
|        - |  9143 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9145 | `		return SXRET_OK;` |
|        - |  9146 | `	}` |
|        3 |  9147 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9148 | `	if ( nArg > 1 ){` |
|        - |  9149 | `		/* Where to redirect output */` |
|        3 |  9150 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9151 | `	}` |
|        - |  9152 | `	/* Generate dump */` |
|        3 |  9153 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9154 | `	if( !ret_string ){` |
|        - |  9155 | `		/* Output dump */` |
|      ! 0 |  9156 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9157 | `		/* Return NULL */` |
|      ! 0 |  9158 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9159 | `	}else{` |
|        - |  9160 | `		/* Generated dump as return value */` |
|        3 |  9161 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9162 | `	}` |
|        - |  9163 | `	/* Release the working buffer */` |
|        3 |  9164 | `	SyBlobRelease(&sDump);` |
|        3 |  9165 | `	return SXRET_OK;` |
|        2 |  9166 |  |
|        - |  9167 | `/*` |
|        - |  9168 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9169 | ` *  Set/get the various assert flags.` |
|        - |  9170 | ` * Parameter` |
|        - |  9171 | ` * $what` |
|        - |  9172 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9173 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9174 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9175 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9176 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9177 | ` * $value` |
|        - |  9178 | ` *   An optional new value for the option.` |
|        - |  9179 | ` * Return` |
|        - |  9180 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9181 | ` */` |
|        8 |  9182 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9183 |  |
|        9 |  9184 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9185 | `	int iOld,iNew,iValue;` |
|        9 |  9186 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9187 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9188 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9189 | `		return PH7_OK;` |
|        - |  9190 | `	}` |
|        - |  9191 | `	/* Save old assertion flags */` |
|        9 |  9192 | `	iOld = pVm->iAssertFlags;` |
|        - |  9193 | `	/* Extract the new flags */` |
|        9 |  9194 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9195 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9196 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9197 | `		if( nArg > 1 ){` |
|        5 |  9198 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9199 | `			if( iValue ){` |
|        - |  9200 | `				/* Disable assertion */` |
|        3 |  9201 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9202 | `			}` |
|        3 |  9203 | `		}` |
|        6 |  9204 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9205 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9206 | `		if( nArg > 1 ){` |
|      ! 0 |  9207 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9208 | `			if( iValue ){` |
|        - |  9209 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9210 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9211 | `			}` |
|      ! 0 |  9212 | `		}` |
|        3 |  9213 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9214 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9215 | `		if( nArg > 1 ){` |
|        3 |  9216 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9217 | `			if( iValue ){` |
|        - |  9218 | `				/* Terminate execution on failed assertions */` |
|        3 |  9219 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9220 | `			}` |
|        2 |  9221 | `		}` |
|        1 |  9222 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9223 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9224 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9225 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9226 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9227 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9228 | `		}` |
|      ! 0 |  9229 | `	}` |
|        - |  9230 | `	/* Return the old flags */` |
|        9 |  9231 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9232 | `	return PH7_OK;` |
|        5 |  9233 |  |
|        - |  9234 | `/*` |
|        - |  9235 | ` * bool assert(mixed $assertion)` |
|        - |  9236 | ` *  Checks if assertion is FALSE.` |
|        - |  9237 | ` * Parameter` |
|        - |  9238 | ` *  $assertion` |
|        - |  9239 | ` *    The assertion to test.` |
|        - |  9240 | ` * Return` |
|        - |  9241 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9242 | ` */` |
|       14 |  9243 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9244 |  |
|       15 |  9245 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9246 | `	ph7_value *pAssert;` |
|        - |  9247 | `	int iFlags,iResult;` |
|       15 |  9248 | `	if( nArg < 1 ){` |
|        - |  9249 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9250 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9251 | `		return PH7_OK;` |
|        - |  9252 | `	}` |
|       15 |  9253 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9254 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9255 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9256 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9257 | `		return PH7_OK;` |
|        - |  9258 | `	}` |
|       15 |  9259 | `	pAssert = apArg[0];` |
|       15 |  9260 | `	iResult = 1; /* cc warning */` |
|       15 |  9261 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9262 | `		SyString sChunk;` |
|        7 |  9263 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  9264 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9265 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9266 | `			/* Extract evaluation result */` |
|        5 |  9267 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9268 | `		}else{` |
|        3 |  9269 | `			iResult = 0;` |
|        - |  9270 | `		}` |
|        4 |  9271 | `	}else{` |
|        - |  9272 | `		/* Perform a boolean cast */` |
|        9 |  9273 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9274 | `	}` |
|       15 |  9275 | `	if( !iResult ){` |
|        - |  9276 | `		/* Assertion failed */` |
|        9 |  9277 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9278 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9279 | `			ph7_value sFile,sLine;` |
|        - |  9280 | `			ph7_value *apCbArg[3];` |
|        - |  9281 | `			SyString *pFile;` |
|        - |  9282 | `			/* Extract the processed script */` |
|      ! 0 |  9283 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9284 | `			if( pFile == 0 ){` |
|      ! 0 |  9285 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9286 | `			}` |
|        - |  9287 | `			/* Invoke the callback */` |
|      ! 0 |  9288 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9289 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9290 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9291 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9292 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9293 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9294 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9295 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9296 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9297 | `		}` |
|        9 |  9298 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9299 | `			/* Emit a warning */` |
|        9 |  9300 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9301 | `		}` |
|        9 |  9302 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9303 | `			/* Abort VM execution immediately */` |
|        3 |  9304 | `			return PH7_ABORT;` |
|        - |  9305 | `		}` |
|        3 |  9306 | `	}` |
|        - |  9307 | `	/* Assertion result */` |
|       13 |  9308 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9309 | `	return PH7_OK;` |
|        8 |  9310 |  |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Section:` |
|        - |  9313 | ` *  Error reporting functions.` |
|        - |  9314 | ` * Status:` |
|        - |  9315 | ` *    Stable.` |
|        - |  9316 | ` */` |
|        - |  9317 | `/*` |
|        - |  9318 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9319 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9320 | ` * Parameters` |
|        - |  9321 | ` *  $error_msg` |
|        - |  9322 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9323 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9324 | ` * $error_type` |
|        - |  9325 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9326 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9327 | ` * Return` |
|        - |  9328 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9329 | ` */` |
|       12 |  9330 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9331 |  |
|       14 |  9332 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9333 | `	int rc = PH7_OK;` |
|       14 |  9334 | `	if( nArg > 0 ){` |
|        - |  9335 | `		const char *zErr;` |
|        - |  9336 | `		int nLen;` |
|        - |  9337 | `		/* Extract the error message */` |
|       12 |  9338 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9339 | `		if( nArg > 1 ){` |
|        - |  9340 | `			/* Extract the error type */` |
|       12 |  9341 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9342 | `			switch( nErr ){` |
|        1 |  9343 | `			case 1:   /* E_ERROR */` |
|        - |  9344 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9345 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9346 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9347 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9348 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9349 | `				break;` |
|        1 |  9350 | `			case 2:   /* E_WARNING */` |
|        - |  9351 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9352 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9353 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9354 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9355 | `				break;` |
|        3 |  9356 | `			default:` |
|        8 |  9357 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9358 | `				break;` |
|        - |  9359 | `			}` |
|        5 |  9360 | `		}` |
|        - |  9361 | `		/* Report error */` |
|       12 |  9362 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9363 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9364 | `			return rc;` |
|        - |  9365 | `		}` |
|        - |  9366 | `		/* Return true */` |
|       12 |  9367 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9368 | `	}else{` |
|        - |  9369 | `		/* Missing arguments,return FALSE */` |
|        3 |  9370 | `		ph7_result_bool(pCtx,0);` |
|        - |  9371 | `	}` |
|       14 |  9372 | `	return rc;` |
|        8 |  9373 |  |
|        - |  9374 | `/*` |
|        - |  9375 | ` * int error_reporting([int $level])` |
|        - |  9376 | ` *  Sets which PHP errors are reported.` |
|        - |  9377 | ` * Parameters` |
|        - |  9378 | ` *  $level` |
|        - |  9379 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9380 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9381 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9382 | ` *   levels will not always behave as expected.` |
|        - |  9383 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9384 | ` *   in the predefined constants.` |
|        - |  9385 | ` * Return` |
|        - |  9386 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9387 | ` *   parameter is given.` |
|        - |  9388 | ` */` |
|       18 |  9389 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9390 |  |
|       19 |  9391 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9392 | `	int nOld;` |
|        - |  9393 | `	/* Extract the old reporting level */` |
|       19 |  9394 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9395 | `	if( nArg > 0 ){` |
|        - |  9396 | `		int nNew;` |
|        - |  9397 | `		/* Extract the desired error reporting level */` |
|       11 |  9398 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9399 | `		if( !nNew ){` |
|        - |  9400 | `			/* Do not report errors at all */` |
|        5 |  9401 | `			pVm->bErrReport = 0;` |
|        3 |  9402 | `		}else{` |
|        - |  9403 | `			/* Report all errors */` |
|        7 |  9404 | `			pVm->bErrReport = 1;` |
|        - |  9405 | `		}` |
|        5 |  9406 | `	}` |
|        - |  9407 | `	/* Return the old level */` |
|       19 |  9408 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9409 | `	return PH7_OK;` |
|        1 |  9410 |  |
|        - |  9411 | `/*` |
|        - |  9412 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9413 | ` *  Send an error message somewhere.` |
|        - |  9414 | ` * Parameter` |
|        - |  9415 | ` *  $message` |
|        - |  9416 | ` *   The error message that should be logged.` |
|        - |  9417 | ` *  $message_type` |
|        - |  9418 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9419 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9420 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9421 | ` *       This is the default option.` |
|        - |  9422 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9423 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9424 | ` *    2  No longer an option.` |
|        - |  9425 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9426 | ` *       to the end of the message string.` |
|        - |  9427 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9428 | ` *  $destination` |
|        - |  9429 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9430 | ` *  $extra_headers` |
|        - |  9431 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9432 | ` * Return` |
|        - |  9433 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9434 | ` * NOTE:` |
|        - |  9435 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9436 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9437 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9438 | ` *  Otherwise this function is no-op.` |
|        - |  9439 | ` */` |
|        4 |  9440 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9441 |  |
|        - |  9442 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9443 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9444 | `	int iType = 0;` |
|        5 |  9445 | `	if( nArg < 1 ){` |
|        - |  9446 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9447 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9448 | `		return PH7_OK;` |
|        - |  9449 | `	}` |
|        5 |  9450 | `	if( pVm->xErrLog  ){` |
|        - |  9451 | `		/* Invoke the user callback */` |
|      ! 0 |  9452 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9453 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9454 | `		if( nArg > 1 ){` |
|      ! 0 |  9455 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9456 | `			if( nArg > 2 ){` |
|      ! 0 |  9457 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9458 | `				if( nArg > 3 ){` |
|      ! 0 |  9459 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9460 | `				}` |
|      ! 0 |  9461 | `			}` |
|      ! 0 |  9462 | `		}` |
|      ! 0 |  9463 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9464 | `	}` |
|        - |  9465 | `	/* Retun TRUE */` |
|        5 |  9466 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9467 | `	return PH7_OK;` |
|        3 |  9468 |  |
|        - |  9469 | `/*` |
|        - |  9470 | ` * bool restore_exception_handler(void)` |
|        - |  9471 | ` *  Restores the previously defined exception handler function.` |
|        - |  9472 | ` * Parameter` |
|        - |  9473 | ` *  None` |
|        - |  9474 | ` * Return` |
|        - |  9475 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9476 | ` */` |
|        4 |  9477 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9478 |  |
|        5 |  9479 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9480 | `	ph7_value *pOld,*pNew;` |
|        - |  9481 | `	/* Point to the old and the new handler */` |
|        5 |  9482 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9483 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9484 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9485 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9486 | `		SXUNUSED(apArg);` |
|        - |  9487 | `		/* No installed handler,return FALSE */` |
|        5 |  9488 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9489 | `		return PH7_OK;` |
|        - |  9490 | `	}` |
|        - |  9491 | `	/* Copy the old handler */` |
|      ! 0 |  9492 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9493 | `	PH7_MemObjRelease(pOld);` |
|        - |  9494 | `	/* Return TRUE */` |
|      ! 0 |  9495 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9496 | `	return PH7_OK;` |
|        3 |  9497 |  |
|        - |  9498 | `/*` |
|        - |  9499 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9500 | ` *  Sets a user-defined exception handler function.` |
|        - |  9501 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9502 | ` * NOTE` |
|        - |  9503 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9504 | ` *  the satndard PHP engine.` |
|        - |  9505 | ` * Parameters` |
|        - |  9506 | ` *  $exception_handler` |
|        - |  9507 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9508 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9509 | ` *   that was thrown.` |
|        - |  9510 | ` *  Note:` |
|        - |  9511 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9512 | ` * Return` |
|        - |  9513 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9514 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9515 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9516 | ` */` |
|        4 |  9517 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9518 |  |
|        5 |  9519 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9520 | `	ph7_value *pOld,*pNew;` |
|        - |  9521 | `	/* Point to the old and the new handler */` |
|        5 |  9522 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9523 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9524 | `	/* Return the old handler */` |
|        5 |  9525 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        5 |  9526 | `	if( nArg > 0 ){` |
|        5 |  9527 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9528 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9529 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9530 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9531 | `		}else{` |
|        5 |  9532 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9533 | `			/* Install the new handler */` |
|        5 |  9534 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9535 | `		}` |
|        2 |  9536 | `	}` |
|        5 |  9537 | `	return PH7_OK;` |
|        1 |  9538 |  |
|        - |  9539 | `/*` |
|        - |  9540 | ` * bool restore_error_handler(void)` |
|        - |  9541 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9542 | ` * Parameters:` |
|        - |  9543 | ` *  None.` |
|        - |  9544 | ` * Return` |
|        - |  9545 | ` *  Always TRUE.` |
|        - |  9546 | ` */` |
|        4 |  9547 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9548 |  |
|        5 |  9549 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9550 | `	ph7_value *pOld,*pNew;` |
|        - |  9551 | `	/* Point to the old and the new handler */` |
|        5 |  9552 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9553 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9554 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9555 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9556 | `		SXUNUSED(apArg);` |
|        - |  9557 | `		/* No installed callback,return FALSE */` |
|        5 |  9558 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9559 | `		return PH7_OK;` |
|        - |  9560 | `	}` |
|        - |  9561 | `	/* Copy the old callback */` |
|      ! 0 |  9562 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9563 | `	PH7_MemObjRelease(pOld);` |
|        - |  9564 | `	/* Return TRUE */` |
|      ! 0 |  9565 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9566 | `	return PH7_OK;` |
|        3 |  9567 |  |
|        - |  9568 | `/*` |
|        - |  9569 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9570 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9571 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9572 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9573 | ` *  Sets a user-defined error handler function.` |
|        - |  9574 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9575 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9576 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9577 | ` *  conditions (using trigger_error()).` |
|        - |  9578 | ` * Parameters` |
|        - |  9579 | ` *  $error_handler` |
|        - |  9580 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9581 | ` *   describing the error.` |
|        - |  9582 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9583 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9584 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9585 | ` *   The function can be shown as:` |
|        - |  9586 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9587 | ` *     errno` |
|        - |  9588 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9589 | ` *   errstr` |
|        - |  9590 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9591 | ` *   errfile` |
|        - |  9592 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9593 | ` *     was raised in, as a string.` |
|        - |  9594 | ` *  Note:` |
|        - |  9595 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9596 | ` * Return` |
|        - |  9597 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9598 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9599 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9600 | ` */` |
|     5266 |  9601 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9602 |  |
|     5268 |  9603 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9604 | `	ph7_value *pOld,*pNew;` |
|        - |  9605 | `	/* Point to the old and the new handler */` |
|     5268 |  9606 | `	pOld = &pVm->aErrCB[0];` |
|     5268 |  9607 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9608 | `	/* Return the old handler */` |
|     5268 |  9609 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     5268 |  9610 | `	if( nArg > 0 ){` |
|     5268 |  9611 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9612 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     2633 |  9613 | `			PH7_MemObjRelease(pNew);` |
|     2633 |  9614 | `			ph7_result_bool(pCtx,1);` |
|     1317 |  9615 | `		}else{` |
|     2636 |  9616 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9617 | `			/* Install the new handler */` |
|     2636 |  9618 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9619 | `		}` |
|     2633 |  9620 | `	}` |
|     5268 |  9621 | `	return PH7_OK;` |
|        2 |  9622 |  |
|        - |  9623 | `/*` |
|        - |  9624 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9625 | ` *  Generates a backtrace.` |
|        - |  9626 | ` * Paramaeter` |
|        - |  9627 | ` *  $options` |
|        - |  9628 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9629 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9630 | ` *   all the function/method arguments, to save memory.` |
|        - |  9631 | ` * $limit` |
|        - |  9632 | ` *   (Not Used)` |
|        - |  9633 | ` * Return` |
|        - |  9634 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9635 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9636 | ` *          Name        Type      Description` |
|        - |  9637 | ` *          ------      ------     -----------` |
|        - |  9638 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9639 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9640 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9641 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9642 | ` *          object      object    The current object.` |
|        - |  9643 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9644 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9645 | ` */` |
|       40 |  9646 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9647 |  |
|       42 |  9648 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9649 | `	ph7_value *pArray;` |
|        - |  9650 | `	ph7_class *pClass;` |
|        - |  9651 | `	ph7_value *pValue;` |
|        - |  9652 | `	SyString *pFile;` |
|        - |  9653 | `	/* Create a new array */` |
|       42 |  9654 | `	pArray = ph7_context_new_array(pCtx);` |
|       42 |  9655 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       42 |  9656 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9657 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9658 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9659 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9660 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9661 | `		SXUNUSED(apArg);` |
|      ! 0 |  9662 | `		return PH7_OK;` |
|        - |  9663 | `	}` |
|        - |  9664 | `	/* Dump running function name and it's arguments  */` |
|       42 |  9665 | `	if( pVm->pFrame->pParent ){` |
|       42 |  9666 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9667 | `		ph7_vm_func *pFunc;` |
|        - |  9668 | `		ph7_value *pArg;` |
|       42 |  9669 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9670 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9671 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9672 | `		}` |
|       42 |  9673 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       42 |  9674 | `		if( pFrame->pParent && pFunc ){` |
|       42 |  9675 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|       42 |  9676 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|       42 |  9677 | `			ph7_value_reset_string_cursor(pValue);` |
|       20 |  9678 | `		}` |
|        - |  9679 | `		/* Function arguments */` |
|       42 |  9680 | `		pArg = ph7_context_new_array(pCtx);` |
|       42 |  9681 | `		if( pArg  ){` |
|        - |  9682 | `			ph7_value *pObj;` |
|        - |  9683 | `			VmSlot *aSlot;` |
|        - |  9684 | `			sxu32 n;` |
|        - |  9685 | `			/* Start filling the array with the given arguments */` |
|       42 |  9686 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      154 |  9687 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      114 |  9688 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      114 |  9689 | `				if( pObj ){` |
|      114 |  9690 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       56 |  9691 | `				}` |
|       58 |  9692 | `			}` |
|        - |  9693 | `			/* Save the array */` |
|       42 |  9694 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|       20 |  9695 | `		}` |
|       20 |  9696 | `	}` |
|       42 |  9697 | `	ph7_value_int(pValue,1);` |
|        - |  9698 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9699 | `	 * line numbers at run-time. )` |
|        - |  9700 | `	 */` |
|       42 |  9701 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9702 | `	/* Current processed script */` |
|       42 |  9703 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  9704 | `	if( pFile ){` |
|       42 |  9705 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|       42 |  9706 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|       42 |  9707 | `		ph7_value_reset_string_cursor(pValue);` |
|       20 |  9708 | `	}` |
|        - |  9709 | `	/* Top class */` |
|       42 |  9710 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|       42 |  9711 | `	if( pClass ){` |
|       38 |  9712 | `		ph7_value_reset_string_cursor(pValue);` |
|       38 |  9713 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       38 |  9714 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|       18 |  9715 | `	}` |
|        - |  9716 | `	/* Return the freshly created array */` |
|       42 |  9717 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9718 | `	/*` |
|        - |  9719 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9720 | `	 * as soon we return from this function.` |
|        - |  9721 | `	 */` |
|       42 |  9722 | `	return PH7_OK;` |
|       22 |  9723 |  |
|        - |  9724 | `/*` |
|        - |  9725 | ` * Generate a small backtrace.` |
|        - |  9726 | ` * Store the generated dump in the given BLOB` |
|        - |  9727 | ` */` |
|        4 |  9728 | `static int VmMiniBacktrace(` |
|        - |  9729 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9730 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9731 | `	)` |
|        1 |  9732 |  |
|        5 |  9733 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9734 | `	ph7_vm_func *pFunc;` |
|        - |  9735 | `	ph7_class *pClass;` |
|        - |  9736 | `	SyString *pFile;` |
|        - |  9737 | `	/* Called function */` |
|        5 |  9738 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9739 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9740 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9741 | `	}` |
|        5 |  9742 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9743 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9744 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9745 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9746 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9747 | `	}else{` |
|      ! 0 |  9748 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9749 | `	}` |
|        5 |  9750 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9751 | `	/* Current processed script */` |
|        5 |  9752 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9753 | `	if( pFile ){` |
|        5 |  9754 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9755 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9756 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9757 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9758 | `	}` |
|        - |  9759 | `	/* Top class */` |
|        5 |  9760 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9761 | `	if( pClass ){` |
|      ! 0 |  9762 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9763 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9764 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9765 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9766 | `	}` |
|        5 |  9767 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9768 | `	/* All done */` |
|        5 |  9769 | `	return SXRET_OK;` |
|        1 |  9770 |  |
|        - |  9771 | `/*` |
|        - |  9772 | ` * void debug_print_backtrace()` |
|        - |  9773 | ` *  Prints a backtrace` |
|        - |  9774 | ` * Parameters` |
|        - |  9775 | ` * None` |
|        - |  9776 | ` * Return` |
|        - |  9777 | ` * NULL` |
|        - |  9778 | ` */` |
|        2 |  9779 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9780 |  |
|        3 |  9781 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9782 | `	SyBlob sDump;` |
|        3 |  9783 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9784 | `	/* Generate the backtrace */` |
|        3 |  9785 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9786 | `	/* Output backtrace */` |
|        3 |  9787 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9788 | `	/* All done,cleanup */` |
|        3 |  9789 | `	SyBlobRelease(&sDump);` |
|        1 |  9790 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9791 | `	SXUNUSED(apArg);` |
|        3 |  9792 | `	return PH7_OK;` |
|        1 |  9793 |  |
|        - |  9794 | `/*` |
|        - |  9795 | ` * string debug_string_backtrace()` |
|        - |  9796 | ` *  Generate a backtrace` |
|        - |  9797 | ` * Parameters` |
|        - |  9798 | ` * None` |
|        - |  9799 | ` * Return` |
|        - |  9800 | ` *  A mini backtrace().` |
|        - |  9801 | ` * Note that this is a symisc extension.` |
|        - |  9802 | ` */` |
|        2 |  9803 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9804 |  |
|        3 |  9805 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9806 | `	SyBlob sDump;` |
|        3 |  9807 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9808 | `	/* Generate the backtrace */` |
|        3 |  9809 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9810 | `	/* Return the backtrace */` |
|        3 |  9811 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9812 | `	/* All done,cleanup */` |
|        3 |  9813 | `	SyBlobRelease(&sDump);` |
|        1 |  9814 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9815 | `	SXUNUSED(apArg);` |
|        3 |  9816 | `	return PH7_OK;` |
|        1 |  9817 |  |
|        - |  9818 | `/*` |
|        - |  9819 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9820 | ` * exception is triggered.` |
|        - |  9821 | ` */` |
|       24 |  9822 | `static sxi32 VmUncaughtException(` |
|        - |  9823 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9824 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9825 | `	)` |
|        2 |  9826 |  |
|        - |  9827 | `	ph7_value *apArg[2],sArg;` |
|       26 |  9828 | `	int nArg = 1;` |
|        - |  9829 | `	sxi32 rc;` |
|       26 |  9830 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9831 | `		/* Nesting limit reached */` |
|      ! 0 |  9832 | `		return SXRET_OK;` |
|        - |  9833 | `	}` |
|        - |  9834 | `	/* Call any exception handler if available */` |
|       26 |  9835 | `	PH7_MemObjInit(pVm,&sArg);` |
|       26 |  9836 | `	if( pThis ){` |
|        - |  9837 | `		/* Load the exception instance */` |
|       26 |  9838 | `		sArg.x.pOther = pThis;` |
|       26 |  9839 | `		pThis->iRef++;` |
|       26 |  9840 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|       14 |  9841 | `	}else{` |
|      ! 0 |  9842 | `		nArg = 0;` |
|        - |  9843 | `	}` |
|       26 |  9844 | `	apArg[0] = &sArg;` |
|        - |  9845 | `	/* Call the exception handler if available */` |
|       26 |  9846 | `	pVm->nExceptDepth++;` |
|       26 |  9847 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|       26 |  9848 | `	pVm->nExceptDepth--;` |
|       26 |  9849 | `	if( rc != SXRET_OK ){` |
|        - |  9850 | `		SyBlob sMsgBuf;` |
|       23 |  9851 | `		const char *zClass = "Exception";` |
|       23 |  9852 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9853 | `		const char *zMsg;` |
|        - |  9854 | `		sxu32 nMsg;` |
|        - |  9855 | `		const char *zFuncName;` |
|        - |  9856 | `		int nFuncLen;` |
|       23 |  9857 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|       23 |  9858 | `		if( pThis ){` |
|        - |  9859 | `			ph7_class_method *pGetMessage;` |
|        - |  9860 | `			ph7_value sMsg;` |
|        - |  9861 | `			const char *zTmp;` |
|        - |  9862 | `			int nTmp;` |
|       23 |  9863 | `			zClass = pThis->pClass->sName.zString;` |
|       23 |  9864 | `			nClass = pThis->pClass->sName.nByte;` |
|       23 |  9865 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|       23 |  9866 | `			if( pGetMessage ){` |
|       23 |  9867 | `				PH7_MemObjInit(pVm,&sMsg);` |
|       23 |  9868 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|       23 |  9869 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|       23 |  9870 | `					if( zTmp && nTmp > 0 ){` |
|       23 |  9871 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|       11 |  9872 | `					}` |
|       11 |  9873 | `				}` |
|       23 |  9874 | `				PH7_MemObjRelease(&sMsg);` |
|       11 |  9875 | `			}` |
|       11 |  9876 | `		}` |
|       23 |  9877 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9878 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9879 | `		}` |
|       23 |  9880 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|       23 |  9881 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|       23 |  9882 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       23 |  9883 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|       23 |  9884 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9885 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|       23 |  9886 | `		rc = SXERR_ABORT;` |
|       11 |  9887 | `	}` |
|       26 |  9888 | `	PH7_MemObjRelease(&sArg);` |
|       26 |  9889 | `	return rc;` |
|       14 |  9890 |  |
|        - |  9891 | `/*` |
|        - |  9892 | ` * Throw an user exception.` |
|        - |  9893 | ` */` |
|       38 |  9894 | `static sxi32 VmThrowException(` |
|        - |  9895 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9896 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9897 | `	)` |
|        2 |  9898 |  |
|        - |  9899 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9900 | `	ph7_exception **apException;` |
|        - |  9901 | `	ph7_exception *pException;` |
|        - |  9902 | `	/* Point to the stack of loaded exceptions */` |
|       40 |  9903 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       40 |  9904 | `	pException = 0;` |
|       40 |  9905 | `	pCatch = 0;` |
|       40 |  9906 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9907 | `		ph7_exception_block *aCatch;` |
|        - |  9908 | `		ph7_class *pClass;` |
|        - |  9909 | `		sxu32 j;` |
|        - |  9910 | `		/* Locate the appropriate block to execute */` |
|       16 |  9911 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9912 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9913 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9914 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9915 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9916 | `			/* Extract the target class */` |
|       16 |  9917 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9918 | `			if( pClass == 0 ){` |
|        - |  9919 | `				/* No such class */` |
|      ! 0 |  9920 | `				continue;` |
|        - |  9921 | `			}` |
|       16 |  9922 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9923 | `				/* Catch block found,break immeditaley */` |
|       16 |  9924 | `				pCatch = &aCatch[j];` |
|       16 |  9925 | `				break;` |
|        - |  9926 | `			}` |
|      ! 0 |  9927 | `		}` |
|        7 |  9928 | `	}` |
|        - |  9929 | `	/* Execute the cached block if available */` |
|       40 |  9930 | `	if( pCatch == 0 ){` |
|        - |  9931 | `		sxi32 rc;` |
|       26 |  9932 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|       26 |  9933 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9934 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9935 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9936 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9937 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9938 | `			}` |
|      ! 0 |  9939 | `			if( pException->pFrame == pFrame ){` |
|        - |  9940 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9941 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9942 | `			}` |
|      ! 0 |  9943 | `		}` |
|       26 |  9944 | `		return rc;` |
|      ! 0 |  9945 | `	}else{` |
|       16 |  9946 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9947 | `		sxi32 rc;` |
|       24 |  9948 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9949 | `			/* Safely ignore the exception frame */` |
|       10 |  9950 | `			pFrame = pFrame->pParent;` |
|        2 |  9951 | `		}` |
|       16 |  9952 | `		if( pException->pFrame == pFrame ){` |
|        - |  9953 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9954 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9955 | `		}` |
|        - |  9956 | `		/* Create a private frame first */` |
|       16 |  9957 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9958 | `		if( rc == SXRET_OK ){` |
|        - |  9959 | `			/* Mark as catch frame */` |
|       16 |  9960 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9961 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9962 | `			if( pObj ){` |
|        - |  9963 | `				/* Install the exception instance */` |
|       16 |  9964 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9965 | `				pObj->x.pOther = pThis;` |
|       16 |  9966 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9967 | `			}` |
|        - |  9968 | `			/* Exceute the block */` |
|       16 |  9969 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9970 | `			/* Leave the frame */` |
|       16 |  9971 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9972 | `		}` |
|        - |  9973 | `	}` |
|        - |  9974 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9975 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9976 | `	 */` |
|       16 |  9977 | `	return SXRET_OK;` |
|       21 |  9978 |  |
|        - |  9979 | `/*` |
|        - |  9980 | ` * Section:` |
|        - |  9981 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9982 | ` * Status:` |
|        - |  9983 | ` *    Stable.` |
|        - |  9984 | ` */` |
|        - |  9985 | `/*` |
|        - |  9986 | ` * string ph7version(void)` |
|        - |  9987 | ` *  Returns the running version of the PH7 version.` |
|        - |  9988 | ` * Parameters` |
|        - |  9989 | ` *  None` |
|        - |  9990 | ` * Return` |
|        - |  9991 | ` * Current PH7 version.` |
|        - |  9992 | ` */` |
|        2 |  9993 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9994 |  |
|        1 |  9995 | `	SXUNUSED(nArg);` |
|        1 |  9996 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9997 | `	/* Current engine version */` |
|        3 |  9998 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9999 | `	return PH7_OK;` |
|        1 | 10000 |  |
|        - | 10001 | `/*` |
|        - | 10002 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10003 | ` */` |
|        - | 10004 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10005 | ` "<html><head>"\` |
|        - | 10006 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10007 | ` "<style type=\"text/css\">"\` |
|        - | 10008 | ` "div {"\` |
|        - | 10009 | `     "border: 1px solid #cccccc;"\` |
|        - | 10010 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10011 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10012 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10013 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10014 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10015 | `     "-o-border-radius: 10px;"\` |
|        - | 10016 | `     "border-radius: 10px;"\` |
|        - | 10017 | `     "padding-left: 2em;"\` |
|        - | 10018 | `     "background-color: white;"\` |
|        - | 10019 | `     "margin-left: auto;"\` |
|        - | 10020 | `     "font-family: verdana;"\` |
|        - | 10021 | `     "padding-right: 2em;"\` |
|        - | 10022 | `     "margin-right: auto;"\` |
|        - | 10023 | `     "}"\` |
|        - | 10024 | `     "body {"\` |
|        - | 10025 | `     "padding: 0.2em;"\` |
|        - | 10026 | `     "font-style: normal;"\` |
|        - | 10027 | `     "font-size: medium;"\` |
|        - | 10028 | `     "background-color: #f2f2f2;"\` |
|        - | 10029 | `     "}"\` |
|        - | 10030 | `     "hr {"\` |
|        - | 10031 | `     "border-style: solid none none;"\` |
|        - | 10032 | `     "border-width: 1px medium medium;"\` |
|        - | 10033 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10034 | `     "height: 1px;"\` |
|        - | 10035 | `     "}"\` |
|        - | 10036 | `     "a {"\` |
|        - | 10037 | `     "color: #3366cc;"\` |
|        - | 10038 | `     "text-decoration: none;"\` |
|        - | 10039 | `     "}"\` |
|        - | 10040 | `     "a:hover {"\` |
|        - | 10041 | `     "color: #999999;"\` |
|        - | 10042 | `     "}"\` |
|        - | 10043 | `     "a:active {"\` |
|        - | 10044 | `     "color: #663399;"\` |
|        - | 10045 | `     "}"\` |
|        - | 10046 | `     "h1 {"\` |
|        - | 10047 | `     "margin: 0;"\` |
|        - | 10048 | `     "padding: 0;"\` |
|        - | 10049 | `     "font-family: Verdana;"\` |
|        - | 10050 | `     "font-weight: bold;"\` |
|        - | 10051 | `     "font-style: normal;"\` |
|        - | 10052 | `     "font-size: medium;"\` |
|        - | 10053 | `     "text-transform: capitalize;"\` |
|        - | 10054 | `     "color: #0a328c;"\` |
|        - | 10055 | `     "}"\` |
|        - | 10056 | `     "p {"\` |
|        - | 10057 | `     "margin: 0 auto;"\` |
|        - | 10058 | `     "font-size: medium;"\` |
|        - | 10059 | `     "font-style: normal;"\` |
|        - | 10060 | `     "font-family: verdana;"\` |
|        - | 10061 | `     "}"\` |
|        - | 10062 | `"</style></head><body>"\` |
|        - | 10063 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10064 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10065 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10066 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10067 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10068 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10069 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10070 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10071 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10072 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10073 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10074 |  |
|        - | 10075 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10076 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10077 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10078 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10079 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10080 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10081 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10082 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10083 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10084 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10085 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10086 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10087 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10088 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10089 |  |
|        - | 10090 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10091 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10092 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10093 | `"&nbsp;*<br>"\` |
|        - | 10094 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10095 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10096 | `"&nbsp;* are met:<br>"\` |
|        - | 10097 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10098 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10099 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10100 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10101 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10102 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10103 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10104 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10105 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10106 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10107 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10108 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10109 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10110 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10111 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10112 | `"&nbsp;*<br>"\` |
|        - | 10113 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10114 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10115 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10116 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10117 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10118 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10119 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10120 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10121 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10122 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10123 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10124 | `"&nbsp;*/<br>"\` |
|        - | 10125 | `"</span></small></small></p>"\` |
|        - | 10126 | `"</div></body></html>"` |
|        - | 10127 | `/*` |
|        - | 10128 | ` * bool ph7credits(void)` |
|        - | 10129 | ` * bool ph7info(void)` |
|        - | 10130 | ` * bool ph7copyright(void)` |
|        - | 10131 | ` *  Prints out the credits for PH7 engine` |
|        - | 10132 | ` * Parameters` |
|        - | 10133 | ` *  None` |
|        - | 10134 | ` * Return` |
|        - | 10135 | ` *  Always TRUE` |
|        - | 10136 | ` */` |
|        2 | 10137 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10138 |  |
|        3 | 10139 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10140 | `	/* Expand the HTML page above*/` |
|        3 | 10141 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10142 | `	ph7_context_output_format(` |
|        1 | 10143 | `		pCtx,` |
|        - | 10144 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10145 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10146 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10147 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10148 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10149 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10150 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10151 | `#ifdef __WINNT__` |
|        - | 10152 | `		"Windows NT"` |
|        - | 10153 | `#elif defined(__UNIXES__)` |
|        - | 10154 | `		"UNIX-Like"` |
|        - | 10155 | `#else` |
|        - | 10156 | `		"Other OS"` |
|        - | 10157 | `#endif` |
|        - | 10158 | `		);` |
|        3 | 10159 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10160 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10161 | `	SXUNUSED(apArg);` |
|        - | 10162 | `	/* Return TRUE */` |
|        - | 10163 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10164 | `	return PH7_OK;` |
|        1 | 10165 |  |
|        - | 10166 | `/*` |
|        - | 10167 | ` * Section:` |
|        - | 10168 | ` *    URL related routines.` |
|        - | 10169 | ` * Status:` |
|        - | 10170 | ` *    Stable.` |
|        - | 10171 | ` */` |
|        - | 10172 | `/* Forward declaration */` |
|        - | 10173 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10174 | `/*` |
|        - | 10175 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10176 | ` *  Parse a URL and return its fields.` |
|        - | 10177 | ` * Parameters` |
|        - | 10178 | ` *  $url` |
|        - | 10179 | ` *   The URL to parse.` |
|        - | 10180 | ` * $component` |
|        - | 10181 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10182 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10183 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10184 | ` *  in which case the return value will be an integer).` |
|        - | 10185 | ` * Return` |
|        - | 10186 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10187 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10188 | ` *  this array are:` |
|        - | 10189 | ` *   scheme - e.g. http` |
|        - | 10190 | ` *   host` |
|        - | 10191 | ` *   port` |
|        - | 10192 | ` *   user` |
|        - | 10193 | ` *   pass` |
|        - | 10194 | ` *   path` |
|        - | 10195 | ` *   query - after the question mark ?` |
|        - | 10196 | ` *   fragment - after the hashmark #` |
|        - | 10197 | ` * Note:` |
|        - | 10198 | ` *  FALSE is returned on failure.` |
|        - | 10199 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10200 | ` *  with the standard PHP engine.` |
|        - | 10201 | ` */` |
|       28 | 10202 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10203 |  |
|        - | 10204 | `	const char *zStr; /* Input string */` |
|        - | 10205 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10206 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10207 | `	int nLen;` |
|        - | 10208 | `	sxi32 rc;` |
|       29 | 10209 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10210 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10211 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10212 | `		return PH7_OK;` |
|        - | 10213 | `	}` |
|        - | 10214 | `	/* Extract the given URI */` |
|       29 | 10215 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10216 | `	if( nLen < 1 ){` |
|        - | 10217 | `		/* Nothing to process,return FALSE */` |
|        3 | 10218 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10219 | `		return PH7_OK;` |
|        - | 10220 | `	}` |
|        - | 10221 | `	/* Get a parse */` |
|       27 | 10222 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10223 | `	if( rc != SXRET_OK ){` |
|        - | 10224 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10225 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10226 | `		return PH7_OK;` |
|        - | 10227 | `	}` |
|       27 | 10228 | `	if( nArg > 1 ){` |
|      ! 0 | 10229 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10230 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10231 | `		switch(nComponent){` |
|      ! 0 | 10232 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10233 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10234 | `			if( pComp->nByte < 1 ){` |
|        - | 10235 | `				/* No available value,return NULL */` |
|      ! 0 | 10236 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10237 | `			}else{` |
|      ! 0 | 10238 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10239 | `			}` |
|      ! 0 | 10240 | `			break;` |
|      ! 0 | 10241 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10242 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10243 | `			if( pComp->nByte < 1 ){` |
|        - | 10244 | `				/* No available value,return NULL */` |
|      ! 0 | 10245 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10246 | `			}else{` |
|      ! 0 | 10247 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10248 | `			}` |
|      ! 0 | 10249 | `			break;` |
|      ! 0 | 10250 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10251 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10252 | `			if( pComp->nByte < 1 ){` |
|        - | 10253 | `				/* No available value,return NULL */` |
|      ! 0 | 10254 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10255 | `			}else{` |
|      ! 0 | 10256 | `				int iPort = 0;` |
|        - | 10257 | `				/* Cast the value to integer */` |
|      ! 0 | 10258 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10259 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10260 | `			}` |
|      ! 0 | 10261 | `			break;` |
|      ! 0 | 10262 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10263 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10264 | `			if( pComp->nByte < 1 ){` |
|        - | 10265 | `				/* No available value,return NULL */` |
|      ! 0 | 10266 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10267 | `			}else{` |
|      ! 0 | 10268 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10269 | `			}` |
|      ! 0 | 10270 | `			break;` |
|      ! 0 | 10271 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10272 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10273 | `			if( pComp->nByte < 1 ){` |
|        - | 10274 | `				/* No available value,return NULL */` |
|      ! 0 | 10275 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10276 | `			}else{` |
|      ! 0 | 10277 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10278 | `			}` |
|      ! 0 | 10279 | `			break;` |
|      ! 0 | 10280 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10281 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10282 | `			if( pComp->nByte < 1 ){` |
|        - | 10283 | `				/* No available value,return NULL */` |
|      ! 0 | 10284 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10285 | `			}else{` |
|      ! 0 | 10286 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10287 | `			}` |
|      ! 0 | 10288 | `			break;` |
|      ! 0 | 10289 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10290 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10291 | `			if( pComp->nByte < 1 ){` |
|        - | 10292 | `				/* No available value,return NULL */` |
|      ! 0 | 10293 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10294 | `			}else{` |
|      ! 0 | 10295 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10296 | `			}` |
|      ! 0 | 10297 | `			break;` |
|      ! 0 | 10298 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10299 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10300 | `			if( pComp->nByte < 1 ){` |
|        - | 10301 | `				/* No available value,return NULL */` |
|      ! 0 | 10302 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10303 | `			}else{` |
|      ! 0 | 10304 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10305 | `			}` |
|      ! 0 | 10306 | `			break;` |
|      ! 0 | 10307 | `		default:` |
|        - | 10308 | `			/* No such entry,return NULL */` |
|      ! 0 | 10309 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10310 | `			break;` |
|        - | 10311 | `		}` |
|      ! 0 | 10312 | `	}else{` |
|        - | 10313 | `		ph7_value *pArray,*pValue;` |
|        - | 10314 | `		/* Return an associative array */` |
|       27 | 10315 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10316 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10317 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10318 | `			/* Out of memory */` |
|      ! 0 | 10319 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10320 | `			/* Return false */` |
|      ! 0 | 10321 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10322 | `			return PH7_OK;` |
|        - | 10323 | `		}` |
|        - | 10324 | `		/* Fill the array */` |
|       27 | 10325 | `		pComp = &sURI.sScheme;` |
|       27 | 10326 | `		if( pComp->nByte > 0 ){` |
|       19 | 10327 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10328 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10329 | `		}` |
|        - | 10330 | `		/* Reset the string cursor */` |
|       27 | 10331 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10332 | `		pComp = &sURI.sHost;` |
|       27 | 10333 | `		if( pComp->nByte > 0 ){` |
|       25 | 10334 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10335 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10336 | `		}` |
|        - | 10337 | `		/* Reset the string cursor */` |
|       27 | 10338 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10339 | `		pComp = &sURI.sPort;` |
|       27 | 10340 | `		if( pComp->nByte > 0 ){` |
|       11 | 10341 | `			int iPort = 0;/* cc warning */` |
|        - | 10342 | `			/* Convert to integer */` |
|       11 | 10343 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10344 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10345 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10346 | `		}` |
|        - | 10347 | `		/* Reset the string cursor */` |
|       27 | 10348 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10349 | `		pComp = &sURI.sUser;` |
|       27 | 10350 | `		if( pComp->nByte > 0 ){` |
|        7 | 10351 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10352 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10353 | `		}` |
|        - | 10354 | `		/* Reset the string cursor */` |
|       27 | 10355 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10356 | `		pComp = &sURI.sPass;` |
|       27 | 10357 | `		if( pComp->nByte > 0 ){` |
|        7 | 10358 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10359 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10360 | `		}` |
|        - | 10361 | `		/* Reset the string cursor */` |
|       27 | 10362 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10363 | `		pComp = &sURI.sPath;` |
|       27 | 10364 | `		if( pComp->nByte > 0 ){` |
|       17 | 10365 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10366 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10367 | `		}` |
|        - | 10368 | `		/* Reset the string cursor */` |
|       27 | 10369 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10370 | `		pComp = &sURI.sQuery;` |
|       27 | 10371 | `		if( pComp->nByte > 0 ){` |
|        5 | 10372 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10373 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10374 | `		}` |
|        - | 10375 | `		/* Reset the string cursor */` |
|       27 | 10376 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10377 | `		pComp = &sURI.sFragment;` |
|       27 | 10378 | `		if( pComp->nByte > 0 ){` |
|        5 | 10379 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10380 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10381 | `		}` |
|        - | 10382 | `		/* Return the created array */` |
|       27 | 10383 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10384 | `		/* NOTE:` |
|        - | 10385 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10386 | `		 * automatically as soon we return from this function.` |
|        - | 10387 | `		 */` |
|        - | 10388 | `	}` |
|        - | 10389 | `	/* All done */` |
|       27 | 10390 | `	return PH7_OK;` |
|       15 | 10391 |  |
|        - | 10392 | `/*` |
|        - | 10393 | ` * Section:` |
|        - | 10394 | ` *   Array related routines.` |
|        - | 10395 | ` * Status:` |
|        - | 10396 | ` *    Stable.` |
|        - | 10397 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10398 | ` *  Array related functions that need access to the underlying` |
|        - | 10399 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10400 | ` */` |
|        - | 10401 | `/*` |
|        - | 10402 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10403 | ` * of the following structure.` |
|        - | 10404 | ` */` |
|        - | 10405 | `struct compact_data` |
|        - | 10406 |  |
|        - | 10407 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10408 | `	int nRecCount;      /* Recursion count */` |
|        - | 10409 | `};` |
|        - | 10410 | `/*` |
|        - | 10411 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10412 | ` */` |
|      ! 0 | 10413 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10414 |  |
|      ! 0 | 10415 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10416 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10417 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10418 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10419 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10420 | `		SyString sVar;` |
|      ! 0 | 10421 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10422 | `		if( sVar.nByte > 0 ){` |
|        - | 10423 | `			/* Query the current frame */` |
|      ! 0 | 10424 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10425 | `			/* ^` |
|        - | 10426 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10427 | `			 */` |
|      ! 0 | 10428 | `			if( pKey ){` |
|        - | 10429 | `				/* Perform the insertion */` |
|      ! 0 | 10430 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10431 | `			}` |
|      ! 0 | 10432 | `		}` |
|      ! 0 | 10433 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10434 | `		int rc;` |
|        - | 10435 | `		/* Recursively traverse this array */` |
|      ! 0 | 10436 | `		pData->nRecCount++;` |
|      ! 0 | 10437 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10438 | `		pData->nRecCount--;` |
|      ! 0 | 10439 | `		return rc;` |
|        - | 10440 | `	}` |
|      ! 0 | 10441 | `	return SXRET_OK;` |
|      ! 0 | 10442 |  |
|        - | 10443 | `/*` |
|        - | 10444 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10445 | ` *  Create array containing variables and their values.` |
|        - | 10446 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10447 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10448 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10449 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10450 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10451 | ` * Parameters` |
|        - | 10452 | ` *  $varname` |
|        - | 10453 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10454 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10455 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10456 | ` *   it recursively.` |
|        - | 10457 | ` * Return` |
|        - | 10458 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10459 | ` */` |
|        2 | 10460 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10461 |  |
|        - | 10462 | `	ph7_value *pArray,*pObj;` |
|        3 | 10463 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10464 | `	const char *zName;` |
|        - | 10465 | `	SyString sVar;` |
|        - | 10466 | `	int i,nLen;` |
|        3 | 10467 | `	if( nArg < 1 ){` |
|        - | 10468 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10469 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10470 | `		return PH7_OK;` |
|        - | 10471 | `	}` |
|        - | 10472 | `	/* Create the array */` |
|        3 | 10473 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10474 | `	if( pArray == 0 ){` |
|        - | 10475 | `		/* Out of memory */` |
|      ! 0 | 10476 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10477 | `		/* Return NULL */` |
|      ! 0 | 10478 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10479 | `		return PH7_OK;` |
|        - | 10480 | `	}` |
|        - | 10481 | `	/* Perform the requested operation */` |
|        7 | 10482 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10483 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10484 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10485 | `				struct compact_data sData;` |
|      ! 0 | 10486 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10487 | `				/* Recursively walk the array */` |
|      ! 0 | 10488 | `				sData.nRecCount = 0;` |
|      ! 0 | 10489 | `				sData.pArray = pArray;` |
|      ! 0 | 10490 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10491 | `			}` |
|      ! 0 | 10492 | `		}else{` |
|        - | 10493 | `			/* Extract variable name */` |
|        5 | 10494 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10495 | `			if( nLen > 0 ){` |
|        5 | 10496 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10497 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10498 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10499 | `				if( pObj ){` |
|        5 | 10500 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10501 | `				}` |
|        2 | 10502 | `			}` |
|        - | 10503 | `		}` |
|        3 | 10504 | `	}` |
|        - | 10505 | `	/* Return the array */` |
|        3 | 10506 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10507 | `	return PH7_OK;` |
|        2 | 10508 |  |
|        - | 10509 | `/*` |
|        - | 10510 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10511 | ` * of the following structure.` |
|        - | 10512 | ` */` |
|        - | 10513 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10514 | `struct extract_aux_data` |
|        - | 10515 |  |
|        - | 10516 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10517 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10518 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10519 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10520 | `	int iFlags;           /* Control flags */` |
|        - | 10521 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10522 | `};` |
|        - | 10523 | `/* Forward declaration */` |
|        - | 10524 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10525 | `/*` |
|        - | 10526 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10527 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10528 | ` * Parameters` |
|        - | 10529 | ` * $var_array` |
|        - | 10530 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10531 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10532 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10533 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10534 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10535 | ` * $extract_type` |
|        - | 10536 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10537 | ` *  It can be one of the following values:` |
|        - | 10538 | ` *   EXTR_OVERWRITE` |
|        - | 10539 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10540 | ` *   EXTR_SKIP` |
|        - | 10541 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10542 | ` *   EXTR_PREFIX_SAME` |
|        - | 10543 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10544 | ` *   EXTR_PREFIX_ALL` |
|        - | 10545 | ` *       Prefix all variable names with prefix.` |
|        - | 10546 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10547 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10548 | ` *   EXTR_IF_EXISTS` |
|        - | 10549 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10550 | ` *       otherwise do nothing.` |
|        - | 10551 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10552 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10553 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10554 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10555 | ` *      the current symbol table.` |
|        - | 10556 | ` * $prefix` |
|        - | 10557 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10558 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10559 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10560 | ` *  underscore character.` |
|        - | 10561 | ` * Return` |
|        - | 10562 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10563 | ` */` |
|        4 | 10564 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10565 |  |
|        - | 10566 | `	extract_aux_data sAux;` |
|        - | 10567 | `	ph7_hashmap *pMap;` |
|        5 | 10568 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10569 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10570 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10571 | `		return PH7_OK;` |
|        - | 10572 | `	}` |
|        - | 10573 | `	/* Point to the target hashmap */` |
|        5 | 10574 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10575 | `	if( pMap->nEntry < 1 ){` |
|        - | 10576 | `		/* Empty map,return  0 */` |
|      ! 0 | 10577 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10578 | `		return PH7_OK;` |
|        - | 10579 | `	}` |
|        - | 10580 | `	/* Prepare the aux data */` |
|        5 | 10581 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10582 | `	if( nArg > 1 ){` |
|        3 | 10583 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10584 | `		if( nArg > 2 ){` |
|      ! 0 | 10585 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10586 | `		}` |
|        1 | 10587 | `	}` |
|        5 | 10588 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10589 | `	/* Invoke the worker callback */` |
|        5 | 10590 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10591 | `	/* Number of variables successfully imported */` |
|        5 | 10592 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10593 | `	return PH7_OK;` |
|        3 | 10594 |  |
|        - | 10595 | `/*` |
|        - | 10596 | ` * Worker callback for the [extract()] function defined` |
|        - | 10597 | ` * below.` |
|        - | 10598 | ` */` |
|        8 | 10599 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10600 |  |
|        9 | 10601 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10602 | `	int iFlags = pAux->iFlags;` |
|        9 | 10603 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10604 | `	ph7_value *pObj;` |
|        - | 10605 | `	SyString sVar;` |
|        9 | 10606 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10607 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10608 | `	}` |
|        - | 10609 | `	/* Perform a string cast */` |
|        9 | 10610 | `	PH7_MemObjToString(pKey);` |
|        9 | 10611 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10612 | `		/* Unavailable variable name */` |
|      ! 0 | 10613 | `		return SXRET_OK;` |
|        - | 10614 | `	}` |
|        9 | 10615 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10616 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10617 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10618 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10619 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10620 | `			);` |
|      ! 0 | 10621 | `	}else{` |
|       13 | 10622 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10623 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10624 | `	}` |
|        9 | 10625 | `	sVar.zString = pAux->zWorker;` |
|        - | 10626 | `	/* Try to extract the variable */` |
|        9 | 10627 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10628 | `	if( pObj ){` |
|        - | 10629 | `		/* Collision */` |
|        3 | 10630 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10631 | `			return SXRET_OK;` |
|        - | 10632 | `		}` |
|        3 | 10633 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10634 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10635 | `				/* Already prefixed */` |
|      ! 0 | 10636 | `				return SXRET_OK;` |
|        - | 10637 | `			}` |
|      ! 0 | 10638 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10639 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10640 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10641 | `				);` |
|      ! 0 | 10642 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10643 | `		}` |
|        2 | 10644 | `	}else{` |
|        - | 10645 | `		/* Create the variable */` |
|        7 | 10646 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10647 | `	}` |
|        9 | 10648 | `	if( pObj ){` |
|        - | 10649 | `		/* Overwrite the old value */` |
|        9 | 10650 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10651 | `		/* Increment counter */` |
|        9 | 10652 | `		pAux->iCount++;` |
|        4 | 10653 | `	}` |
|        9 | 10654 | `	return SXRET_OK;` |
|        5 | 10655 |  |
|        - | 10656 | `/*` |
|        - | 10657 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10658 | ` * defined below.` |
|        - | 10659 | ` */` |
|        2 | 10660 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10661 |  |
|        3 | 10662 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10663 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10664 | `	ph7_value *pObj;` |
|        - | 10665 | `	SyString sVar;` |
|        - | 10666 | `	/* Perform a string cast */` |
|        3 | 10667 | `	PH7_MemObjToString(pKey);` |
|        3 | 10668 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10669 | `		/* Unavailable variable name */` |
|      ! 0 | 10670 | `		return SXRET_OK;` |
|        - | 10671 | `	}` |
|        3 | 10672 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10673 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10674 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10675 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10676 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10677 | `			);` |
|        2 | 10678 | `	}else{` |
|      ! 0 | 10679 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10680 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10681 | `	}` |
|        3 | 10682 | `	sVar.zString = pAux->zWorker;` |
|        - | 10683 | `	/* Extract the variable */` |
|        3 | 10684 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10685 | `	if( pObj ){` |
|        3 | 10686 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10687 | `	}` |
|        3 | 10688 | `	return SXRET_OK;` |
|        2 | 10689 |  |
|        - | 10690 | `/*` |
|        - | 10691 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10692 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10693 | ` * Parameters` |
|        - | 10694 | ` * $types` |
|        - | 10695 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10696 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10697 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10698 | ` *  POST includes the POST uploaded file information.` |
|        - | 10699 | ` *  Note:` |
|        - | 10700 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10701 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10702 | ` * $prefix` |
|        - | 10703 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10704 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10705 | ` *  variable named $pref_userid.` |
|        - | 10706 | ` * Return` |
|        - | 10707 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10708 | ` */` |
|        2 | 10709 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10710 |  |
|        - | 10711 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10712 | `	extract_aux_data sAux;` |
|        - | 10713 | `	int nLen,nPrefixLen;` |
|        - | 10714 | `	ph7_value *pSuper;` |
|        - | 10715 | `	ph7_vm *pVm;` |
|        - | 10716 | `	/* By default import only $_GET variables  */` |
|        3 | 10717 | `	zImport = "G";` |
|        3 | 10718 | `	nLen = (int)sizeof(char);` |
|        3 | 10719 | `	zPrefix = 0;` |
|        3 | 10720 | `	nPrefixLen = 0;` |
|        3 | 10721 | `	if( nArg > 0 ){` |
|        3 | 10722 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10723 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10724 | `		}` |
|        3 | 10725 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10726 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10727 | `		}` |
|        1 | 10728 | `	}` |
|        - | 10729 | `	/* Point to the underlying VM */` |
|        3 | 10730 | `	pVm = pCtx->pVm;` |
|        - | 10731 | `	/* Initialize the aux data */` |
|        3 | 10732 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10733 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10734 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10735 | `	sAux.pVm = pVm;` |
|        - | 10736 | `	/* Extract */` |
|        3 | 10737 | `	zEnd = &zImport[nLen];` |
|        5 | 10738 | `	while( zImport < zEnd ){` |
|        3 | 10739 | `		int c = zImport[0];` |
|        3 | 10740 | `		pSuper = 0;` |
|        3 | 10741 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10742 | `			/* Import $_GET variables */` |
|        3 | 10743 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10744 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10745 | `			/* Import $_POST variables */` |
|      ! 0 | 10746 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10747 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10748 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10749 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10750 | `		}` |
|        3 | 10751 | `		if( pSuper ){` |
|        - | 10752 | `			/* Iterate throw array entries */` |
|        3 | 10753 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10754 | `		}` |
|        - | 10755 | `		/* Advance the cursor */` |
|        3 | 10756 | `		zImport++;` |
|        1 | 10757 | `	}` |
|        - | 10758 | `	/* All done,return TRUE*/` |
|        3 | 10759 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10760 | `	return PH7_OK;` |
|        1 | 10761 |  |
|        - | 10762 | `/*` |
|        - | 10763 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10764 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10765 | ` * information.` |
|        - | 10766 | ` */` |
|     7750 | 10767 | `static sxi32 VmEvalChunk(` |
|        - | 10768 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10769 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10770 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10771 | `	int iFlags,         /* Compile flag */` |
|        - | 10772 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10773 | `	)` |
|        2 | 10774 |  |
|        - | 10775 | `	SySet *pByteCode,aByteCode;` |
|     7752 | 10776 | `	ProcConsumer xErr = 0;` |
|     7752 | 10777 | `	void *pErrData = 0;` |
|        - | 10778 | `	/* Initialize bytecode container */` |
|     7752 | 10779 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     7752 | 10780 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10781 | `	/* Reset the code generator */` |
|     7752 | 10782 | `	if( bTrueReturn ){` |
|        - | 10783 | `		/* Included file,log compile-time errors */` |
|     6499 | 10784 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6499 | 10785 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3249 | 10786 | `	}` |
|     7752 | 10787 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10788 | `	/* Swap bytecode container */` |
|     7752 | 10789 | `	pByteCode = pVm->pByteContainer;` |
|     7752 | 10790 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10791 | `	/* Compile the chunk */` |
|     7752 | 10792 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    11627 | 10793 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10794 | `		/* Compilation error,return false */` |
|        3 | 10795 | `		if( pCtx ){` |
|        3 | 10796 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10797 | `		}` |
|        2 | 10798 | `	}else{` |
|        - | 10799 | `		/* Mount any newly defined classes */` |
|        - | 10800 | `		SyHashEntry *pEntry;` |
|        - | 10801 | `		ph7_class *pClass;` |
|        - | 10802 | `		ph7_value sResult; /* Return value */` |
|        - | 10803 | `		sxi32 rc;` |
|     7750 | 10804 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   214700 | 10805 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   203078 | 10806 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10807 | `			/* Only mount classes that haven't been mounted yet */` |
|   203078 | 10808 | `			if( !pClass->bMounted ){` |
|    41224 | 10809 | `				rc = VmMountUserClass(pVm,pClass);` |
|    41224 | 10810 | `				if( rc != SXRET_OK ){` |
|        - | 10811 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10812 | `					if( pCtx ){` |
|      ! 0 | 10813 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10814 | `					}` |
|      ! 0 | 10815 | `					goto Cleanup;` |
|        - | 10816 | `				}` |
|    20611 | 10817 | `			}` |
|        2 | 10818 | `		}` |
|     7750 | 10819 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10820 | `			/* Out of memory */` |
|      ! 0 | 10821 | `			if( pCtx ){` |
|      ! 0 | 10822 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10823 | `			}` |
|      ! 0 | 10824 | `			goto Cleanup;` |
|        - | 10825 | `		}` |
|     7750 | 10826 | `		if( bTrueReturn ){` |
|        - | 10827 | `			/* Assume a boolean true return value */` |
|     6499 | 10828 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3250 | 10829 | `		}else{` |
|        - | 10830 | `			/* Assume a null return value */` |
|     1252 | 10831 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10832 | `		}` |
|        - | 10833 | `		/* Execute the compiled chunk */` |
|     7750 | 10834 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     7750 | 10835 | `		if( pCtx ){` |
|        - | 10836 | `			/* Set the execution result */` |
|     6516 | 10837 | `			ph7_result_value(pCtx,&sResult);` |
|     3257 | 10838 | `		}` |
|     7750 | 10839 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10840 | `	}` |
|     3875 | 10841 | `Cleanup:` |
|        - | 10842 | `	/* Cleanup the mess left behind */` |
|     7752 | 10843 | `	pVm->pByteContainer = pByteCode;` |
|     7752 | 10844 | `	SySetRelease(&aByteCode);` |
|     7752 | 10845 | `	return SXRET_OK;` |
|        2 | 10846 |  |
|        - | 10847 | `/*` |
|        - | 10848 | ` * value eval(string $code)` |
|        - | 10849 | ` *   Evaluate a string as PHP code.` |
|        - | 10850 | ` * Parameter` |
|        - | 10851 | ` *  code: PHP code to evaluate.` |
|        - | 10852 | ` * Return` |
|        - | 10853 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10854 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10855 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10856 | ` */` |
|       16 | 10857 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10858 |  |
|        - | 10859 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10860 | `	if( nArg < 1 ){` |
|        - | 10861 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10862 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10863 | `		return SXRET_OK;` |
|        - | 10864 | `	}` |
|        - | 10865 | `	/* Chunk to evaluate */` |
|       18 | 10866 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10867 | `	if( sChunk.nByte < 1 ){` |
|        - | 10868 | `		/* Empty string,return NULL */` |
|        3 | 10869 | `		ph7_result_null(pCtx);` |
|        3 | 10870 | `		return SXRET_OK;` |
|        - | 10871 | `	}` |
|        - | 10872 | `	/* Eval the chunk */` |
|       16 | 10873 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10874 | `	return SXRET_OK;` |
|       10 | 10875 |  |
|        - | 10876 | `/*` |
|        - | 10877 | ` * Check if a file path is already included.` |
|        - | 10878 | ` */` |
|    12992 | 10879 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10880 |  |
|        - | 10881 | `	SyString *aEntries;` |
|        - | 10882 | `	sxu32 n;` |
|    12993 | 10883 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10884 | `	/* Perform a linear search */` |
| 42186779 | 10885 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 42173793 | 10886 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10887 | `			/* Already included */` |
|        7 | 10888 | `			return TRUE;` |
|        - | 10889 | `		}` |
| 21086894 | 10890 | `	}` |
|    12987 | 10891 | `	return FALSE;` |
|     6497 | 10892 |  |
|        - | 10893 | `/*` |
|        - | 10894 | ` * Push a file path in the appropriate VM container.` |
|        - | 10895 | ` */` |
|    14218 | 10896 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10897 |  |
|        - | 10898 | `	SyString sPath;` |
|        - | 10899 | `	char *zDup;` |
|        - | 10900 | `#ifdef __WINNT__` |
|        - | 10901 | `	char *zCur;` |
|        - | 10902 | `#endif` |
|        - | 10903 | `	sxi32 rc;` |
|    14220 | 10904 | `	if( nLen < 0 ){` |
|     1228 | 10905 | `		nLen = SyStrlen(zPath);` |
|      613 | 10906 | `	}` |
|        - | 10907 | `	/* Duplicate the file path first */` |
|    14220 | 10908 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14220 | 10909 | `	if( zDup == 0 ){` |
|      ! 0 | 10910 | `		return SXERR_MEM;` |
|        - | 10911 | `	}` |
|        - | 10912 | `#ifdef __WINNT__` |
|        - | 10913 | `	/* Normalize path on windows` |
|        - | 10914 | `	 * Example:` |
|        - | 10915 | `	 *    Path/To/File.php` |
|        - | 10916 | `	 * becomes` |
|        - | 10917 | `	 *   path\to\file.php` |
|        - | 10918 | `	 */` |
|        2 | 10919 | `	zCur = zDup;` |
|        2 | 10920 | `	while( zCur[0] != 0 ){` |
|        2 | 10921 | `		if( zCur[0] == '/' ){` |
|        2 | 10922 | `			zCur[0] = '\\';` |
|        2 | 10923 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10924 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10925 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10926 | `		}` |
|        2 | 10927 | `		zCur++;` |
|        2 | 10928 | `	}` |
|        - | 10929 | `#endif` |
|        - | 10930 | `	/* Install the file path */` |
|    14220 | 10931 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14220 | 10932 | `	if( !bMain ){` |
|    12993 | 10933 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10934 | `			/* Already included */` |
|        7 | 10935 | `			*pNew = 0;` |
|        4 | 10936 | `		}else{` |
|        - | 10937 | `			/* Insert in the corresponding container */` |
|    12987 | 10938 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    12987 | 10939 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10940 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10941 | `				return rc;` |
|        - | 10942 | `			}` |
|    12987 | 10943 | `			*pNew = 1;` |
|        - | 10944 | `		}` |
|     6496 | 10945 | `	}` |
|    14220 | 10946 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14220 | 10947 | `	return SXRET_OK;` |
|     7111 | 10948 |  |
|        - | 10949 | `/*` |
|        - | 10950 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10951 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10952 | ` * indicates failure.` |
|        - | 10953 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10954 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10955 | ` * operations.` |
|        - | 10956 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10957 | ` * this function is a no-op.` |
|        - | 10958 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10959 | ` * constructs for more information.` |
|        - | 10960 | ` */` |
|     6504 | 10961 | `static sxi32 VmExecIncludedFile(` |
|        - | 10962 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10963 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10964 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10965 | `	 )` |
|        2 | 10966 |  |
|        - | 10967 | `	sxi32 rc;` |
|        - | 10968 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10969 | `	const ph7_io_stream *pStream;` |
|        - | 10970 | `	SyBlob sContents;` |
|        - | 10971 | `	void *pHandle;` |
|        - | 10972 | `	ph7_vm *pVm;` |
|        - | 10973 | `	int isNew;` |
|        - | 10974 | `	/* Initialize fields */` |
|     6506 | 10975 | `	pVm = pCtx->pVm;` |
|     6506 | 10976 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6506 | 10977 | `	isNew = 0;` |
|        - | 10978 | `	/* Extract the associated stream */` |
|     6506 | 10979 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10980 | `	/*` |
|        - | 10981 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10982 | `	 * in a read-only mode.` |
|        - | 10983 | `	 */` |
|     6506 | 10984 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6506 | 10985 | `	if( pHandle == 0 ){` |
|        3 | 10986 | `		return SXERR_IO;` |
|        - | 10987 | `	}` |
|     6503 | 10988 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6503 | 10989 | `	if( IncludeOnce && !isNew ){` |
|        - | 10990 | `		/* Already included */` |
|        5 | 10991 | `		rc = SXERR_EXISTS;` |
|        3 | 10992 | `	}else{` |
|        - | 10993 | `		/* Read the whole file contents */` |
|     6499 | 10994 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6499 | 10995 | `		if( rc == SXRET_OK ){` |
|        - | 10996 | `			SyString sScript;` |
|        - | 10997 | `			/* Compile and execute the script */` |
|     6499 | 10998 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6499 | 10999 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3249 | 11000 | `		}` |
|        - | 11001 | `	}` |
|        - | 11002 | `	/* Pop from the set of included file */` |
|     6503 | 11003 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11004 | `	/* Close the handle */` |
|     6503 | 11005 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11006 | `	/* Release the working buffer */` |
|     6503 | 11007 | `	SyBlobRelease(&sContents);` |
|        - | 11008 | `#else` |
|        - | 11009 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11010 | `	SXUNUSED(pPath);` |
|        - | 11011 | `	SXUNUSED(IncludeOnce);` |
|        - | 11012 | `	rc = SXERR_IO;` |
|        - | 11013 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6503 | 11014 | `	return rc;` |
|     3254 | 11015 |  |
|        - | 11016 | `/*` |
|        - | 11017 | ` * string get_include_path(void)` |
|        - | 11018 | ` *  Gets the current include_path configuration option.` |
|        - | 11019 | ` * Parameter` |
|        - | 11020 | ` *  None` |
|        - | 11021 | ` * Return` |
|        - | 11022 | ` *  Included paths as a string` |
|        - | 11023 | ` */` |
|        2 | 11024 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11025 |  |
|        3 | 11026 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11027 | `	SyString *aEntry;` |
|        - | 11028 | `	int dir_sep;` |
|        - | 11029 | `	sxu32 n;` |
|        - | 11030 | `#ifdef __WINNT__` |
|        1 | 11031 | `	dir_sep = ';';` |
|        - | 11032 | `#else` |
|        - | 11033 | `	/* Assume UNIX path separator */` |
|        2 | 11034 | `	dir_sep = ':';` |
|        - | 11035 | `#endif` |
|        1 | 11036 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11037 | `	SXUNUSED(apArg);` |
|        - | 11038 | `	/* Point to the list of import paths */` |
|        3 | 11039 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11040 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11041 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11042 | `		if( n > 0 ){` |
|        - | 11043 | `			/* Append dir seprator */` |
|      ! 0 | 11044 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11045 | `		}` |
|        - | 11046 | `		/* Append path */` |
|        3 | 11047 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11048 | `	}` |
|        3 | 11049 | `	return PH7_OK;` |
|        1 | 11050 |  |
|        - | 11051 | `/*` |
|        - | 11052 | ` * string get_get_included_files(void)` |
|        - | 11053 | ` *  Gets the current include_path configuration option.` |
|        - | 11054 | ` * Parameter` |
|        - | 11055 | ` *  None` |
|        - | 11056 | ` * Return` |
|        - | 11057 | ` *  Included paths as a string` |
|        - | 11058 | ` */` |
|        2 | 11059 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11060 |  |
|        3 | 11061 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11062 | `	ph7_value *pArray,*pWorker;` |
|        - | 11063 | `	SyString *pEntry;` |
|        - | 11064 | `	int c,d;` |
|        - | 11065 | `	/* Create an array and a working value */` |
|        3 | 11066 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11067 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11068 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11069 | `		/* Out of memory,return null */` |
|      ! 0 | 11070 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11071 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11072 | `		SXUNUSED(apArg);` |
|      ! 0 | 11073 | `		return PH7_OK;` |
|        - | 11074 | `	}` |
|        3 | 11075 | `	c = d = '/';` |
|        - | 11076 | `#ifdef __WINNT__` |
|        1 | 11077 | `	d = '\\';` |
|        - | 11078 | `#endif` |
|        - | 11079 | `	/* Iterate throw entries */` |
|        3 | 11080 | `	SySetResetCursor(pFiles);` |
|     2709 | 11081 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11082 | `		const char *zBase,*zEnd;` |
|        - | 11083 | `		int iLen;` |
|        - | 11084 | `		/* reset the string cursor */` |
|     2707 | 11085 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11086 | `		/* Extract base name */` |
|     2707 | 11087 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11088 | `		/* Ignore trailing '/' */` |
|     4060 | 11089 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11090 | `			zEnd--;` |
|      ! 0 | 11091 | `		}` |
|     2707 | 11092 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    75890 | 11093 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    71831 | 11094 | `			zEnd--;` |
|        1 | 11095 | `		}` |
|     2707 | 11096 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     2707 | 11097 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11098 | `		/* Copy entry name */` |
|     2707 | 11099 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11100 | `		/* Perform the insertion */` |
|     2707 | 11101 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11102 | `	}` |
|        - | 11103 | `	/* All done,return the created array */` |
|        3 | 11104 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11105 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11106 | `	 * by the engine as soon we return from this foreign` |
|        - | 11107 | `	 * function.` |
|        - | 11108 | `	 */` |
|        3 | 11109 | `	return PH7_OK;` |
|        2 | 11110 |  |
|        - | 11111 | `/*` |
|        - | 11112 | ` * include:` |
|        - | 11113 | ` * According to the PHP reference manual.` |
|        - | 11114 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11115 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11116 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11117 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11118 | ` *  and the current working directory before failing. The include()` |
|        - | 11119 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11120 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11121 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11122 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11123 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11124 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11125 | ` *  directory to find the requested file.` |
|        - | 11126 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11127 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11128 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11129 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11130 | ` */` |
|     6492 | 11131 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11132 |  |
|        - | 11133 | `	SyString sFile;` |
|        - | 11134 | `	sxi32 rc;` |
|     6494 | 11135 | `	if( nArg < 1 ){` |
|        - | 11136 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11137 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11138 | `		return SXRET_OK;` |
|        - | 11139 | `	}` |
|        - | 11140 | `	/* File to include */` |
|     6494 | 11141 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6494 | 11142 | `	if( sFile.nByte < 1 ){` |
|        - | 11143 | `		/* Empty string,return NULL */` |
|      ! 0 | 11144 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11145 | `		return SXRET_OK;` |
|        - | 11146 | `	}` |
|        - | 11147 | `	/* Open,compile and execute the desired script */` |
|     6494 | 11148 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6494 | 11149 | `	if( rc != SXRET_OK ){` |
|        - | 11150 | `		/* Emit a warning and return false */` |
|        3 | 11151 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11152 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11153 | `	}` |
|     6494 | 11154 | `	return SXRET_OK;` |
|     3248 | 11155 |  |
|        - | 11156 | `/*` |
|        - | 11157 | ` * include_once:` |
|        - | 11158 | ` *  According to the PHP reference manual.` |
|        - | 11159 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11160 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11161 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11162 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11163 | ` *   just once.` |
|        - | 11164 | ` */` |
|        4 | 11165 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11166 |  |
|        - | 11167 | `	SyString sFile;` |
|        - | 11168 | `	sxi32 rc;` |
|        5 | 11169 | `	if( nArg < 1 ){` |
|        - | 11170 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11171 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11172 | `		return SXRET_OK;` |
|        - | 11173 | `	}` |
|        - | 11174 | `	/* File to include */` |
|        5 | 11175 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11176 | `	if( sFile.nByte < 1 ){` |
|        - | 11177 | `		/* Empty string,return NULL */` |
|      ! 0 | 11178 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11179 | `		return SXRET_OK;` |
|        - | 11180 | `	}` |
|        - | 11181 | `	/* Open,compile and execute the desired script */` |
|        5 | 11182 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11183 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11184 | `		/* File already included,return TRUE */` |
|        3 | 11185 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11186 | `		return SXRET_OK;` |
|        - | 11187 | `	}` |
|        3 | 11188 | `	if( rc != SXRET_OK ){` |
|        - | 11189 | `		/* Emit a warning and return false */` |
|      ! 0 | 11190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11191 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11192 | ` 	}` |
|        3 | 11193 | `	return SXRET_OK;` |
|        3 | 11194 |  |
|        - | 11195 | `/*` |
|        - | 11196 | ` * require.` |
|        - | 11197 | ` *  According to the PHP reference manual.` |
|        - | 11198 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11199 | ` *   also produce a fatal level error.` |
|        - | 11200 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11201 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11202 | ` */` |
|        4 | 11203 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11204 |  |
|        - | 11205 | `	SyString sFile;` |
|        - | 11206 | `	sxi32 rc;` |
|        5 | 11207 | `	if( nArg < 1 ){` |
|        - | 11208 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11209 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11210 | `		return SXRET_OK;` |
|        - | 11211 | `	}` |
|        - | 11212 | `	/* File to include */` |
|        5 | 11213 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11214 | `	if( sFile.nByte < 1 ){` |
|        - | 11215 | `		/* Empty string,return NULL */` |
|      ! 0 | 11216 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11217 | `		return SXRET_OK;` |
|        - | 11218 | `	}` |
|        - | 11219 | `	/* Open,compile and execute the desired script */` |
|        5 | 11220 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11221 | `	if( rc != SXRET_OK ){` |
|        - | 11222 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11223 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11224 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11225 | `		return PH7_ABORT;` |
|        - | 11226 | `	}` |
|        5 | 11227 | `	return SXRET_OK;` |
|        3 | 11228 |  |
|        - | 11229 | `/*` |
|        - | 11230 | ` * require_once:` |
|        - | 11231 | ` *  According to the PHP reference manual.` |
|        - | 11232 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11233 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11234 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11235 | ` *   and how it differs from its non _once siblings.` |
|        - | 11236 | ` */` |
|        4 | 11237 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11238 |  |
|        - | 11239 | `	SyString sFile;` |
|        - | 11240 | `	sxi32 rc;` |
|        5 | 11241 | `	if( nArg < 1 ){` |
|        - | 11242 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11243 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11244 | `		return SXRET_OK;` |
|        - | 11245 | `	}` |
|        - | 11246 | `	/* File to include */` |
|        5 | 11247 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11248 | `	if( sFile.nByte < 1 ){` |
|        - | 11249 | `		/* Empty string,return NULL */` |
|      ! 0 | 11250 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11251 | `		return SXRET_OK;` |
|        - | 11252 | `	}` |
|        - | 11253 | `	/* Open,compile and execute the desired script */` |
|        5 | 11254 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11255 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11256 | `		/* File already included,return TRUE */` |
|        3 | 11257 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11258 | `		return SXRET_OK;` |
|        - | 11259 | `	}` |
|        3 | 11260 | `	if( rc != SXRET_OK ){` |
|        - | 11261 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11262 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11263 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11264 | `		return PH7_ABORT;` |
|        - | 11265 | `	}` |
|        3 | 11266 | `	return SXRET_OK;` |
|        3 | 11267 |  |
|        - | 11268 | `/*` |
|        - | 11269 | ` * Section:` |
|        - | 11270 | ` *  Command line arguments processing.` |
|        - | 11271 | ` * Status:` |
|        - | 11272 | ` *    Stable.` |
|        - | 11273 | ` */` |
|        - | 11274 | `/*` |
|        - | 11275 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11276 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11277 | ` * NULL otherwise.` |
|        - | 11278 | ` */` |
|        6 | 11279 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11280 |  |
|      199 | 11281 | `	while( zIn < zEnd ){` |
|      193 | 11282 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11283 | `			/* Got one */` |
|      ! 0 | 11284 | `			return &zIn[1];` |
|        - | 11285 | `		}` |
|        - | 11286 | `		/* Advance the cursor */` |
|      193 | 11287 | `		zIn++;` |
|        1 | 11288 | `	}` |
|        - | 11289 | `	/* No such option */` |
|        7 | 11290 | `	return 0;` |
|        4 | 11291 |  |
|        - | 11292 | `/*` |
|        - | 11293 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11294 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11295 | ` * NULL otherwise.` |
|        - | 11296 | ` */` |
|      ! 0 | 11297 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11298 |  |
|        - | 11299 | `	const char *zOpt;` |
|      ! 0 | 11300 | `	while( zIn < zEnd ){` |
|      ! 0 | 11301 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11302 | `			zIn += 2;` |
|      ! 0 | 11303 | `			zOpt = zIn;` |
|      ! 0 | 11304 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11305 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11306 | `					break;` |
|        - | 11307 | `				}` |
|      ! 0 | 11308 | `				zIn++;` |
|      ! 0 | 11309 | `			}` |
|        - | 11310 | `			/* Test */` |
|      ! 0 | 11311 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11312 | `				/* Got one,return it's value */` |
|      ! 0 | 11313 | `				return zIn;` |
|        - | 11314 | `			}` |
|        - | 11315 |  |
|      ! 0 | 11316 | `		}else{` |
|      ! 0 | 11317 | `			zIn++;` |
|        - | 11318 | `		}` |
|      ! 0 | 11319 | `	}` |
|        - | 11320 | `	/* No such option */` |
|      ! 0 | 11321 | `	return 0;` |
|      ! 0 | 11322 |  |
|        - | 11323 | `/*` |
|        - | 11324 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11325 | ` */` |
|        - | 11326 | `struct getopt_long_opt` |
|        - | 11327 |  |
|        - | 11328 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11329 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11330 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11331 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11332 | `};` |
|        - | 11333 | `/* Forward declaration */` |
|        - | 11334 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11335 | `/*` |
|        - | 11336 | ` * Extract short or long argument option values.` |
|        - | 11337 | ` */` |
|      ! 0 | 11338 | `static void VmExtractOptArgValue(` |
|        - | 11339 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11340 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11341 | `	const char *zArg,   /* Argument stream */` |
|        - | 11342 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11343 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11344 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11345 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11346 |  |
|      ! 0 | 11347 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11348 | `	if( !need_val ){` |
|        - | 11349 | `		/*` |
|        - | 11350 | `		 * Option does not need arguments.` |
|        - | 11351 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11352 | `		 */` |
|      ! 0 | 11353 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11354 | `	}else{` |
|        - | 11355 | `		const char *zCur;` |
|        - | 11356 | `		/* Extract option argument */` |
|      ! 0 | 11357 | `		zArg++;` |
|      ! 0 | 11358 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11359 | `			zArg++;` |
|      ! 0 | 11360 | `		}` |
|      ! 0 | 11361 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11362 | `			zArg++;` |
|      ! 0 | 11363 | `		}` |
|      ! 0 | 11364 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11365 | `			/*` |
|        - | 11366 | `			 * Argument not found.` |
|        - | 11367 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11368 | `			 */` |
|      ! 0 | 11369 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11370 | `			return;` |
|        - | 11371 | `		}` |
|        - | 11372 | `		/* Delimit the value */` |
|      ! 0 | 11373 | `		zCur = zArg;` |
|      ! 0 | 11374 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11375 | `			int d = zArg[0];` |
|        - | 11376 | `			/* Delimt the argument */` |
|      ! 0 | 11377 | `			zArg++;` |
|      ! 0 | 11378 | `			zCur = zArg;` |
|      ! 0 | 11379 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11380 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11381 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11382 | `					break;` |
|        - | 11383 | `				}` |
|      ! 0 | 11384 | `				zArg++;` |
|      ! 0 | 11385 | `			}` |
|        - | 11386 | `			/* Save the value */` |
|      ! 0 | 11387 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11388 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11389 | `		}else{` |
|      ! 0 | 11390 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11391 | `				zArg++;` |
|      ! 0 | 11392 | `			}` |
|        - | 11393 | `			/* Save the value */` |
|      ! 0 | 11394 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11395 | `		}` |
|        - | 11396 | `		/*` |
|        - | 11397 | `		 * Check if we are dealing with multiple values.` |
|        - | 11398 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11399 | `		 */` |
|      ! 0 | 11400 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11401 | `			zArg++;` |
|      ! 0 | 11402 | `		}` |
|      ! 0 | 11403 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11404 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11405 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11406 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11407 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11408 | `			}else{` |
|        - | 11409 | `				/* Insert the first value */` |
|      ! 0 | 11410 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11411 | `				for(;;){` |
|      ! 0 | 11412 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11413 | `						/* No more value */` |
|      ! 0 | 11414 | `						break;` |
|        - | 11415 | `					}` |
|        - | 11416 | `					/* Delimit the value */` |
|      ! 0 | 11417 | `					zCur = zArg;` |
|      ! 0 | 11418 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11419 | `						zArg++;` |
|      ! 0 | 11420 | `						zCur = zArg;` |
|      ! 0 | 11421 | `					}` |
|      ! 0 | 11422 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11423 | `						zArg++;` |
|      ! 0 | 11424 | `					}` |
|        - | 11425 | `					/* Reset the string cursor */` |
|      ! 0 | 11426 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11427 | `					/* Save the value */` |
|      ! 0 | 11428 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11429 | `					/* Insert */` |
|      ! 0 | 11430 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11431 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11432 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11433 | `						zArg++;` |
|      ! 0 | 11434 | `					}` |
|      ! 0 | 11435 | `				}` |
|        - | 11436 | `				/* Insert the option arg array */` |
|      ! 0 | 11437 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11438 | `				/* Safely release */` |
|      ! 0 | 11439 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11440 | `			}` |
|      ! 0 | 11441 | `		}else{` |
|        - | 11442 | `			/* Single value */` |
|      ! 0 | 11443 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11444 | `		}` |
|        - | 11445 | `	}` |
|      ! 0 | 11446 |  |
|        - | 11447 | `/*` |
|        - | 11448 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11449 | ` *   Gets options from the command line argument list.` |
|        - | 11450 | ` * Parameters` |
|        - | 11451 | ` *  $options` |
|        - | 11452 | ` *   Each character in this string will be used as option characters` |
|        - | 11453 | ` *   and matched against options passed to the script starting with` |
|        - | 11454 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11455 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11456 | ` *  $longopts` |
|        - | 11457 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11458 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11459 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11460 | ` *   option --opt.` |
|        - | 11461 | ` * Return` |
|        - | 11462 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11463 | ` *  on failure.` |
|        - | 11464 | ` */` |
|        2 | 11465 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11466 |  |
|        - | 11467 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11468 | `	struct getopt_long_opt sLong;` |
|        - | 11469 | `	ph7_value *pArray,*pWorker;` |
|        - | 11470 | `	SyBlob *pArg;` |
|        - | 11471 | `	int nByte;` |
|        3 | 11472 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11473 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11474 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11476 | `		return PH7_OK;` |
|        - | 11477 | `	}` |
|        - | 11478 | `	/* Extract option arguments */` |
|        3 | 11479 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11480 | `	zEnd = &zIn[nByte];` |
|        - | 11481 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11482 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11483 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11484 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11485 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11486 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11487 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11488 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11489 | `		return PH7_OK;` |
|        - | 11490 | `	}` |
|        3 | 11491 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11492 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11493 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11494 | `		/* Everything will be released automatically when we return` |
|        - | 11495 | `		 * from this function.` |
|        - | 11496 | `		 */` |
|      ! 0 | 11497 | `		return PH7_OK;` |
|        - | 11498 | `	}` |
|        3 | 11499 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11500 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11501 | `	/* Fill the long option structure */` |
|        3 | 11502 | `	sLong.pArray = pArray;` |
|        3 | 11503 | `	sLong.pWorker = pWorker;` |
|        3 | 11504 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11505 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11506 | `	sLong.pCtx = pCtx;` |
|        - | 11507 | `	/* Start processing */` |
|        9 | 11508 | `	while( zIn < zEnd ){` |
|        7 | 11509 | `		int c = zIn[0];` |
|        7 | 11510 | `		int need_val = 0;` |
|        - | 11511 | `		/* Advance the stream cursor */` |
|        7 | 11512 | `		zIn++;` |
|        - | 11513 | `		/* Ignore non-alphanum characters */` |
|        7 | 11514 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11515 | `			continue;` |
|        - | 11516 | `		}` |
|        7 | 11517 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11518 | `			zIn++;` |
|        5 | 11519 | `			need_val = 1;` |
|        5 | 11520 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11521 | `				zIn++;` |
|      ! 0 | 11522 | `			}` |
|        2 | 11523 | `		}` |
|        - | 11524 | `		/* Find option */` |
|        7 | 11525 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11526 | `		if( zArg == 0 ){` |
|        - | 11527 | `			/* No such option */` |
|        7 | 11528 | `			continue;` |
|        - | 11529 | `		}` |
|        - | 11530 | `		/* Extract option argument value */` |
|      ! 0 | 11531 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11532 | `	}` |
|        3 | 11533 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11534 | `		/* Process long options */` |
|      ! 0 | 11535 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11536 | `	}` |
|        - | 11537 | `	/* Return the option array */` |
|        3 | 11538 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11539 | `	/*` |
|        - | 11540 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11541 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11542 | `	 */` |
|        3 | 11543 | `	return PH7_OK;` |
|        2 | 11544 |  |
|        - | 11545 | `/*` |
|        - | 11546 | ` * Array walker callback used for processing long options values.` |
|        - | 11547 | ` */` |
|      ! 0 | 11548 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11549 |  |
|      ! 0 | 11550 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11551 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11552 | `	int need_value = 0;` |
|        - | 11553 | `	int nByte;` |
|        - | 11554 | `	/* Value must be of type string */` |
|      ! 0 | 11555 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11556 | `		/* Simply ignore */` |
|      ! 0 | 11557 | `		return PH7_OK;` |
|        - | 11558 | `	}` |
|      ! 0 | 11559 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11560 | `	if( nByte < 1 ){` |
|        - | 11561 | `		/* Empty string,ignore */` |
|      ! 0 | 11562 | `		return PH7_OK;` |
|        - | 11563 | `	}` |
|      ! 0 | 11564 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11565 | `	if( zEnd[0] == ':' ){` |
|        - | 11566 | `		char *zTerm;` |
|        - | 11567 | `		/* Try to extract a value */` |
|      ! 0 | 11568 | `		need_value = 1;` |
|      ! 0 | 11569 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11570 | `			zEnd--;` |
|      ! 0 | 11571 | `		}` |
|      ! 0 | 11572 | `		if( zOpt >= zEnd ){` |
|        - | 11573 | `			/* Empty string,ignore */` |
|      ! 0 | 11574 | `			SXUNUSED(pKey);` |
|      ! 0 | 11575 | `			return PH7_OK;` |
|        - | 11576 | `		}` |
|      ! 0 | 11577 | `		zEnd++;` |
|      ! 0 | 11578 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11579 | `		zTerm[0] = 0;` |
|      ! 0 | 11580 | `	}else{` |
|      ! 0 | 11581 | `		zEnd = &zOpt[nByte];` |
|        - | 11582 | `	}` |
|        - | 11583 | `	/* Find the option */` |
|      ! 0 | 11584 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11585 | `	if( zArg == 0 ){` |
|        - | 11586 | `		/* No such option,return immediately */` |
|      ! 0 | 11587 | `		return PH7_OK;` |
|        - | 11588 | `	}` |
|        - | 11589 | `	/* Try to extract a value */` |
|      ! 0 | 11590 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11591 | `	return PH7_OK;` |
|      ! 0 | 11592 |  |
|        - | 11593 | `/*` |
|        - | 11594 | ` * Section:` |
|        - | 11595 | ` *  JSON encoding/decoding routines.` |
|        - | 11596 | ` * Status:` |
|        - | 11597 | ` *    Devel.` |
|        - | 11598 | ` */` |
|        - | 11599 | `/* Forward reference */` |
|        - | 11600 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11601 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11602 | `/*` |
|        - | 11603 | ` * JSON encoder state is stored in an instance` |
|        - | 11604 | ` * of the following structure.` |
|        - | 11605 | ` */` |
|        - | 11606 | `typedef struct json_private_data json_private_data;` |
|        - | 11607 | `struct json_private_data` |
|        - | 11608 |  |
|        - | 11609 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11610 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11611 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11612 | `	int nRecCount;     /* Recursion count */` |
|        - | 11613 | `};` |
|        - | 11614 | `/*` |
|        - | 11615 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11616 | ` * According to wikipedia` |
|        - | 11617 | ` * JSON's basic types are:` |
|        - | 11618 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11619 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11620 | ` *   Boolean (true or false)` |
|        - | 11621 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11622 | ` *    do not need to be of the same type)` |
|        - | 11623 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11624 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11625 | ` *     be distinct from each other)` |
|        - | 11626 | ` *   null (empty)` |
|        - | 11627 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11628 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11629 | ` */` |
|        8 | 11630 | `static sxi32 VmJsonEncode(` |
|        - | 11631 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11632 | `	json_private_data *pData /* Context data */` |
|        1 | 11633 | `	){` |
|        9 | 11634 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11635 | `		int iFlags = pData->iFlags;` |
|        - | 11636 | `		int nByte;` |
|        9 | 11637 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11638 | `			/* null */` |
|      ! 0 | 11639 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11640 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11641 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11642 | `			int iLen;` |
|        - | 11643 | `			/* true/false */` |
|      ! 0 | 11644 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11645 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11646 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11647 | `			const char *zNum;` |
|        - | 11648 | `			/* Get a string representation of the number */` |
|        7 | 11649 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11650 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11651 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11652 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11653 | `				const char *zNum;` |
|        - | 11654 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11655 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11656 | `				/* Get a string representation of the number */` |
|      ! 0 | 11657 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11658 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11659 | `			}else{` |
|        - | 11660 | `				const char *zIn,*zEnd;` |
|        - | 11661 | `				int c;` |
|        - | 11662 | `				/* Encode the string */` |
|      ! 0 | 11663 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11664 | `				zEnd = &zIn[nByte];` |
|        - | 11665 | `				/* Append the double quote */` |
|      ! 0 | 11666 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11667 | `				for(;;){` |
|      ! 0 | 11668 | `					if( zIn >= zEnd ){` |
|        - | 11669 | `						/* No more input to process */` |
|      ! 0 | 11670 | `						break;` |
|        - | 11671 | `					}` |
|      ! 0 | 11672 | `					c = zIn[0];` |
|        - | 11673 | `					/* Advance the stream cursor */` |
|      ! 0 | 11674 | `					zIn++;` |
|      ! 0 | 11675 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11676 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11677 | `						if( c == '<' ){` |
|      ! 0 | 11678 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11679 | `						}else{` |
|      ! 0 | 11680 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11681 | `						}` |
|      ! 0 | 11682 | `						continue;` |
|      ! 0 | 11683 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11684 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11685 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11686 | `						continue;` |
|      ! 0 | 11687 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11688 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11689 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11690 | `						continue;` |
|      ! 0 | 11691 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11692 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11693 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11694 | `						continue;` |
|        - | 11695 | `					}` |
|      ! 0 | 11696 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11697 | `						/* Unescape the character */` |
|      ! 0 | 11698 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11699 | `					}` |
|        - | 11700 | `					/* Append character verbatim */` |
|      ! 0 | 11701 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11702 | `				}` |
|        - | 11703 | `				/* Append the double quote */` |
|      ! 0 | 11704 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11705 | `			}` |
|        3 | 11706 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11707 | `			int c = '[',d = ']';` |
|        - | 11708 | `			/* Encode the array */` |
|        3 | 11709 | `			pData->isFirst = 1;` |
|        3 | 11710 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11711 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11712 | `				c = '{';` |
|      ! 0 | 11713 | `				d = '}';` |
|      ! 0 | 11714 | `			}` |
|        - | 11715 | `			/* Append the square bracket or curly braces */` |
|        3 | 11716 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11717 | `			/* Iterate throw array entries */` |
|        3 | 11718 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11719 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11720 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11721 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11722 | `			/* Encode the class instance */` |
|      ! 0 | 11723 | `			pData->isFirst = 1;` |
|        - | 11724 | `			/* Append the curly braces */` |
|      ! 0 | 11725 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11726 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11727 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11728 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11729 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11730 | `		}else{` |
|        - | 11731 | `			/* Can't happen */` |
|      ! 0 | 11732 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11733 | `		}` |
|        - | 11734 | `		/* All done */` |
|        9 | 11735 | `		return PH7_OK;` |
|        1 | 11736 |  |
|        - | 11737 | `/*` |
|        - | 11738 | ` * The following walker callback is invoked each time we need` |
|        - | 11739 | ` * to encode an array to JSON.` |
|        - | 11740 | ` */` |
|        6 | 11741 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11742 |  |
|        7 | 11743 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11744 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11745 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11746 | `		return PH7_OK;` |
|        - | 11747 | `	}` |
|        7 | 11748 | `	if( !pJson->isFirst ){` |
|        - | 11749 | `		/* Append the colon first */` |
|        5 | 11750 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11751 | `	}` |
|        7 | 11752 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11753 | `		/* Outputs an object rather than an array */` |
|        - | 11754 | `		const char *zKey;` |
|        - | 11755 | `		int nByte;` |
|        - | 11756 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11757 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11758 | `		/* Append the key and the double colon */` |
|      ! 0 | 11759 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11760 | `	}` |
|        - | 11761 | `	/* Encode the value */` |
|        7 | 11762 | `	pJson->nRecCount++;` |
|        7 | 11763 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11764 | `	pJson->nRecCount--;` |
|        7 | 11765 | `	pJson->isFirst = 0;` |
|        7 | 11766 | `	return PH7_OK;` |
|        4 | 11767 |  |
|        - | 11768 | `/*` |
|        - | 11769 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11770 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11771 | ` */` |
|      ! 0 | 11772 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11773 |  |
|      ! 0 | 11774 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11775 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11776 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11777 | `		return PH7_OK;` |
|        - | 11778 | `	}` |
|      ! 0 | 11779 | `	if( !pJson->isFirst ){` |
|        - | 11780 | `		/* Append the colon first */` |
|      ! 0 | 11781 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11782 | `	}` |
|        - | 11783 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11784 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11785 | `	/* Encode the value */` |
|      ! 0 | 11786 | `	pJson->nRecCount++;` |
|      ! 0 | 11787 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11788 | `	pJson->nRecCount--;` |
|      ! 0 | 11789 | `	pJson->isFirst = 0;` |
|      ! 0 | 11790 | `	return PH7_OK;` |
|      ! 0 | 11791 |  |
|        - | 11792 | `/*` |
|        - | 11793 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11794 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11795 | ` * Parameters` |
|        - | 11796 | ` *  $value` |
|        - | 11797 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11798 | ` * $options` |
|        - | 11799 | ` *  Bitmask consisting of:` |
|        - | 11800 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11801 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11802 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11803 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11804 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11805 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11806 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11807 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11808 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11809 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11810 | ` * Return` |
|        - | 11811 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11812 | ` */` |
|        2 | 11813 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11814 |  |
|        - | 11815 | `	json_private_data sJson;` |
|        3 | 11816 | `	if( nArg < 1 ){` |
|        - | 11817 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11818 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11819 | `		return PH7_OK;` |
|        - | 11820 | `	}` |
|        - | 11821 | `	/* Prepare the JSON data */` |
|        3 | 11822 | `	sJson.nRecCount = 0;` |
|        3 | 11823 | `	sJson.pCtx = pCtx;` |
|        3 | 11824 | `	sJson.isFirst = 1;` |
|        3 | 11825 | `	sJson.iFlags = 0;` |
|        3 | 11826 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11827 | `		/* Extract option flags */` |
|      ! 0 | 11828 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11829 | `	}` |
|        - | 11830 | `	/* Perform the encoding operation */` |
|        3 | 11831 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11832 | `	/* All done */` |
|        3 | 11833 | `	return PH7_OK;` |
|        2 | 11834 |  |
|        - | 11835 | `/*` |
|        - | 11836 | ` * int json_last_error(void)` |
|        - | 11837 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11838 | ` * Parameters` |
|        - | 11839 | ` *  None` |
|        - | 11840 | ` * Return` |
|        - | 11841 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11842 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11843 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11844 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11845 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11846 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11847 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11848 | ` */` |
|        8 | 11849 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11850 |  |
|       10 | 11851 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11852 | `	/* Return the error code */` |
|       10 | 11853 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11854 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11855 | `	SXUNUSED(apArg);` |
|       10 | 11856 | `	return PH7_OK;` |
|        2 | 11857 |  |
|        - | 11858 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11859 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11860 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11861 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11862 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11863 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11864 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11865 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11866 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11867 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11868 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11869 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11870 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11871 | `/*` |
|        - | 11872 | ` * Tokenize an entire JSON input.` |
|        - | 11873 | ` * Get a single low-level token from the input file.` |
|        - | 11874 | ` * Update the stream pointer so that it points to the first` |
|        - | 11875 | ` * character beyond the extracted token.` |
|        - | 11876 | ` */` |
|       60 | 11877 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11878 |  |
|       62 | 11879 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11880 | `	SyString *pStr;` |
|        - | 11881 | `	int c;` |
|        - | 11882 | `	/* Ignore leading white spaces */` |
|       66 | 11883 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11884 | `		/* Advance the stream cursor */` |
|        6 | 11885 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11886 | `			/* Update line counter */` |
|      ! 0 | 11887 | `			pStream->nLine++;` |
|      ! 0 | 11888 | `		}` |
|        6 | 11889 | `		pStream->zText++;` |
|        2 | 11890 | `	}` |
|       62 | 11891 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11892 | `		/* End of input reached */` |
|      ! 0 | 11893 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11894 | `		return SXERR_EOF;` |
|        - | 11895 | `	}` |
|        - | 11896 | `	/* Record token starting position and line */` |
|       62 | 11897 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11898 | `	pToken->pUserData = 0;` |
|       62 | 11899 | `	pStr = &pToken->sData;` |
|       62 | 11900 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11901 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11902 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11903 | `			/* Single character */` |
|       36 | 11904 | `			c = pStream->zText[0];` |
|        - | 11905 | `			/* Set token type */` |
|       36 | 11906 | `			switch(c){` |
|        5 | 11907 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11908 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11909 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11910 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11911 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11912 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11913 | `			default:` |
|      ! 0 | 11914 | `				break;` |
|        - | 11915 | `			}` |
|        - | 11916 | `			/* Advance the stream cursor */` |
|       36 | 11917 | `			pStream->zText++;` |
|       45 | 11918 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11919 | `		/* JSON string */` |
|       10 | 11920 | `		pStream->zText++;` |
|       10 | 11921 | `		pStr->zString++;` |
|        - | 11922 | `		/* Delimit the string */` |
|       32 | 11923 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11924 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11925 | `				break;` |
|        - | 11926 | `			}` |
|       24 | 11927 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11928 | `				/* Update line counter */` |
|      ! 0 | 11929 | `				pStream->nLine++;` |
|      ! 0 | 11930 | `			}` |
|       24 | 11931 | `			pStream->zText++;` |
|        2 | 11932 | `		}` |
|       10 | 11933 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11934 | `			/* Missing closing '"' */` |
|      ! 0 | 11935 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11936 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11937 | `		}else{` |
|       10 | 11938 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11939 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11940 | `		}` |
|       24 | 11941 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11942 | `		/* Number */` |
|       13 | 11943 | `		pStream->zText++;` |
|       13 | 11944 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11945 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11946 | `			pStream->zText++;` |
|      ! 0 | 11947 | `		}` |
|       13 | 11948 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11949 | `			c = pStream->zText[0];` |
|       13 | 11950 | `			if( c == '.' ){` |
|        - | 11951 | `					/* Real number */` |
|      ! 0 | 11952 | `					pStream->zText++;` |
|      ! 0 | 11953 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11954 | `						pStream->zText++;` |
|      ! 0 | 11955 | `					}` |
|      ! 0 | 11956 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11957 | `						c = pStream->zText[0];` |
|      ! 0 | 11958 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11959 | `							pStream->zText++;` |
|      ! 0 | 11960 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11961 | `								c = pStream->zText[0];` |
|      ! 0 | 11962 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11963 | `									pStream->zText++;` |
|      ! 0 | 11964 | `								}` |
|      ! 0 | 11965 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11966 | `									pStream->zText++;` |
|      ! 0 | 11967 | `								}` |
|      ! 0 | 11968 | `							}` |
|      ! 0 | 11969 | `						}` |
|      ! 0 | 11970 | `					}` |
|       13 | 11971 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11972 | `					/* Real number */` |
|      ! 0 | 11973 | `					pStream->zText++;` |
|      ! 0 | 11974 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11975 | `						c = pStream->zText[0];` |
|      ! 0 | 11976 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11977 | `							pStream->zText++;` |
|      ! 0 | 11978 | `						}` |
|      ! 0 | 11979 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11980 | `							pStream->zText++;` |
|      ! 0 | 11981 | `						}` |
|      ! 0 | 11982 | `					}` |
|      ! 0 | 11983 | `				}` |
|        7 | 11984 | `			}` |
|       17 | 11985 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 11986 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 11987 | `			/* boolean true */` |
|      ! 0 | 11988 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 11989 | `			/* Advance the stream cursor */` |
|      ! 0 | 11990 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 11991 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 11992 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 11993 | `			/* boolean false */` |
|      ! 0 | 11994 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 11995 | `			/* Advance the stream cursor */` |
|      ! 0 | 11996 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 11997 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 11998 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 11999 | `			/* NULL */` |
|      ! 0 | 12000 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 12001 | `			/* Advance the stream cursor */` |
|      ! 0 | 12002 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 12003 | `	}else{` |
|        - | 12004 | `		/* Unexpected token */` |
|        8 | 12005 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 12006 | `		/* Advance the stream cursor */` |
|        8 | 12007 | `		pStream->zText++;` |
|        8 | 12008 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 12009 | `		/* Abort processing immediatley */` |
|        8 | 12010 | `		return SXERR_ABORT;` |
|        - | 12011 | `	}` |
|        - | 12012 | `	/* record token length */` |
|       56 | 12013 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 12014 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 12015 | `		pStr->nByte--;` |
|        4 | 12016 | `	}` |
|        - | 12017 | `	/* Return to the lexer */` |
|       56 | 12018 | `	return SXRET_OK;` |
|       32 | 12019 |  |
|        - | 12020 | `/*` |
|        - | 12021 | ` * JSON decoded input consumer callback signature.` |
|        - | 12022 | ` */` |
|        - | 12023 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 12024 | `/*` |
|        - | 12025 | ` * JSON decoder state is kept in the following structure.` |
|        - | 12026 | ` */` |
|        - | 12027 | `typedef struct json_decoder json_decoder;` |
|        - | 12028 | `struct json_decoder` |
|        - | 12029 |  |
|        - | 12030 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12031 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 12032 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 12033 | `	int iFlags;        /* Configuration flags */` |
|        - | 12034 | `	SyToken *pIn;      /* Token stream */` |
|        - | 12035 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 12036 | `	int rec_depth;     /* Recursion limit */` |
|        - | 12037 | `	int rec_count;     /* Current nesting level */` |
|        - | 12038 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12039 | `};` |
|        - | 12040 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12041 | `/* Forward declaration */` |
|        - | 12042 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12043 | `/*` |
|        - | 12044 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12045 | ` * the result in the given ph7_value.` |
|        - | 12046 | ` */` |
|        8 | 12047 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12048 |  |
|       10 | 12049 | `	const char *zIn = pStr->zString;` |
|       10 | 12050 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12051 | `	const char *zCur;` |
|        - | 12052 | `	int c;` |
|        - | 12053 | `	/* Mark the value as a string */` |
|       10 | 12054 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12055 | `	for(;;){` |
|       10 | 12056 | `		zCur = zIn;` |
|       32 | 12057 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12058 | `			zIn++;` |
|        2 | 12059 | `		}` |
|       10 | 12060 | `		if( zIn > zCur ){` |
|        - | 12061 | `			/* Append chunk verbatim */` |
|       10 | 12062 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12063 | `		}` |
|       10 | 12064 | `		zIn++;` |
|       10 | 12065 | `		if( zIn >= zEnd ){` |
|        - | 12066 | `			/* End of the input reached */` |
|       10 | 12067 | `			break;` |
|        - | 12068 | `		}` |
|      ! 0 | 12069 | `		c = zIn[0];` |
|        - | 12070 | `		/* Unescape the character */` |
|      ! 0 | 12071 | `		switch(c){` |
|      ! 0 | 12072 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12073 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12074 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12075 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12076 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12077 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12078 | `		default:` |
|      ! 0 | 12079 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12080 | `			break;` |
|        - | 12081 | `		}` |
|        - | 12082 | `		/* Advance the stream cursor */` |
|      ! 0 | 12083 | `		zIn++;` |
|      ! 0 | 12084 | `	}` |
|       10 | 12085 |  |
|        - | 12086 | `/*` |
|        - | 12087 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12088 | ` * According to wikipedia` |
|        - | 12089 | ` * JSON's basic types are:` |
|        - | 12090 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12091 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12092 | ` *   Boolean (true or false)` |
|        - | 12093 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12094 | ` *    do not need to be of the same type)` |
|        - | 12095 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12096 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12097 | ` *     be distinct from each other)` |
|        - | 12098 | ` *   null (empty)` |
|        - | 12099 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12100 | ` */` |
|       24 | 12101 | `static sxi32 VmJsonDecode(` |
|        - | 12102 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12103 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12104 | `	){` |
|        - | 12105 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12106 | `	sxi32 rc;` |
|        - | 12107 | `	/* Check if we do not nest to much */` |
|       26 | 12108 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12109 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12110 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12111 | `		return SXERR_ABORT;` |
|        - | 12112 | `	}` |
|       26 | 12113 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12114 | `		/* Scalar value */` |
|       16 | 12115 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12116 | `		if( pWorker == 0 ){` |
|      ! 0 | 12117 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12118 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12119 | `			return SXERR_ABORT;` |
|        - | 12120 | `		}` |
|        - | 12121 | `		/* Reflect the JSON image */` |
|       16 | 12122 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12123 | `			/* Nullify the value.*/` |
|      ! 0 | 12124 | `			ph7_value_null(pWorker);` |
|       16 | 12125 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12126 | `			/* Boolean value */` |
|      ! 0 | 12127 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12128 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12129 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12130 | `			/*` |
|        - | 12131 | `			 * Numeric value.` |
|        - | 12132 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12133 | `			 * value.` |
|        - | 12134 | `			 */` |
|       13 | 12135 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12136 | `			/* Obtain a numeric representation */` |
|       13 | 12137 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12138 | `		}else{` |
|        - | 12139 | `			/* Dequote the string */` |
|        3 | 12140 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12141 | `		}` |
|        - | 12142 | `		/* Invoke the consumer callback */` |
|       16 | 12143 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12144 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12145 | `			return SXERR_ABORT;` |
|        - | 12146 | `		}` |
|        - | 12147 | `		/* All done,advance the stream cursor */` |
|       16 | 12148 | `		pDecoder->pIn++;` |
|       19 | 12149 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12150 | `		ProcJsonConsumer xOld;` |
|        - | 12151 | `		void *pOld;` |
|        - | 12152 | `		/* Array representation*/` |
|        5 | 12153 | `		pDecoder->pIn++;` |
|        - | 12154 | `		/* Create a working array */` |
|        5 | 12155 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12156 | `		if( pWorker == 0 ){` |
|      ! 0 | 12157 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12158 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12159 | `			return SXERR_ABORT;` |
|        - | 12160 | `		}` |
|        - | 12161 | `		/* Save the old consumer */` |
|        5 | 12162 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12163 | `		pOld = pDecoder->pUserData;` |
|        - | 12164 | `		/* Set the new consumer */` |
|        5 | 12165 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12166 | `		pDecoder->pUserData = pWorker;` |
|        - | 12167 | `		/* Decode the array */` |
|        7 | 12168 | `		for(;;){` |
|        - | 12169 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12170 | `			 * do this.` |
|        - | 12171 | `			 */` |
|       21 | 12172 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12173 | `				pDecoder->pIn++;` |
|        1 | 12174 | `			}` |
|       15 | 12175 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12176 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12177 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12178 | `				}` |
|        5 | 12179 | `				break;` |
|        - | 12180 | `			}` |
|        - | 12181 | `			/* Recurse and decode the entry */` |
|       11 | 12182 | `			pDecoder->rec_count++;` |
|       11 | 12183 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12184 | `			pDecoder->rec_count--;` |
|       11 | 12185 | `			if( rc == SXERR_ABORT ){` |
|        - | 12186 | `				/* Abort processing immediately */` |
|      ! 0 | 12187 | `				return SXERR_ABORT;` |
|        - | 12188 | `			}` |
|        - | 12189 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12190 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12191 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12192 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12193 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12194 | `					return SXERR_ABORT;` |
|        - | 12195 | `			}` |
|        1 | 12196 | `		}` |
|        - | 12197 | `		/* Restore the old consumer */` |
|        5 | 12198 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12199 | `		pDecoder->pUserData = pOld;` |
|        - | 12200 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12201 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12202 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12203 | `		ProcJsonConsumer xOld;` |
|        - | 12204 | `		ph7_value *pKey;` |
|        - | 12205 | `		void *pOld;` |
|        - | 12206 | `		/* Object representation*/` |
|        8 | 12207 | `		pDecoder->pIn++;` |
|        - | 12208 | `		/* Return the object as an associative array */` |
|        8 | 12209 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12210 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12211 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12212 | `				);` |
|        1 | 12213 | `		}` |
|        - | 12214 | `		/* Create a working array */` |
|        8 | 12215 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12216 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12217 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12218 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12219 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12220 | `			return SXERR_ABORT;` |
|        - | 12221 | `		}` |
|        - | 12222 | `		/* Save the old consumer */` |
|        8 | 12223 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12224 | `		pOld = pDecoder->pUserData;` |
|        - | 12225 | `		/* Set the new consumer */` |
|        8 | 12226 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12227 | `		pDecoder->pUserData = pWorker;` |
|        - | 12228 | `		/* Decode the object */` |
|        6 | 12229 | `		for(;;){` |
|        - | 12230 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12231 | `			 * do this.` |
|        - | 12232 | `			 */` |
|       16 | 12233 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12234 | `				pDecoder->pIn++;` |
|        1 | 12235 | `			}` |
|       14 | 12236 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12237 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12238 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12239 | `				}` |
|        8 | 12240 | `				break;` |
|        - | 12241 | `			}` |
|        6 | 12242 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12243 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12244 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12245 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12246 | `					return SXERR_ABORT;` |
|        - | 12247 | `			}` |
|        - | 12248 | `			/* Dequote the key */` |
|        8 | 12249 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12250 | `			/* Jump the key and the colon */` |
|        8 | 12251 | `			pDecoder->pIn += 2;` |
|        - | 12252 | `			/* Recurse and decode the value */` |
|        8 | 12253 | `			pDecoder->rec_count++;` |
|        8 | 12254 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12255 | `			pDecoder->rec_count--;` |
|        8 | 12256 | `			if( rc == SXERR_ABORT ){` |
|        - | 12257 | `				/* Abort processing immediately */` |
|      ! 0 | 12258 | `				return SXERR_ABORT;` |
|        - | 12259 | `			}` |
|        - | 12260 | `			/* Reset the internal buffer of the key */` |
|        8 | 12261 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12262 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12263 | `		}` |
|        - | 12264 | `		/* Restore the old consumer */` |
|        8 | 12265 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12266 | `		pDecoder->pUserData = pOld;` |
|        - | 12267 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12268 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12269 | `		/* Release the key */` |
|        8 | 12270 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12271 | `	}else{` |
|        - | 12272 | `		/* Unexpected token */` |
|      ! 0 | 12273 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12274 | `	}` |
|        - | 12275 | `	/* Release the worker variable */` |
|       26 | 12276 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12277 | `	return SXRET_OK;` |
|       14 | 12278 |  |
|        - | 12279 | `/*` |
|        - | 12280 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12281 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12282 | ` * is being decoded.` |
|        - | 12283 | ` */` |
|       16 | 12284 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12285 |  |
|       18 | 12286 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12287 | `	/* Insert the entry */` |
|       18 | 12288 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12289 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12290 | `	/* All done */` |
|       18 | 12291 | `	return SXRET_OK;` |
|        2 | 12292 |  |
|        - | 12293 | `/*` |
|        - | 12294 | ` * Standard JSON decoder callback.` |
|        - | 12295 | ` */` |
|        8 | 12296 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12297 |  |
|        - | 12298 | `	/* Return the value directly */` |
|       10 | 12299 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12300 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12301 | `	SXUNUSED(pUserData);` |
|        - | 12302 | `	/* All done */` |
|       10 | 12303 | `	return SXRET_OK;` |
|        2 | 12304 |  |
|        - | 12305 | `/*` |
|        - | 12306 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12307 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12308 | ` * Parameters` |
|        - | 12309 | ` *  $json` |
|        - | 12310 | ` *    The json string being decoded.` |
|        - | 12311 | ` * $assoc` |
|        - | 12312 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12313 | ` * $depth` |
|        - | 12314 | ` *   User specified recursion depth.` |
|        - | 12315 | ` * $options` |
|        - | 12316 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12317 | ` * (default is to cast large integers as floats)` |
|        - | 12318 | ` * Return` |
|        - | 12319 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12320 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12321 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12322 | ` */` |
|       16 | 12323 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12324 |  |
|       18 | 12325 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12326 | `	json_decoder sDecoder;` |
|        - | 12327 | `	const char *zIn;` |
|        - | 12328 | `	SySet sToken;` |
|        - | 12329 | `	SyLex sLex;` |
|        - | 12330 | `	int nByte;` |
|        - | 12331 | `	sxi32 rc;` |
|       18 | 12332 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12333 | `		/* Missing/Invalid arguments, return NULL */` |
|      ! 0 | 12334 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12335 | `		return PH7_OK;` |
|        - | 12336 | `	}` |
|        - | 12337 | `	/* Extract the JSON string */` |
|       18 | 12338 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       18 | 12339 | `	if( nByte < 1 ){` |
|        - | 12340 | `		/* Empty string,return NULL */` |
|        3 | 12341 | `		ph7_result_null(pCtx);` |
|        3 | 12342 | `		return PH7_OK;` |
|        - | 12343 | `	}` |
|        - | 12344 | `	/* Clear JSON error code */` |
|       16 | 12345 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12346 | `	/* Tokenize the input */` |
|       16 | 12347 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12348 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12349 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12350 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12351 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12352 | `		SyLexRelease(&sLex);` |
|        8 | 12353 | `		SySetRelease(&sToken);` |
|        - | 12354 | `		/* return NULL */` |
|        8 | 12355 | `		ph7_result_null(pCtx);` |
|        8 | 12356 | `		return PH7_OK;` |
|        - | 12357 | `	}` |
|        - | 12358 | `	/* Fill the decoder */` |
|       10 | 12359 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12360 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12361 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12362 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12363 | `	sDecoder.iFlags = 0;` |
|       10 | 12364 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12365 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12366 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12367 | `	}` |
|       10 | 12368 | `	sDecoder.rec_depth = 32;` |
|       10 | 12369 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12370 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12371 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12372 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12373 | `		}` |
|      ! 0 | 12374 | `	}` |
|       10 | 12375 | `	sDecoder.rec_count = 0;` |
|        - | 12376 | `	/* Set a default consumer */` |
|       10 | 12377 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12378 | `	sDecoder.pUserData = 0;` |
|        - | 12379 | `	/* Decode the raw JSON input */` |
|       10 | 12380 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12381 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12382 | `		/*` |
|        - | 12383 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12384 | `		 */` |
|      ! 0 | 12385 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12386 | `	}` |
|        - | 12387 | `	/* Clean-up the mess left behind */` |
|       10 | 12388 | `	SyLexRelease(&sLex);` |
|       10 | 12389 | `	SySetRelease(&sToken);` |
|        - | 12390 | `	/* All done */` |
|       10 | 12391 | `	return PH7_OK;` |
|       10 | 12392 |  |
|        - | 12393 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12394 | `/*` |
|        - | 12395 | ` * XML processing Functions.` |
|        - | 12396 | ` * Status:` |
|        - | 12397 | ` *    Devel.` |
|        - | 12398 | ` */` |
|        - | 12399 | `enum ph7_xml_handler_id{` |
|        - | 12400 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12401 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12402 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12403 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12404 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12405 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12406 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12407 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12408 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12409 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12410 | `};` |
|        - | 12411 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12412 | `/* An instance of the following structure describe a working` |
|        - | 12413 | ` * XML engine instance.` |
|        - | 12414 | ` */` |
|        - | 12415 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12416 | `struct ph7_xml_engine` |
|        - | 12417 |  |
|        - | 12418 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12419 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12420 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12421 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12422 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12423 | `							  * as the first argument to the user callbacks.` |
|        - | 12424 | `							  */` |
|        - | 12425 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12426 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12427 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12428 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12429 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12430 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12431 | `};` |
|        - | 12432 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12433 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12434 | `/*` |
|        - | 12435 | ` * Allocate and initialize an XML engine.` |
|        - | 12436 | ` */` |
|       84 | 12437 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12438 |  |
|        - | 12439 | `	ph7_xml_engine *pEngine;` |
|       85 | 12440 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12441 | `	ph7_value *pValue;` |
|        - | 12442 | `	sxu32 n;` |
|        - | 12443 | `	/* Allocate a new instance */` |
|       85 | 12444 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12445 | `	if( pEngine == 0 ){` |
|        - | 12446 | `		/* Out of memory */` |
|      ! 0 | 12447 | `		return 0;` |
|        - | 12448 | `	}` |
|        - | 12449 | `	/* Zero the structure */` |
|       85 | 12450 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12451 | `	/* Initialize fields */` |
|       85 | 12452 | `	pEngine->pVm = pVm;` |
|       85 | 12453 | `	pEngine->pCtx = 0;` |
|       85 | 12454 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12455 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12456 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12457 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12458 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12459 | `		pValue = &pEngine->aCB[n];` |
|        - | 12460 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12461 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12462 | `	}` |
|       85 | 12463 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12464 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12465 | `	/* Finally set the magic number */` |
|       85 | 12466 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12467 | `	return pEngine;` |
|       43 | 12468 |  |
|        - | 12469 | `/*` |
|        - | 12470 | ` * Release an XML engine.` |
|        - | 12471 | ` */` |
|       84 | 12472 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12473 |  |
|       85 | 12474 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12475 | `	ph7_value *pValue;` |
|        - | 12476 | `	sxu32 n;` |
|        - | 12477 | `	/* Release fields */` |
|       85 | 12478 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12479 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12480 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12481 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12482 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12483 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12484 | `	}` |
|       85 | 12485 | `	pEngine->nMagic = 0x2621;` |
|        - | 12486 | `	/* Finally,release the whole instance */` |
|       85 | 12487 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12488 |  |
|        - | 12489 | `/*` |
|        - | 12490 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12491 | ` *  Create an UTF-8 XML parser.` |
|        - | 12492 | ` * Parameter` |
|        - | 12493 | ` *  $encoding` |
|        - | 12494 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12495 | ` * Return` |
|        - | 12496 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12497 | ` */` |
|       80 | 12498 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12499 |  |
|        - | 12500 | `	ph7_xml_engine *pEngine;` |
|        - | 12501 | `	/* Allocate a new instance */` |
|       81 | 12502 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12503 | `	if( pEngine == 0 ){` |
|      ! 0 | 12504 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12505 | `		/* Return null */` |
|      ! 0 | 12506 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12507 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12508 | `		SXUNUSED(apArg);` |
|      ! 0 | 12509 | `		return PH7_OK;` |
|        - | 12510 | `	}` |
|        - | 12511 | `	/* Return the engine as a resource */` |
|       81 | 12512 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12513 | `	return PH7_OK;` |
|       41 | 12514 |  |
|        - | 12515 | `/*` |
|        - | 12516 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12517 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12518 | ` * Parameter` |
|        - | 12519 | ` *  $encoding` |
|        - | 12520 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12521 | ` *  $separtor` |
|        - | 12522 | ` *   Namespace separator (a single character)` |
|        - | 12523 | ` * Return` |
|        - | 12524 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12525 | ` */` |
|        4 | 12526 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12527 |  |
|        - | 12528 | `	ph7_xml_engine *pEngine;` |
|        5 | 12529 | `	int ns_sep = ':';` |
|        5 | 12530 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12531 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12532 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12533 | `			ns_sep = zSep[0];` |
|      ! 0 | 12534 | `		}` |
|      ! 0 | 12535 | `	}` |
|        - | 12536 | `	/* Allocate a new instance */` |
|        5 | 12537 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12538 | `	if( pEngine == 0 ){` |
|      ! 0 | 12539 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12540 | `		/* Return null */` |
|      ! 0 | 12541 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12542 | `		return PH7_OK;` |
|        - | 12543 | `	}` |
|        - | 12544 | `	/* Return the engine as a resource */` |
|        5 | 12545 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12546 | `	return PH7_OK;` |
|        3 | 12547 |  |
|        - | 12548 | `/*` |
|        - | 12549 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12550 | ` *  Release an XML engine.` |
|        - | 12551 | ` * Parameter` |
|        - | 12552 | ` *  $parser` |
|        - | 12553 | ` *   A reference to the XML parser to free.` |
|        - | 12554 | ` * Return` |
|        - | 12555 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12556 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12557 | ` */` |
|       84 | 12558 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12559 |  |
|        - | 12560 | `	ph7_xml_engine *pEngine;` |
|       85 | 12561 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12562 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12563 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12564 | `		return PH7_OK;` |
|        - | 12565 | `	}` |
|        - | 12566 | `	/* Point to the XML engine */` |
|       85 | 12567 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12568 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12569 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12570 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12571 | `		return PH7_OK;` |
|        - | 12572 | `	}` |
|        - | 12573 | `	/* Safely release the engine */` |
|       85 | 12574 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12575 | `	/* Return TRUE */` |
|       85 | 12576 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12577 | `	return PH7_OK;` |
|       43 | 12578 |  |
|        - | 12579 | `/*` |
|        - | 12580 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12581 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12582 | ` * are strings containing the names of functions.` |
|        - | 12583 | ` * Parameters` |
|        - | 12584 | ` *  $parser` |
|        - | 12585 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12586 | ` *  $start_element_handler` |
|        - | 12587 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12588 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12589 | ` *    $parser` |
|        - | 12590 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12591 | ` *   $name` |
|        - | 12592 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12593 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12594 | ` *  $attribs` |
|        - | 12595 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12596 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12597 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12598 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12599 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12600 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12601 | ` * $end_element_handler` |
|        - | 12602 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12603 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12604 | ` *    $parser` |
|        - | 12605 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12606 | ` *   $name` |
|        - | 12607 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12608 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12609 | ` *      letters.` |
|        - | 12610 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12611 | ` * Return` |
|        - | 12612 | ` * TRUE on success or FALSE on failure.` |
|        - | 12613 | ` */` |
|       66 | 12614 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12615 |  |
|        - | 12616 | `	ph7_xml_engine *pEngine;` |
|       67 | 12617 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12618 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12619 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12620 | `		return PH7_OK;` |
|        - | 12621 | `	}` |
|        - | 12622 | `	/* Point to the XML engine */` |
|       67 | 12623 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12624 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12625 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12626 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12627 | `		return PH7_OK;` |
|        - | 12628 | `	}` |
|       67 | 12629 | `	if( nArg > 1 ){` |
|        - | 12630 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12631 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12632 | `		if( nArg > 2 ){` |
|        - | 12633 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12634 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12635 | `		}` |
|       33 | 12636 | `	}` |
|        - | 12637 | `	/* All done,return TRUE */` |
|       67 | 12638 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12639 | `	return PH7_OK;` |
|       34 | 12640 |  |
|        - | 12641 | `/*` |
|        - | 12642 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12643 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12644 | ` * Parameters` |
|        - | 12645 | ` * $parser` |
|        - | 12646 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12647 | ` * $handler` |
|        - | 12648 | ` *  handler is a string containing the name of the callback.` |
|        - | 12649 | ` *  The function named by handler must accept two parameters:` |
|        - | 12650 | ` *   handler(resource $parser,string $data)` |
|        - | 12651 | ` *  $parser` |
|        - | 12652 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12653 | ` *  $data` |
|        - | 12654 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12655 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12656 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12657 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12658 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12659 | ` *   can also be supplied.` |
|        - | 12660 | ` * Return` |
|        - | 12661 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12662 | ` */` |
|       40 | 12663 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12664 |  |
|        - | 12665 | `	ph7_xml_engine *pEngine;` |
|       41 | 12666 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12667 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12668 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12669 | `		return PH7_OK;` |
|        - | 12670 | `	}` |
|        - | 12671 | `	/* Point to the XML engine */` |
|       41 | 12672 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12673 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12674 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12675 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12676 | `		return PH7_OK;` |
|        - | 12677 | `	}` |
|       41 | 12678 | `	if( nArg > 1 ){` |
|        - | 12679 | `		/* Save the user callback for later invocation */` |
|       41 | 12680 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12681 | `	}` |
|        - | 12682 | `	/* All done,return TRUE */` |
|       41 | 12683 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12684 | `	return PH7_OK;` |
|       21 | 12685 |  |
|        - | 12686 | `/*` |
|        - | 12687 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12688 | ` *  Set up default handler.` |
|        - | 12689 | ` * Parameters` |
|        - | 12690 | ` * $parser` |
|        - | 12691 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12692 | ` * $handler` |
|        - | 12693 | ` *  handler is a string containing the name of the callback.` |
|        - | 12694 | ` *  The function named by handler must accept two parameters:` |
|        - | 12695 | ` *   handler(resource $parser,string $data)` |
|        - | 12696 | ` *  $parser` |
|        - | 12697 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12698 | ` *  $data` |
|        - | 12699 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12700 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12701 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12702 | ` *   can also be supplied.` |
|        - | 12703 | ` * Return` |
|        - | 12704 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12705 | ` */` |
|        2 | 12706 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12707 |  |
|        - | 12708 | `	ph7_xml_engine *pEngine;` |
|        3 | 12709 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12710 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12711 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12712 | `		return PH7_OK;` |
|        - | 12713 | `	}` |
|        - | 12714 | `	/* Point to the XML engine */` |
|        3 | 12715 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12716 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12717 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12718 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12719 | `		return PH7_OK;` |
|        - | 12720 | `	}` |
|        3 | 12721 | `	if( nArg > 1 ){` |
|        - | 12722 | `		/* Save the user callback for later invocation */` |
|        3 | 12723 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12724 | `	}` |
|        - | 12725 | `	/* All done,return TRUE */` |
|        3 | 12726 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12727 | `	return PH7_OK;` |
|        2 | 12728 |  |
|        - | 12729 | `/*` |
|        - | 12730 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12731 | ` *  Set up end namespace declaration handler.` |
|        - | 12732 | ` * Parameters` |
|        - | 12733 | ` * $parser` |
|        - | 12734 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12735 | ` * $handler` |
|        - | 12736 | ` *  handler is a string containing the name of the callback.` |
|        - | 12737 | ` *  The function named by handler must accept two parameters:` |
|        - | 12738 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12739 | ` *  $parser` |
|        - | 12740 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12741 | ` *  $prefix` |
|        - | 12742 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12743 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12744 | ` *   can also be supplied.` |
|        - | 12745 | ` * Return` |
|        - | 12746 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12747 | ` */` |
|        2 | 12748 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12749 |  |
|        - | 12750 | `	ph7_xml_engine *pEngine;` |
|        3 | 12751 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12752 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12753 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12754 | `		return PH7_OK;` |
|        - | 12755 | `	}` |
|        - | 12756 | `	/* Point to the XML engine */` |
|        3 | 12757 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12758 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12759 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12760 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12761 | `		return PH7_OK;` |
|        - | 12762 | `	}` |
|        3 | 12763 | `	if( nArg > 1 ){` |
|        - | 12764 | `		/* Save the user callback for later invocation */` |
|        3 | 12765 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12766 | `	}` |
|        - | 12767 | `	/* All done,return TRUE */` |
|        3 | 12768 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12769 | `	return PH7_OK;` |
|        2 | 12770 |  |
|        - | 12771 | `/*` |
|        - | 12772 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12773 | ` *  Set up start namespace declaration handler.` |
|        - | 12774 | ` * Parameters` |
|        - | 12775 | ` * $parser` |
|        - | 12776 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12777 | ` * $handler` |
|        - | 12778 | ` *  handler is a string containing the name of the callback.` |
|        - | 12779 | ` *  The function named by handler must accept two parameters:` |
|        - | 12780 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12781 | ` *  $parser` |
|        - | 12782 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12783 | ` *  $prefix` |
|        - | 12784 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12785 | ` *  $uri` |
|        - | 12786 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12787 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12788 | ` *   can also be supplied.` |
|        - | 12789 | ` * Return` |
|        - | 12790 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12791 | ` */` |
|        2 | 12792 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12793 |  |
|        - | 12794 | `	ph7_xml_engine *pEngine;` |
|        3 | 12795 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12796 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12797 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12798 | `		return PH7_OK;` |
|        - | 12799 | `	}` |
|        - | 12800 | `	/* Point to the XML engine */` |
|        3 | 12801 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12802 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12803 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12804 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12805 | `		return PH7_OK;` |
|        - | 12806 | `	}` |
|        3 | 12807 | `	if( nArg > 1 ){` |
|        - | 12808 | `		/* Save the user callback for later invocation */` |
|        3 | 12809 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12810 | `	}` |
|        - | 12811 | `	/* All done,return TRUE */` |
|        3 | 12812 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12813 | `	return PH7_OK;` |
|        2 | 12814 |  |
|        - | 12815 | `/*` |
|        - | 12816 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12817 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12818 | ` * Parameters` |
|        - | 12819 | ` * $parser` |
|        - | 12820 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12821 | ` * $handler` |
|        - | 12822 | ` *  handler is a string containing the name of the callback.` |
|        - | 12823 | ` *  The function named by handler must accept three parameters:` |
|        - | 12824 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12825 | ` *  $parser` |
|        - | 12826 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12827 | ` *  $target` |
|        - | 12828 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12829 | ` *  $data` |
|        - | 12830 | `     The third parameter, data, contains the PI data.` |
|        - | 12831 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12832 | ` *   can also be supplied.` |
|        - | 12833 | ` * Return` |
|        - | 12834 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12835 | ` */` |
|        8 | 12836 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12837 |  |
|        - | 12838 | `	ph7_xml_engine *pEngine;` |
|        9 | 12839 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12840 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12841 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12842 | `		return PH7_OK;` |
|        - | 12843 | `	}` |
|        - | 12844 | `	/* Point to the XML engine */` |
|        9 | 12845 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12846 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12847 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12848 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12849 | `		return PH7_OK;` |
|        - | 12850 | `	}` |
|        9 | 12851 | `	if( nArg > 1 ){` |
|        - | 12852 | `		/* Save the user callback for later invocation */` |
|        9 | 12853 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12854 | `	}` |
|        - | 12855 | `	/* All done,return TRUE */` |
|        9 | 12856 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12857 | `	return PH7_OK;` |
|        5 | 12858 |  |
|        - | 12859 | `/*` |
|        - | 12860 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12861 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12862 | ` * Parameters` |
|        - | 12863 | ` * $parser` |
|        - | 12864 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12865 | ` * $handler` |
|        - | 12866 | ` *  handler is a string containing the name of the callback.` |
|        - | 12867 | ` *  The function named by handler must accept six parameters:` |
|        - | 12868 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12869 | ` *  $parser` |
|        - | 12870 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12871 | ` *  $entity_name` |
|        - | 12872 | ` *   The name of the entity that is about to be defined.` |
|        - | 12873 | ` *  $base` |
|        - | 12874 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12875 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12876 | ` *  $system_id` |
|        - | 12877 | ` *   System identifier for the external entity.` |
|        - | 12878 | ` *  $public_id` |
|        - | 12879 | ` *    Public identifier for the external entity.` |
|        - | 12880 | ` *  $notation_name` |
|        - | 12881 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12882 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12883 | ` *   can also be supplied.` |
|        - | 12884 | ` * Return` |
|        - | 12885 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12886 | ` */` |
|        2 | 12887 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12888 |  |
|        - | 12889 | `	ph7_xml_engine *pEngine;` |
|        3 | 12890 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12891 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12892 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12893 | `		return PH7_OK;` |
|        - | 12894 | `	}` |
|        - | 12895 | `	/* Point to the XML engine */` |
|        3 | 12896 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12897 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12898 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12899 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12900 | `		return PH7_OK;` |
|        - | 12901 | `	}` |
|        3 | 12902 | `	if( nArg > 1 ){` |
|        - | 12903 | `		/* Save the user callback for later invocation */` |
|        3 | 12904 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12905 | `	}` |
|        - | 12906 | `	/* All done,return TRUE */` |
|        3 | 12907 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12908 | `	return PH7_OK;` |
|        2 | 12909 |  |
|        - | 12910 | `/*` |
|        - | 12911 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12912 | ` *  Set up notation declaration handler.` |
|        - | 12913 | ` * Parameters` |
|        - | 12914 | ` * $parser` |
|        - | 12915 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12916 | ` * $handler` |
|        - | 12917 | ` *  handler is a string containing the name of the callback.` |
|        - | 12918 | ` *  The function named by handler must accept five parameters:` |
|        - | 12919 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12920 | ` *  $parser` |
|        - | 12921 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12922 | ` *  $entity_name` |
|        - | 12923 | ` *   The name of the entity that is about to be defined.` |
|        - | 12924 | ` *  $base` |
|        - | 12925 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12926 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12927 | ` *  $system_id` |
|        - | 12928 | ` *   System identifier for the external entity.` |
|        - | 12929 | ` *  $public_id` |
|        - | 12930 | ` *    Public identifier for the external entity.` |
|        - | 12931 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12932 | ` *  can also be supplied.` |
|        - | 12933 | ` * Return` |
|        - | 12934 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12935 | ` */` |
|        2 | 12936 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12937 |  |
|        - | 12938 | `	ph7_xml_engine *pEngine;` |
|        3 | 12939 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12940 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12941 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12942 | `		return PH7_OK;` |
|        - | 12943 | `	}` |
|        - | 12944 | `	/* Point to the XML engine */` |
|        3 | 12945 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12946 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12947 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12948 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12949 | `		return PH7_OK;` |
|        - | 12950 | `	}` |
|        3 | 12951 | `	if( nArg > 1 ){` |
|        - | 12952 | `		/* Save the user callback for later invocation */` |
|        3 | 12953 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12954 | `	}` |
|        - | 12955 | `	/* All done,return TRUE */` |
|        3 | 12956 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12957 | `	return PH7_OK;` |
|        2 | 12958 |  |
|        - | 12959 | `/*` |
|        - | 12960 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12961 | ` *  Set up external entity reference handler.` |
|        - | 12962 | ` * Parameters` |
|        - | 12963 | ` * $parser` |
|        - | 12964 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12965 | ` * $handler` |
|        - | 12966 | ` *  handler is a string containing the name of the callback.` |
|        - | 12967 | ` *  The function named by handler must accept five parameters:` |
|        - | 12968 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12969 | ` *  $parser` |
|        - | 12970 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12971 | ` *  $open_entity_names` |
|        - | 12972 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12973 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12974 | ` *  $base` |
|        - | 12975 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12976 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12977 | ` *  $system_id` |
|        - | 12978 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12979 | ` *  $public_id` |
|        - | 12980 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12981 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12982 | ` *   normalized as required by the XML spec.` |
|        - | 12983 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12984 | ` * can also be supplied.` |
|        - | 12985 | ` * Return` |
|        - | 12986 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12987 | ` */` |
|        2 | 12988 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12989 |  |
|        - | 12990 | `	ph7_xml_engine *pEngine;` |
|        3 | 12991 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12992 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12993 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12994 | `		return PH7_OK;` |
|        - | 12995 | `	}` |
|        - | 12996 | `	/* Point to the XML engine */` |
|        3 | 12997 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12998 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12999 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13000 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13001 | `		return PH7_OK;` |
|        - | 13002 | `	}` |
|        3 | 13003 | `	if( nArg > 1 ){` |
|        - | 13004 | `		/* Save the user callback for later invocation */` |
|        3 | 13005 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 13006 | `	}` |
|        - | 13007 | `	/* All done,return TRUE */` |
|        3 | 13008 | `	ph7_result_bool(pCtx,1);` |
|        3 | 13009 | `	return PH7_OK;` |
|        2 | 13010 |  |
|        - | 13011 | `/*` |
|        - | 13012 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 13013 | ` *  Gets the current line number for the given XML parser.` |
|        - | 13014 | ` * Parameters` |
|        - | 13015 | ` * $parser` |
|        - | 13016 | ` *   A reference to the XML parser.` |
|        - | 13017 | ` * Return` |
|        - | 13018 | ` *  This function returns FALSE if parser does not refer` |
|        - | 13019 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 13020 | ` *  is currently at in its data buffer.` |
|        - | 13021 | ` */` |
|        8 | 13022 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13023 |  |
|        - | 13024 | `	ph7_xml_engine *pEngine;` |
|        9 | 13025 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13026 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13027 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13028 | `		return PH7_OK;` |
|        - | 13029 | `	}` |
|        - | 13030 | `	/* Point to the XML engine */` |
|        9 | 13031 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 13032 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13033 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13034 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13035 | `		return PH7_OK;` |
|        - | 13036 | `	}` |
|        - | 13037 | `	/* Return the line number */` |
|        9 | 13038 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13039 | `	return PH7_OK;` |
|        5 | 13040 |  |
|        - | 13041 | `/*` |
|        - | 13042 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13043 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13044 | ` * Parameters` |
|        - | 13045 | ` * $parser` |
|        - | 13046 | ` *   A reference to the XML parser.` |
|        - | 13047 | ` * Return` |
|        - | 13048 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13049 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13050 | ` *  at in its data buffer (starting at 0).` |
|        - | 13051 | ` */` |
|        4 | 13052 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13053 |  |
|        - | 13054 | `	ph7_xml_engine *pEngine;` |
|        - | 13055 | `	SyStream *pStream;` |
|        - | 13056 | `	SyToken *pToken;` |
|        5 | 13057 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13058 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13059 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13060 | `		return PH7_OK;` |
|        - | 13061 | `	}` |
|        - | 13062 | `	/* Point to the XML engine */` |
|        5 | 13063 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13064 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13065 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13066 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13067 | `		return PH7_OK;` |
|        - | 13068 | `	}` |
|        - | 13069 | `	/* Point to the current processed token */` |
|        5 | 13070 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13071 | `	if( pToken == 0 ){` |
|        - | 13072 | `		/* Stream not yet processed */` |
|        3 | 13073 | `		ph7_result_int(pCtx,0);` |
|        3 | 13074 | `		return 0;` |
|        - | 13075 | `	}` |
|        - | 13076 | `	/* Point to the input stream */` |
|        3 | 13077 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13078 | `	/* Return the byte index */` |
|        3 | 13079 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13080 | `	return PH7_OK;` |
|        3 | 13081 |  |
|        - | 13082 | `/*` |
|        - | 13083 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13084 | ` *  Use XML Parser within an object.` |
|        - | 13085 | ` * NOTE` |
|        - | 13086 | ` *  This function is depreceated and is a no-op.` |
|        - | 13087 | ` * Parameters` |
|        - | 13088 | ` * $parser` |
|        - | 13089 | ` *   A reference to the XML parser.` |
|        - | 13090 | ` * $object` |
|        - | 13091 | ` *  The object where to use the XML parser.` |
|        - | 13092 | ` * Return` |
|        - | 13093 | ` * Always FALSE.` |
|        - | 13094 | ` */` |
|        2 | 13095 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13096 |  |
|        - | 13097 | `	ph7_xml_engine *pEngine;` |
|        3 | 13098 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13099 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13100 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13101 | `		return PH7_OK;` |
|        - | 13102 | `	}` |
|        - | 13103 | `	/* Point to the XML engine */` |
|      ! 0 | 13104 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13105 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13106 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13107 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13108 | `		return PH7_OK;` |
|        - | 13109 | `	}` |
|        - | 13110 | `	/*  Throw a notice and return */` |
|      ! 0 | 13111 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13112 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13113 | `		"containing an object reference and a method name."` |
|        - | 13114 | `		);` |
|        - | 13115 | `	/* Return FALSE */` |
|      ! 0 | 13116 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13117 | `	return PH7_OK;` |
|        2 | 13118 |  |
|        - | 13119 | `/*` |
|        - | 13120 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13121 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13122 | ` * Parameters` |
|        - | 13123 | ` * $parser` |
|        - | 13124 | ` *   A reference to the XML parser.` |
|        - | 13125 | ` * Return` |
|        - | 13126 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13127 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13128 | ` *  is currently at.` |
|        - | 13129 | ` */` |
|        4 | 13130 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13131 |  |
|        - | 13132 | `	ph7_xml_engine *pEngine;` |
|        - | 13133 | `	SyStream *pStream;` |
|        - | 13134 | `	SyToken *pToken;` |
|        5 | 13135 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13136 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13137 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13138 | `		return PH7_OK;` |
|        - | 13139 | `	}` |
|        - | 13140 | `	/* Point to the XML engine */` |
|        5 | 13141 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13142 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13143 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13145 | `		return PH7_OK;` |
|        - | 13146 | `	}` |
|        - | 13147 | `	/* Point to the current processed token */` |
|        5 | 13148 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13149 | `	if( pToken == 0 ){` |
|        - | 13150 | `		/* Stream not yet processed */` |
|      ! 0 | 13151 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13152 | `		return 0;` |
|        - | 13153 | `	}` |
|        - | 13154 | `	/* Point to the input stream */` |
|        5 | 13155 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13156 | `	/* Return the byte index */` |
|        5 | 13157 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13158 | `	return PH7_OK;` |
|        3 | 13159 |  |
|        - | 13160 | `/*` |
|        - | 13161 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13162 | ` *  Get XML parser error code.` |
|        - | 13163 | ` * Parameters` |
|        - | 13164 | ` * $parser` |
|        - | 13165 | ` *   A reference to the XML parser.` |
|        - | 13166 | ` * Return` |
|        - | 13167 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13168 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13169 | ` *  codes section.` |
|        - | 13170 | ` */` |
|       32 | 13171 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13172 |  |
|        - | 13173 | `	ph7_xml_engine *pEngine;` |
|       33 | 13174 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13175 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13176 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13177 | `		return PH7_OK;` |
|        - | 13178 | `	}` |
|        - | 13179 | `	/* Point to the XML engine */` |
|       33 | 13180 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13181 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13182 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13183 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13184 | `		return PH7_OK;` |
|        - | 13185 | `	}` |
|        - | 13186 | `	/* Return the error code if any */` |
|       33 | 13187 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13188 | `	return PH7_OK;` |
|       17 | 13189 |  |
|        - | 13190 | `/*` |
|        - | 13191 | ` * XML parser event callbacks` |
|        - | 13192 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13193 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13194 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13195 | ` */` |
|        - | 13196 | `/*` |
|        - | 13197 | ` * Create a scalar ph7_value holding the value` |
|        - | 13198 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13199 | ` */` |
|      148 | 13200 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13201 |  |
|        - | 13202 | `	ph7_value *pValue;` |
|        - | 13203 | `	/* Allocate a new scalar variable */` |
|      149 | 13204 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13205 | `	if( pValue == 0 ){` |
|      ! 0 | 13206 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13207 | `		return 0;` |
|        - | 13208 | `	}` |
|      149 | 13209 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13210 | `		/* Append namespace URI and the separator */` |
|        9 | 13211 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13212 | `	}` |
|        - | 13213 | `	/* Copy the tag value */` |
|      149 | 13214 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13215 | `	return pValue;` |
|       75 | 13216 |  |
|        - | 13217 | `/*` |
|        - | 13218 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13219 | ` * of an XML tag attributes.` |
|        - | 13220 | ` */` |
|       62 | 13221 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13222 |  |
|        - | 13223 | `	ph7_value *pArray;` |
|        - | 13224 | `	/* Create an empty array */` |
|       63 | 13225 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13226 | `	if( pArray == 0 ){` |
|      ! 0 | 13227 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13228 | `		return 0;` |
|        - | 13229 | `	}` |
|       63 | 13230 | `	if( nAttr > 0 ){` |
|        - | 13231 | `		ph7_value *pKey,*pValue;` |
|        - | 13232 | `		sxu32 n;` |
|        - | 13233 | `		/* Create worker variables */` |
|        5 | 13234 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13235 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13236 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13237 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13238 | `			return 0;` |
|        - | 13239 | `		}` |
|        - | 13240 | `		/* Copy attributes */` |
|        9 | 13241 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13242 | `			/* Reset string cursors */` |
|        5 | 13243 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13244 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13245 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13246 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13247 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13248 | `			/* Insert in the array */` |
|        5 | 13249 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13250 | `		}` |
|        - | 13251 | `		/* Release the worker variables */` |
|        5 | 13252 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13253 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13254 | `	}` |
|        - | 13255 | `	/* Return the freshly created array */` |
|       63 | 13256 | `	return pArray;` |
|       32 | 13257 |  |
|        - | 13258 | `/*` |
|        - | 13259 | ` * Start element handler.` |
|        - | 13260 | ` * The user defined callback must accept three parameters:` |
|        - | 13261 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13262 | ` *    $parser` |
|        - | 13263 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13264 | ` *    $name` |
|        - | 13265 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13266 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13267 | ` *    $attribs` |
|        - | 13268 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13269 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13270 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13271 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13272 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13273 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13274 | ` */` |
|       78 | 13275 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13276 |  |
|       79 | 13277 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13278 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13279 | `	/* Point to the target user defined callback */` |
|       79 | 13280 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13281 | `	/* Make sure the given callback is callable */` |
|       79 | 13282 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13283 | `		/* Not callable,return immediately*/` |
|       17 | 13284 | `		return SXRET_OK;` |
|        - | 13285 | `	}` |
|        - | 13286 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13287 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13288 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13289 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13290 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13291 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13292 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13293 | `		return SXRET_OK;` |
|        - | 13294 | `	}` |
|        - | 13295 | `	/* Invoke the user callback */` |
|       63 | 13296 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13297 | `	/* Clean-up the mess left behind */` |
|       63 | 13298 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13299 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13300 | `	return SXRET_OK;` |
|       40 | 13301 |  |
|        - | 13302 | `/*` |
|        - | 13303 | ` * End element handler.` |
|        - | 13304 | ` * The user defined callback must accept two parameters:` |
|        - | 13305 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13306 | ` *  $parser` |
|        - | 13307 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13308 | ` *  $name` |
|        - | 13309 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13310 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13311 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13312 | ` *   can also be supplied.` |
|        - | 13313 | ` */` |
|       62 | 13314 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13315 |  |
|       63 | 13316 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13317 | `	ph7_value *pCallback,*pTag;` |
|        - | 13318 | `	/* Point to the target user defined callback */` |
|       63 | 13319 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13320 | `	/* Make sure the given callback is callable */` |
|       63 | 13321 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13322 | `		/* Not callable,return immediately*/` |
|        9 | 13323 | `		return SXRET_OK;` |
|        - | 13324 | `	}` |
|        - | 13325 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13326 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13327 | `	if( pTag == 0  ){` |
|      ! 0 | 13328 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13329 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13330 | `		return SXRET_OK;` |
|        - | 13331 | `	}` |
|        - | 13332 | `	/* Invoke the user callback */` |
|       55 | 13333 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13334 | `	/* Clean-up the mess left behind */` |
|       55 | 13335 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13336 | `	return SXRET_OK;` |
|       32 | 13337 |  |
|        - | 13338 | `/*` |
|        - | 13339 | ` * Character data handler.` |
|        - | 13340 | ` *  The user defined callback must accept two parameters:` |
|        - | 13341 | ` *  handler(resource $parser,string $data)` |
|        - | 13342 | ` *  $parser` |
|        - | 13343 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13344 | ` *  $data` |
|        - | 13345 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13346 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13347 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13348 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13349 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13350 | ` */` |
|       28 | 13351 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13352 |  |
|       29 | 13353 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13354 | `	ph7_value *pCallback,*pData;` |
|        - | 13355 | `	/* Point to the target user defined callback */` |
|       29 | 13356 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13357 | `	/* Make sure the given callback is callable */` |
|       29 | 13358 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13359 | `		/* Not callable,return immediately*/` |
|       11 | 13360 | `		return SXRET_OK;` |
|        - | 13361 | `	}` |
|        - | 13362 | `	/* Create a ph7_value holding the data */` |
|       19 | 13363 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13364 | `	if( pData == 0  ){` |
|        - | 13365 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13366 | `		return SXRET_OK;` |
|        - | 13367 | `	}` |
|        - | 13368 | `	/* Invoke the user callback */` |
|       19 | 13369 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13370 | `	/* Clean-up the mess left behind */` |
|       19 | 13371 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13372 | `	return SXRET_OK;` |
|       15 | 13373 |  |
|        - | 13374 | `/*` |
|        - | 13375 | ` * Processing instruction (PI) handler.` |
|        - | 13376 | ` * The user defined callback must accept two parameters:` |
|        - | 13377 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13378 | ` *  $parser` |
|        - | 13379 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13380 | ` *  $target` |
|        - | 13381 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13382 | ` *  $data` |
|        - | 13383 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13384 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13385 | ` *    and a method name can also be supplied.` |
|        - | 13386 | ` */` |
|        8 | 13387 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13388 |  |
|        9 | 13389 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13390 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13391 | `	/* Point to the target user defined callback */` |
|        9 | 13392 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13393 | `	/* Make sure the given callback is callable */` |
|        9 | 13394 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13395 | `		/* Not callable,return immediately*/` |
|        5 | 13396 | `		return SXRET_OK;` |
|        - | 13397 | `	}` |
|        - | 13398 | `	/* Get a ph7_value holding the data */` |
|        5 | 13399 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13400 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13401 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13402 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13403 | `		return SXRET_OK;` |
|        - | 13404 | `	}` |
|        - | 13405 | `	/* Invoke the user callback */` |
|        5 | 13406 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13407 | `	/* Clean-up the mess left behind */` |
|        5 | 13408 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13409 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13410 | `	return SXRET_OK;` |
|        5 | 13411 |  |
|        - | 13412 | `/*` |
|        - | 13413 | ` * Namespace declaration handler.` |
|        - | 13414 | ` * The user defined callback must accept two parameters:` |
|        - | 13415 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13416 | ` * $parser` |
|        - | 13417 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13418 | ` * $prefix` |
|        - | 13419 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13420 | ` * $uri` |
|        - | 13421 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13422 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13423 | ` *   and a method name can also be supplied.` |
|        - | 13424 | ` */` |
|        4 | 13425 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13426 |  |
|        5 | 13427 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13428 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13429 | `	/* Point to the target user defined callback */` |
|        5 | 13430 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13431 | `	/* Make sure the given callback is callable */` |
|        5 | 13432 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13433 | `		/* Not callable,return immediately*/` |
|        3 | 13434 | `		return SXRET_OK;` |
|        - | 13435 | `	}` |
|        - | 13436 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13437 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13438 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13439 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13440 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13441 | `		return SXRET_OK;` |
|        - | 13442 | `	}` |
|        - | 13443 | `	/* Invoke the user callback */` |
|        3 | 13444 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13445 | `	/* Clean-up the mess left behind */` |
|        3 | 13446 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13447 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13448 | `	return SXRET_OK;` |
|        3 | 13449 |  |
|        - | 13450 | `/*` |
|        - | 13451 | ` * Namespace end declaration handler.` |
|        - | 13452 | ` * The user defined callback must accept two parameters:` |
|        - | 13453 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13454 | ` * $parser` |
|        - | 13455 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13456 | ` * $prefix` |
|        - | 13457 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13458 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13459 | ` *   and a method name can also be supplied.` |
|        - | 13460 | ` */` |
|        4 | 13461 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13462 |  |
|        5 | 13463 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13464 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13465 | `	/* Point to the target user defined callback */` |
|        5 | 13466 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13467 | `	/* Make sure the given callback is callable */` |
|        5 | 13468 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13469 | `		/* Not callable,return immediately*/` |
|        3 | 13470 | `		return SXRET_OK;` |
|        - | 13471 | `	}` |
|        - | 13472 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13473 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13474 | `	if( pPrefix == 0 ){` |
|        - | 13475 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13476 | `		return SXRET_OK;` |
|        - | 13477 | `	}` |
|        - | 13478 | `	/* Invoke the user callback */` |
|        3 | 13479 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13480 | `	/* Clean-up the mess left behind */` |
|        3 | 13481 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13482 | `	return SXRET_OK;` |
|        3 | 13483 |  |
|        - | 13484 | `/*` |
|        - | 13485 | ` * Error Message consumer handler.` |
|        - | 13486 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13487 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13488 | ` * underlying XML parser.` |
|        - | 13489 | ` */` |
|       34 | 13490 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13491 |  |
|       35 | 13492 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13493 | `	/* Save the error code */` |
|       35 | 13494 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13495 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13496 | `	if( pToken ){` |
|       35 | 13497 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13498 | `	}` |
|        - | 13499 | `	/* Abort XML processing immediately */` |
|       35 | 13500 | `	return SXERR_ABORT;` |
|        1 | 13501 |  |
|        - | 13502 | `/*` |
|        - | 13503 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13504 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13505 | ` *  as many times as necessary.` |
|        - | 13506 | ` * Parameters` |
|        - | 13507 | ` *  $parser` |
|        - | 13508 | ` *   A reference to the XML parser.` |
|        - | 13509 | ` *  $data` |
|        - | 13510 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13511 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13512 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13513 | ` * $is_final` |
|        - | 13514 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13515 | ` *   entirely loaded in memory.` |
|        - | 13516 | ` * Return` |
|        - | 13517 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13518 | ` */` |
|       74 | 13519 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13520 |  |
|        - | 13521 | `	ph7_xml_engine *pEngine;` |
|        - | 13522 | `	SyXMLParser *pParser;` |
|        - | 13523 | `	const char *zData;` |
|        - | 13524 | `	int nByte;` |
|       75 | 13525 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13526 | `		/* Missing/Ivalid arguments,return FALSE */` |
|      ! 0 | 13527 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13528 | `		return PH7_OK;` |
|        - | 13529 | `	}` |
|        - | 13530 | `	/* Point to the XML engine */` |
|       75 | 13531 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       75 | 13532 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13533 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13534 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13535 | `		return PH7_OK;` |
|        - | 13536 | `	}` |
|       75 | 13537 | `	if( pEngine->iNest > 0 ){` |
|        - | 13538 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13539 | `		 * in it's body which is forbidden.` |
|        - | 13540 | `		 */` |
|      ! 0 | 13541 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13542 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13543 | `			ph7_function_name(pCtx)` |
|        - | 13544 | `			);` |
|        - | 13545 | `		/* Return FALSE */` |
|      ! 0 | 13546 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13547 | `		return PH7_OK;` |
|        - | 13548 | `	}` |
|       75 | 13549 | `	pEngine->pCtx = pCtx;` |
|        - | 13550 | `	/* Point to the underlying XML parser */` |
|       75 | 13551 | `	pParser = &pEngine->sParser;` |
|        - | 13552 | `	/* Register elements handler */` |
|       75 | 13553 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13554 | `		VmXMLStartElementHandler,` |
|        - | 13555 | `		VmXMLTextHandler,` |
|        - | 13556 | `		VmXMLErrorHandler,` |
|        - | 13557 |  |
|        - | 13558 | `		VmXMLEndElementHandler,` |
|        - | 13559 | `		VmXMLPIHandler,` |
|        - | 13560 |  |
|        - | 13561 |  |
|        - | 13562 | `		VmXMLNSStartHandler,` |
|        - | 13563 | `		VmXMLNSEndHandler` |
|        - | 13564 | `		);` |
|       75 | 13565 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13566 | `	/* Extract the raw XML input */` |
|       75 | 13567 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13568 | `	/* Start the parse process */` |
|       75 | 13569 | `	pEngine->iNest++;` |
|       75 | 13570 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       75 | 13571 | `	pEngine->iNest--;` |
|        - | 13572 | `	/* Return the parse result */` |
|       75 | 13573 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       75 | 13574 | `	return PH7_OK;` |
|       38 | 13575 |  |
|        - | 13576 | `/*` |
|        - | 13577 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13578 | ` *  Sets an option in an XML parser.` |
|        - | 13579 | ` * Parameters` |
|        - | 13580 | ` *  $parser` |
|        - | 13581 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13582 | ` *  $option` |
|        - | 13583 | ` *    Which option to set. See below.` |
|        - | 13584 | ` *   The following options are available:` |
|        - | 13585 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13586 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13587 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13588 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13589 | ` * $value` |
|        - | 13590 | ` *   The option's new value.` |
|        - | 13591 | ` * Return` |
|        - | 13592 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13593 | ` * Note:` |
|        - | 13594 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13595 | ` *  function is a no-op.` |
|        - | 13596 | ` */` |
|        6 | 13597 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13598 |  |
|        - | 13599 | `	ph7_xml_engine *pEngine;` |
|        7 | 13600 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13601 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13602 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13603 | `		return PH7_OK;` |
|        - | 13604 | `	}` |
|        - | 13605 | `	/* Point to the XML engine */` |
|        7 | 13606 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13607 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13608 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13609 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13610 | `		return PH7_OK;` |
|        - | 13611 | `	}` |
|        - | 13612 | `	/* Always return FALSE */` |
|        7 | 13613 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13614 | `	return PH7_OK;` |
|        4 | 13615 |  |
|        - | 13616 | `/*` |
|        - | 13617 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13618 | ` *  Get options from an XML parser.` |
|        - | 13619 | ` * Parameters` |
|        - | 13620 | ` *  $parser` |
|        - | 13621 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13622 | ` * $option` |
|        - | 13623 | ` *   Which option to fetch.` |
|        - | 13624 | ` * Return` |
|        - | 13625 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13626 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13627 | ` */` |
|        2 | 13628 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13629 |  |
|        - | 13630 | `	ph7_xml_engine *pEngine;` |
|        - | 13631 | `	int nOp;` |
|        3 | 13632 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13633 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13634 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13635 | `		return PH7_OK;` |
|        - | 13636 | `	}` |
|        - | 13637 | `	/* Point to the XML engine */` |
|        3 | 13638 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13639 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13640 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13641 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13642 | `		return PH7_OK;` |
|        - | 13643 | `	}` |
|        - | 13644 | `	/* Extract the option */` |
|        3 | 13645 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13646 | `	switch(nOp){` |
|      ! 0 | 13647 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13648 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13649 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13650 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13651 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13652 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13653 | `		break;` |
|        1 | 13654 | `	default:` |
|        - | 13655 | `		/* Unknown option,return FALSE*/` |
|        3 | 13656 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13657 | `		break;` |
|        - | 13658 | `	}` |
|        3 | 13659 | `	return PH7_OK;` |
|        2 | 13660 |  |
|        - | 13661 | `/*` |
|        - | 13662 | ` * string xml_error_string(int $code)` |
|        - | 13663 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13664 | ` * Parameters` |
|        - | 13665 | ` *  $code` |
|        - | 13666 | ` *   An error code from xml_get_error_code().` |
|        - | 13667 | ` * Return` |
|        - | 13668 | ` *  Returns a string with a textual description of the error` |
|        - | 13669 | ` *  code, or FALSE if no description was found.` |
|        - | 13670 | ` */` |
|       30 | 13671 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13672 |  |
|       31 | 13673 | `	int nErr = -1;` |
|       31 | 13674 | `	if( nArg > 0 ){` |
|       31 | 13675 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13676 | `	}` |
|       31 | 13677 | `	switch(nErr){` |
|        1 | 13678 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13679 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13680 | `		break;` |
|      ! 0 | 13681 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13682 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13683 | `		break;` |
|      ! 0 | 13684 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13685 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13686 | `		break;` |
|        3 | 13687 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13688 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13689 | `		break;` |
|      ! 0 | 13690 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13691 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13692 | `		break;` |
|        1 | 13693 | `	case SXML_ERROR_NONE:` |
|        3 | 13694 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13695 | `		break;` |
|        1 | 13696 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13697 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13698 | `		break;` |
|      ! 0 | 13699 | `	case -1:` |
|      ! 0 | 13700 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13701 | `		break;` |
|        9 | 13702 | `	default:` |
|       19 | 13703 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13704 | `		break;` |
|        - | 13705 | `	}` |
|       31 | 13706 | `	return PH7_OK;` |
|        1 | 13707 |  |
|        - | 13708 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13709 | `/*` |
|        - | 13710 | ` * int utf8_encode(string $input)` |
|        - | 13711 | ` *  UTF-8 encoding.` |
|        - | 13712 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13713 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13714 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13715 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13716 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13717 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13718 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13719 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13720 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13721 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13722 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13723 | ` * Parameters` |
|        - | 13724 | ` * $input` |
|        - | 13725 | ` *   String to encode or NULL on failure.` |
|        - | 13726 | ` * Return` |
|        - | 13727 | ` *  An UTF-8 encoded string.` |
|        - | 13728 | ` */` |
|        2 | 13729 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13730 |  |
|        - | 13731 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13732 | `	int nByte,c,e;` |
|        3 | 13733 | `	if( nArg < 1 ){` |
|        - | 13734 | `		/* Missing arguments,return null */` |
|      ! 0 | 13735 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13736 | `		return PH7_OK;` |
|        - | 13737 | `	}` |
|        - | 13738 | `	/* Extract the target string */` |
|        3 | 13739 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13740 | `	if( nByte < 1 ){` |
|        - | 13741 | `		/* Empty string,return null */` |
|      ! 0 | 13742 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13743 | `		return PH7_OK;` |
|        - | 13744 | `	}` |
|        3 | 13745 | `	zEnd = &zIn[nByte];` |
|        - | 13746 | `	/* Start the encoding process */` |
|        2 | 13747 | `	for(;;){` |
|        5 | 13748 | `		if( zIn >= zEnd ){` |
|        - | 13749 | `			/* End of input */` |
|        3 | 13750 | `			break;` |
|        - | 13751 | `		}` |
|        3 | 13752 | `		c = zIn[0];` |
|        - | 13753 | `		/* Advance the stream cursor */` |
|        3 | 13754 | `		zIn++;` |
|        - | 13755 | `		/* Encode */` |
|        3 | 13756 | `		if( c<0x00080 ){` |
|      ! 0 | 13757 | `			e = (c&0xFF);` |
|      ! 0 | 13758 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13759 | `		}else if( c<0x00800 ){` |
|        3 | 13760 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13761 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13762 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13763 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13764 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13765 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13766 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13767 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13768 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13769 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13770 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13771 | `		}else{` |
|      ! 0 | 13772 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13773 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13774 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13775 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13776 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13777 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13778 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13779 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13780 | `		}` |
|        1 | 13781 | `	}` |
|        - | 13782 | `	/* All done */` |
|        3 | 13783 | `	return PH7_OK;` |
|        2 | 13784 |  |
|        - | 13785 | `/*` |
|        - | 13786 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13787 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13788 | ` * Status: Public Domain` |
|        - | 13789 | ` */` |
|        - | 13790 | `/*` |
|        - | 13791 | `** This lookup table is used to help decode the first byte of` |
|        - | 13792 | `** a multi-byte UTF8 character.` |
|        - | 13793 | `*/` |
|        - | 13794 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13795 |  |
|        - | 13796 |  |
|        - | 13797 |  |
|        - | 13798 |  |
|        - | 13799 |  |
|        - | 13800 |  |
|        - | 13801 |  |
|        - | 13802 |  |
|        - | 13803 | `};` |
|        - | 13804 | `/*` |
|        - | 13805 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13806 | `**` |
|        - | 13807 | `** During translation, assume that the byte that zTerm points` |
|        - | 13808 | `** is a 0x00.` |
|        - | 13809 | `**` |
|        - | 13810 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13811 | `**` |
|        - | 13812 | `** Notes On Invalid UTF-8:` |
|        - | 13813 | `**` |
|        - | 13814 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13815 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13816 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13817 | `**` |
|        - | 13818 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13819 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13820 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13821 | `**` |
|        - | 13822 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13823 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13824 | `**     and rendered as themselves even though they are technically` |
|        - | 13825 | `**     invalid characters.` |
|        - | 13826 | `**` |
|        - | 13827 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13828 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13829 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13830 | `*/` |
|        - | 13831 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13832 | `  c = *(zIn++);                                            \` |
|        - | 13833 | `  if( c>=0xc0 ){                                           \` |
|        - | 13834 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13835 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13836 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13837 | `    }                                                      \` |
|        - | 13838 | `    if( c<0x80                                             \` |
|        - | 13839 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13840 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13841 | `  }` |
|      150 | 13842 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13843 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13844 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13845 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13846 | `){` |
|        - | 13847 | `  int c;` |
|      153 | 13848 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13849 | `  *pzNext = z;` |
|      151 | 13850 | `  return c;` |
|        1 | 13851 |  |
|        - | 13852 | `/*` |
|        - | 13853 | ` * string utf8_decode(string $data)` |
|        - | 13854 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13855 | ` * Parameters` |
|        - | 13856 | ` * data` |
|        - | 13857 | ` *  An UTF-8 encoded string.` |
|        - | 13858 | ` * Return` |
|        - | 13859 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13860 | ` */` |
|        2 | 13861 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13862 |  |
|        - | 13863 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13864 | `	int nByte,c;` |
|        3 | 13865 | `	if( nArg < 1 ){` |
|        - | 13866 | `		/* Missing arguments,return null */` |
|      ! 0 | 13867 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13868 | `		return PH7_OK;` |
|        - | 13869 | `	}` |
|        - | 13870 | `	/* Extract the target string */` |
|        3 | 13871 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13872 | `	if( nByte < 1 ){` |
|        - | 13873 | `		/* Empty string,return null */` |
|      ! 0 | 13874 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13875 | `		return PH7_OK;` |
|        - | 13876 | `	}` |
|        3 | 13877 | `	zEnd = &zIn[nByte];` |
|        - | 13878 | `	/* Start the decoding process */` |
|        5 | 13879 | `	while( zIn < zEnd ){` |
|        3 | 13880 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13881 | `		if( c == 0x0 ){` |
|      ! 0 | 13882 | `			break;` |
|        - | 13883 | `		}` |
|        3 | 13884 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13885 | `	}` |
|        3 | 13886 | `	return PH7_OK;` |
|        2 | 13887 |  |
|        - | 13888 | `/* Table of built-in VM functions. */` |
|        - | 13889 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13890 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13891 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13892 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13893 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13894 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13895 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13896 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13897 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13898 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13899 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13900 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13901 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13902 | `	    /* Constants management */` |
|        - | 13903 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13904 | `	{ "define",   vm_builtin_define               },` |
|        - | 13905 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13906 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13907 | `	   /* Class/Object functions */` |
|        - | 13908 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13909 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13910 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13911 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13912 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13913 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13914 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13915 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13916 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13917 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13918 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13919 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13920 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13921 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13922 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13923 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13924 | `	   /* Random numbers/strings generators */` |
|        - | 13925 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13926 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13927 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13928 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13929 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13930 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13931 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13932 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13933 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13934 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13935 | `	   /* Language constructs functions */` |
|        - | 13936 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13937 | `	{ "print", vm_builtin_print                   },` |
|        - | 13938 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13939 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13940 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13941 | `	  /* Variable handling functions */` |
|        - | 13942 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13943 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13944 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13945 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13946 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13947 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13948 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13949 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13950 | `	  /* Ouput control functions */` |
|        - | 13951 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13952 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13953 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13954 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13955 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13956 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13957 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13958 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13959 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13960 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13961 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13962 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13963 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13964 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13965 | `	  /* Assertion functions */` |
|        - | 13966 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13967 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13968 | `	  /* Error reporting functions */` |
|        - | 13969 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13970 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13971 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13972 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13973 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13974 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13975 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13976 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13977 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13978 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13979 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13980 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13981 | `	  /* Release info */` |
|        - | 13982 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13983 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13984 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13985 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13986 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13987 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13988 | `	  /* hashmap */` |
|        - | 13989 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13990 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13991 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13992 | `	  /* URL related function */` |
|        - | 13993 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13994 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13995 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13996 | `	   /* XML processing functions */` |
|        - | 13997 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13998 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13999 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14000 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14001 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14002 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14003 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14004 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14005 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14006 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14007 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14008 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14009 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14010 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14011 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14012 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14013 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14014 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14015 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14016 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14017 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14018 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14019 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14020 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14021 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14022 | `	   /* Command line processing */` |
|        - | 14023 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14024 | `	   /* JSON encoding/decoding */` |
|        - | 14025 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14026 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14027 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14028 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14029 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14030 | `	   /* Files/URI inclusion facility */` |
|        - | 14031 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14032 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14033 | `	{ "include",      vm_builtin_include          },` |
|        - | 14034 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14035 | `	{ "require",      vm_builtin_require          },` |
|        - | 14036 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14037 | `};` |
|        - | 14038 | `/*` |
|        - | 14039 | ` * Register the built-in VM functions defined above.` |
|        - | 14040 | ` */` |
|      974 | 14041 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14042 |  |
|        - | 14043 | `	sxi32 rc;` |
|        - | 14044 | `	sxu32 n;` |
|   121752 | 14045 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14046 | `		/* Note that these special functions have access` |
|        - | 14047 | `		 * to the underlying virtual machine as their` |
|        - | 14048 | `		 * private data.` |
|        - | 14049 | `		 */` |
|   120778 | 14050 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   120778 | 14051 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14052 | `			return rc;` |
|        - | 14053 | `		}` |
|    60390 | 14054 | `	}` |
|      976 | 14055 | `	return SXRET_OK;` |
|      489 | 14056 |  |
|        - | 14057 | `/*` |
|        - | 14058 | ` * Check if the given name refer to an installed class.` |
|        - | 14059 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14060 | ` */` |
|     5644 | 14061 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14062 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14063 | `	const char *zName,  /* Name of the target class */` |
|        - | 14064 | `	sxu32 nByte,        /* zName length */` |
|        - | 14065 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14066 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14067 | `						 */` |
|        - | 14068 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14069 | `	)` |
|        2 | 14070 |  |
|        - | 14071 | `	SyHashEntry *pEntry;` |
|        - | 14072 | `	ph7_class *pClass;` |
|     2822 | 14073 | `		SXUNUSED(iNest);` |
|        - | 14074 | `	/* Perform a hash lookup */` |
|     5646 | 14075 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14076 |  |
|     5646 | 14077 | `	if( pEntry == 0 ){` |
|        - | 14078 | `		/* No such entry,return NULL */` |
|      ! 0 | 14079 | `		return 0;` |
|        - | 14080 | `	}` |
|     5646 | 14081 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     5646 | 14082 | `	if( !iLoadable ){` |
|        - | 14083 | `		/* Return the first class seen */` |
|     5100 | 14084 | `		return pClass;` |
|      ! 0 | 14085 | `	}else{` |
|        - | 14086 | `		/* Check the collision list */` |
|      548 | 14087 | `		while(pClass){` |
|      548 | 14088 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14089 | `				/* Class is loadable */` |
|      548 | 14090 | `				return pClass;` |
|        - | 14091 | `			}` |
|        - | 14092 | `			/* Point to the next entry */` |
|      ! 0 | 14093 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14094 | `		}` |
|        - | 14095 | `	}` |
|        - | 14096 | `	/* No such loadable class */` |
|      ! 0 | 14097 | `	return 0;` |
|     2824 | 14098 |  |
|        - | 14099 | `/*` |
|        - | 14100 | ` * Reference Table Implementation` |
|        - | 14101 | ` * Status: stable <chm@symisc.net>` |
|        - | 14102 | ` * Intro` |
|        - | 14103 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14104 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14105 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14106 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14107 | ` *  Refer to the official for more information on this powerful` |
|        - | 14108 | ` *  extension.` |
|        - | 14109 | ` */` |
|        - | 14110 | `/*` |
|        - | 14111 | ` * Allocate a new reference entry.` |
|        - | 14112 | ` */` |
|   614266 | 14113 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14114 |  |
|        - | 14115 | `	VmRefObj *pRef;` |
|        - | 14116 | `	/* Allocate a new instance */` |
|   614268 | 14117 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|   614268 | 14118 | `	if( pRef == 0 ){` |
|      ! 0 | 14119 | `		return 0;` |
|        - | 14120 | `	}` |
|        - | 14121 | `	/* Zero the structure */` |
|   614268 | 14122 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14123 | `	/* Initialize fields */` |
|   614268 | 14124 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|   614268 | 14125 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|   614268 | 14126 | `	pRef->nIdx = nIdx;` |
|   614268 | 14127 | `	return pRef;` |
|   307135 | 14128 |  |
|        - | 14129 | `/*` |
|        - | 14130 | ` * Default hash function used by the reference table` |
|        - | 14131 | ` * for lookup/insertion operations.` |
|        - | 14132 | ` */` |
|  2789526 | 14133 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14134 |  |
|        - | 14135 | `	/* Calculate the hash based on the memory object index */` |
|  2789528 | 14136 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14137 |  |
|        - | 14138 | `/*` |
|        - | 14139 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14140 | ` * in the reference table.` |
|        - | 14141 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14142 | ` * otherwise.` |
|        - | 14143 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14144 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14145 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14146 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14147 | ` * Refer to the official for more information on this powerful` |
|        - | 14148 | ` * extension.` |
|        - | 14149 | ` */` |
|  1820218 | 14150 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14151 |  |
|        - | 14152 | `	VmRefObj *pRef;` |
|        - | 14153 | `	sxu32 nBucket;` |
|        - | 14154 | `	/* Point to the appropriate bucket */` |
|  1820220 | 14155 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14156 | `	/* Perform the lookup */` |
|  1820220 | 14157 | `	pRef = pVm->apRefObj[nBucket];` |
|  5305470 | 14158 | `	for(;;){` |
| 10601344 | 14159 | `		if( pRef == 0 ){` |
|   664540 | 14160 | `			break;` |
|        - | 14161 | `		}` |
|  9936806 | 14162 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14163 | `			/* Entry found */` |
|  1155682 | 14164 | `			return pRef;` |
|        - | 14165 | `		}` |
|        - | 14166 | `		/* Point to the next entry */` |
|  8781126 | 14167 | `		pRef = pRef->pNextCollide;` |
|        2 | 14168 | `	}` |
|        - | 14169 | `	/* No such entry,return NULL */` |
|   664540 | 14170 | `	return 0;` |
|   910111 | 14171 |  |
|        - | 14172 | `/*` |
|        - | 14173 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14174 | ` *` |
|        - | 14175 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14176 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14177 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14178 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14179 | ` * Refer to the official for more information on this powerful` |
|        - | 14180 | ` * extension.` |
|        - | 14181 | ` */` |
|   614266 | 14182 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14183 |  |
|        - | 14184 | `	sxu32 nBucket;` |
|   614268 | 14185 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14186 | `		VmRefObj **apNew;` |
|        - | 14187 | `		sxu32 nNew;` |
|        - | 14188 | `		/* Allocate a larger table */` |
|     1186 | 14189 | `		nNew = pVm->nRefSize << 1;` |
|     1186 | 14190 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1186 | 14191 | `		if( apNew ){` |
|     1186 | 14192 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14193 | `			sxu32 n;` |
|        - | 14194 | `			/* Zero the structure */` |
|     1186 | 14195 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14196 | `			/* Rehash all referenced entries */` |
|    98424 | 14197 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14198 | `				/* Remove old collision links */` |
|    97240 | 14199 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14200 | `				/* Point to the appropriate bucket */` |
|    97240 | 14201 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14202 | `				/* Insert the entry  */` |
|    97240 | 14203 | `				pEntry->pNextCollide = apNew[nBucket];` |
|    97240 | 14204 | `				if( apNew[nBucket] ){` |
|    82784 | 14205 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|    41391 | 14206 | `				}` |
|    97240 | 14207 | `				apNew[nBucket] = pEntry;` |
|        - | 14208 | `				/* Point to the next entry */` |
|    97240 | 14209 | `				pEntry = pEntry->pNext;` |
|    48621 | 14210 | `			}` |
|        - | 14211 | `			/* Release the old table */` |
|     1186 | 14212 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14213 | `			/* Install the new one */` |
|     1186 | 14214 | `			pVm->apRefObj = apNew;` |
|     1186 | 14215 | `			pVm->nRefSize = nNew;` |
|      592 | 14216 | `		}` |
|      592 | 14217 | `	}` |
|        - | 14218 | `	/* Point to the appropriate bucket */` |
|   614268 | 14219 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14220 | `	/* Insert the entry */` |
|   614268 | 14221 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|   614268 | 14222 | `	if( pVm->apRefObj[nBucket] ){` |
|   574157 | 14223 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   287558 | 14224 | `	}` |
|   614268 | 14225 | `	pVm->apRefObj[nBucket] = pRef;` |
|   614268 | 14226 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|   614268 | 14227 | `	pVm->nRefUsed++;` |
|   614268 | 14228 | `	return SXRET_OK;` |
|        2 | 14229 |  |
|        - | 14230 | `/*` |
|        - | 14231 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14232 | ` * the reference table.` |
|        - | 14233 | ` * This function is invoked when the user perform an unset` |
|        - | 14234 | ` * call [i.e: unset($var); ].` |
|        - | 14235 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14236 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14237 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14238 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14239 | ` * Refer to the official for more information on this powerful` |
|        - | 14240 | ` * extension.` |
|        - | 14241 | ` */` |
|   598314 | 14242 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14243 |  |
|        - | 14244 | `	ph7_hashmap_node **apNode;` |
|        - | 14245 | `	SyHashEntry **apEntry;` |
|        - | 14246 | `	sxu32 n;` |
|        - | 14247 | `	/* Point to the reference table */` |
|   598316 | 14248 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   598316 | 14249 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14250 | `	/* Unlink the entry from the reference table */` |
|   652778 | 14251 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    54464 | 14252 | `		if( apEntry[n] ){` |
|    54432 | 14253 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    27215 | 14254 | `		}` |
|    27233 | 14255 | `	}` |
|  1146004 | 14256 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   547690 | 14257 | `		if( apNode[n] ){` |
|     4879 | 14258 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2439 | 14259 | `		}` |
|   273846 | 14260 | `	}` |
|   598316 | 14261 | `	if( pRef->pPrevCollide ){` |
|   340512 | 14262 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   170774 | 14263 | `	}else{` |
|   257806 | 14264 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14265 | `	}` |
|   598316 | 14266 | `	if( pRef->pNextCollide ){` |
|   552467 | 14267 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   276698 | 14268 | `	}` |
|   598316 | 14269 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14270 | `	/* Release the node */` |
|   598316 | 14271 | `	SySetRelease(&pRef->aReference);` |
|   598316 | 14272 | `	SySetRelease(&pRef->aArrEntries);` |
|   598316 | 14273 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|   598316 | 14274 | `	pVm->nRefUsed--;` |
|   598316 | 14275 | `	return SXRET_OK;` |
|        2 | 14276 |  |
|        - | 14277 | `/*` |
|        - | 14278 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14279 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14280 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14281 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14282 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14283 | ` * Refer to the official for more information on this powerful` |
|        - | 14284 | ` * extension.` |
|        - | 14285 | ` */` |
|   628784 | 14286 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14287 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14288 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14289 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14290 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14291 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14292 | `	)` |
|        2 | 14293 |  |
|   628786 | 14294 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14295 | `	VmRefObj *pRef;` |
|        - | 14296 | `	/* Check if the referenced object already exists */` |
|   628786 | 14297 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   628786 | 14298 | `	if( pRef == 0 ){` |
|        - | 14299 | `		/* Create a new entry */` |
|   614268 | 14300 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|   614268 | 14301 | `		if( pRef == 0 ){` |
|      ! 0 | 14302 | `			return SXERR_MEM;` |
|        - | 14303 | `		}` |
|   614268 | 14304 | `		pRef->iFlags = iFlags;` |
|        - | 14305 | `		/* Install the entry */` |
|   614268 | 14306 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   307133 | 14307 | `	}` |
|   633698 | 14308 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14309 | `		/* Safely ignore the exception frame */` |
|     4914 | 14310 | `		pFrame = pFrame->pParent;` |
|        2 | 14311 | `	}` |
|   628786 | 14312 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14313 | `		VmSlot sRef;` |
|        - | 14314 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14315 | `		 * be deleted when we leave this frame.` |
|        - | 14316 | `		 */` |
|    50286 | 14317 | `		sRef.nIdx = nIdx;` |
|    50286 | 14318 | `		sRef.pUserData = pEntry;` |
|    50286 | 14319 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14320 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14321 | `		}` |
|    25142 | 14322 | `	}` |
|   628786 | 14323 | `	if( pEntry ){` |
|        - | 14324 | `		/* Address of the hash-entry */` |
|    64638 | 14325 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    32318 | 14326 | `	}` |
|   628786 | 14327 | `	if( pMapEntry ){` |
|        - | 14328 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   562100 | 14329 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   281049 | 14330 | `	}` |
|   628786 | 14331 | `	return SXRET_OK;` |
|   314394 | 14332 |  |
|        - | 14333 | `/*` |
|        - | 14334 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14335 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14336 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14337 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14338 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14339 | ` * Refer to the official for more information on this powerful` |
|        - | 14340 | ` * extension.` |
|        - | 14341 | ` */` |
|   593100 | 14342 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14343 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14344 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14345 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14346 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14347 | `	)` |
|        2 | 14348 |  |
|        - | 14349 | `	VmRefObj *pRef;` |
|        - | 14350 | `	sxu32 n;` |
|        - | 14351 | `	/* Check if the referenced object already exists */` |
|   593102 | 14352 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   593102 | 14353 | `	if( pRef == 0 ){` |
|        - | 14354 | `		/* Not such entry */` |
|    50254 | 14355 | `		return SXERR_NOTFOUND;` |
|        - | 14356 | `	}` |
|        - | 14357 | `	/* Remove the desired entry */` |
|   542850 | 14358 | `	if( pEntry ){` |
|        - | 14359 | `		SyHashEntry **apEntry;` |
|       33 | 14360 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14361 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14362 | `			if( apEntry[n] == pEntry ){` |
|        - | 14363 | `				/* Nullify the entry */` |
|       33 | 14364 | `				apEntry[n] = 0;` |
|        - | 14365 | `				/*` |
|        - | 14366 | `				 * NOTE:` |
|        - | 14367 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14368 | `				 * we avoid wasting spaces.` |
|        - | 14369 | `				 */` |
|       16 | 14370 | `			}` |
|       49 | 14371 | `		}` |
|       16 | 14372 | `	}` |
|   542850 | 14373 | `	if( pMapEntry ){` |
|        - | 14374 | `		ph7_hashmap_node **apNode;` |
|   542818 | 14375 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  1085722 | 14376 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   542906 | 14377 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14378 | `				/* nullify the entry */` |
|   542818 | 14379 | `				apNode[n] = 0;` |
|   271408 | 14380 | `			}` |
|   271454 | 14381 | `		}` |
|   271408 | 14382 | `	}` |
|   542850 | 14383 | `	return SXRET_OK;` |
|   296552 | 14384 |  |
|        - | 14385 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14386 | `/*` |
|        - | 14387 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14388 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14389 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14390 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14391 | ` * For more information on how to register IO stream devices,please` |
|        - | 14392 | ` * refer to the official documentation.` |
|        - | 14393 | ` */` |
|    18386 | 14394 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14395 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14396 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14397 | `	int nByte              /* *pzDevice length*/` |
|        - | 14398 | `	)` |
|        2 | 14399 |  |
|        - | 14400 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14401 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14402 | `	SyString sDev,sCur;` |
|        - | 14403 | `	sxu32 n,nEntry;` |
|        - | 14404 | `	int rc;` |
|        - | 14405 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    18388 | 14406 | `	zNext = zCur = zIn = *pzDevice;` |
|    18388 | 14407 | `	zEnd = &zIn[nByte];` |
|  1115265 | 14408 | `	while( zIn < zEnd ){` |
|  1096881 | 14409 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14410 | `			/* Got one */` |
|        3 | 14411 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14412 | `			break;` |
|        - | 14413 | `		}` |
|        - | 14414 | `		/* Advance the cursor */` |
|  1096879 | 14415 | `		zIn++;` |
|        2 | 14416 | `	}` |
|    18388 | 14417 | `	if( zIn >= zEnd ){` |
|        - | 14418 | `		/* No such scheme,return the default stream */` |
|    18386 | 14419 | `		return pVm->pDefStream;` |
|        - | 14420 | `	}` |
|        3 | 14421 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14422 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14423 | `	SyStringFullTrim(&sDev);` |
|        - | 14424 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14425 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14426 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14427 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14428 | `		pStream = apStream[n];` |
|        3 | 14429 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14430 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14431 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14432 | `		if( rc == 0 ){` |
|        - | 14433 | `			/* Stream device found */` |
|        3 | 14434 | `			*pzDevice = zNext;` |
|        3 | 14435 | `			return pStream;` |
|        - | 14436 | `		}` |
|      ! 0 | 14437 | `	}` |
|        - | 14438 | `	/* No such stream,return NULL */` |
|      ! 0 | 14439 | `	return 0;` |
|     9195 | 14440 |  |
|        - | 14441 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14442 | `/*` |
|        - | 14443 | ` * Section:` |
|        - | 14444 | ` *    HTTP/URI related routines.` |
|        - | 14445 | ` * Status:` |
|        - | 14446 | ` *    Stable.` |
|        - | 14447 | ` */` |
|        - | 14448 | ` /*` |
|        - | 14449 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14450 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14451 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14452 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14453 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14454 | `  * the input to its fields.` |
|        - | 14455 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14456 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14457 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14458 | `  * input is encountered.` |
|        - | 14459 | `  */` |
|       26 | 14460 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14461 | ` {` |
|       27 | 14462 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14463 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14464 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14465 | `	 const char *zCur;` |
|        - | 14466 | `	 SyString *pComp;` |
|       27 | 14467 | `	 sxu32 nPos = 0;` |
|        - | 14468 | `	 sxi32 rc;` |
|        - | 14469 | `	 /* Zero the structure first */` |
|       27 | 14470 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14471 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14472 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14473 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14474 | `	 /* Find the first '/' separator */` |
|       27 | 14475 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14476 | `	 if( rc != SXRET_OK ){` |
|        - | 14477 | `		 /* Assume a host name only */` |
|        7 | 14478 | `		 zCur = zEnd;` |
|        7 | 14479 | `		 bHostOnly = TRUE;` |
|        7 | 14480 | `		 goto ProcessHost;` |
|        - | 14481 | `	 }` |
|       21 | 14482 | `	 zCur = &zUri[nPos];` |
|       21 | 14483 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14484 | `		 /* Extract a scheme:` |
|        - | 14485 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14486 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14487 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14488 | `		  * fail.` |
|        - | 14489 | `		  */` |
|       19 | 14490 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14491 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14492 | `		SyStringLeftTrim(pComp);` |
|        9 | 14493 | `	 }` |
|       21 | 14494 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14495 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14496 | `		  /* No authority */` |
|      ! 0 | 14497 | `		  goto PathSplit;` |
|        - | 14498 | `		}` |
|        - | 14499 | `		 /* There is something here , we will assume its an authority` |
|        - | 14500 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14501 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14502 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14503 | `		  * and let the caller handle all the validation process.` |
|        - | 14504 | `		  */` |
|      ! 0 | 14505 | `		 goto ProcessHost;` |
|        - | 14506 | `	 }` |
|       21 | 14507 | `	 zUri = &zCur[2];` |
|       21 | 14508 | `	 zCur = zEnd;` |
|       21 | 14509 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14510 | `	 if( rc == SXRET_OK ){` |
|       17 | 14511 | `		 zCur = &zUri[nPos];` |
|        8 | 14512 | `	 }` |
|        2 | 14513 | ` ProcessHost:` |
|        - | 14514 | `	 /* Extract user information if present */` |
|       27 | 14515 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14516 | `	 if( rc == SXRET_OK ){` |
|        7 | 14517 | `		 if( nPos > 0 ){` |
|        - | 14518 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14519 | `			 pComp = &pOut->sUser;` |
|        7 | 14520 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14521 | `			 /* Extract the password if available */` |
|        7 | 14522 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14523 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14524 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14525 | `				 pComp = &pOut->sPass;` |
|        7 | 14526 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14527 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14528 | `			 }` |
|        - | 14529 | `			 /* Update the cursor */` |
|        7 | 14530 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14531 | `		 }else{` |
|      ! 0 | 14532 | `			 zUri++;` |
|        - | 14533 | `		 }` |
|        3 | 14534 | `	 }` |
|       27 | 14535 | `	 pComp = &pOut->sHost;` |
|       27 | 14536 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14537 | `		 zUri++;` |
|      ! 0 | 14538 | `	 }` |
|       27 | 14539 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14540 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14541 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14542 | `		  */` |
|        3 | 14543 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14544 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14545 | `			 zUri++; pComp->nByte++;` |
|        1 | 14546 | `		 }` |
|        3 | 14547 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14548 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14549 | `		 }` |
|        3 | 14550 | `		 zUri++;` |
|        3 | 14551 | `		 bIPv6 = TRUE;` |
|        1 | 14552 | `	 }` |
|        - | 14553 | `	 /* Extract a port number if available */` |
|       27 | 14554 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14555 | `	 if( rc == SXRET_OK ){` |
|       11 | 14556 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14557 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14558 | `		 }` |
|       11 | 14559 | `		 pComp = &pOut->sPort;` |
|       11 | 14560 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14561 | `	 }` |
|       27 | 14562 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14563 | `		 return SXRET_OK;` |
|        - | 14564 | `	 }` |
|       10 | 14565 | `PathSplit:` |
|       21 | 14566 | `	 zUri = zCur;` |
|       21 | 14567 | `	 pComp = &pOut->sPath;` |
|       21 | 14568 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14569 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14570 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14571 | `	 }` |
|       17 | 14572 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14573 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14574 | `		 pComp = &pOut->sQuery;` |
|        5 | 14575 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14576 | `	 }` |
|       17 | 14577 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14578 | `		 /* Update path or query length */` |
|        5 | 14579 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14580 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14581 | `		 }else{` |
|        5 | 14582 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14583 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14584 | `				 return SXERR_SYNTAX;` |
|        - | 14585 | `			 }` |
|        5 | 14586 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14587 | `		 }` |
|        5 | 14588 | `		 pComp = &pOut->sFragment;` |
|        5 | 14589 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14590 | `	 }` |
|       17 | 14591 | `	 return SXRET_OK;` |
|       14 | 14592 | ` }` |
|        - | 14593 | ` /*` |
|        - | 14594 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14595 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14596 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14597 | ` */` |
|      ! 0 | 14598 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14599 |  |
|        - | 14600 | `  	const char *zIn;` |
|        - | 14601 | `  	sxu32 nPos;` |
|        - | 14602 | `	/* Jump leading white spaces */` |
|      ! 0 | 14603 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14604 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14605 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14606 | `		return SXERR_EOF; /* End of input */` |
|        - | 14607 | `	}` |
|      ! 0 | 14608 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14609 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14610 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14611 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14612 | `		return SXERR_MORE;` |
|        - | 14613 | `	}` |
|      ! 0 | 14614 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14615 | `  	pCurrent->nByte	= nPos;` |
|        - | 14616 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14617 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14618 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14619 | `  	return SXRET_OK;` |
|      ! 0 | 14620 | ` }` |
|        - | 14621 | ` /*` |
|        - | 14622 | `  * Split a single MIME header into a name value pair.` |
|        - | 14623 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14624 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14625 | `  * is encountered.` |
|        - | 14626 | `  * Note: This function handle also mult-line headers.` |
|        - | 14627 | `  */` |
|      ! 0 | 14628 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14629 | ` {` |
|        - | 14630 | `	 SyString *pName;` |
|        - | 14631 | `	 sxu32 nPos;` |
|        - | 14632 | `	 sxi32 rc;` |
|      ! 0 | 14633 | `	 if( nLen < 1 ){` |
|      ! 0 | 14634 | `		 return SXERR_NEXT;` |
|        - | 14635 | `	 }` |
|        - | 14636 | `	 /* Check for multi-line header */` |
|      ! 0 | 14637 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14638 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14639 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14640 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14641 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14642 | `		}else{` |
|        - | 14643 | `			/* Update header value length */` |
|      ! 0 | 14644 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14645 | `		}` |
|        - | 14646 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14647 | `		 return SXERR_CONTINUE;` |
|        - | 14648 | `	 }` |
|        - | 14649 | `	/* Split the header */` |
|      ! 0 | 14650 | `	pName = &pHdr->sName;` |
|      ! 0 | 14651 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14652 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14653 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14654 | `	}` |
|      ! 0 | 14655 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14656 | `	SyStringFullTrim(pName);` |
|        - | 14657 | `	/* Extract a header value */` |
|      ! 0 | 14658 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14659 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14660 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14661 | `	return SXRET_OK;` |
|      ! 0 | 14662 | ` }` |
|        - | 14663 | ` /*` |
|        - | 14664 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14665 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14666 | `  * routine is called in order to extract MIME headers.` |
|        - | 14667 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14668 | `  * more inputs.` |
|        - | 14669 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14670 | `  */` |
|      ! 0 | 14671 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14672 | ` {` |
|      ! 0 | 14673 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14674 | `	 SyString sCurrent;` |
|        - | 14675 | `	 SyhttpHeader sHdr;` |
|        - | 14676 | `	 sxu8 bEol;` |
|        - | 14677 | `	 sxi32 rc;` |
|      ! 0 | 14678 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14679 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14680 | `	 }` |
|      ! 0 | 14681 | `	 bEol = FALSE;` |
|      ! 0 | 14682 | `	 for(;;){` |
|      ! 0 | 14683 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14684 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14685 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14686 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14687 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14688 | `				 break;` |
|        - | 14689 | `			 }` |
|      ! 0 | 14690 | `			 bEol = TRUE;` |
|      ! 0 | 14691 | `		 }` |
|        - | 14692 | `		 /* Process the header */` |
|      ! 0 | 14693 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14694 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14695 | `				 break;` |
|        - | 14696 | `			 }` |
|        - | 14697 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14698 | `			  * in case we face one of them.` |
|        - | 14699 | `			  */` |
|      ! 0 | 14700 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14701 | `		 }` |
|      ! 0 | 14702 | `		 if( bEol ){` |
|      ! 0 | 14703 | `			 break;` |
|        - | 14704 | `		 }` |
|      ! 0 | 14705 | `	 } /* for(;;) */` |
|      ! 0 | 14706 | `	 return SXRET_OK;` |
|      ! 0 | 14707 | ` }` |
|        - | 14708 | ` /*` |
|        - | 14709 | `  * Process the first line of a HTTP request.` |
|        - | 14710 | `  * This routine perform the following operations` |
|        - | 14711 | `  *  1) Extract the HTTP method.` |
|        - | 14712 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14713 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14714 | `  */` |
|      ! 0 | 14715 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14716 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14717 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14718 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14719 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14720 | `	 )` |
|      ! 0 | 14721 | ` {` |
|        - | 14722 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14723 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14724 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14725 | `	 SyString sLine;` |
|        - | 14726 | `	 sxu32 nLen;` |
|        - | 14727 | `	 sxi32 rc;` |
|        - | 14728 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14729 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14730 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14731 | `		 return rc;` |
|        - | 14732 | `	 }` |
|      ! 0 | 14733 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14734 | `		 /* Empty HTTP request */` |
|      ! 0 | 14735 | `		 return SXERR_EMPTY;` |
|        - | 14736 | `	 }` |
|        - | 14737 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14738 | `	 zIn = sLine.zString;` |
|      ! 0 | 14739 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14740 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14741 | `		 zIn++;` |
|      ! 0 | 14742 | `	 }` |
|        - | 14743 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14744 | `	 zPtr = zIn;` |
|      ! 0 | 14745 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14746 | `		 zIn++;` |
|      ! 0 | 14747 | `	 }` |
|      ! 0 | 14748 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14749 | `	 if( zIn > zPtr ){` |
|        - | 14750 | `		 sxu32 i;` |
|      ! 0 | 14751 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14752 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14753 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14754 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14755 | `				 break;` |
|        - | 14756 | `			 }` |
|      ! 0 | 14757 | `		 }` |
|      ! 0 | 14758 | `	 }` |
|        - | 14759 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14760 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14761 | `		 zIn++;` |
|      ! 0 | 14762 | `	 }` |
|        - | 14763 | `	  /* Extract the request URI */` |
|      ! 0 | 14764 | `	 zPtr = zIn;` |
|      ! 0 | 14765 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14766 | `		 zIn++;` |
|      ! 0 | 14767 | `	 }` |
|      ! 0 | 14768 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14769 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14770 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14771 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14772 | `	 }` |
|        - | 14773 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14774 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14775 | `		 zIn++;` |
|      ! 0 | 14776 | `	 }` |
|        - | 14777 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14778 | `	 zPtr = zIn;` |
|      ! 0 | 14779 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14780 | `		 zIn++;` |
|      ! 0 | 14781 | `	 }` |
|      ! 0 | 14782 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14783 | `	 rc = 1;` |
|      ! 0 | 14784 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14785 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14786 | `	 }` |
|      ! 0 | 14787 | `	 if( !rc ){` |
|      ! 0 | 14788 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14789 | `	 }` |
|      ! 0 | 14790 | `	 return SXRET_OK;` |
|      ! 0 | 14791 | ` }` |
|        - | 14792 | ` /*` |
|        - | 14793 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14794 | `  * into a name value pair.` |
|        - | 14795 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14796 | `  * After the tokenization process,register the decoded queries` |
|        - | 14797 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14798 | `  */` |
|      ! 0 | 14799 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14800 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14801 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14802 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14803 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14804 | `	 )` |
|      ! 0 | 14805 | ` {` |
|      ! 0 | 14806 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14807 | `	 const char *zIn = pQuery->zString;` |
|        - | 14808 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14809 | `	 SyString sName,sValue;` |
|        - | 14810 | `	 const char *zPtr;` |
|        - | 14811 | `	 sxu32 nBlobOfft;` |
|        - | 14812 | `	 /* Extract superglobals */` |
|      ! 0 | 14813 | `	 if( is_post ){` |
|        - | 14814 | `		 /* $_POST superglobal */` |
|      ! 0 | 14815 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14816 | `	 }else{` |
|        - | 14817 | `		 /* $_GET superglobal */` |
|      ! 0 | 14818 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14819 | `	 }` |
|      ! 0 | 14820 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14821 | `	 /* Split up the raw query */` |
|      ! 0 | 14822 | `	 for(;;){` |
|        - | 14823 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14824 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14825 | `			 zIn++;` |
|      ! 0 | 14826 | `		 }` |
|      ! 0 | 14827 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14828 | `			 break;` |
|        - | 14829 | `		 }` |
|      ! 0 | 14830 | `		 zPtr = zIn;` |
|      ! 0 | 14831 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14832 | `			 zPtr++;` |
|      ! 0 | 14833 | `		 }` |
|        - | 14834 | `		 /* Reset the working buffer */` |
|      ! 0 | 14835 | `		 SyBlobReset(pWorker);` |
|        - | 14836 | `		 /* Decode the entry */` |
|      ! 0 | 14837 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14838 | `		 /* Save the entry */` |
|      ! 0 | 14839 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14840 | `		 sValue.zString = 0;` |
|      ! 0 | 14841 | `		 sValue.nByte = 0;` |
|      ! 0 | 14842 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14843 | `			 zPtr++;` |
|      ! 0 | 14844 | `			 zIn = zPtr;` |
|        - | 14845 | `			 /* Store field value */` |
|      ! 0 | 14846 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14847 | `				 zPtr++;` |
|      ! 0 | 14848 | `			 }` |
|      ! 0 | 14849 | `			 if( zPtr > zIn ){` |
|        - | 14850 | `				 /* Decode the value */` |
|      ! 0 | 14851 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14852 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14853 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14854 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14855 |  |
|      ! 0 | 14856 | `			 }` |
|        - | 14857 | `			 /* Synchronize pointers */` |
|      ! 0 | 14858 | `			 zIn = zPtr;` |
|      ! 0 | 14859 | `		 }` |
|      ! 0 | 14860 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14861 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14862 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14863 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14864 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14865 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14866 | `				 );` |
|      ! 0 | 14867 | `		 }` |
|      ! 0 | 14868 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14869 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14870 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14871 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14872 | `					 );` |
|      ! 0 | 14873 | `		 }` |
|        - | 14874 | `		 /* Advance the pointer */` |
|      ! 0 | 14875 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14876 | `	 }` |
|        - | 14877 | `	/* All done*/` |
|      ! 0 | 14878 | `	return SXRET_OK;` |
|      ! 0 | 14879 | ` }` |
|        - | 14880 | ` /*` |
|        - | 14881 | `  * Extract MIME header value from the given set.` |
|        - | 14882 | `  * Return header value on success. NULL otherwise.` |
|        - | 14883 | `  */` |
|      ! 0 | 14884 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14885 | ` {` |
|        - | 14886 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14887 | `	 SyString sMime;` |
|        - | 14888 | `	 sxu32 n;` |
|      ! 0 | 14889 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14890 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14891 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14892 | `	 /* Perform the lookup */` |
|      ! 0 | 14893 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14894 | `		 pMime = &aMime[n];` |
|      ! 0 | 14895 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14896 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14897 | `			 return &pMime->sValue;` |
|        - | 14898 | `		 }` |
|      ! 0 | 14899 | `	 }` |
|        - | 14900 | `	 /* No such MIME header */` |
|      ! 0 | 14901 | `	 return 0;` |
|      ! 0 | 14902 | ` }` |
|        - | 14903 | ` /*` |
|        - | 14904 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14905 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14906 | `  */` |
|      ! 0 | 14907 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14908 | ` {` |
|      ! 0 | 14909 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14910 | `	 SyString sName,sValue;` |
|        - | 14911 | `	 ph7_value *pCookie;` |
|        - | 14912 | `	 sxu32 nOfft;` |
|        - | 14913 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14914 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14915 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14916 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14917 | `		 return SXERR_NOTFOUND;` |
|        - | 14918 | `	 }` |
|      ! 0 | 14919 | `	 for(;;){` |
|        - | 14920 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14921 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14922 | `			 zIn++;` |
|      ! 0 | 14923 | `		 }` |
|      ! 0 | 14924 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14925 | `			 break;` |
|        - | 14926 | `		 }` |
|        - | 14927 | `		  /* Reset the working buffer */` |
|      ! 0 | 14928 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14929 | `		 zDelimiter = zIn;` |
|        - | 14930 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14931 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14932 | `			 zDelimiter++;` |
|      ! 0 | 14933 | `		 }` |
|      ! 0 | 14934 | `		 zPtr = zIn;` |
|      ! 0 | 14935 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14936 | `			 zPtr++;` |
|      ! 0 | 14937 | `		 }` |
|        - | 14938 | `		 /* Decode the cookie */` |
|      ! 0 | 14939 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14940 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14941 | `		 zPtr++;` |
|      ! 0 | 14942 | `		 sValue.zString = 0;` |
|      ! 0 | 14943 | `		 sValue.nByte = 0;` |
|      ! 0 | 14944 | `		 if( zPtr < zDelimiter ){` |
|        - | 14945 | `			 /* Got a Cookie value */` |
|      ! 0 | 14946 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14947 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14948 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14949 | `		 }` |
|        - | 14950 | `		 /* Synchronize pointers */` |
|      ! 0 | 14951 | `		 zIn = &zDelimiter[1];` |
|        - | 14952 | `		 /* Perform the insertion */` |
|      ! 0 | 14953 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14954 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14955 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14956 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14957 | `			 );` |
|      ! 0 | 14958 | `	 }` |
|      ! 0 | 14959 | `	 return SXRET_OK;` |
|      ! 0 | 14960 | ` }` |
|        - | 14961 | ` /*` |
|        - | 14962 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14963 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14964 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14965 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14966 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14967 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14968 | `  * a malformed HTTP request.` |
|        - | 14969 | `  */` |
|      ! 0 | 14970 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14971 | ` {` |
|        - | 14972 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14973 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14974 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14975 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14976 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14977 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14978 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14979 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14980 | `	 sxi32 rc;` |
|      ! 0 | 14981 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14982 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 14983 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 14984 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 14985 | `	 SyStringFullTrim(&sRequest);` |
|        - | 14986 | `	 /* Process the first line */` |
|      ! 0 | 14987 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 14988 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14989 | `		 return rc;` |
|        - | 14990 | `	 }` |
|        - | 14991 | `	 /* Process MIME headers */` |
|      ! 0 | 14992 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 14993 | `	 /*` |
|        - | 14994 | `	  * Setup $_SERVER environments` |
|        - | 14995 | `	  */` |
|        - | 14996 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 14997 | `	 ph7_vm_config(pVm,` |
|        - | 14998 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14999 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 15000 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 15001 | `		 sizeof("HTTP/1.1")-1` |
|        - | 15002 | `		 );` |
|        - | 15003 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 15004 | `	 ph7_vm_config(pVm,` |
|        - | 15005 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15006 | `		 "REQUEST_METHOD",` |
|      ! 0 | 15007 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 15008 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 15009 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 15010 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 15011 | `		 -1 /* Compute attribute length automatically */` |
|        - | 15012 | `		 );` |
|      ! 0 | 15013 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 15014 | `		 pValue = &sUri.sQuery;` |
|        - | 15015 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 15016 | `		 ph7_vm_config(pVm,` |
|        - | 15017 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15018 | `			 "QUERY_STRING",` |
|      ! 0 | 15019 | `			 pValue->zString,` |
|      ! 0 | 15020 | `			 pValue->nByte` |
|        - | 15021 | `			 );` |
|        - | 15022 | `		 /* Decoded the raw query */` |
|      ! 0 | 15023 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 15024 | `	 }` |
|        - | 15025 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 15026 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 15027 | `	 ph7_vm_config(pVm,` |
|        - | 15028 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15029 | `		 "REQUEST_URI",` |
|      ! 0 | 15030 | `		 pValue->zString,` |
|      ! 0 | 15031 | `		 pValue->nByte` |
|        - | 15032 | `		 );` |
|        - | 15033 | `	 /*` |
|        - | 15034 | `	  * 'PATH_INFO'` |
|        - | 15035 | `	  * 'ORIG_PATH_INFO'` |
|        - | 15036 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 15037 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 15038 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15039 | `	  * /some/stuff.` |
|        - | 15040 | `	  */` |
|      ! 0 | 15041 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15042 | `	 ph7_vm_config(pVm,` |
|        - | 15043 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15044 | `		 "PATH_INFO",` |
|      ! 0 | 15045 | `		 pValue->zString,` |
|      ! 0 | 15046 | `		 pValue->nByte` |
|        - | 15047 | `		 );` |
|      ! 0 | 15048 | `	 ph7_vm_config(pVm,` |
|        - | 15049 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15050 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15051 | `		 pValue->zString,` |
|      ! 0 | 15052 | `		 pValue->nByte` |
|        - | 15053 | `		 );` |
|        - | 15054 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15055 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15056 | `	 if( pValue ){` |
|      ! 0 | 15057 | `		 ph7_vm_config(pVm,` |
|        - | 15058 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15059 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15060 | `			 pValue->zString,` |
|      ! 0 | 15061 | `			 pValue->nByte` |
|        - | 15062 | `		 );` |
|      ! 0 | 15063 | `	 }` |
|        - | 15064 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15065 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15066 | `	 if( pValue ){` |
|      ! 0 | 15067 | `		 ph7_vm_config(pVm,` |
|        - | 15068 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15069 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15070 | `			 pValue->zString,` |
|      ! 0 | 15071 | `			 pValue->nByte` |
|        - | 15072 | `		 );` |
|      ! 0 | 15073 | `	 }` |
|        - | 15074 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15075 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15076 | `	 if( pValue ){` |
|      ! 0 | 15077 | `		 ph7_vm_config(pVm,` |
|        - | 15078 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15079 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15080 | `			 pValue->zString,` |
|      ! 0 | 15081 | `			 pValue->nByte` |
|        - | 15082 | `		 );` |
|      ! 0 | 15083 | `	 }` |
|        - | 15084 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15085 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15086 | `	 if( pValue ){` |
|      ! 0 | 15087 | `		 ph7_vm_config(pVm,` |
|        - | 15088 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15089 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15090 | `			 pValue->zString,` |
|      ! 0 | 15091 | `			 pValue->nByte` |
|        - | 15092 | `		 );` |
|      ! 0 | 15093 | `	 }` |
|        - | 15094 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15095 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15096 | `	 if( pValue ){` |
|      ! 0 | 15097 | `		 ph7_vm_config(pVm,` |
|        - | 15098 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15099 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15100 | `			 pValue->zString,` |
|      ! 0 | 15101 | `			 pValue->nByte` |
|        - | 15102 | `		 );` |
|      ! 0 | 15103 | `	 }` |
|        - | 15104 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15105 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15106 | `	 if( pValue ){` |
|      ! 0 | 15107 | `		 ph7_vm_config(pVm,` |
|        - | 15108 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15109 | `			 "HTTP_HOST",` |
|      ! 0 | 15110 | `			 pValue->zString,` |
|      ! 0 | 15111 | `			 pValue->nByte` |
|        - | 15112 | `		 );` |
|      ! 0 | 15113 | `	 }` |
|        - | 15114 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15115 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15116 | `	 if( pValue ){` |
|      ! 0 | 15117 | `		 ph7_vm_config(pVm,` |
|        - | 15118 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15119 | `			 "HTTP_REFERER",` |
|      ! 0 | 15120 | `			 pValue->zString,` |
|      ! 0 | 15121 | `			 pValue->nByte` |
|        - | 15122 | `		 );` |
|      ! 0 | 15123 | `	 }` |
|        - | 15124 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15125 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15126 | `	 if( pValue ){` |
|      ! 0 | 15127 | `		 ph7_vm_config(pVm,` |
|        - | 15128 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15129 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15130 | `			 pValue->zString,` |
|      ! 0 | 15131 | `			 pValue->nByte` |
|        - | 15132 | `		 );` |
|      ! 0 | 15133 | `	 }` |
|        - | 15134 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15135 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15136 | `	   */` |
|      ! 0 | 15137 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15138 | `	 if( pValue ){` |
|      ! 0 | 15139 | `		 ph7_vm_config(pVm,` |
|        - | 15140 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15141 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15142 | `			 pValue->zString,` |
|      ! 0 | 15143 | `			 pValue->nByte` |
|        - | 15144 | `		 );` |
|      ! 0 | 15145 | `		 ph7_vm_config(pVm,` |
|        - | 15146 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15147 | `			 "PHP_AUTH",` |
|      ! 0 | 15148 | `			 pValue->zString,` |
|      ! 0 | 15149 | `			 pValue->nByte` |
|        - | 15150 | `		 );` |
|      ! 0 | 15151 | `	 }` |
|        - | 15152 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15153 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15154 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15155 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15156 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15157 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15158 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15159 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15160 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15161 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15162 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15163 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15164 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15165 | `				 );` |
|      ! 0 | 15166 | `		 }` |
|      ! 0 | 15167 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15168 | `			 && pValue->nByte > 0){` |
|        - | 15169 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15170 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15171 | `		 }` |
|      ! 0 | 15172 | `	 }` |
|      ! 0 | 15173 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15174 | `		 /* Extract raw POST data */` |
|      ! 0 | 15175 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15176 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15177 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15178 | `				 /* Extract POST data length */` |
|      ! 0 | 15179 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15180 | `				 if( pValue ){` |
|      ! 0 | 15181 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15182 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15183 | `					 if( iLen > 0 ){` |
|        - | 15184 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15185 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15186 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15187 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15188 | `						 }` |
|        - | 15189 | `						 /* Decode POST data now */` |
|      ! 0 | 15190 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15191 | `					 }` |
|      ! 0 | 15192 | `				 }` |
|      ! 0 | 15193 | `		 }` |
|      ! 0 | 15194 | `	 }` |
|        - | 15195 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15196 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15197 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15198 | `	 return SXRET_OK;` |
|      ! 0 | 15199 | ` }` |
|        - | 15200 |  |
