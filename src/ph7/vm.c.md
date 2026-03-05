# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5229/7401 lines (70.65%)

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
|   617736 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   617738 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   617716 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   617708 |   123 | `	return FALSE;` |
|   308892 |   124 |  |
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
|   263426 |   183 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   263428 |   194 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   263428 |   195 | `	if( pEntry ){` |
|        - |   196 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   197 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   198 | `		pCons->xExpand = xExpand;` |
|        6 |   199 | `		pCons->pUserData = pUserData;` |
|        6 |   200 | `		return SXRET_OK;` |
|        - |   201 | `	}` |
|        - |   202 | `	/* Allocate a new constant instance */` |
|   263424 |   203 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   263424 |   204 | `	if( pCons == 0 ){` |
|      ! 0 |   205 | `		return 0;` |
|        - |   206 | `	}` |
|        - |   207 | `	/* Duplicate constant name */` |
|   263424 |   208 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   263424 |   209 | `	if( zDupName == 0 ){` |
|      ! 0 |   210 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   211 | `		return 0;` |
|        - |   212 | `	}` |
|        - |   213 | `	/* Install the constant */` |
|   263424 |   214 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   263424 |   215 | `	pCons->xExpand = xExpand;` |
|   263424 |   216 | `	pCons->pUserData = pUserData;` |
|   263424 |   217 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   263424 |   218 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   219 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   220 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   221 | `		return rc;` |
|        - |   222 | `	}` |
|        - |   223 | `	/* All done,constant can be invoked from PHP code */` |
|   263424 |   224 | `	return SXRET_OK;` |
|   131715 |   225 |  |
|        - |   226 | `/*` |
|        - |   227 | ` * Allocate a new foreign function instance.` |
|        - |   228 | ` * This function return SXRET_OK on success. Any other` |
|        - |   229 | ` * return value indicates failure.` |
|        - |   230 | ` * Please refer to the official documentation for an introduction to` |
|        - |   231 | ` * the foreign function mechanism.` |
|        - |   232 | ` */` |
|   567240 |   233 | `static sxi32 PH7_NewForeignFunction(` |
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
|   567242 |   244 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   567242 |   245 | `	if( pFunc == 0 ){` |
|      ! 0 |   246 | `		return SXERR_MEM;` |
|        - |   247 | `	}` |
|        - |   248 | `	/* Duplicate function name */` |
|   567242 |   249 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   567242 |   250 | `	if( zDup == 0 ){` |
|      ! 0 |   251 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   252 | `		return SXERR_MEM;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Zero the structure */` |
|   567242 |   255 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   256 | `	/* Initialize structure fields */` |
|   567242 |   257 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   567242 |   258 | `	pFunc->pVm   = pVm;` |
|   567242 |   259 | `	pFunc->xFunc = xFunc;` |
|   567242 |   260 | `	pFunc->pUserData = pUserData;` |
|   567242 |   261 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   262 | `	/* Write a pointer to the new function */` |
|   567242 |   263 | `	*ppOut = pFunc;` |
|   567242 |   264 | `	return SXRET_OK;` |
|   283622 |   265 |  |
|        - |   266 | `/*` |
|        - |   267 | ` * Install a foreign function and it's associated callback so that` |
|        - |   268 | ` * it can be invoked from the target PHP code.` |
|        - |   269 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   270 | ` * return value indicates failure.` |
|        - |   271 | ` * Please refer to the official documentation for an introduction to` |
|        - |   272 | ` * the foreign function mechanism.` |
|        - |   273 | ` */` |
|   568544 |   274 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   568546 |   285 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   568546 |   286 | `	if( pEntry ){` |
|     1306 |   287 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1306 |   288 | `		pFunc->pUserData = pUserData;` |
|     1306 |   289 | `		pFunc->xFunc = xFunc;` |
|     1306 |   290 | `		SySetReset(&pFunc->aAux);` |
|     1306 |   291 | `		return SXRET_OK;` |
|        - |   292 | `	}` |
|        - |   293 | `	/* Create a new user function */` |
|   567242 |   294 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   567242 |   295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   296 | `		return rc;` |
|        - |   297 | `	}` |
|        - |   298 | `	/* Install the function in the corresponding hashtable */` |
|   567242 |   299 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   567242 |   300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   301 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   302 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   303 | `		return rc;` |
|        - |   304 | `	}` |
|        - |   305 | `	/* User function successfully installed */` |
|   567242 |   306 | `	return SXRET_OK;` |
|   284274 |   307 |  |
|        - |   308 | `/*` |
|        - |   309 | ` * Initialize a VM function.` |
|        - |   310 | ` */` |
|    66226 |   311 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   312 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   313 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   314 | `	const char *zName,  /* Function name */` |
|        - |   315 | `	sxu32 nByte,        /* zName length */` |
|        - |   316 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   317 | `	void *pUserData     /* Function private data */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	/* Zero the structure */` |
|    66228 |   321 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   322 | `	/* Initialize structure fields */` |
|        - |   323 | `	/* Arguments container */` |
|    66228 |   324 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   325 | `	/* Static variable container */` |
|    66228 |   326 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   327 | `	/* Bytecode container */` |
|    66228 |   328 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   329 | `    /* Preallocate some instruction slots */` |
|    66228 |   330 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   331 | `	/* Closure environment */` |
|    66228 |   332 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    66228 |   333 | `	pFunc->iFlags = iFlags;` |
|    66228 |   334 | `	pFunc->pUserData = pUserData;` |
|    66228 |   335 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    66228 |   336 | `	return SXRET_OK;` |
|        2 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   340 | ` */` |
|   207236 |   341 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   344 | `	SyString *pName     /* Function name */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	SyHashEntry *pEntry;` |
|        - |   348 | `	sxi32 rc;` |
|   207238 |   349 | `	if( pName == 0 ){` |
|        - |   350 | `		/* Use the built-in name */` |
|    20718 |   351 | `		pName = &pFunc->sName;` |
|    10358 |   352 | `	}` |
|        - |   353 | `	/* Check for duplicates (functions with the same name) first */` |
|   207238 |   354 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   207238 |   355 | `	if( pEntry ){` |
|   153714 |   356 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   153714 |   357 | `		if( pLink != pFunc ){` |
|        - |   358 | `			/* Link */` |
|      179 |   359 | `			pFunc->pNextName = pLink;` |
|      179 |   360 | `			pEntry->pUserData = pFunc;` |
|       89 |   361 | `		}` |
|   153714 |   362 | `		return SXRET_OK;` |
|        - |   363 | `	}` |
|        - |   364 | `	/* First time seen */` |
|    53526 |   365 | `	pFunc->pNextName = 0;` |
|    53526 |   366 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    53526 |   367 | `	return rc;` |
|   103620 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   371 | ` */` |
|    17436 |   372 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   373 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   374 | `	ph7_class *pClass /* Target Class */` |
|        - |   375 | `	)` |
|        2 |   376 |  |
|    17438 |   377 | `	SyString *pName = &pClass->sName;` |
|        - |   378 | `	SyHashEntry *pEntry;` |
|        - |   379 | `	sxi32 rc;` |
|        - |   380 | `	/* Check for duplicates */` |
|    17438 |   381 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    17438 |   382 | `	if( pEntry ){` |
|       31 |   383 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   384 | `		/* Link entry with the same name */` |
|       31 |   385 | `		pClass->pNextName = pLink;` |
|       31 |   386 | `		pEntry->pUserData = pClass;` |
|       31 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|    17408 |   389 | `	pClass->pNextName = 0;` |
|        - |   390 | `	/* Perform a simple hashtable insertion */` |
|    17408 |   391 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    17408 |   392 | `	return rc;` |
|     8720 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Instruction builder interface.` |
|        - |   396 | ` */` |
|  1645720 |   397 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  1645722 |   409 | `	sInstr.iOp = (sxu8)iOp;` |
|  1645722 |   410 | `	sInstr.iP1 = iP1;` |
|  1645722 |   411 | `	sInstr.iP2 = iP2;` |
|  1645722 |   412 | `	sInstr.p3  = p3;` |
|  1645722 |   413 | `	if( pIndex ){` |
|        - |   414 | `		/* Instruction index in the bytecode array */` |
|    98694 |   415 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    49346 |   416 | `	}` |
|        - |   417 | `	/* Finally,record the instruction */` |
|  1645722 |   418 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1645722 |   419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   420 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   421 | `		/* Fall throw */` |
|      ! 0 |   422 | `	}` |
|  1645722 |   423 | `	return rc;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Swap the current bytecode container with the given one.` |
|        - |   427 | ` */` |
|   161064 |   428 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   429 |  |
|   161066 |   430 | `	if( pContainer == 0 ){` |
|        - |   431 | `		/* Point to the default container */` |
|      ! 0 |   432 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   433 | `	}else{` |
|        - |   434 | `		/* Change container */` |
|   161066 |   435 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   436 | `	}` |
|   161066 |   437 | `	return SXRET_OK;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Return the current bytecode container.` |
|        - |   441 | ` */` |
|    80532 |   442 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   443 |  |
|    80534 |   444 | `	return pVm->pByteContainer;` |
|        2 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   448 | ` */` |
|    97054 |   449 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *pInstr;` |
|    97056 |   452 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    97056 |   453 | `	return pInstr;` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   457 | ` */` |
|   478584 |   458 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   459 |  |
|   478586 |   460 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Pop the last VM instruction.` |
|        - |   464 | ` */` |
|    93604 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|    93606 |   467 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   468 |  |
|        - |   469 | `/*` |
|        - |   470 | ` * Peek the last VM instruction.` |
|        - |   471 | ` */` |
|   250304 |   472 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   473 |  |
|   250306 |   474 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   475 |  |
|     3346 |   476 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *aInstr;` |
|        - |   479 | `	sxu32 n;` |
|     3348 |   480 | `	n = SySetUsed(pVm->pByteContainer);` |
|     3348 |   481 | `	if( n < 2 ){` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     3348 |   484 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     3348 |   485 | `	return &aInstr[n - 2];` |
|     1675 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Allocate a new virtual machine frame.` |
|        - |   489 | ` */` |
|    10718 |   490 | `static VmFrame * VmNewFrame(` |
|        - |   491 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   492 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	)` |
|        2 |   495 |  |
|        - |   496 | `	VmFrame *pFrame;` |
|        - |   497 | `	/* Allocate a new vm frame */` |
|    10720 |   498 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    10720 |   499 | `	if( pFrame == 0 ){` |
|      ! 0 |   500 | `		return 0;` |
|        - |   501 | `	}` |
|        - |   502 | `	/* Zero the structure */` |
|    10720 |   503 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   504 | `	/* Initialize frame fields */` |
|    10720 |   505 | `	pFrame->pUserData = pUserData;` |
|    10720 |   506 | `	pFrame->pThis = pThis;` |
|    10720 |   507 | `	pFrame->pVm = pVm;` |
|    10720 |   508 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    10720 |   509 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    10720 |   510 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    10720 |   511 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    10720 |   512 | `	return pFrame;` |
|     5361 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Enter a VM frame.` |
|        - |   516 | ` */` |
|    10718 |   517 | `static sxi32 VmEnterFrame(` |
|        - |   518 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   519 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   522 | `	)` |
|        2 |   523 |  |
|        - |   524 | `	VmFrame *pFrame;` |
|        - |   525 | `	/* Allocate a new frame */` |
|    10720 |   526 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    10720 |   527 | `	if( pFrame == 0 ){` |
|      ! 0 |   528 | `		return SXERR_MEM;` |
|        - |   529 | `	}` |
|        - |   530 | `	/* Link to the list of active VM frame */` |
|    10720 |   531 | `	pFrame->pParent = pVm->pFrame;` |
|    10720 |   532 | `	pVm->pFrame = pFrame;` |
|    10720 |   533 | `	if( ppFrame ){` |
|        - |   534 | `		/* Write a pointer to the new VM frame */` |
|     9158 |   535 | `		*ppFrame = pFrame;` |
|     4578 |   536 | `	}` |
|    10720 |   537 | `	return SXRET_OK;` |
|     5361 |   538 |  |
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
|     9154 |   585 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   586 |  |
|     9156 |   587 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     9156 |   588 | `	if( pCurFrame ){` |
|        - |   589 | `		/* Unlink from the list of active VM frame */` |
|     9156 |   590 | `		pVm->pFrame = pCurFrame->pParent;` |
|     9156 |   591 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   592 | `			VmSlot  *aSlot;` |
|        - |   593 | `			sxu32 n;` |
|        - |   594 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     9138 |   595 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    67092 |   596 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   597 | `				/* Unset the local variable */` |
|    57956 |   598 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    28979 |   599 | `			}` |
|        - |   600 | `			/* Remove local reference */` |
|     9138 |   601 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    67126 |   602 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    57990 |   603 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    28996 |   604 | `			}` |
|     4568 |   605 | `		}` |
|        - |   606 | `		/* Release internal containers */` |
|     9156 |   607 | `		SyHashRelease(&pCurFrame->hVar);` |
|     9156 |   608 | `		SySetRelease(&pCurFrame->sArg);` |
|     9156 |   609 | `		SySetRelease(&pCurFrame->sLocal);` |
|     9156 |   610 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   611 | `		/* Release the whole structure */` |
|     9156 |   612 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     4577 |   613 | `	}` |
|     9156 |   614 |  |
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
|      122 |   644 | `static ph7_vm_func * VmOverload(` |
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
|      123 |   657 | `	pLink = pList;` |
|      123 |   658 | `	i = 0;` |
|        - |   659 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   660 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   661 | `		if( pLink == 0 ){` |
|       61 |   662 | `			break;` |
|        - |   663 | `		}` |
|      909 |   664 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   665 | `			/* Candidate for overloading */` |
|      863 |   666 | `			apSet[i++] = pLink;` |
|      431 |   667 | `		}` |
|        - |   668 | `		/* Point to the next entry */` |
|      909 |   669 | `		pLink = pLink->pNextName;` |
|        1 |   670 | `	}` |
|      123 |   671 | `	if( i < 1 ){` |
|        - |   672 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   673 | `		return pList;` |
|        - |   674 | `	}` |
|      123 |   675 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   676 | `		/* Return the only candidate */` |
|       21 |   677 | `		return apSet[0];` |
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
|       62 |   724 |  |
|        - |   725 | `/* Forward declaration */` |
|        - |   726 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   727 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   728 | `/*` |
|        - |   729 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   730 | ` * it can be instanciated from the executed PHP script.` |
|        - |   731 | ` */` |
|    61528 |   732 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   733 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   734 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   735 | `	)` |
|        2 |   736 |  |
|        - |   737 | `	ph7_class_method *pMeth;` |
|        - |   738 | `	ph7_class_attr *pAttr;` |
|        - |   739 | `	SyHashEntry *pEntry;` |
|        - |   740 | `	sxi32 rc;` |
|        - |   741 | `	/* Reset the loop cursor */` |
|    61530 |   742 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   743 | `	/* Process only static and constant attribute */` |
|   208410 |   744 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   745 | `		/* Extract the current attribute */` |
|   116118 |   746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   116118 |   747 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   748 | `			ph7_value *pMemObj;` |
|        - |   749 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   750 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   751 | `			if( pMemObj == 0 ){` |
|      ! 0 |   752 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   753 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   754 | `					&pClass->sName,&pAttr->sName` |
|        - |   755 | `					);` |
|      ! 0 |   756 | `				return SXERR_MEM;` |
|        - |   757 | `			}` |
|     1290 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   761 | `			}` |
|        - |   762 | `			/* Record attribute index */` |
|     1290 |   763 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   764 | `			/* Install static attribute in the reference table */` |
|     1290 |   765 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   766 | `		}` |
|        2 |   767 | `	}` |
|        - |   768 | `	/* Install class methods */` |
|    61530 |   769 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   770 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   771 | `		 */` |
|    38372 |   772 | `		return SXRET_OK;` |
|        - |   773 | `	}` |
|        - |   774 | `	/* Create constructor alias if not yet done */` |
|    23160 |   775 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   776 | `		/* User constructor with the same base class name */` |
|      202 |   777 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      202 |   778 | `		if( pEntry ){` |
|      ! 0 |   779 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   780 | `			/* Create the alias */` |
|      ! 0 |   781 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   782 | `		}` |
|      100 |   783 | `	}` |
|        - |   784 | `	/* Install the methods now */` |
|    23160 |   785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   221265 |   786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   186528 |   787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   186528 |   788 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   186522 |   789 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   186522 |   790 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   791 | `				return rc;` |
|        - |   792 | `			}` |
|    93260 |   793 | `		}` |
|        2 |   794 | `	}` |
|        - |   795 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    23160 |   796 | `	pClass->bMounted = TRUE;` |
|    23160 |   797 | `	return SXRET_OK;` |
|    30766 |   798 |  |
|        - |   799 | `/*` |
|        - |   800 | ` * Allocate a private frame for attributes of the given` |
|        - |   801 | ` * class instance (Object in the PHP jargon).` |
|        - |   802 | ` */` |
|      738 |   803 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   804 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   805 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   806 | `	)` |
|        2 |   807 |  |
|      740 |   808 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   809 | `	ph7_class_attr *pAttr;` |
|        - |   810 | `	SyHashEntry *pEntry;` |
|        - |   811 | `	sxi32 rc;` |
|        - |   812 | `	/* Install class attribute in the private frame associated with this instance */` |
|      740 |   813 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     2482 |   814 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   815 | `		VmClassAttr *pVmAttr;` |
|        - |   816 | `		/* Extract the current attribute */` |
|     1744 |   817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     1744 |   818 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     1744 |   819 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   820 | `			return SXERR_MEM;` |
|        - |   821 | `		}` |
|     1744 |   822 | `		pVmAttr->pAttr = pAttr;` |
|     1744 |   823 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   824 | `			ph7_value *pMemObj;` |
|        - |   825 | `			/* Reserve a memory object for this attribute */` |
|     1738 |   826 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1738 |   827 | `			if( pMemObj == 0 ){` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|     1738 |   831 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     1738 |   832 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   833 | `				/* Initialize attribute default value (any complex expression) */` |
|      556 |   834 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      277 |   835 | `			}` |
|     1738 |   836 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     1738 |   837 | `			if( rc != SXRET_OK ){` |
|        - |   838 | `				VmSlot sSlot;` |
|        - |   839 | `				/* Restore memory object */` |
|      ! 0 |   840 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   841 | `				sSlot.pUserData = 0;` |
|      ! 0 |   842 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   843 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   844 | `				return SXERR_MEM;` |
|        - |   845 | `			}` |
|        - |   846 | `			/* Install attribute in the reference table */` |
|     1738 |   847 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      870 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|        8 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|      740 |   858 | `	return SXRET_OK;` |
|      371 |   859 |  |
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
|   186540 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   186542 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   181856 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    90927 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   186542 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   186542 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   186542 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   186542 |   888 | `	return pObj;` |
|    93272 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|  2121184 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|  2121186 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|  2121186 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1060592 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|  2121186 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2121186 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|  2121186 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2121186 |   911 | `	return pObj;` |
|  1060594 |   912 |  |
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
|        - |   967 | `	"class ValueError extends Error { }"\` |
|        - |   968 | `	"class ErrorException extends Exception { "\` |
|        - |   969 | `	"protected $severity;"\` |
|        - |   970 | `	"public function __construct(string $message = null,"\` |
|        - |   971 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   972 | `	"   if( isset($message) ){"\` |
|        - |   973 | `	"	  $this->message = $message;"\` |
|        - |   974 | `	"   }"\` |
|        - |   975 | `	"   $this->severity = $severity;"\` |
|        - |   976 | `	"   $this->code = $code;"\` |
|        - |   977 | `	"   $this->file = $filename;"\` |
|        - |   978 | `	"   $this->line = $lineno;"\` |
|        - |   979 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   980 | `	"   if( isset($previous) ){"\` |
|        - |   981 | `	"     $this->previous = $previous;"\` |
|        - |   982 | `	"   }"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"public function getSeverity(){"\` |
|        - |   985 | `	"   return $this->severity;"\` |
|        - |   986 | `    "}"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"interface Iterator {"\` |
|        - |   989 | `	"public function current();"\` |
|        - |   990 | `	"public function key();"\` |
|        - |   991 | `	"public function next();"\` |
|        - |   992 | `	"public function rewind();"\` |
|        - |   993 | `	"public function valid();"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"interface IteratorAggregate {"\` |
|        - |   996 | `	"public function getIterator();"\` |
|        - |   997 | `	"}"\` |
|        - |   998 | `	"interface Serializable {"\` |
|        - |   999 | `	"public function serialize();"\` |
|        - |  1000 | `	"public function unserialize(string $serialized);"\` |
|        - |  1001 | `	"}"\` |
|        - |  1002 | `	"/* Directory releated IO */"\` |
|        - |  1003 | `	"class Directory {"\` |
|        - |  1004 | `	"public $handle = null;"\` |
|        - |  1005 | `	"public $path  = null;"\` |
|        - |  1006 | `	"public function __construct(string $path)"\` |
|        - |  1007 | `	"{"\` |
|        - |  1008 | `	"   $this->handle = opendir($path);"\` |
|        - |  1009 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1010 | `	"      $this->path = $path;"\` |
|        - |  1011 | `	"   }"\` |
|        - |  1012 | `	"}"\` |
|        - |  1013 | `	"public function __destruct()"\` |
|        - |  1014 | `	"{"\` |
|        - |  1015 | `	"  if( $this->handle != null ){"\` |
|        - |  1016 | `	"       closedir($this->handle);"\` |
|        - |  1017 | `	"  }"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"public function read()"\` |
|        - |  1020 | `	"{"\` |
|        - |  1021 | `	"    return readdir($this->handle);"\` |
|        - |  1022 | `	"}"\` |
|        - |  1023 | `	"public function rewind()"\` |
|        - |  1024 | `	"{"\` |
|        - |  1025 | `	"    rewinddir($this->handle);"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"public function close()"\` |
|        - |  1028 | `	"{"\` |
|        - |  1029 | `	"    closedir($this->handle);"\` |
|        - |  1030 | `	"    $this->handle = null;"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"}"\` |
|        - |  1033 | `	"class stdClass{"\` |
|        - |  1034 | `	"  public $value;"\` |
|        - |  1035 | `	" /* Magic methods */"\` |
|        - |  1036 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1037 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1038 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1039 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1040 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1041 | `	"}"\` |
|        - |  1042 | `	"function dir(string $path){"\` |
|        - |  1043 | `	"   return new Directory($path);"\` |
|        - |  1044 | `	"}"\` |
|        - |  1045 | `	"function Dir(string $path){"\` |
|        - |  1046 | `	"   return new Directory($path);"\` |
|        - |  1047 | `	"}"\` |
|        - |  1048 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1049 | `    "{"\` |
|        - |  1050 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1051 | `	"  $aDir = array();"\` |
|        - |  1052 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1053 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1054 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1055 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1056 | `	"   }"\` |
|        - |  1057 | `	"  closedir($pHandle);"\` |
|        - |  1058 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1059 | `	"      rsort($aDir);"\` |
|        - |  1060 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1061 | `	"      sort($aDir);"\` |
|        - |  1062 | `	"  }"\` |
|        - |  1063 | `	"  return $aDir;"\` |
|        - |  1064 | `	"}"\` |
|        - |  1065 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1066 | `	"/* Open the target directory */"\` |
|        - |  1067 | `	"$zDir = dirname($pattern);"\` |
|        - |  1068 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1069 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1070 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1071 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1072 | `	"	return FALSE;"\` |
|        - |  1073 | `	"}"\` |
|        - |  1074 | `	"$pattern = basename($pattern);"\` |
|        - |  1075 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1076 | `	"/* Loop throw available entries */"\` |
|        - |  1077 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1078 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1079 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1080 | `	"	if( $rc ){"\` |
|        - |  1081 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1082 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1083 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1084 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1085 | `	"		  }"\` |
|        - |  1086 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1087 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1088 | `	"		 continue;"\` |
|        - |  1089 | `	"	   }"\` |
|        - |  1090 | `	"	   /* Add the entry */"\` |
|        - |  1091 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1092 | `	"	}"\` |
|        - |  1093 | `	" }"\` |
|        - |  1094 | `	"/* Close the handle */"\` |
|        - |  1095 | `	"closedir($pHandle);"\` |
|        - |  1096 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1097 | `	"  /* Sort the array */"\` |
|        - |  1098 | `	"  sort($pArray);"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1101 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1102 | `	"  $pArray[] = $pattern;"\` |
|        - |  1103 | `	"}"\` |
|        - |  1104 | `	"/* Return the created array */"\` |
|        - |  1105 | `	"return $pArray;"\` |
|        - |  1106 | `   "}"\` |
|        - |  1107 | `   "/* Creates a temporary file */"\` |
|        - |  1108 | `   "function tmpfile(){"\` |
|        - |  1109 | `   "  /* Extract the temp directory */"\` |
|        - |  1110 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1111 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1112 | `   "    /* Use the current dir */"\` |
|        - |  1113 | `   "    $zTempDir = '.';"\` |
|        - |  1114 | `   "  }"\` |
|        - |  1115 | `   "  /* Create the file */"\` |
|        - |  1116 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1117 | `   "  return $pHandle;"\` |
|        - |  1118 | `   "}"\` |
|        - |  1119 | `   "/* Creates a temporary filename */"\` |
|        - |  1120 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1121 | `   "{"\` |
|        - |  1122 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1123 | `   "}"\` |
|        - |  1124 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1125 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1126 | `   "/* Copy arguments */"\` |
|        - |  1127 | `   "$nArgs = func_num_args();"\` |
|        - |  1128 | `   "$pNew = array();"\` |
|        - |  1129 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1130 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1131 | `    "}"\` |
|        - |  1132 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1133 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1134 | `	"/* Erase */"\` |
|        - |  1135 | `	"array_erase($pArray);"\` |
|        - |  1136 | `	"/* Unshift */"\` |
|        - |  1137 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1138 | `	"return sizeof($pArray);"\` |
|        - |  1139 | `    "}"\` |
|        - |  1140 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1141 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1142 | `    "$arrays = func_get_args();"\` |
|        - |  1143 | `    "$narrays = count($arrays);"\` |
|        - |  1144 | `    "$ret = $arrays[0];"\` |
|        - |  1145 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1146 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1147 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1148 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1149 | `     "   $ret[] = $value;"\` |
|        - |  1150 | `     "  }else{"\` |
|        - |  1151 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1152 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1153 | `     " }else {"\` |
|        - |  1154 | `     "   $ret[$key] = $value;"\` |
|        - |  1155 | `     "  }"\` |
|        - |  1156 | `     " }"\` |
|        - |  1157 | `     " }"\` |
|        - |  1158 | `	 "}"\` |
|        - |  1159 | `	 " return $ret;"\` |
|        - |  1160 | `    "}"\` |
|        - |  1161 | `	"function max(){"\` |
|        - |  1162 | `    "  $pArgs = func_get_args();"\` |
|        - |  1163 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1164 | `	"  return null;"\` |
|        - |  1165 | `    " }"\` |
|        - |  1166 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1167 | `    " $pArg = $pArgs[0];"\` |
|        - |  1168 | `	" if( !is_array($pArg) ){"\` |
|        - |  1169 | `	"   return $pArg; "\` |
|        - |  1170 | `	" }"\` |
|        - |  1171 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1172 | `	"   return null;"\` |
|        - |  1173 | `	" }"\` |
|        - |  1174 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1175 | `	" reset($pArg);"\` |
|        - |  1176 | `	" $max = current($pArg);"\` |
|        - |  1177 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1178 | `	"   if( $val > $max ){"\` |
|        - |  1179 | `	"     $max = $val;"\` |
|        - |  1180 | `    " }"\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" return $max;"\` |
|        - |  1183 | `    " }"\` |
|        - |  1184 | `    " $max = $pArgs[0];"\` |
|        - |  1185 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1186 | `    " $val = $pArgs[$i];"\` |
|        - |  1187 | `	"if( $val > $max ){"\` |
|        - |  1188 | `	" $max = $val;"\` |
|        - |  1189 | `	"}"\` |
|        - |  1190 | `    " }"\` |
|        - |  1191 | `	" return $max;"\` |
|        - |  1192 | `    "}"\` |
|        - |  1193 | `	"function min(){"\` |
|        - |  1194 | `    "  $pArgs = func_get_args();"\` |
|        - |  1195 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1196 | `	"  return null;"\` |
|        - |  1197 | `    " }"\` |
|        - |  1198 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1199 | `    " $pArg = $pArgs[0];"\` |
|        - |  1200 | `	" if( !is_array($pArg) ){"\` |
|        - |  1201 | `	"   return $pArg; "\` |
|        - |  1202 | `	" }"\` |
|        - |  1203 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1204 | `	"   return null;"\` |
|        - |  1205 | `	" }"\` |
|        - |  1206 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1207 | `	" reset($pArg);"\` |
|        - |  1208 | `	" $min = current($pArg);"\` |
|        - |  1209 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1210 | `	"   if( $val < $min ){"\` |
|        - |  1211 | `	"     $min = $val;"\` |
|        - |  1212 | `    " }"\` |
|        - |  1213 | `	" }"\` |
|        - |  1214 | `	" return $min;"\` |
|        - |  1215 | `    " }"\` |
|        - |  1216 | `    " $min = $pArgs[0];"\` |
|        - |  1217 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1218 | `    " $val = $pArgs[$i];"\` |
|        - |  1219 | `	"if( $val < $min ){"\` |
|        - |  1220 | `	" $min = $val;"\` |
|        - |  1221 | `	" }"\` |
|        - |  1222 | `    " }"\` |
|        - |  1223 | `	" return $min;"\` |
|        - |  1224 | `	"}"\` |
|        - |  1225 | `	"function fileowner(string $file){"\` |
|        - |  1226 | `    " $a = stat($file);"\` |
|        - |  1227 | `	" if( !is_array($a) ){"\` |
|        - |  1228 | `	"	return false;"\` |
|        - |  1229 | `	" }"\` |
|        - |  1230 | `	" return $a['uid'];"\` |
|        - |  1231 | `    "}"\` |
|        - |  1232 | `    "function filegroup(string $file){"\` |
|        - |  1233 | `	" $a = stat($file);"\` |
|        - |  1234 | `	" if( !is_array($a) ){"\` |
|        - |  1235 | `	"	return false;"\` |
|        - |  1236 | `	" }"\` |
|        - |  1237 | `	" return $a['gid'];"\` |
|        - |  1238 | `    "}"\` |
|        - |  1239 | `	 "function fileinode(string $file){"\` |
|        - |  1240 | `	" $a = stat($file);"\` |
|        - |  1241 | `	" if( !is_array($a) ){"\` |
|        - |  1242 | `	"	return false;"\` |
|        - |  1243 | `	" }"\` |
|        - |  1244 | `	" return $a['ino'];"\` |
|        - |  1245 | `    "}"` |
|        - |  1246 |  |
|        - |  1247 | `/*` |
|        - |  1248 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1249 | ` * start compiling the target PHP program.` |
|        - |  1250 | ` */` |
|     1562 |  1251 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1252 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1253 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1254 | `	 )` |
|        2 |  1255 |  |
|        - |  1256 | `	SyString sBuiltin;` |
|        - |  1257 | `	ph7_value *pObj;` |
|        - |  1258 | `	sxi32 rc;` |
|        - |  1259 | `	/* Zero the structure */` |
|     1564 |  1260 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1261 | `	/* Initialize VM fields */` |
|     1564 |  1262 | `	pVm->pEngine = &(*pEngine);` |
|     1564 |  1263 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1264 | `	/* Instructions containers */` |
|     1564 |  1265 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1564 |  1266 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1564 |  1267 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1268 | `	/* Object containers */` |
|     1564 |  1269 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1564 |  1270 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1271 | `	/* Virtual machine internal containers */` |
|     1564 |  1272 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1564 |  1273 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1564 |  1274 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1564 |  1275 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1564 |  1276 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1564 |  1277 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1564 |  1278 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1564 |  1279 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1564 |  1280 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1564 |  1281 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1564 |  1282 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1564 |  1283 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1564 |  1284 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1564 |  1285 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1564 |  1286 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1287 | `	/* Configuration containers */` |
|     1564 |  1288 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1564 |  1289 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1564 |  1290 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1564 |  1291 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1564 |  1292 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1293 | `	/* Error callbacks containers */` |
|     1564 |  1294 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1564 |  1295 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1564 |  1296 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1564 |  1297 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1564 |  1298 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1299 | `	/* Set a default recursion limit */` |
|        - |  1300 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1564 |  1301 | `	pVm->nMaxDepth = 32;` |
|        - |  1302 | `#else` |
|        - |  1303 | `	pVm->nMaxDepth = 16;` |
|        - |  1304 | `#endif` |
|        - |  1305 | `	/* Default assertion flags */` |
|     1564 |  1306 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1307 | `	/* JSON return status */` |
|     1564 |  1308 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1309 | `	/* PRNG context */` |
|     1564 |  1310 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1311 | `	/* Install the null constant */` |
|     1564 |  1312 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1564 |  1313 | `	if( pObj == 0 ){` |
|      ! 0 |  1314 | `		rc = SXERR_MEM;` |
|      ! 0 |  1315 | `		goto Err;` |
|        - |  1316 | `	}` |
|     1564 |  1317 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1318 | `	/* Install the boolean TRUE constant */` |
|     1564 |  1319 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1564 |  1320 | `	if( pObj == 0 ){` |
|      ! 0 |  1321 | `		rc = SXERR_MEM;` |
|      ! 0 |  1322 | `		goto Err;` |
|        - |  1323 | `	}` |
|     1564 |  1324 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1325 | `	/* Install the boolean FALSE constant */` |
|     1564 |  1326 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1564 |  1327 | `	if( pObj == 0 ){` |
|      ! 0 |  1328 | `		rc = SXERR_MEM;` |
|      ! 0 |  1329 | `		goto Err;` |
|        - |  1330 | `	}` |
|     1564 |  1331 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1332 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1333 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1334 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1564 |  1335 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1564 |  1336 | `	if( pObj == 0 ){` |
|      ! 0 |  1337 | `		rc = SXERR_MEM;` |
|      ! 0 |  1338 | `		goto Err;` |
|        - |  1339 | `	}` |
|     1564 |  1340 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1341 | `	/* Create the global frame */` |
|     1564 |  1342 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1564 |  1343 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1344 | `		goto Err;` |
|        - |  1345 | `	}` |
|        - |  1346 | `	/* Initialize the code generator */` |
|     1564 |  1347 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1564 |  1348 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1349 | `		goto Err;` |
|        - |  1350 | `	}` |
|        - |  1351 | `	/* VM correctly initialized,set the magic number */` |
|     1564 |  1352 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1564 |  1353 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1354 | `	/* Compile the built-in library */` |
|     1564 |  1355 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1356 | `	/* Reset the code generator */` |
|     1564 |  1357 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1564 |  1358 | `	return SXRET_OK;` |
|      ! 0 |  1359 | `Err:` |
|      ! 0 |  1360 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1361 | `	return rc;` |
|      783 |  1362 |  |
|        - |  1363 | `/*` |
|        - |  1364 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1365 | ` * routine which store the output in an internal blob.` |
|        - |  1366 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1367 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1368 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1369 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1370 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1371 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1372 | ` * to finish executing and extracting the output.` |
|        - |  1373 | ` */` |
|      ! 0 |  1374 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1375 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1376 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1377 | `	void *pUserData     /* User private data */` |
|        - |  1378 | `	)` |
|      ! 0 |  1379 |  |
|        - |  1380 | `	 sxi32 rc;` |
|        - |  1381 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1382 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1383 | `	 return rc;` |
|      ! 0 |  1384 |  |
|        - |  1385 | `#define VM_STACK_GUARD 16` |
|        - |  1386 | `/*` |
|        - |  1387 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1388 | ` * our compiled PHP program.` |
|        - |  1389 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1390 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1391 | ` */` |
|    22936 |  1392 | `static ph7_value * VmNewOperandStack(` |
|        - |  1393 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1394 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1395 | `	)` |
|        2 |  1396 |  |
|        - |  1397 | `	ph7_value *pStack;` |
|        - |  1398 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1399 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1400 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1401 | `  ** on the maximum stack depth required.` |
|        - |  1402 | `  **` |
|        - |  1403 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1404 | `  */` |
|    22938 |  1405 | `	nInstr += VM_STACK_GUARD;` |
|    22938 |  1406 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    22938 |  1407 | `	if( pStack == 0 ){` |
|      ! 0 |  1408 | `		return 0;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Initialize the operand stack */` |
|  1455974 |  1411 | `	while( nInstr > 0 ){` |
|  1433038 |  1412 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1433038 |  1413 | `		--nInstr;` |
|        2 |  1414 | `	}` |
|        - |  1415 | `	/* Ready for bytecode execution */` |
|    22938 |  1416 | `	return pStack;` |
|    11470 |  1417 |  |
|        - |  1418 | `/* Forward declaration */` |
|        - |  1419 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1420 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1421 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1422 | `/*` |
|        - |  1423 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1424 | ` * This routine gets called by the PH7 engine after` |
|        - |  1425 | ` * successful compilation of the target PHP program.` |
|        - |  1426 | ` */` |
|     1304 |  1427 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1428 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1429 | `	)` |
|        2 |  1430 |  |
|        - |  1431 | `	SyHashEntry *pEntry;` |
|        - |  1432 | `	sxi32 rc;` |
|     1306 |  1433 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1434 | `		/* Initialize your VM first */` |
|      ! 0 |  1435 | `		return SXERR_CORRUPT;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* Mark the VM ready for byte-code execution */` |
|     1306 |  1438 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1439 | `	/* Release the code generator now we have compiled our program */` |
|     1306 |  1440 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1441 | `	/* Emit the DONE instruction */` |
|     1306 |  1442 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1306 |  1443 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1444 | `		return SXERR_MEM;` |
|        - |  1445 | `	}` |
|        - |  1446 | `	/* Script return value */` |
|     1306 |  1447 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1448 | `	/* Allocate a new operand stack */` |
|     1306 |  1449 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1306 |  1450 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1451 | `		return SXERR_MEM;` |
|        - |  1452 | `	}` |
|        - |  1453 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1454 | `	 * private data. */` |
|     1306 |  1455 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1306 |  1456 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1457 | `	/* Allocate the reference table */` |
|     1306 |  1458 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1306 |  1459 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1306 |  1460 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1461 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1462 | `		return SXERR_MEM;` |
|        - |  1463 | `	}` |
|        - |  1464 | `	/* Zero the reference table */` |
|     1306 |  1465 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1466 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1306 |  1467 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1306 |  1468 | `	if( rc != SXRET_OK ){` |
|        - |  1469 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1470 | `		return rc;` |
|        - |  1471 | `	}` |
|        - |  1472 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1306 |  1473 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1306 |  1474 | `	if( rc != SXRET_OK ){` |
|        - |  1475 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1476 | `		return rc;` |
|        - |  1477 | `	}` |
|        - |  1478 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1306 |  1479 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1480 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1306 |  1481 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1482 | `	/* Initialize and install static and constants class attributes */` |
|     1306 |  1483 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    15676 |  1484 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    14372 |  1485 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    14372 |  1486 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1487 | `			return rc;` |
|        - |  1488 | `		}` |
|        2 |  1489 | `	}` |
|        - |  1490 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1306 |  1491 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1492 | `	/* VM is ready for bytecode execution */` |
|     1306 |  1493 | `	return SXRET_OK;` |
|      654 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1497 | ` */` |
|      ! 0 |  1498 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1499 |  |
|      ! 0 |  1500 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1501 | `		return SXERR_CORRUPT;` |
|        - |  1502 | `	}` |
|        - |  1503 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1504 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1505 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1506 | `	/* Set the ready flag */` |
|      ! 0 |  1507 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1508 | `	return SXRET_OK;` |
|      ! 0 |  1509 |  |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Release a Virtual Machine.` |
|        - |  1512 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1513 | ` */` |
|     1296 |  1514 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1515 |  |
|        - |  1516 | `	/* Set the stale magic number */` |
|     1298 |  1517 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1518 | `	/* Release the private memory subsystem */` |
|     1298 |  1519 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1298 |  1520 | `	return SXRET_OK;` |
|        2 |  1521 |  |
|        - |  1522 | `/*` |
|        - |  1523 | ` * Initialize a foreign function call context.` |
|        - |  1524 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1525 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1526 | ` * functions.` |
|        - |  1527 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1528 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1529 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1530 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1531 | ` */` |
|   463454 |  1532 | `static sxi32 VmInitCallContext(` |
|        - |  1533 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1534 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1535 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1536 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1537 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1538 | `	)` |
|        2 |  1539 |  |
|   463456 |  1540 | `	pOut->pFunc = pFunc;` |
|   463456 |  1541 | `	pOut->pVm   = pVm;` |
|   463456 |  1542 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   463456 |  1543 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1544 | `	/* Assume a null return value */` |
|   463456 |  1545 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   463456 |  1546 | `	pOut->pRet = pRet;` |
|   463456 |  1547 | `	pOut->iFlags = iFlags;` |
|   463456 |  1548 | `	return SXRET_OK;` |
|        2 |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1552 | ` * left behind.` |
|        - |  1553 | ` */` |
|   463454 |  1554 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1555 |  |
|        - |  1556 | `	sxu32 n;` |
|   463456 |  1557 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     5214 |  1558 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    14670 |  1559 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     9458 |  1560 | `			if( apObj[n] == 0 ){` |
|        - |  1561 | `				/* Already released */` |
|      250 |  1562 | `				continue;` |
|        - |  1563 | `			}` |
|     9210 |  1564 | `			PH7_MemObjRelease(apObj[n]);` |
|     9210 |  1565 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     4606 |  1566 | `		}` |
|     5214 |  1567 | `		SySetRelease(&pCtx->sVar);` |
|     2606 |  1568 | `	}` |
|   463456 |  1569 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1570 | `		ph7_aux_data *aAux;` |
|        - |  1571 | `		void *pChunk;` |
|        - |  1572 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1573 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1574 | `		 */` |
|        9 |  1575 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1576 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1577 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1578 | `			/* Release the chunk */` |
|       25 |  1579 | `			if( pChunk ){` |
|       25 |  1580 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1581 | `			}` |
|       13 |  1582 | `		}` |
|        9 |  1583 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1584 | `	}` |
|   463456 |  1585 |  |
|        - |  1586 | `/*` |
|        - |  1587 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1588 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1589 | ` */` |
|      248 |  1590 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1591 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1592 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1593 | `	)` |
|        2 |  1594 |  |
|      250 |  1595 | `	if( pValue == 0 ){` |
|        - |  1596 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1597 | `		return;` |
|        - |  1598 | `	}` |
|      250 |  1599 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1600 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1601 | `		sxu32 n;` |
|      936 |  1602 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1603 | `			if( apObj[n] == pValue ){` |
|      250 |  1604 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1605 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1606 | `				/* Mark as released */` |
|      250 |  1607 | `				apObj[n] = 0;` |
|      250 |  1608 | `				break;` |
|        - |  1609 | `			}` |
|      345 |  1610 | `		}` |
|      124 |  1611 | `	}` |
|      126 |  1612 |  |
|        - |  1613 | `/*` |
|        - |  1614 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1615 | ` */` |
|  2666962 |  1616 | `static void VmPopOperand(` |
|        - |  1617 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1618 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1619 | `	)` |
|        2 |  1620 |  |
|  2666964 |  1621 | `	ph7_value *pTos = *ppTos;` |
|  5669344 |  1622 | `	while( nPop > 0 ){` |
|  3002382 |  1623 | `		PH7_MemObjRelease(pTos);` |
|  3002382 |  1624 | `		pTos--;` |
|  3002382 |  1625 | `		nPop--;` |
|        2 |  1626 | `	}` |
|        - |  1627 | `	/* Top of the stack */` |
|  2666964 |  1628 | `	*ppTos = pTos;` |
|  2666964 |  1629 |  |
|        - |  1630 | `/*` |
|        - |  1631 | ` * Reserve a memory object.` |
|        - |  1632 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1633 | ` */` |
|  2807484 |  1634 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1635 |  |
|  2807486 |  1636 | `	ph7_value *pObj = 0;` |
|        - |  1637 | `	VmSlot *pSlot;` |
|        - |  1638 | `	sxu32 nIdx;` |
|        - |  1639 | `	/* Check for a free slot */` |
|  2807486 |  1640 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2807486 |  1641 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2807486 |  1642 | `	if( pSlot ){` |
|   686302 |  1643 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   686302 |  1644 | `		nIdx = pSlot->nIdx;` |
|   343150 |  1645 | `	}` |
|  2807486 |  1646 | `	if( pObj == 0 ){` |
|        - |  1647 | `		/* Reserve a new memory object */` |
|  2121186 |  1648 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2121186 |  1649 | `		if( pObj == 0 ){` |
|      ! 0 |  1650 | `			return 0;` |
|        - |  1651 | `		}` |
|  1060592 |  1652 | `	}` |
|        - |  1653 | `	/* Set a null default value */` |
|  2807486 |  1654 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2807486 |  1655 | `	pObj->nIdx = nIdx;` |
|  2807486 |  1656 | `	return pObj;` |
|  1403744 |  1657 |  |
|        - |  1658 | `/*` |
|        - |  1659 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1660 | ` */` |
|    17926 |  1661 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1662 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1663 | `	const char *zKey,  /* Entry key */` |
|        - |  1664 | `	sxu32 nByte,       /* Key length */` |
|        - |  1665 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1666 | `	)` |
|        2 |  1667 |  |
|        - |  1668 | `	ph7_value sKey;` |
|        - |  1669 | `	sxi32 rc;` |
|    17928 |  1670 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    17928 |  1671 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1672 | `	/* Perform the insertion */` |
|    17928 |  1673 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    17928 |  1674 | `	PH7_MemObjRelease(&sKey);` |
|    17928 |  1675 | `	return rc;` |
|        2 |  1676 |  |
|        - |  1677 | `/*` |
|        - |  1678 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1679 | ` * Return a pointer to the variable value on success.` |
|        - |  1680 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1681 | ` */` |
|  2469116 |  1682 | `static ph7_value * VmExtractMemObj(` |
|        - |  1683 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1684 | `	const SyString *pName, /* Variable name */` |
|        - |  1685 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1686 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1687 | `	)` |
|        2 |  1688 |  |
|  2469118 |  1689 | `	int bNullify = FALSE;` |
|        - |  1690 | `	SyHashEntry *pEntry;` |
|        - |  1691 | `	VmFrame *pFrame;` |
|        - |  1692 | `	ph7_value *pObj;` |
|        - |  1693 | `	sxu32 nIdx;` |
|        - |  1694 | `	sxi32 rc;` |
|        - |  1695 | `	/* Point to the top active frame */` |
|  2469118 |  1696 | `	pFrame = pVm->pFrame;` |
|  2518470 |  1697 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1698 | `		/* Safely ignore the exception frame */` |
|    49353 |  1699 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1700 | `	}` |
|        - |  1701 | `	/* Perform the lookup */` |
|  2469118 |  1702 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1703 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1704 | `		pName = &sAnnon;` |
|        - |  1705 | `		/* Always nullify the object */` |
|      ! 0 |  1706 | `		bNullify = TRUE;` |
|      ! 0 |  1707 | `		bDup = FALSE;` |
|      ! 0 |  1708 | `	}` |
|        - |  1709 | `	/* Check the superglobals table first */` |
|  2469118 |  1710 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2469118 |  1711 | `	if( pEntry == 0 ){` |
|        - |  1712 | `		/* Query the top active frame */` |
|  2469082 |  1713 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2469082 |  1714 | `		if( pEntry == 0 ){` |
|    63308 |  1715 | `			char *zName = (char *)pName->zString;` |
|        - |  1716 | `			VmSlot sLocal;` |
|    63308 |  1717 | `			if( !bCreate ){` |
|        - |  1718 | `				/* Do not create the variable,return NULL instead */` |
|      492 |  1719 | `				return 0;` |
|        - |  1720 | `			}` |
|        - |  1721 | `			/* No such variable,automatically create a new one and install` |
|        - |  1722 | `			 * it in the current frame.` |
|        - |  1723 | `			 */` |
|    62818 |  1724 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    62818 |  1725 | `			if( pObj == 0 ){` |
|      ! 0 |  1726 | `				return 0;` |
|        - |  1727 | `			}` |
|    62818 |  1728 | `			nIdx = pObj->nIdx;` |
|    62818 |  1729 | `			if( bDup ){` |
|        - |  1730 | `				/* Duplicate name */` |
|      115 |  1731 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1732 | `				if( zName == 0 ){` |
|      ! 0 |  1733 | `					return 0;` |
|        - |  1734 | `				}` |
|       57 |  1735 | `			}` |
|        - |  1736 | `			/* Link to the top active VM frame */` |
|    62818 |  1737 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    62818 |  1738 | `			if( rc != SXRET_OK ){` |
|        - |  1739 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1740 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1741 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1742 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1743 | `				return 0;` |
|        - |  1744 | `			}` |
|    62818 |  1745 | `			if( pFrame->pParent != 0 ){` |
|        - |  1746 | `				/* Local variable */` |
|    57956 |  1747 | `				sLocal.nIdx = nIdx;` |
|    57956 |  1748 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    28979 |  1749 | `			}else{` |
|        - |  1750 | `				/* Register in the $GLOBALS array */` |
|     4864 |  1751 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1752 | `			}` |
|        - |  1753 | `			/* Install in the reference table */` |
|    62818 |  1754 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1755 | `			/* Save object index */` |
|    62818 |  1756 | `			pObj->nIdx = nIdx;` |
|    31410 |  1757 | `		}else{` |
|        - |  1758 | `			/* Extract variable contents */` |
|  2405776 |  1759 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2405776 |  1760 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2405776 |  1761 | `			if( bNullify && pObj ){` |
|      ! 0 |  1762 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1763 | `			}` |
|        - |  1764 | `		}` |
|  1234407 |  1765 | `	}else{` |
|        - |  1766 | `		/* Superglobal */` |
|       38 |  1767 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1768 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1769 | `	}` |
|  2468628 |  1770 | `	return pObj;` |
|  1234670 |  1771 |  |
|        - |  1772 | `/*` |
|        - |  1773 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1774 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1775 | ` */` |
|     1330 |  1776 | `static ph7_value * VmExtractSuper(` |
|        - |  1777 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1778 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1779 | `	sxu32 nByte        /* zName length */` |
|        - |  1780 | `	)` |
|        2 |  1781 |  |
|        - |  1782 | `	SyHashEntry *pEntry;` |
|        - |  1783 | `	ph7_value *pValue;` |
|        - |  1784 | `	sxu32 nIdx;` |
|        - |  1785 | `	/* Query the superglobal table */` |
|     1332 |  1786 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1332 |  1787 | `	if( pEntry == 0 ){` |
|        - |  1788 | `		/* No such entry */` |
|      ! 0 |  1789 | `		return 0;` |
|        - |  1790 | `	}` |
|        - |  1791 | `	/* Extract the superglobal index in the global object pool */` |
|     1332 |  1792 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1793 | `	/* Extract the variable value  */` |
|     1332 |  1794 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1332 |  1795 | `	return pValue;` |
|      667 |  1796 |  |
|        - |  1797 | `/*` |
|        - |  1798 | ` * Perform a raw hashmap insertion.` |
|        - |  1799 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1800 | ` */` |
|     1328 |  1801 | `static sxi32 VmHashmapInsert(` |
|        - |  1802 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1803 | `	const char *zKey,   /* Entry key */` |
|        - |  1804 | `	int nKeylen,        /* zKey length*/` |
|        - |  1805 | `	const char *zData,  /* Entry data */` |
|        - |  1806 | `	int nLen            /* zData length */` |
|        - |  1807 | `	)` |
|        2 |  1808 |  |
|        - |  1809 | `	ph7_value sKey,sValue;` |
|        - |  1810 | `	sxi32 rc;` |
|     1330 |  1811 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1330 |  1812 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1330 |  1813 | `	if( zKey ){` |
|     1308 |  1814 | `		if( nKeylen < 0 ){` |
|     1308 |  1815 | `			nKeylen = (int)SyStrlen(zKey);` |
|      653 |  1816 | `		}` |
|     1308 |  1817 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      653 |  1818 | `	}` |
|     1330 |  1819 | `	if( zData ){` |
|     1330 |  1820 | `		if( nLen < 0 ){` |
|        - |  1821 | `			/* Compute length automatically */` |
|      ! 0 |  1822 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1823 | `		}` |
|     1330 |  1824 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      664 |  1825 | `	}` |
|        - |  1826 | `	/* Perform the insertion */` |
|     1330 |  1827 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1330 |  1828 | `	PH7_MemObjRelease(&sKey);` |
|     1330 |  1829 | `	PH7_MemObjRelease(&sValue);` |
|     1330 |  1830 | `	return rc;` |
|        2 |  1831 |  |
|        - |  1832 | `/* Forward declaration */` |
|        - |  1833 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1834 | `/*` |
|        - |  1835 | ` * Configure a working virtual machine instance.` |
|        - |  1836 | ` *` |
|        - |  1837 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1838 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1839 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1840 | ` * The second argument to this function is an integer configuration option` |
|        - |  1841 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1842 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1843 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1844 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1845 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1846 | ` */` |
|    20888 |  1847 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1848 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1849 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1850 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1851 | `	)` |
|        2 |  1852 |  |
|    20890 |  1853 | `	sxi32 rc = SXRET_OK;` |
|    20890 |  1854 | `	switch(nOp){` |
|      652 |  1855 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1306 |  1856 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1306 |  1857 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1858 | `		/* VM output consumer callback */` |
|        - |  1859 | `#ifdef UNTRUST` |
|        - |  1860 | `		if( xConsumer == 0 ){` |
|        - |  1861 | `			rc = SXERR_CORRUPT;` |
|        - |  1862 | `			break;` |
|        - |  1863 | `		}` |
|        - |  1864 | `#endif` |
|        - |  1865 | `		/* Install the output consumer */` |
|     1306 |  1866 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1306 |  1867 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1306 |  1868 | `		break;` |
|        - |  1869 | `							   }` |
|      652 |  1870 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1871 | `		/* Import path */` |
|        - |  1872 | `		  const char *zPath;` |
|        - |  1873 | `		  SyString sPath;` |
|     1306 |  1874 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1875 | `#if defined(UNTRUST)` |
|        - |  1876 | `		  if( zPath == 0 ){` |
|        - |  1877 | `			  rc = SXERR_EMPTY;` |
|        - |  1878 | `			  break;` |
|        - |  1879 | `		  }` |
|        - |  1880 | `#endif` |
|     1306 |  1881 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1882 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1883 | `#ifdef __WINNT__` |
|        2 |  1884 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1885 | `#endif` |
|     2610 |  1886 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1887 | `		  /* Remove leading and trailing white spaces */` |
|     1306 |  1888 | `		  SyStringFullTrim(&sPath);` |
|     1306 |  1889 | `		  if( sPath.nByte > 0 ){` |
|        - |  1890 | `			  /* Store the path in the corresponding conatiner */` |
|     1306 |  1891 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      652 |  1892 | `		  }` |
|     1306 |  1893 | `		  break;` |
|        - |  1894 | `									 }` |
|      652 |  1895 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1896 | `		/* Run-Time Error report */` |
|     1306 |  1897 | `		pVm->bErrReport = 1;` |
|     1306 |  1898 | `		break;` |
|      ! 0 |  1899 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1900 | `		/* Recursion depth */` |
|      ! 0 |  1901 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1902 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1903 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1904 | `		}` |
|      ! 0 |  1905 | `		break;` |
|        - |  1906 | `									   }` |
|      ! 0 |  1907 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1908 | `		/* VM output length in bytes */` |
|      ! 0 |  1909 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1910 | `#ifdef UNTRUST` |
|        - |  1911 | `		if( pOut == 0 ){` |
|        - |  1912 | `			rc = SXERR_CORRUPT;` |
|        - |  1913 | `			break;` |
|        - |  1914 | `		}` |
|        - |  1915 | `#endif` |
|      ! 0 |  1916 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1917 | `		break;` |
|        - |  1918 | `							   }` |
|        - |  1919 |  |
|     6520 |  1920 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1921 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1922 | `		/* Create a new superglobal/global variable */` |
|    13042 |  1923 | `		const char *zName = va_arg(ap,const char *);` |
|    13042 |  1924 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1925 | `		SyHashEntry *pEntry;` |
|        - |  1926 | `		ph7_value *pObj;` |
|        - |  1927 | `		sxu32 nByte;` |
|        - |  1928 | `		sxu32 nIdx;` |
|        - |  1929 | `#ifdef UNTRUST` |
|        - |  1930 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1931 | `			rc = SXERR_CORRUPT;` |
|        - |  1932 | `			break;` |
|        - |  1933 | `		}` |
|        - |  1934 | `#endif` |
|    13042 |  1935 | `		nByte = SyStrlen(zName);` |
|    13042 |  1936 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1937 | `			/* Check if the superglobal is already installed */` |
|    13042 |  1938 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     6522 |  1939 | `		}else{` |
|        - |  1940 | `			/* Query the top active VM frame */` |
|      ! 0 |  1941 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1942 | `		}` |
|    13042 |  1943 | `		if( pEntry ){` |
|        - |  1944 | `			/* Variable already installed */` |
|      ! 0 |  1945 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1946 | `			/* Extract contents */` |
|      ! 0 |  1947 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1948 | `			if( pObj ){` |
|        - |  1949 | `				/* Overwrite old contents */` |
|      ! 0 |  1950 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1951 | `			}` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Install a new variable */` |
|    13042 |  1954 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    13042 |  1955 | `			if( pObj == 0 ){` |
|      ! 0 |  1956 | `				rc = SXERR_MEM;` |
|      ! 0 |  1957 | `				break;` |
|        - |  1958 | `			}` |
|    13042 |  1959 | `			nIdx = pObj->nIdx;` |
|        - |  1960 | `			/* Copy value */` |
|    13042 |  1961 | `			PH7_MemObjStore(pValue,pObj);` |
|    13042 |  1962 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1963 | `				/* Install the superglobal */` |
|    13042 |  1964 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     6522 |  1965 | `			}else{` |
|        - |  1966 | `				/* Install in the current frame */` |
|      ! 0 |  1967 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1968 | `			}` |
|    13042 |  1969 | `			if( rc == SXRET_OK ){` |
|        - |  1970 | `				SyHashEntry *pRef;` |
|    13042 |  1971 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    13042 |  1972 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     6522 |  1973 | `				}else{` |
|      ! 0 |  1974 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1975 | `				}` |
|        - |  1976 | `				/* Install in the reference table */` |
|    13042 |  1977 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    13042 |  1978 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1979 | `					/* Register in the $GLOBALS array */` |
|    13042 |  1980 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     6520 |  1981 | `				}` |
|     6520 |  1982 | `			}` |
|        - |  1983 | `		}` |
|    13042 |  1984 | `		break;` |
|        - |  1985 | `									}` |
|      653 |  1986 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1987 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1988 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1989 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1990 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1991 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1992 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1308 |  1993 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1308 |  1994 | `		const char *zValue = va_arg(ap,const char *);` |
|     1308 |  1995 | `		int nLen = va_arg(ap,int);` |
|        - |  1996 | `		ph7_hashmap *pMap;` |
|        - |  1997 | `		ph7_value *pValue;` |
|     1308 |  1998 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1999 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2000 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1307 |  2001 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2002 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2003 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1306 |  2004 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2005 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2006 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1306 |  2007 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2008 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2009 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1306 |  2010 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2011 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2012 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1306 |  2013 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2014 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2015 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2016 | `		}else{` |
|        - |  2017 | `			/* Extract the $_SERVER superglobal */` |
|     1306 |  2018 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2019 | `		}` |
|     1308 |  2020 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2021 | `			/* No such entry */` |
|      ! 0 |  2022 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2023 | `			break;` |
|        - |  2024 | `		}` |
|        - |  2025 | `		/* Point to the hashmap */` |
|     1308 |  2026 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2027 | `		/* Perform the insertion */` |
|     1308 |  2028 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1308 |  2029 | `		break;` |
|        - |  2030 | `								   }` |
|       11 |  2031 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2032 | `		/* Script arguments */` |
|       24 |  2033 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2034 | `		ph7_hashmap *pMap;` |
|        - |  2035 | `		ph7_value *pValue;` |
|        - |  2036 | `		sxu32 n;` |
|       24 |  2037 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2038 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2039 | `			break;` |
|        - |  2040 | `		}` |
|        - |  2041 | `		/* Extract the $argv array */` |
|       24 |  2042 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2043 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2044 | `			/* No such entry */` |
|      ! 0 |  2045 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2046 | `			break;` |
|        - |  2047 | `		}` |
|        - |  2048 | `		/* Point to the hashmap */` |
|       24 |  2049 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2050 | `		/* Perform the insertion */` |
|       24 |  2051 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2052 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2053 | `		if( rc == SXRET_OK ){` |
|       24 |  2054 | `			if( pMap->nEntry > 1 ){` |
|        - |  2055 | `				/* Append space separator first */` |
|       18 |  2056 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2057 | `			}` |
|       24 |  2058 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2059 | `		}` |
|       24 |  2060 | `		break;` |
|        - |  2061 | `								  }` |
|      ! 0 |  2062 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2063 | `		/* error_log() consumer */` |
|      ! 0 |  2064 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2065 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2066 | `		break;` |
|        - |  2067 | `										}` |
|      ! 0 |  2068 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2069 | `		/* Script return value */` |
|      ! 0 |  2070 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2071 | `#ifdef UNTRUST` |
|        - |  2072 | `		if( ppValue == 0 ){` |
|        - |  2073 | `			rc = SXERR_CORRUPT;` |
|        - |  2074 | `			break;` |
|        - |  2075 | `		}` |
|        - |  2076 | `#endif` |
|      ! 0 |  2077 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2078 | `		break;` |
|        - |  2079 | `								   }` |
|     1304 |  2080 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2081 | `		/* Register an IO stream device */` |
|     2610 |  2082 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2083 | `		/* Make sure we are dealing with a valid IO stream */` |
|     3912 |  2084 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     2610 |  2085 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2086 | `				/* Invalid stream */` |
|      ! 0 |  2087 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2088 | `				break;` |
|        - |  2089 | `		}` |
|     2610 |  2090 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2091 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1306 |  2092 | `			pVm->pDefStream = pStream;` |
|      652 |  2093 | `		}` |
|        - |  2094 | `		/* Insert in the appropriate container */` |
|     2610 |  2095 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     2610 |  2096 | `		break;` |
|        - |  2097 | `								  }` |
|      ! 0 |  2098 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2099 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2100 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2101 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2102 | `#ifdef UNTRUST` |
|        - |  2103 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2104 | `			rc = SXERR_CORRUPT;` |
|        - |  2105 | `			break;` |
|        - |  2106 | `		}` |
|        - |  2107 | `#endif` |
|      ! 0 |  2108 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2109 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2110 | `		break;` |
|        - |  2111 | `									   }` |
|      ! 0 |  2112 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2113 | `		/* Raw HTTP request*/` |
|      ! 0 |  2114 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2115 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2116 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2117 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2118 | `			break;` |
|        - |  2119 | `		}` |
|      ! 0 |  2120 | `		if( nByte < 0 ){` |
|        - |  2121 | `			/* Compute length automatically */` |
|      ! 0 |  2122 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2123 | `		}` |
|        - |  2124 | `		/* Process the request */` |
|      ! 0 |  2125 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2126 | `		break;` |
|        - |  2127 | `									}` |
|      ! 0 |  2128 | `	default:` |
|        - |  2129 | `		/* Unknown configuration option */` |
|      ! 0 |  2130 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2131 | `		break;` |
|        - |  2132 | `	}` |
|    20890 |  2133 | `	return rc;` |
|        2 |  2134 |  |
|        - |  2135 | `/* Forward declaration */` |
|        - |  2136 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2137 | `/*` |
|        - |  2138 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2139 | ` * format.` |
|        - |  2140 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2141 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2142 | ` * (STDOUT).` |
|        - |  2143 | ` */` |
|        2 |  2144 | `static sxi32 VmByteCodeDump(` |
|        - |  2145 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2146 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2147 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2148 | `	)` |
|        1 |  2149 |  |
|        - |  2150 | `	static const char zDump[] = {` |
|        - |  2151 | `		"====================================================\n"` |
|        - |  2152 | `		"PH7 VM Dump\n"` |
|        - |  2153 | `		"====================================================\n"` |
|        - |  2154 | `	};` |
|        - |  2155 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2156 | `	sxi32 rc = SXRET_OK;` |
|        - |  2157 | `	sxu32 n;` |
|        - |  2158 | `	/* Point to the PH7 instructions */` |
|        3 |  2159 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2160 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2161 | `	n = 0;` |
|        3 |  2162 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2163 | `	/* Dump instructions */` |
|        6 |  2164 | `	for(;;){` |
|       13 |  2165 | `		if( pInstr >= pEnd ){` |
|        - |  2166 | `			/* No more instructions */` |
|        3 |  2167 | `			break;` |
|        - |  2168 | `		}` |
|        - |  2169 | `		/* Format and call the consumer callback */` |
|       16 |  2170 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2171 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2172 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2173 | `		if( rc != SXRET_OK ){` |
|        - |  2174 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2175 | `			return rc;` |
|        - |  2176 | `		}` |
|       11 |  2177 | `		++n;` |
|       11 |  2178 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2179 | `	}` |
|        3 |  2180 | `	return rc;` |
|        2 |  2181 |  |
|        - |  2182 | `/* Forward declaration */` |
|        - |  2183 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2184 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2185 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2186 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2187 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2188 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2189 | `/*` |
|        - |  2190 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2191 | ` * consumer callback.` |
|        - |  2192 | ` */` |
|      262 |  2193 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2194 |  |
|      263 |  2195 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      263 |  2196 | `	sxi32 rc = SXRET_OK;` |
|        - |  2197 | `	/* Append a new line */` |
|        - |  2198 | `#ifdef __WINNT__` |
|        1 |  2199 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2200 | `#else` |
|      262 |  2201 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2202 | `#endif` |
|        - |  2203 | `	/* Invoke the output consumer callback */` |
|      263 |  2204 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      263 |  2205 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2206 | `		/* Increment output length */` |
|      263 |  2207 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      131 |  2208 | `	}` |
|      263 |  2209 | `	return rc;` |
|        1 |  2210 |  |
|        - |  2211 | `/*` |
|        - |  2212 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2213 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2214 | ` * information.` |
|        - |  2215 | ` */` |
|      118 |  2216 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2217 |  |
|      120 |  2218 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2219 | `		ph7_value apArg[4];` |
|        - |  2220 | `		ph7_value *apArgPtr[4];` |
|        - |  2221 | `		ph7_value sResult;` |
|        - |  2222 | `		SyString sErr;` |
|        - |  2223 | `		/* Prepare arguments */` |
|       41 |  2224 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2225 | `			/* use explicit message length to avoid reading past buffer */` |
|       41 |  2226 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       41 |  2227 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       41 |  2228 | `		if( pFile ){` |
|       41 |  2229 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       41 |  2230 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       21 |  2231 | `		}else{` |
|      ! 0 |  2232 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2233 | `		}` |
|       41 |  2234 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       41 |  2235 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2236 | `		/* Set up pointer array */` |
|       41 |  2237 | `		apArgPtr[0] = &apArg[0];` |
|       41 |  2238 | `		apArgPtr[1] = &apArg[1];` |
|       41 |  2239 | `		apArgPtr[2] = &apArg[2];` |
|       41 |  2240 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2241 | `		/* Call the handler */` |
|       41 |  2242 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2243 | `		/* Check return value */` |
|       41 |  2244 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2245 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2246 | `		}` |
|        - |  2247 | `		/* Release */` |
|       41 |  2248 | `		PH7_MemObjRelease(&apArg[0]);` |
|       41 |  2249 | `		PH7_MemObjRelease(&apArg[1]);` |
|       41 |  2250 | `		PH7_MemObjRelease(&apArg[2]);` |
|       41 |  2251 | `		PH7_MemObjRelease(&apArg[3]);` |
|       41 |  2252 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2253 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2254 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       41 |  2255 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2256 | `	}` |
|        - |  2257 | `	/* No handler, always call error handler */` |
|       79 |  2258 | `	return TRUE;` |
|       61 |  2259 |  |
|       82 |  2260 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2261 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2262 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2263 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2264 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2265 | `	)` |
|        2 |  2266 |  |
|       84 |  2267 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2268 | `	SyString *pFile;` |
|        - |  2269 | `	char *zErr;` |
|       84 |  2270 | `	sxi32 rc = SXRET_OK;` |
|       84 |  2271 | `	if( !pVm->bErrReport ){` |
|        - |  2272 | `		/* Don't bother reporting errors */` |
|        3 |  2273 | `		return SXRET_OK;` |
|        - |  2274 | `	}` |
|        - |  2275 | `	/* Reset the working buffer */` |
|       82 |  2276 | `	SyBlobReset(pWorker);` |
|        - |  2277 | `	/* Peek the processed file if available */` |
|       82 |  2278 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       82 |  2279 | `	if( pFile ){` |
|        - |  2280 | `		/* Append file name */` |
|       82 |  2281 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       82 |  2282 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       40 |  2283 | `	}` |
|        - |  2284 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2285 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2286 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2287 | `	 * E_DEPRECATED). */` |
|       82 |  2288 | `	zErr = "Error:  ";` |
|       82 |  2289 | `	switch(iErr){` |
|       20 |  2290 | `	case PH7_CTX_WARNING:` |
|       42 |  2291 | `		zErr = "Warning:  ";` |
|       42 |  2292 | `		break;` |
|        6 |  2293 | `	case PH7_CTX_NOTICE:` |
|       14 |  2294 | `		zErr = "Notice:  ";` |
|       12 |  2295 | `		break;` |
|       14 |  2296 | `	default:` |
|        - |  2297 | `		/* keep iErr unchanged */` |
|       28 |  2298 | `		break;` |
|        - |  2299 | `	}` |
|       82 |  2300 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       82 |  2301 | `	if( pFuncName ){` |
|        - |  2302 | `		/* Append function name first */` |
|       29 |  2303 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2304 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2305 | `	}` |
|       82 |  2306 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2307 | `	/* Check for user error handler.  compute length of C string */` |
|       82 |  2308 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2309 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2310 | `	}` |
|       82 |  2311 | `	return rc;` |
|       43 |  2312 |  |
|        - |  2313 | `/*` |
|        - |  2314 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2315 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2316 | ` * information.` |
|        - |  2317 | ` */` |
|       38 |  2318 | `static sxi32 VmThrowErrorAp(` |
|        - |  2319 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2320 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2321 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2322 | `	const char *zFormat, /* Format message */` |
|        - |  2323 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2324 | `	)` |
|        2 |  2325 |  |
|       40 |  2326 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2327 | `	SyBlob sMsg;` |
|        - |  2328 | `	SyString *pFile;` |
|        - |  2329 | `	char *zErr;` |
|       40 |  2330 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2331 | `	if( !pVm->bErrReport ){` |
|        - |  2332 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2333 | `		return SXRET_OK;` |
|        - |  2334 | `	}` |
|        - |  2335 | `	/* Reset the working buffer */` |
|       40 |  2336 | `	SyBlobReset(pWorker);` |
|        - |  2337 | `	/* Peek the processed file if available */` |
|       40 |  2338 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2339 | `	if( pFile ){` |
|        - |  2340 | `		/* Append file name */` |
|       40 |  2341 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2342 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2343 | `	}` |
|        - |  2344 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2345 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2346 | `	 * the correct errno value. */` |
|       40 |  2347 | `	zErr = "Error:  ";` |
|       40 |  2348 | `	switch(iErr){` |
|        4 |  2349 | `	case PH7_CTX_WARNING:` |
|        9 |  2350 | `		zErr = "Warning:  ";` |
|        9 |  2351 | `		break;` |
|        3 |  2352 | `	case PH7_CTX_NOTICE:` |
|        7 |  2353 | `		zErr = "Notice:  ";` |
|        6 |  2354 | `		break;` |
|       12 |  2355 | `	default:` |
|        - |  2356 | `		/* do not change iErr */` |
|       24 |  2357 | `		break;` |
|        - |  2358 | `	}` |
|       40 |  2359 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2360 | `	if( pFuncName ){` |
|        - |  2361 | `		/* Append function name first */` |
|       26 |  2362 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2363 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2364 | `	}` |
|        - |  2365 | `	/* Format the raw message */` |
|       40 |  2366 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2367 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2368 | `	/* Check if a user error handler is installed */` |
|       40 |  2369 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2370 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2371 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2372 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2373 | `	}` |
|       40 |  2374 | `	SyBlobRelease(&sMsg);` |
|       40 |  2375 | `	return rc;` |
|       21 |  2376 |  |
|        - |  2377 | `/*` |
|        - |  2378 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2379 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2380 | ` * information.` |
|        - |  2381 | ` * ------------------------------------` |
|        - |  2382 | ` * Simple boring wrapper function.` |
|        - |  2383 | ` * ------------------------------------` |
|        - |  2384 | ` */` |
|       14 |  2385 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2386 |  |
|        - |  2387 | `	va_list ap;` |
|        - |  2388 | `	sxi32 rc;` |
|       15 |  2389 | `	va_start(ap,zFormat);` |
|       15 |  2390 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2391 | `	va_end(ap);` |
|       15 |  2392 | `	return rc;` |
|        1 |  2393 |  |
|        - |  2394 | `/*` |
|        - |  2395 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2396 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2397 | ` * information.` |
|        - |  2398 | ` * ------------------------------------` |
|        - |  2399 | ` * Simple boring wrapper function.` |
|        - |  2400 | ` * ------------------------------------` |
|        - |  2401 | ` */` |
|       24 |  2402 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2403 |  |
|        - |  2404 | `	sxi32 rc;` |
|       26 |  2405 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2406 | `	return rc;` |
|        2 |  2407 |  |
|        - |  2408 | `/*` |
|        - |  2409 | ` * Resolve function context from the current frame.` |
|        - |  2410 | ` */` |
|      368 |  2411 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2412 |  |
|        - |  2413 | `	VmFrame *pFrame;` |
|        - |  2414 | `	ph7_vm_func *pFunc;` |
|      369 |  2415 | `	*pzFuncName = 0;` |
|      369 |  2416 | `	*pnFuncLen = 0;` |
|      369 |  2417 | `	pFrame = pVm->pFrame;` |
|      369 |  2418 | `	if( pFrame == 0 ){` |
|      ! 0 |  2419 | `		return;` |
|        - |  2420 | `	}` |
|      369 |  2421 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2422 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2423 | `	}` |
|      369 |  2424 | `	if( pFrame->pParent == 0 ){` |
|      369 |  2425 | `		return;` |
|        - |  2426 | `	}` |
|      ! 0 |  2427 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  2428 | `	if( pFunc == 0 ){` |
|      ! 0 |  2429 | `		return;` |
|        - |  2430 | `	}` |
|      ! 0 |  2431 | `	*pzFuncName = pFunc->sName.zString;` |
|      ! 0 |  2432 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      185 |  2433 |  |
|        - |  2434 | `/*` |
|        - |  2435 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2436 | ` */` |
|      184 |  2437 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2438 |  |
|        - |  2439 | `	SyBlob sOut;` |
|        - |  2440 | `	SyString *pFile;` |
|      185 |  2441 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2442 | `		return PH7_OK;` |
|        - |  2443 | `	}` |
|      185 |  2444 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2445 | `		zClass = "Exception";` |
|      ! 0 |  2446 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2447 | `	}` |
|      185 |  2448 | `	if( zMsg == 0 ){` |
|      ! 0 |  2449 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2450 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2451 | `	}` |
|      185 |  2452 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      185 |  2453 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       92 |  2454 | `	}` |
|      185 |  2455 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      185 |  2456 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      185 |  2457 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      185 |  2458 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      185 |  2459 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      185 |  2460 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      185 |  2461 | `	if( pFile ){` |
|      185 |  2462 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      185 |  2463 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      185 |  2464 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|       92 |  2465 | `	}` |
|      185 |  2466 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      185 |  2467 | `	if( pFile ){` |
|      185 |  2468 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      185 |  2469 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      185 |  2470 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2471 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2472 | `		}else{` |
|      185 |  2473 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2474 | `		}` |
|       92 |  2475 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2476 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2477 | `	}else{` |
|      ! 0 |  2478 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2479 | `	}` |
|      185 |  2480 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      185 |  2481 | `	if( pFile ){` |
|      185 |  2482 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      185 |  2483 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      185 |  2484 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      185 |  2485 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|       92 |  2486 | `	}` |
|      185 |  2487 | `	VmCallErrorHandler(pVm,&sOut);` |
|      185 |  2488 | `	SyBlobRelease(&sOut);` |
|      185 |  2489 | `	return PH7_ABORT;` |
|       93 |  2490 |  |
|        - |  2491 | `/*` |
|        - |  2492 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2493 | ` */` |
|      184 |  2494 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2495 |  |
|        - |  2496 | `	ph7_vm *pVm;` |
|        - |  2497 | `	ph7_class *pClass;` |
|        - |  2498 | `	ph7_class_instance *pThis;` |
|        - |  2499 | `	ph7_class_method *pCons;` |
|        - |  2500 | `	ph7_value sArg;` |
|        - |  2501 | `	ph7_value *apArg[1];` |
|        - |  2502 | `	SyBlob sMsg;` |
|        - |  2503 | `	SyString sMsgStr;` |
|        - |  2504 | `	VmFrame *pFrame;` |
|        - |  2505 | `	va_list ap;` |
|        - |  2506 | `	sxi32 rc;` |
|        - |  2507 |  |
|      186 |  2508 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2509 | `		return PH7_ABORT;` |
|        - |  2510 | `	}` |
|      186 |  2511 | `	pVm = pCtx->pVm;` |
|      186 |  2512 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2513 | `		zClass = "Error";` |
|      ! 0 |  2514 | `	}` |
|      186 |  2515 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      186 |  2516 | `	if( pClass == 0 ){` |
|      ! 0 |  2517 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2518 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2519 | `			zClass` |
|        - |  2520 | `			);` |
|        - |  2521 | `	}` |
|      186 |  2522 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      186 |  2523 | `	if( pThis == 0 ){` |
|      ! 0 |  2524 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2525 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2526 | `			);` |
|        - |  2527 | `	}` |
|        - |  2528 |  |
|      186 |  2529 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      186 |  2530 | `	va_start(ap,zFormat);` |
|      186 |  2531 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      186 |  2532 | `	va_end(ap);` |
|        - |  2533 |  |
|      186 |  2534 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      186 |  2535 | `	if( pCons ){` |
|      186 |  2536 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      186 |  2537 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      186 |  2538 | `		apArg[0] = &sArg;` |
|      186 |  2539 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      186 |  2540 | `		PH7_MemObjRelease(&sArg);` |
|       92 |  2541 | `	}` |
|      186 |  2542 | `	SyBlobRelease(&sMsg);` |
|        - |  2543 |  |
|      186 |  2544 | `	pFrame = pVm->pFrame;` |
|      186 |  2545 | `	if( pFrame ){` |
|      188 |  2546 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2547 | `			pFrame = pFrame->pParent;` |
|        1 |  2548 | `		}` |
|      186 |  2549 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       92 |  2550 | `	}` |
|      186 |  2551 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      186 |  2552 | `	PH7_ClassInstanceUnref(pThis);` |
|      186 |  2553 | `	if( rc == SXERR_ABORT ){` |
|      183 |  2554 | `		return PH7_ABORT;` |
|        - |  2555 | `	}` |
|        3 |  2556 | `	return PH7_EXCEPTION;` |
|       94 |  2557 |  |
|        - |  2558 | `/*` |
|        - |  2559 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2560 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2561 | ` */` |
|      ! 0 |  2562 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2563 |  |
|        - |  2564 | `	ph7_vm *pVm;` |
|        - |  2565 | `	SyBlob sMsg;` |
|      ! 0 |  2566 | `	const char *zFuncName = 0;` |
|      ! 0 |  2567 | `	int nFuncLen = 0;` |
|        - |  2568 | `	va_list ap;` |
|        - |  2569 | `	sxi32 rc;` |
|        - |  2570 |  |
|      ! 0 |  2571 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2572 | `		return PH7_OK;` |
|        - |  2573 | `	}` |
|      ! 0 |  2574 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2575 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2576 | `		zClass = "Error";` |
|      ! 0 |  2577 | `	}` |
|        - |  2578 |  |
|      ! 0 |  2579 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2580 |  |
|      ! 0 |  2581 | `	va_start(ap,zFormat);` |
|      ! 0 |  2582 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2583 | `	va_end(ap);` |
|        - |  2584 |  |
|      ! 0 |  2585 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2586 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2587 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2588 | `	}` |
|      ! 0 |  2589 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2590 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2591 | `	}` |
|      ! 0 |  2592 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2593 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2594 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2595 | `	return rc;` |
|      ! 0 |  2596 |  |
|        - |  2597 | `/*` |
|        - |  2598 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2599 | ` *` |
|        - |  2600 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2601 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2602 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2603 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2604 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2605 | ` * then the program execution is halted.` |
|        - |  2606 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2607 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2608 | ` * or to reset the VM to it's initial state.` |
|        - |  2609 | ` */` |
|    22936 |  2610 | `static sxi32 VmByteCodeExec(` |
|        - |  2611 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2612 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2613 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2614 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2615 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2616 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2617 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2618 | `	)` |
|        2 |  2619 |  |
|        - |  2620 | `	VmInstr *pInstr;` |
|        - |  2621 | `	ph7_value *pTos;` |
|        - |  2622 | `	SySet aArg;` |
|        - |  2623 | `	sxi32 pc;` |
|        - |  2624 | `	sxi32 rc;` |
|        - |  2625 | `	/* Argument container */` |
|    22938 |  2626 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    22938 |  2627 | `	if( nTos < 0 ){` |
|    22012 |  2628 | `		pTos = &pStack[-1];` |
|    11007 |  2629 | `	}else{` |
|      928 |  2630 | `		pTos = &pStack[nTos];` |
|        - |  2631 | `	}` |
|    22938 |  2632 | `	pc = 0;` |
|        - |  2633 | `	/* Execute as much as we can */` |
|  3991600 |  2634 | `	for(;;){` |
|        - |  2635 | `		/* Fetch the instruction to execute */` |
|  7982498 |  2636 | `		pInstr = &aInstr[pc];` |
|  7982498 |  2637 | `		rc = SXRET_OK;` |
|        - |  2638 | `/*` |
|        - |  2639 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2640 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2641 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2642 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2643 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2644 | ` */` |
|  7982498 |  2645 | `		switch(pInstr->iOp){` |
|        - |  2646 | `/*` |
|        - |  2647 | ` * DONE: P1 * *` |
|        - |  2648 | ` *` |
|        - |  2649 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2650 | ` * and return immediately.` |
|        - |  2651 | ` */` |
|    11369 |  2652 | `case PH7_OP_DONE:` |
|    22740 |  2653 | `	if( pInstr->iP1 ){` |
|        - |  2654 | `#ifdef UNTRUST` |
|        - |  2655 | `		if( pTos < pStack ){` |
|        - |  2656 | `			goto Abort;` |
|        - |  2657 | `		}` |
|        - |  2658 | `#endif` |
|    12636 |  2659 | `		if( pLastRef ){` |
|     8570 |  2660 | `			*pLastRef = pTos->nIdx;` |
|     4284 |  2661 | `		}` |
|    12636 |  2662 | `		if( pResult ){` |
|        - |  2663 | `			/* Execution result */` |
|    12166 |  2664 | `			PH7_MemObjStore(pTos,pResult);` |
|     6082 |  2665 | `		}` |
|    12636 |  2666 | `		VmPopOperand(&pTos,1);` |
|    16423 |  2667 | `	}else if( pLastRef ){` |
|        - |  2668 | `		/* Nothing referenced */` |
|      556 |  2669 | `		*pLastRef = SXU32_HIGH;` |
|      277 |  2670 | `	}` |
|    22740 |  2671 | `	goto Done;` |
|        - |  2672 | `/*` |
|        - |  2673 | ` * HALT: P1 * *` |
|        - |  2674 | ` *` |
|        - |  2675 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2676 | ` * and abort immediately.` |
|        - |  2677 | ` */` |
|        4 |  2678 | `case PH7_OP_HALT:` |
|        9 |  2679 | `	if( pInstr->iP1 ){` |
|        - |  2680 | `#ifdef UNTRUST` |
|        - |  2681 | `		if( pTos < pStack ){` |
|        - |  2682 | `			goto Abort;` |
|        - |  2683 | `		}` |
|        - |  2684 | `#endif` |
|        9 |  2685 | `		if( pLastRef ){` |
|      ! 0 |  2686 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2687 | `		}` |
|        9 |  2688 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2689 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2690 | `				/* Output the exit message */` |
|        7 |  2691 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2692 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2693 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2694 | `					/* Increment output length */` |
|        5 |  2695 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2696 | `				}` |
|        3 |  2697 | `			}` |
|        7 |  2698 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2699 | `			/* Record exit status */` |
|        5 |  2700 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2701 | `		}` |
|        9 |  2702 | `		VmPopOperand(&pTos,1);` |
|        4 |  2703 | `	}else if( pLastRef ){` |
|        - |  2704 | `		/* Nothing referenced */` |
|      ! 0 |  2705 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2706 | `	}` |
|        - |  2707 | `	/* Check if we're in an included file context */` |
|        9 |  2708 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2709 | `		/* Terminate the entire process */` |
|        9 |  2710 | `		exit(pVm->iExitStatus);` |
|        - |  2711 | `	}` |
|      ! 0 |  2712 | `	goto Abort;` |
|        - |  2713 | `/*` |
|        - |  2714 | ` * JMP: * P2 *` |
|        - |  2715 | ` *` |
|        - |  2716 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2717 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2718 | ` */` |
|   176994 |  2719 | `case PH7_OP_JMP:` |
|   354034 |  2720 | `	pc = pInstr->iP2 - 1;` |
|   354034 |  2721 | `	break;` |
|        - |  2722 | `/*` |
|        - |  2723 | ` * JZ: P1 P2 *` |
|        - |  2724 | ` *` |
|        - |  2725 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2726 | ` * entry in the stack if P1 is zero.` |
|        - |  2727 | ` */` |
|   402985 |  2728 | `case PH7_OP_JZ:` |
|        - |  2729 | `#ifdef UNTRUST` |
|        - |  2730 | `	if( pTos < pStack ){` |
|        - |  2731 | `		goto Abort;` |
|        - |  2732 | `	}` |
|        - |  2733 | `#endif` |
|        - |  2734 | `	/* Get a boolean value */` |
|   806060 |  2735 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2736 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2737 | `	}` |
|   806060 |  2738 | `	if( !pTos->x.iVal ){` |
|        - |  2739 | `		/* Take the jump */` |
|   385938 |  2740 | `		pc = pInstr->iP2 - 1;` |
|   192968 |  2741 | `	}` |
|   806060 |  2742 | `	if( !pInstr->iP1 ){` |
|   632210 |  2743 | `		VmPopOperand(&pTos,1);` |
|   316126 |  2744 | `	}` |
|   806060 |  2745 | `	break;` |
|        - |  2746 | `/*` |
|        - |  2747 | ` * JNZ: P1 P2 *` |
|        - |  2748 | ` *` |
|        - |  2749 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2750 | ` * entry in the stack if P1 is zero.` |
|        - |  2751 | ` */` |
|    39493 |  2752 | `case PH7_OP_JNZ:` |
|        - |  2753 | `#ifdef UNTRUST` |
|        - |  2754 | `	if( pTos < pStack ){` |
|        - |  2755 | `		goto Abort;` |
|        - |  2756 | `	}` |
|        - |  2757 | `#endif` |
|        - |  2758 | `	/* Get a boolean value */` |
|    78988 |  2759 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2760 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2761 | `	}` |
|    78988 |  2762 | `	if( pTos->x.iVal ){` |
|        - |  2763 | `		/* Take the jump */` |
|     3456 |  2764 | `		pc = pInstr->iP2 - 1;` |
|     1727 |  2765 | `	}` |
|    78988 |  2766 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2767 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2768 | `	}` |
|    78988 |  2769 | `	break;` |
|        - |  2770 | `/*` |
|        - |  2771 | ` * NOOP: * * *` |
|        - |  2772 | ` *` |
|        - |  2773 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2774 | ` * destination.` |
|        - |  2775 | ` */` |
|      ! 0 |  2776 | `case PH7_OP_NOOP:` |
|      ! 0 |  2777 | `	break;` |
|        - |  2778 | `/*` |
|        - |  2779 | ` * POP: P1 * *` |
|        - |  2780 | ` *` |
|        - |  2781 | ` * Pop P1 elements from the operand stack.` |
|        - |  2782 | ` */` |
|   317546 |  2783 | `case PH7_OP_POP: {` |
|   635138 |  2784 | `	sxi32 n = pInstr->iP1;` |
|   635138 |  2785 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2786 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2787 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2788 | `	}` |
|   635138 |  2789 | `	VmPopOperand(&pTos,n);` |
|   635138 |  2790 | `	break;` |
|        - |  2791 | `				 }` |
|        - |  2792 | `/*` |
|        - |  2793 | ` * CVT_INT: * * *` |
|        - |  2794 | ` *` |
|        - |  2795 | ` * Force the top of the stack to be an integer.` |
|        - |  2796 | ` */` |
|       35 |  2797 | `case PH7_OP_CVT_INT:` |
|        - |  2798 | `#ifdef UNTRUST` |
|        - |  2799 | `	if( pTos < pStack ){` |
|        - |  2800 | `		goto Abort;` |
|        - |  2801 | `	}` |
|        - |  2802 | `#endif` |
|       72 |  2803 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2804 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2805 | `	}` |
|        - |  2806 | `	/* Invalidate any prior representation */` |
|       72 |  2807 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2808 | `	break;` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * CVT_REAL: * * *` |
|        - |  2811 | ` *` |
|        - |  2812 | ` * Force the top of the stack to be a real.` |
|        - |  2813 | ` */` |
|        4 |  2814 | `case PH7_OP_CVT_REAL:` |
|        - |  2815 | `#ifdef UNTRUST` |
|        - |  2816 | `	if( pTos < pStack ){` |
|        - |  2817 | `		goto Abort;` |
|        - |  2818 | `	}` |
|        - |  2819 | `#endif` |
|        9 |  2820 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2821 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2822 | `	}` |
|        - |  2823 | `	/* Invalidate any prior representation */` |
|        9 |  2824 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2825 | `	break;` |
|        - |  2826 | `/*` |
|        - |  2827 | ` * CVT_STR: * * *` |
|        - |  2828 | ` *` |
|        - |  2829 | ` * Force the top of the stack to be a string.` |
|        - |  2830 | ` */` |
|      136 |  2831 | `case PH7_OP_CVT_STR:` |
|        - |  2832 | `#ifdef UNTRUST` |
|        - |  2833 | `	if( pTos < pStack ){` |
|        - |  2834 | `		goto Abort;` |
|        - |  2835 | `	}` |
|        - |  2836 | `#endif` |
|      274 |  2837 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2838 | `		PH7_MemObjToString(pTos);` |
|      136 |  2839 | `	}` |
|      274 |  2840 | `	break;` |
|        - |  2841 | `/*` |
|        - |  2842 | ` * CVT_BOOL: * * *` |
|        - |  2843 | ` *` |
|        - |  2844 | ` * Force the top of the stack to be a boolean.` |
|        - |  2845 | ` */` |
|        5 |  2846 | `case PH7_OP_CVT_BOOL:` |
|        - |  2847 | `#ifdef UNTRUST` |
|        - |  2848 | `	if( pTos < pStack ){` |
|        - |  2849 | `		goto Abort;` |
|        - |  2850 | `	}` |
|        - |  2851 | `#endif` |
|       11 |  2852 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2853 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2854 | `	}` |
|       11 |  2855 | `	break;` |
|        - |  2856 | `/*` |
|        - |  2857 | ` * CVT_NULL: * * *` |
|        - |  2858 | ` *` |
|        - |  2859 | ` * Nullify the top of the stack.` |
|        - |  2860 | ` */` |
|        3 |  2861 | `case PH7_OP_CVT_NULL:` |
|        - |  2862 | `#ifdef UNTRUST` |
|        - |  2863 | `	if( pTos < pStack ){` |
|        - |  2864 | `		goto Abort;` |
|        - |  2865 | `	}` |
|        - |  2866 | `#endif` |
|        7 |  2867 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2868 | `	break;` |
|        - |  2869 | `/*` |
|        - |  2870 | ` * CVT_NUMC: * * *` |
|        - |  2871 | ` *` |
|        - |  2872 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2873 | ` */` |
|      ! 0 |  2874 | `case PH7_OP_CVT_NUMC:` |
|        - |  2875 | `#ifdef UNTRUST` |
|        - |  2876 | `	if( pTos < pStack ){` |
|        - |  2877 | `		goto Abort;` |
|        - |  2878 | `	}` |
|        - |  2879 | `#endif` |
|        - |  2880 | `	/* Force a numeric cast */` |
|      ! 0 |  2881 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2882 | `	break;` |
|        - |  2883 | `/*` |
|        - |  2884 | ` * CVT_ARRAY: * * *` |
|        - |  2885 | ` *` |
|        - |  2886 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2887 | ` */` |
|       10 |  2888 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2889 | `#ifdef UNTRUST` |
|        - |  2890 | `	if( pTos < pStack ){` |
|        - |  2891 | `		goto Abort;` |
|        - |  2892 | `	}` |
|        - |  2893 | `#endif` |
|        - |  2894 | `	/* Force a hashmap cast */` |
|       21 |  2895 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2896 | `	if( rc != SXRET_OK ){` |
|        - |  2897 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2898 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2899 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2900 | `	}` |
|       21 |  2901 | `	break;` |
|        - |  2902 | `/*` |
|        - |  2903 | ` * CVT_OBJ: * * *` |
|        - |  2904 | ` *` |
|        - |  2905 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2906 | ` */` |
|        8 |  2907 | `case PH7_OP_CVT_OBJ:` |
|        - |  2908 | `#ifdef UNTRUST` |
|        - |  2909 | `	if( pTos < pStack ){` |
|        - |  2910 | `		goto Abort;` |
|        - |  2911 | `	}` |
|        - |  2912 | `#endif` |
|       17 |  2913 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2914 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2915 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2916 | `	}` |
|       17 |  2917 | `	break;` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * ERR_CTRL * * *` |
|        - |  2920 | ` *` |
|        - |  2921 | ` * Error control operator.` |
|        - |  2922 | ` */` |
|    10064 |  2923 | `case PH7_OP_ERR_CTRL:` |
|        - |  2924 | `	/*` |
|        - |  2925 | `	 * TICKET 1433-038:` |
|        - |  2926 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2927 | `	 * use the public API,to control error output.` |
|        - |  2928 | `	 */` |
|    20128 |  2929 | `	break;` |
|        - |  2930 | `/*` |
|        - |  2931 | ` * IS_A * * *` |
|        - |  2932 | ` *` |
|        - |  2933 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2934 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2935 | ` * holding a class name or an object).` |
|        - |  2936 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2937 | ` */` |
|       11 |  2938 | `case PH7_OP_IS_A:{` |
|       23 |  2939 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2940 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2941 | `#ifdef UNTRUST` |
|        - |  2942 | `	if( pNos < pStack ){` |
|        - |  2943 | `		goto Abort;` |
|        - |  2944 | `	}` |
|        - |  2945 | `#endif` |
|       23 |  2946 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2947 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2948 | `		ph7_class *pClass = 0;` |
|        - |  2949 | `		/* Extract the target class */` |
|       21 |  2950 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2951 | `			/* Instance already loaded */` |
|      ! 0 |  2952 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2953 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2954 | `			/* Perform the query */` |
|       31 |  2955 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2956 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2957 | `		}` |
|       21 |  2958 | `		if( pClass ){` |
|        - |  2959 | `			/* Perform the query */` |
|       21 |  2960 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2961 | `		}` |
|       10 |  2962 | `	}` |
|        - |  2963 | `	/* Push result */` |
|       23 |  2964 | `	VmPopOperand(&pTos,1);` |
|       23 |  2965 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2966 | `	pTos->x.iVal = iRes;` |
|       23 |  2967 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2968 | `	break;` |
|        - |  2969 | `				 }` |
|        - |  2970 |  |
|        - |  2971 | `/*` |
|        - |  2972 | ` * LOADC P1 P2 *` |
|        - |  2973 | ` *` |
|        - |  2974 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2975 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2976 | ` */` |
|   672783 |  2977 | `case PH7_OP_LOADC: {` |
|        - |  2978 | `	ph7_value *pObj;` |
|        - |  2979 | `	/* Reserve a room */` |
|  1345612 |  2980 | `	pTos++;` |
|  1345612 |  2981 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1345612 |  2982 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2983 | `			SyHashEntry *pEntry;` |
|        - |  2984 | `			/* Candidate for expansion via user defined callbacks */` |
|    14384 |  2985 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    14384 |  2986 | `			if( pEntry ){` |
|    12364 |  2987 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2988 | `				/* Set a NULL default value */` |
|    12364 |  2989 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    12364 |  2990 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2991 | `				/* Invoke the callback and deal with the expanded value */` |
|    12364 |  2992 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2993 | `				/* Mark as constant */` |
|    12364 |  2994 | `				pTos->nIdx = SXU32_HIGH;` |
|    12364 |  2995 | `				break;` |
|        - |  2996 | `			}` |
|     1010 |  2997 | `		}` |
|  1333250 |  2998 | `		PH7_MemObjLoad(pObj,pTos);` |
|   666648 |  2999 | `	}else{` |
|        - |  3000 | `		/* Set a NULL value */` |
|      ! 0 |  3001 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3002 | `	}` |
|        - |  3003 | `	/* Mark as constant */` |
|  1333250 |  3004 | `	pTos->nIdx = SXU32_HIGH;` |
|  1333250 |  3005 | `	break;` |
|        - |  3006 | `				  }` |
|        - |  3007 | `/*` |
|        - |  3008 | ` * LOAD: P1 * P3` |
|        - |  3009 | ` *` |
|        - |  3010 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3011 | ` * from the P3 operand.` |
|        - |  3012 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3013 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3014 | ` */` |
|  1077770 |  3015 | `case PH7_OP_LOAD:{` |
|        - |  3016 | `	ph7_value *pObj;` |
|        - |  3017 | `	SyString sName;` |
|  2155762 |  3018 | `	if( pInstr->p3 == 0 ){` |
|        - |  3019 | `		/* Take the variable name from the top of the stack */` |
|        - |  3020 | `#ifdef UNTRUST` |
|        - |  3021 | `		if( pTos < pStack ){` |
|        - |  3022 | `			goto Abort;` |
|        - |  3023 | `		}` |
|        - |  3024 | `#endif` |
|        - |  3025 | `		/* Force a string cast */` |
|       19 |  3026 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3027 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3028 | `		}` |
|       19 |  3029 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3030 | `	}else{` |
|  2155744 |  3031 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3032 | `		/* Reserve a room for the target object */` |
|  2155744 |  3033 | `		pTos++;` |
|        - |  3034 | `	}` |
|        - |  3035 | `	/* Extract the requested memory object */` |
|  2155762 |  3036 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2155762 |  3037 | `	if( pObj == 0 ){` |
|      482 |  3038 | `		if( pInstr->iP1 ){` |
|        - |  3039 | `			/* Variable not found,load NULL */` |
|      482 |  3040 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3041 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3042 | `			}else{` |
|      482 |  3043 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3044 | `			}` |
|      482 |  3045 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1078012 |  3046 | `			break;` |
|      ! 0 |  3047 | `		}else{` |
|        - |  3048 | `			/* Fatal error */` |
|      ! 0 |  3049 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3050 | `			goto Abort;` |
|        - |  3051 | `		}` |
|        - |  3052 | `	}` |
|        - |  3053 | `	/* Load variable contents */` |
|  2155282 |  3054 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2155282 |  3055 | `	pTos->nIdx = pObj->nIdx;` |
|  2155282 |  3056 | `	break;` |
|        - |  3057 | `				   }` |
|        - |  3058 | `/*` |
|        - |  3059 | ` * LOAD_MAP P1 * *` |
|        - |  3060 | ` *` |
|        - |  3061 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3062 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3063 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3064 | ` */` |
|    14531 |  3065 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3066 | `	ph7_hashmap *pMap;` |
|        - |  3067 | `	/* Allocate a new hashmap instance */` |
|    29064 |  3068 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    29064 |  3069 | `	if( pMap == 0 ){` |
|      ! 0 |  3070 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3071 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3072 | `		goto Abort;` |
|        - |  3073 | `	}` |
|    29064 |  3074 | `	if( pInstr->iP1 > 0 ){` |
|     1628 |  3075 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3076 | `		/* Perform the insertion */` |
|     4698 |  3077 | `		while( pEntry < pTos ){` |
|     3072 |  3078 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3079 | `				/* Insertion by reference */` |
|      142 |  3080 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3081 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3082 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3083 | `					);` |
|       48 |  3084 | `			}else{` |
|        - |  3085 | `				/* Standard insertion */` |
|     4466 |  3086 | `				PH7_HashmapInsert(pMap,` |
|     2976 |  3087 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1488 |  3088 | `					&pEntry[1]` |
|        - |  3089 | `				);` |
|        - |  3090 | `			}` |
|        - |  3091 | `			/* Next pair on the stack */` |
|     3072 |  3092 | `			pEntry += 2;` |
|        2 |  3093 | `		}` |
|        - |  3094 | `		/* Pop P1 elements */` |
|     1628 |  3095 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      813 |  3096 | `	}` |
|        - |  3097 | `	/* Push the hashmap */` |
|    29064 |  3098 | `	pTos++;` |
|    29064 |  3099 | `	pTos->nIdx = SXU32_HIGH;` |
|    29064 |  3100 | `	pTos->x.pOther = pMap;` |
|    29064 |  3101 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    29064 |  3102 | `	break;` |
|        - |  3103 | `					  }` |
|        - |  3104 | `/*` |
|        - |  3105 | ` * LOAD_LIST: P1 * *` |
|        - |  3106 | ` *` |
|        - |  3107 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3108 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3109 | ` * Caveats:` |
|        - |  3110 | ` *  This implementation support only a single nesting level.` |
|        - |  3111 | ` */` |
|       17 |  3112 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3113 | `	ph7_value *pEntry;` |
|       35 |  3114 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3115 | `		/* Empty list,break immediately */` |
|      ! 0 |  3116 | `		break;` |
|        - |  3117 | `	}` |
|       35 |  3118 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3119 | `#ifdef UNTRUST` |
|        - |  3120 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3121 | `		goto Abort;` |
|        - |  3122 | `	}` |
|        - |  3123 | `#endif` |
|       35 |  3124 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3125 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3126 | `		ph7_hashmap_node *pNode;` |
|        - |  3127 | `		ph7_value sKey,*pObj;` |
|        - |  3128 | `		/* Start Copying */` |
|       31 |  3129 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3130 | `		while( pEntry <= pTos ){` |
|       69 |  3131 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3132 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3133 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3134 | `					if( rc == SXRET_OK ){` |
|        - |  3135 | `						/* Store node value */` |
|       65 |  3136 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3137 | `					}else{` |
|        - |  3138 | `						/* Nullify the variable */` |
|      ! 0 |  3139 | `						PH7_MemObjRelease(pObj);` |
|        - |  3140 | `					}` |
|       32 |  3141 | `				}` |
|       32 |  3142 | `			}` |
|       69 |  3143 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3144 | `			pEntry++;` |
|        1 |  3145 | `		}` |
|       15 |  3146 | `	}` |
|       35 |  3147 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3148 | `	break;` |
|        - |  3149 | `					   }` |
|        - |  3150 | `/*` |
|        - |  3151 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3152 | ` *` |
|        - |  3153 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3154 | ` * from the stack.` |
|        - |  3155 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3156 | ` * instead.` |
|        - |  3157 | ` */` |
|   167578 |  3158 | `case PH7_OP_LOAD_IDX: {` |
|   335202 |  3159 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   335202 |  3160 | `	ph7_hashmap *pMap = 0;` |
|        - |  3161 | `	ph7_value *pIdx;` |
|   335202 |  3162 | `	pIdx = 0;` |
|   335202 |  3163 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3164 | `		if( !pInstr->iP2){` |
|        - |  3165 | `			/* No available index,load NULL */` |
|      ! 0 |  3166 | `			if( pTos >= pStack ){` |
|      ! 0 |  3167 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3168 | `			}else{` |
|        - |  3169 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3170 | `				pTos++;` |
|      ! 0 |  3171 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3172 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3173 | `			}` |
|        - |  3174 | `			/* Emit a notice */` |
|      ! 0 |  3175 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3176 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3177 | `			break;` |
|        - |  3178 | `		}` |
|      ! 0 |  3179 | `	}else{` |
|   335202 |  3180 | `		pIdx = pTos;` |
|   335202 |  3181 | `		pTos--;` |
|        - |  3182 | `	}` |
|   335202 |  3183 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3184 | `		/* String access */` |
|   263124 |  3185 | `		if( pIdx ){` |
|        - |  3186 | `			sxu32 nOfft;` |
|   263124 |  3187 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3188 | `				/* Force an int cast */` |
|      ! 0 |  3189 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3190 | `			}` |
|   263124 |  3191 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   263124 |  3192 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3193 | `				/* Invalid offset,load null */` |
|      ! 0 |  3194 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3195 | `			}else{` |
|   263124 |  3196 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   263124 |  3197 | `				int c = zData[nOfft];` |
|   263124 |  3198 | `				PH7_MemObjRelease(pTos);` |
|   263124 |  3199 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   263124 |  3200 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3201 | `			}` |
|   131585 |  3202 | `		}else{` |
|        - |  3203 | `			/* No available index,load NULL */` |
|      ! 0 |  3204 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3205 | `		}` |
|   263124 |  3206 | `		break;` |
|        - |  3207 | `	}` |
|    72080 |  3208 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3209 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3210 | `			ph7_value *pObj;` |
|      ! 0 |  3211 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3212 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3213 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3214 | `			}` |
|      ! 0 |  3215 | `		}` |
|      ! 0 |  3216 | `	}` |
|    72080 |  3217 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    72080 |  3218 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3219 | `		/* Point to the hashmap */` |
|    72080 |  3220 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    72080 |  3221 | `		if( pIdx ){` |
|        - |  3222 | `			/* Load the desired entry */` |
|    72080 |  3223 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    36039 |  3224 | `		}` |
|    72080 |  3225 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3226 | `			/* Create a new empty entry */` |
|      ! 0 |  3227 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3228 | `			if( rc == SXRET_OK ){` |
|        - |  3229 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3230 | `				pNode = pMap->pLast;` |
|      ! 0 |  3231 | `			}` |
|      ! 0 |  3232 | `		}` |
|    36039 |  3233 | `	}` |
|    72080 |  3234 | `	if( pIdx ){` |
|    72080 |  3235 | `		PH7_MemObjRelease(pIdx);` |
|    36039 |  3236 | `	}` |
|    72080 |  3237 | `	if( rc == SXRET_OK ){` |
|        - |  3238 | `		/* Load entry contents */` |
|    34078 |  3239 | `		if( pMap->iRef < 2 ){` |
|        - |  3240 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3241 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3242 | `			 */` |
|        7 |  3243 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3244 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3245 | `		}else{` |
|    34072 |  3246 | `			pTos->nIdx = pNode->nValIdx;` |
|    34072 |  3247 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    34072 |  3248 | `			PH7_HashmapUnref(pMap);` |
|        - |  3249 | `		}` |
|    17040 |  3250 | `	}else{` |
|        - |  3251 | `		/* No such entry,load NULL */` |
|    38004 |  3252 | `		PH7_MemObjRelease(pTos);` |
|    38004 |  3253 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3254 | `	}` |
|    72080 |  3255 | `	break;` |
|        - |  3256 | `					  }` |
|        - |  3257 | `/*` |
|        - |  3258 | ` * LOAD_CLOSURE * * P3` |
|        - |  3259 | ` *` |
|        - |  3260 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3261 | ` * name in the stack.` |
|        - |  3262 | ` */` |
|        2 |  3263 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3264 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3265 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3266 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3267 | `		ph7_vm_func *pClosure;` |
|        - |  3268 | `		char *zName;` |
|        - |  3269 | `		sxu32 mLen;` |
|        - |  3270 | `		sxu32 n;` |
|        - |  3271 | `		/* Create a new VM function */` |
|        5 |  3272 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3273 | `		/* Generate an unique closure name */` |
|        5 |  3274 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3275 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3276 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3277 | `			goto Abort;` |
|        - |  3278 | `		}` |
|        5 |  3279 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3280 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3281 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3282 | `		}` |
|        - |  3283 | `		/* Zero the stucture */` |
|        5 |  3284 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3285 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3286 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3287 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3288 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3289 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3290 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3291 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3292 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3293 | `		/* Register the closure */` |
|        5 |  3294 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3295 | `		/* Set up closure environment */` |
|        5 |  3296 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3297 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3298 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3299 | `			ph7_value *pValue;` |
|        9 |  3300 | `			pEnv = &aEnv[n];` |
|        9 |  3301 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3302 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3303 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3304 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3305 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3306 | `				/* Pass by reference */` |
|      ! 0 |  3307 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3308 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3309 | `					);` |
|      ! 0 |  3310 | `			}` |
|        - |  3311 | `			/* Standard pass by value */` |
|        9 |  3312 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3313 | `			if( pValue ){` |
|        - |  3314 | `				/* Copy imported value */` |
|        5 |  3315 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3316 | `			}` |
|        - |  3317 | `			/* Insert the imported variable */` |
|        9 |  3318 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3319 | `		}` |
|        - |  3320 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3321 | `		pTos++;` |
|        5 |  3322 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3323 | `	}` |
|        5 |  3324 | `	break;` |
|        - |  3325 | `						 }` |
|        - |  3326 | `/*` |
|        - |  3327 | ` * STORE * P2 P3` |
|        - |  3328 | ` *` |
|        - |  3329 | ` * Perform a store (Assignment) operation.` |
|        - |  3330 | ` */` |
|    88095 |  3331 | `case PH7_OP_STORE: {` |
|        - |  3332 | `	ph7_value *pObj;` |
|        - |  3333 | `	SyString sName;` |
|        - |  3334 | `#ifdef UNTRUST` |
|        - |  3335 | `	if( pTos < pStack ){` |
|        - |  3336 | `		goto Abort;` |
|        - |  3337 | `	}` |
|        - |  3338 | `#endif` |
|   176192 |  3339 | `	if( pInstr->iP2 ){` |
|        - |  3340 | `		sxu32 nIdx;` |
|        - |  3341 | `		/* Member store operation */` |
|     1388 |  3342 | `		nIdx = pTos->nIdx;` |
|     1388 |  3343 | `		VmPopOperand(&pTos,1);` |
|     1388 |  3344 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3345 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3346 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3347 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3348 | `		}else{` |
|        - |  3349 | `			/* Point to the desired memory object */` |
|     1384 |  3350 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1384 |  3351 | `			if( pObj ){` |
|        - |  3352 | `				/* Perform the store operation */` |
|     1384 |  3353 | `				PH7_MemObjStore(pTos,pObj);` |
|      691 |  3354 | `			}` |
|        - |  3355 | `		}` |
|    88790 |  3356 | `		break;` |
|   174806 |  3357 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3358 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3359 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3360 | `			/* Force a string cast */` |
|      ! 0 |  3361 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3362 | `		}` |
|        7 |  3363 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3364 | `		pTos--;` |
|        - |  3365 | `#ifdef UNTRUST` |
|        - |  3366 | `		if( pTos < pStack  ){` |
|        - |  3367 | `			goto Abort;` |
|        - |  3368 | `		}` |
|        - |  3369 | `#endif` |
|        4 |  3370 | `	}else{` |
|   174800 |  3371 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3372 | `	}` |
|        - |  3373 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   174806 |  3374 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   174806 |  3375 | `	if( pObj == 0 ){` |
|      ! 0 |  3376 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3377 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3378 | `		goto Abort;` |
|        - |  3379 | `	}` |
|   174806 |  3380 | `	if( !pInstr->p3 ){` |
|        7 |  3381 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3382 | `	}` |
|        - |  3383 | `	/* Perform the store operation */` |
|   174806 |  3384 | `	PH7_MemObjStore(pTos,pObj);` |
|   174806 |  3385 | `	break;` |
|        - |  3386 | `				   }` |
|        - |  3387 | `/*` |
|        - |  3388 | ` * STORE_IDX:   P1 * P3` |
|        - |  3389 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3390 | ` *` |
|        - |  3391 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3392 | ` */` |
|    71469 |  3393 | `case PH7_OP_STORE_IDX:` |
|        - |  3394 | `case PH7_OP_STORE_IDX_REF: {` |
|   142940 |  3395 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3396 | `	ph7_value *pKey;` |
|        - |  3397 | `	sxu32 nIdx;` |
|   142940 |  3398 | `	if( pInstr->iP1 ){` |
|        - |  3399 | `		/* Key is next on stack */` |
|    52550 |  3400 | `		pKey = pTos;` |
|    52550 |  3401 | `		pTos--;` |
|    26276 |  3402 | `	}else{` |
|    90392 |  3403 | `		pKey = 0;` |
|        - |  3404 | `	}` |
|   142940 |  3405 | `	nIdx = pTos->nIdx;` |
|   142940 |  3406 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3407 | `		/* Hashmap already loaded */` |
|   142888 |  3408 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   142888 |  3409 | `		if( pMap->iRef < 2 ){` |
|        - |  3410 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3411 | `			pMap->iRef = 2;` |
|      ! 0 |  3412 | `		}` |
|    71445 |  3413 | `	}else{` |
|        - |  3414 | `		ph7_value *pObj;` |
|       53 |  3415 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3416 | `		if( pObj == 0 ){` |
|      ! 0 |  3417 | `			if( pKey ){` |
|      ! 0 |  3418 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3419 | `			}` |
|      ! 0 |  3420 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3421 | `			break;` |
|        - |  3422 | `		}` |
|        - |  3423 | `		/* Phase#1: Load the array */` |
|       53 |  3424 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3425 | `			VmPopOperand(&pTos,1);` |
|       53 |  3426 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3427 | `				/* Force a string cast */` |
|      ! 0 |  3428 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3429 | `			}` |
|       53 |  3430 | `			if( pKey == 0 ){` |
|        - |  3431 | `				/* Append string */` |
|        3 |  3432 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3433 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3434 | `				}` |
|        2 |  3435 | `			}else{` |
|        - |  3436 | `				sxu32 nOfft;` |
|       51 |  3437 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3438 | `					/* Force an int cast */` |
|       51 |  3439 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3440 | `				}` |
|       51 |  3441 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3442 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3443 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3444 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3445 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3446 | `				}else{` |
|      ! 0 |  3447 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3448 | `						/* Perform an append operation */` |
|      ! 0 |  3449 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3450 | `					}` |
|        - |  3451 | `				}` |
|        - |  3452 | `			}` |
|       53 |  3453 | `			if( pKey ){` |
|       51 |  3454 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3455 | `			}` |
|       53 |  3456 | `			break;` |
|      ! 0 |  3457 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3458 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3459 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3460 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3461 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3462 | `				goto Abort;` |
|        - |  3463 | `			}` |
|      ! 0 |  3464 | `		}` |
|      ! 0 |  3465 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3466 | `	}` |
|   142888 |  3467 | `	VmPopOperand(&pTos,1);` |
|        - |  3468 | `	/* Phase#2: Perform the insertion */` |
|   142888 |  3469 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3470 | `		/* Insertion by reference */` |
|       13 |  3471 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3472 | `	}else{` |
|   142876 |  3473 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3474 | `	}` |
|   142888 |  3475 | `	if( pKey ){` |
|    52500 |  3476 | `		PH7_MemObjRelease(pKey);` |
|    26249 |  3477 | `	}` |
|   142888 |  3478 | `	break;` |
|        - |  3479 | `					   }` |
|        - |  3480 | `/*` |
|        - |  3481 | ` * INCR: P1 * *` |
|        - |  3482 | ` *` |
|        - |  3483 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3484 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3485 | ` * the stack and increment after that.` |
|        - |  3486 | ` */` |
|   125236 |  3487 | `case PH7_OP_INCR:` |
|        - |  3488 | `#ifdef UNTRUST` |
|        - |  3489 | `	if( pTos < pStack ){` |
|        - |  3490 | `		goto Abort;` |
|        - |  3491 | `	}` |
|        - |  3492 | `#endif` |
|   250518 |  3493 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   250518 |  3494 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3495 | `			ph7_value *pObj;` |
|   250518 |  3496 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3497 | `				/* Force a numeric cast */` |
|   250518 |  3498 | `				PH7_MemObjToNumeric(pObj);` |
|   250518 |  3499 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3500 | `					pObj->rVal++;` |
|        - |  3501 | `					/* Try to get an integer representation */` |
|      ! 0 |  3502 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3503 | `				}else{` |
|   250518 |  3504 | `					pObj->x.iVal++;` |
|   250518 |  3505 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3506 | `				}` |
|   250518 |  3507 | `				if( pInstr->iP1 ){` |
|        - |  3508 | `					/* Pre-icrement */` |
|       55 |  3509 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3510 | `				}` |
|   125280 |  3511 | `			}` |
|   125282 |  3512 | `		}else{` |
|      ! 0 |  3513 | `			if( pInstr->iP1 ){` |
|        - |  3514 | `				/* Force a numeric cast */` |
|      ! 0 |  3515 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3516 | `				/* Pre-increment */` |
|      ! 0 |  3517 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3518 | `					pTos->rVal++;` |
|        - |  3519 | `					/* Try to get an integer representation */` |
|      ! 0 |  3520 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3521 | `				}else{` |
|      ! 0 |  3522 | `					pTos->x.iVal++;` |
|      ! 0 |  3523 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3524 | `				}` |
|      ! 0 |  3525 | `			}` |
|        - |  3526 | `		}` |
|   125280 |  3527 | `	}` |
|   250518 |  3528 | `	break;` |
|        - |  3529 | `/*` |
|        - |  3530 | ` * DECR: P1 * *` |
|        - |  3531 | ` *` |
|        - |  3532 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3533 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3534 | ` * and decrement after that.` |
|        - |  3535 | ` */` |
|        2 |  3536 | `case PH7_OP_DECR:` |
|        - |  3537 | `#ifdef UNTRUST` |
|        - |  3538 | `	if( pTos < pStack ){` |
|        - |  3539 | `		goto Abort;` |
|        - |  3540 | `	}` |
|        - |  3541 | `#endif` |
|        5 |  3542 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3543 | `		/* Force a numeric cast */` |
|        5 |  3544 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3545 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3546 | `			ph7_value *pObj;` |
|        5 |  3547 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3548 | `				/* Force a numeric cast */` |
|        5 |  3549 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3550 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3551 | `					pObj->rVal--;` |
|        - |  3552 | `					/* Try to get an integer representation */` |
|      ! 0 |  3553 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3554 | `				}else{` |
|        5 |  3555 | `					pObj->x.iVal--;` |
|        5 |  3556 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3557 | `				}` |
|        5 |  3558 | `				if( pInstr->iP1 ){` |
|        - |  3559 | `					/* Pre-icrement */` |
|      ! 0 |  3560 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3561 | `				}` |
|        2 |  3562 | `			}` |
|        3 |  3563 | `		}else{` |
|      ! 0 |  3564 | `			if( pInstr->iP1 ){` |
|        - |  3565 | `				/* Pre-increment */` |
|      ! 0 |  3566 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3567 | `					pTos->rVal--;` |
|        - |  3568 | `					/* Try to get an integer representation */` |
|      ! 0 |  3569 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3570 | `				}else{` |
|      ! 0 |  3571 | `					pTos->x.iVal--;` |
|      ! 0 |  3572 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3573 | `				}` |
|      ! 0 |  3574 | `			}` |
|        - |  3575 | `		}` |
|        2 |  3576 | `	}` |
|        5 |  3577 | `	break;` |
|        - |  3578 | `/*` |
|        - |  3579 | ` * UMINUS: * * *` |
|        - |  3580 | ` *` |
|        - |  3581 | ` * Perform a unary minus operation.` |
|        - |  3582 | ` */` |
|    18877 |  3583 | `case PH7_OP_UMINUS:` |
|        - |  3584 | `#ifdef UNTRUST` |
|        - |  3585 | `	if( pTos < pStack ){` |
|        - |  3586 | `		goto Abort;` |
|        - |  3587 | `	}` |
|        - |  3588 | `#endif` |
|        - |  3589 | `	/* Force a numeric (integer,real or both) cast */` |
|    37756 |  3590 | `	PH7_MemObjToNumeric(pTos);` |
|    37756 |  3591 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3592 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3593 | `	}` |
|    37756 |  3594 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    37732 |  3595 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    18865 |  3596 | `	}` |
|    37756 |  3597 | `	break;` |
|        - |  3598 | `/*` |
|        - |  3599 | ` * UPLUS: * * *` |
|        - |  3600 | ` *` |
|        - |  3601 | ` * Perform a unary plus operation.` |
|        - |  3602 | ` */` |
|       16 |  3603 | `case PH7_OP_UPLUS:` |
|        - |  3604 | `#ifdef UNTRUST` |
|        - |  3605 | `	if( pTos < pStack ){` |
|        - |  3606 | `		goto Abort;` |
|        - |  3607 | `	}` |
|        - |  3608 | `#endif` |
|        - |  3609 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3610 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3611 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3612 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3613 | `	}` |
|       33 |  3614 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3615 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3616 | `	}` |
|       33 |  3617 | `	break;` |
|        - |  3618 | `/*` |
|        - |  3619 | ` * OP_LNOT: * * *` |
|        - |  3620 | ` *` |
|        - |  3621 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3622 | ` * with its complement.` |
|        - |  3623 | ` */` |
|    37013 |  3624 | `case PH7_OP_LNOT:` |
|        - |  3625 | `#ifdef UNTRUST` |
|        - |  3626 | `	if( pTos < pStack ){` |
|        - |  3627 | `		goto Abort;` |
|        - |  3628 | `	}` |
|        - |  3629 | `#endif` |
|        - |  3630 | `	/* Force a boolean cast */` |
|    74072 |  3631 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3632 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3633 | `	}` |
|    74072 |  3634 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    74072 |  3635 | `	break;` |
|        - |  3636 | `/*` |
|        - |  3637 | ` * OP_BITNOT: * * *` |
|        - |  3638 | ` *` |
|        - |  3639 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3640 | ` * with its ones-complement.` |
|        - |  3641 | ` */` |
|        3 |  3642 | `case PH7_OP_BITNOT:` |
|        - |  3643 | `#ifdef UNTRUST` |
|        - |  3644 | `	if( pTos < pStack ){` |
|        - |  3645 | `		goto Abort;` |
|        - |  3646 | `	}` |
|        - |  3647 | `#endif` |
|        - |  3648 | `	/* Force an integer cast */` |
|        7 |  3649 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3650 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3651 | `	}` |
|        7 |  3652 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3653 | `	break;` |
|        - |  3654 | `/* OP_MUL * * *` |
|        - |  3655 | ` * OP_MUL_STORE * * *` |
|        - |  3656 | ` *` |
|        - |  3657 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3658 | ` * and push the result back onto the stack.` |
|        - |  3659 | ` */` |
|     1231 |  3660 | `case PH7_OP_MUL:` |
|        - |  3661 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3662 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3663 | `	/* Force the operand to be numeric */` |
|        - |  3664 | `#ifdef UNTRUST` |
|        - |  3665 | `	if( pNos < pStack ){` |
|        - |  3666 | `		goto Abort;` |
|        - |  3667 | `	}` |
|        - |  3668 | `#endif` |
|     2464 |  3669 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3670 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3671 | `	/* Perform the requested operation */` |
|     2464 |  3672 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3673 | `		/* Floating point arithemic */` |
|        - |  3674 | `		ph7_real a,b,r;` |
|       17 |  3675 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3676 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3677 | `		}` |
|       17 |  3678 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3679 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3680 | `		}` |
|       17 |  3681 | `		a = pNos->rVal;` |
|       17 |  3682 | `		b = pTos->rVal;` |
|       17 |  3683 | `		r = a * b;` |
|        - |  3684 | `		/* Push the result */` |
|       17 |  3685 | `		pNos->rVal = r;` |
|       17 |  3686 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3687 | `		/* Try to get an integer representation */` |
|       17 |  3688 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3689 | `	}else{` |
|        - |  3690 | `		/* Integer arithmetic */` |
|        - |  3691 | `		sxi64 a,b,r;` |
|     2448 |  3692 | `		a = pNos->x.iVal;` |
|     2448 |  3693 | `		b = pTos->x.iVal;` |
|     2448 |  3694 | `		r = a * b;` |
|        - |  3695 | `		/* Push the result */` |
|     2448 |  3696 | `		pNos->x.iVal = r;` |
|     2448 |  3697 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3698 | `	}` |
|     2464 |  3699 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3700 | `		ph7_value *pObj;` |
|       19 |  3701 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3702 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3703 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3704 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3705 | `		}` |
|        9 |  3706 | `	}` |
|     2464 |  3707 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3708 | `	break;` |
|        - |  3709 | `				 }` |
|        - |  3710 | `/* OP_ADD * * *` |
|        - |  3711 | ` *` |
|        - |  3712 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3713 | ` * and push the result back onto the stack.` |
|        - |  3714 | ` */` |
|      420 |  3715 | `case PH7_OP_ADD:{` |
|      842 |  3716 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3717 | `#ifdef UNTRUST` |
|        - |  3718 | `	if( pNos < pStack ){` |
|        - |  3719 | `		goto Abort;` |
|        - |  3720 | `	}` |
|        - |  3721 | `#endif` |
|        - |  3722 | `	/* Perform the addition */` |
|      842 |  3723 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      842 |  3724 | `	VmPopOperand(&pTos,1);` |
|      842 |  3725 | `	break;` |
|        - |  3726 | `				}` |
|        - |  3727 | `/*` |
|        - |  3728 | ` * OP_ADD_STORE * * *` |
|        - |  3729 | ` *` |
|        - |  3730 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3731 | ` * and push the result back onto the stack.` |
|        - |  3732 | ` */` |
|      481 |  3733 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3734 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3735 | `	ph7_value *pObj;` |
|        - |  3736 | `	sxu32 nIdx;` |
|        - |  3737 | `#ifdef UNTRUST` |
|        - |  3738 | `	if( pNos < pStack ){` |
|        - |  3739 | `		goto Abort;` |
|        - |  3740 | `	}` |
|        - |  3741 | `#endif` |
|        - |  3742 | `	/* Perform the addition */` |
|      963 |  3743 | `	nIdx = pTos->nIdx;` |
|      963 |  3744 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3745 | `	/* Peform the store operation */` |
|      963 |  3746 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3747 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3748 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3749 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3750 | `	}` |
|        - |  3751 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3752 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3753 | `	VmPopOperand(&pTos,1);` |
|      963 |  3754 | `	break;` |
|        - |  3755 | `				}` |
|        - |  3756 | `/* OP_SUB * * *` |
|        - |  3757 | ` *` |
|        - |  3758 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3759 | ` * first (what was next on the stack) from the second (the` |
|        - |  3760 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3761 | ` */` |
|      294 |  3762 | `case PH7_OP_SUB: {` |
|      589 |  3763 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3764 | `#ifdef UNTRUST` |
|        - |  3765 | `	if( pNos < pStack ){` |
|        - |  3766 | `		goto Abort;` |
|        - |  3767 | `	}` |
|        - |  3768 | `#endif` |
|      589 |  3769 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3770 | `		/* Floating point arithemic */` |
|        - |  3771 | `		ph7_real a,b,r;` |
|       95 |  3772 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3773 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3774 | `		}` |
|       95 |  3775 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3776 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3777 | `		}` |
|       95 |  3778 | `		a = pNos->rVal;` |
|       95 |  3779 | `		b = pTos->rVal;` |
|       95 |  3780 | `		r = a - b;` |
|        - |  3781 | `		/* Push the result */` |
|       95 |  3782 | `		pNos->rVal = r;` |
|       95 |  3783 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3784 | `		/* Try to get an integer representation */` |
|       95 |  3785 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3786 | `	}else{` |
|        - |  3787 | `		/* Integer arithmetic */` |
|        - |  3788 | `		sxi64 a,b,r;` |
|      495 |  3789 | `		a = pNos->x.iVal;` |
|      495 |  3790 | `		b = pTos->x.iVal;` |
|      495 |  3791 | `		r = a - b;` |
|        - |  3792 | `		/* Push the result */` |
|      495 |  3793 | `		pNos->x.iVal = r;` |
|      495 |  3794 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3795 | `	}` |
|      589 |  3796 | `	VmPopOperand(&pTos,1);` |
|      589 |  3797 | `	break;` |
|        - |  3798 | `				 }` |
|        - |  3799 | `/* OP_SUB_STORE * * *` |
|        - |  3800 | ` *` |
|        - |  3801 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3802 | ` * first (what was next on the stack) from the second (the` |
|        - |  3803 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3804 | ` */` |
|        1 |  3805 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3806 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3807 | `	ph7_value *pObj;` |
|        - |  3808 | `#ifdef UNTRUST` |
|        - |  3809 | `	if( pNos < pStack ){` |
|        - |  3810 | `		goto Abort;` |
|        - |  3811 | `	}` |
|        - |  3812 | `#endif` |
|        3 |  3813 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3814 | `		/* Floating point arithemic */` |
|        - |  3815 | `		ph7_real a,b,r;` |
|      ! 0 |  3816 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3817 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3818 | `		}` |
|      ! 0 |  3819 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3820 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3821 | `		}` |
|      ! 0 |  3822 | `		a = pTos->rVal;` |
|      ! 0 |  3823 | `		b = pNos->rVal;` |
|      ! 0 |  3824 | `		r = a - b;` |
|        - |  3825 | `		/* Push the result */` |
|      ! 0 |  3826 | `		pNos->rVal = r;` |
|      ! 0 |  3827 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3828 | `		/* Try to get an integer representation */` |
|      ! 0 |  3829 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3830 | `	}else{` |
|        - |  3831 | `		/* Integer arithmetic */` |
|        - |  3832 | `		sxi64 a,b,r;` |
|        3 |  3833 | `		a = pTos->x.iVal;` |
|        3 |  3834 | `		b = pNos->x.iVal;` |
|        3 |  3835 | `		r = a - b;` |
|        - |  3836 | `		/* Push the result */` |
|        3 |  3837 | `		pNos->x.iVal = r;` |
|        3 |  3838 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3839 | `	}` |
|        3 |  3840 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3841 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3842 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3843 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3844 | `	}` |
|        3 |  3845 | `	VmPopOperand(&pTos,1);` |
|        3 |  3846 | `	break;` |
|        - |  3847 | `				 }` |
|        - |  3848 |  |
|        - |  3849 | `/*` |
|        - |  3850 | ` * OP_MOD * * *` |
|        - |  3851 | ` *` |
|        - |  3852 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3853 | ` * first (what was next on the stack) from the second (the` |
|        - |  3854 | ` * top of the stack) and push the remainder after division` |
|        - |  3855 | ` * onto the stack.` |
|        - |  3856 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3857 | ` */` |
|      296 |  3858 | `case PH7_OP_MOD:{` |
|      594 |  3859 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3860 | `	sxi64 a,b,r;` |
|        - |  3861 | `#ifdef UNTRUST` |
|        - |  3862 | `	if( pNos < pStack ){` |
|        - |  3863 | `		goto Abort;` |
|        - |  3864 | `	}` |
|        - |  3865 | `#endif` |
|        - |  3866 | `	/* Force the operands to be integer */` |
|      594 |  3867 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3868 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3869 | `	}` |
|      594 |  3870 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3871 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3872 | `	}` |
|        - |  3873 | `	/* Perform the requested operation */` |
|      594 |  3874 | `	a = pNos->x.iVal;` |
|      594 |  3875 | `	b = pTos->x.iVal;` |
|      594 |  3876 | `	if( b == 0 ){` |
|        3 |  3877 | `		r = 0;` |
|        3 |  3878 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3879 | `		/* goto Abort; */` |
|        2 |  3880 | `	}else{` |
|      591 |  3881 | `		r = a%b;` |
|        - |  3882 | `	}` |
|        - |  3883 | `	/* Push the result */` |
|      594 |  3884 | `	pNos->x.iVal = r;` |
|      594 |  3885 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3886 | `	VmPopOperand(&pTos,1);` |
|      594 |  3887 | `	break;` |
|        - |  3888 | `				}` |
|        - |  3889 | `/*` |
|        - |  3890 | ` * OP_MOD_STORE * * *` |
|        - |  3891 | ` *` |
|        - |  3892 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3893 | ` * first (what was next on the stack) from the second (the` |
|        - |  3894 | ` * top of the stack) and push the remainder after division` |
|        - |  3895 | ` * onto the stack.` |
|        - |  3896 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3897 | ` */` |
|        1 |  3898 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3899 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3900 | `	ph7_value *pObj;` |
|        - |  3901 | `	sxi64 a,b,r;` |
|        - |  3902 | `#ifdef UNTRUST` |
|        - |  3903 | `	if( pNos < pStack ){` |
|        - |  3904 | `		goto Abort;` |
|        - |  3905 | `	}` |
|        - |  3906 | `#endif` |
|        - |  3907 | `	/* Force the operands to be integer */` |
|        3 |  3908 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3909 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3910 | `	}` |
|        3 |  3911 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3912 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3913 | `	}` |
|        - |  3914 | `	/* Perform the requested operation */` |
|        3 |  3915 | `	a = pTos->x.iVal;` |
|        3 |  3916 | `	b = pNos->x.iVal;` |
|        3 |  3917 | `	if( b == 0 ){` |
|      ! 0 |  3918 | `		r = 0;` |
|      ! 0 |  3919 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3920 | `		/* goto Abort; */` |
|      ! 0 |  3921 | `	}else{` |
|        3 |  3922 | `		r = a%b;` |
|        - |  3923 | `	}` |
|        - |  3924 | `	/* Push the result */` |
|        3 |  3925 | `	pNos->x.iVal = r;` |
|        3 |  3926 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3927 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3928 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3929 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3930 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3931 | `	}` |
|        3 |  3932 | `	VmPopOperand(&pTos,1);` |
|        3 |  3933 | `	break;` |
|        - |  3934 | `				}` |
|        - |  3935 | `/*` |
|        - |  3936 | ` * OP_DIV * * *` |
|        - |  3937 | ` *` |
|        - |  3938 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3939 | ` * first (what was next on the stack) from the second (the` |
|        - |  3940 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3941 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3942 | ` */` |
|       28 |  3943 | `case PH7_OP_DIV:{` |
|       58 |  3944 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3945 | `	ph7_real a,b,r;` |
|        - |  3946 | `#ifdef UNTRUST` |
|        - |  3947 | `	if( pNos < pStack ){` |
|        - |  3948 | `		goto Abort;` |
|        - |  3949 | `	}` |
|        - |  3950 | `#endif` |
|        - |  3951 | `	/* Force the operands to be real */` |
|       58 |  3952 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3953 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3954 | `	}` |
|       58 |  3955 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3956 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3957 | `	}` |
|        - |  3958 | `	/* Perform the requested operation */` |
|       58 |  3959 | `	a = pNos->rVal;` |
|       58 |  3960 | `	b = pTos->rVal;` |
|       58 |  3961 | `	if( b == 0 ){` |
|        - |  3962 | `		/* Division by zero */` |
|        3 |  3963 | `		pNos->rVal = 0;` |
|        3 |  3964 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3965 | `		/* goto Abort; */` |
|        2 |  3966 | `	}else{` |
|       55 |  3967 | `		r = a/b;` |
|        - |  3968 | `		/* Push the result */` |
|       55 |  3969 | `		pNos->rVal = r;` |
|       55 |  3970 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3971 | `		/* Try to get an integer representation */` |
|       55 |  3972 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3973 | `	}` |
|       58 |  3974 | `	VmPopOperand(&pTos,1);` |
|       58 |  3975 | `	break;` |
|        - |  3976 | `				}` |
|        - |  3977 | `/*` |
|        - |  3978 | ` * OP_DIV_STORE * * *` |
|        - |  3979 | ` *` |
|        - |  3980 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3981 | ` * first (what was next on the stack) from the second (the` |
|        - |  3982 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3983 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3984 | ` */` |
|        1 |  3985 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3986 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3987 | `	ph7_value *pObj;` |
|        - |  3988 | `	ph7_real a,b,r;` |
|        - |  3989 | `#ifdef UNTRUST` |
|        - |  3990 | `	if( pNos < pStack ){` |
|        - |  3991 | `		goto Abort;` |
|        - |  3992 | `	}` |
|        - |  3993 | `#endif` |
|        - |  3994 | `	/* Force the operands to be real */` |
|        3 |  3995 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3996 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3997 | `	}` |
|        3 |  3998 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3999 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4000 | `	}` |
|        - |  4001 | `	/* Perform the requested operation */` |
|        3 |  4002 | `	a = pTos->rVal;` |
|        3 |  4003 | `	b = pNos->rVal;` |
|        3 |  4004 | `	if( b == 0 ){` |
|        - |  4005 | `		/* Division by zero */` |
|      ! 0 |  4006 | `		r = 0;` |
|      ! 0 |  4007 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4008 | `		/* goto Abort; */` |
|      ! 0 |  4009 | `	}else{` |
|        3 |  4010 | `		r = a/b;` |
|        - |  4011 | `		/* Push the result */` |
|        3 |  4012 | `		pNos->rVal = r;` |
|        3 |  4013 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4014 | `		/* Try to get an integer representation */` |
|        3 |  4015 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4016 | `	}` |
|        3 |  4017 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4018 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4019 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4020 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4021 | `	}` |
|        3 |  4022 | `	VmPopOperand(&pTos,1);` |
|        3 |  4023 | `	break;` |
|        - |  4024 | `				}` |
|        - |  4025 | `/* OP_BAND * * *` |
|        - |  4026 | ` *` |
|        - |  4027 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4028 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4029 | ` * two elements.` |
|        - |  4030 | `*/` |
|        - |  4031 | `/* OP_BOR * * *` |
|        - |  4032 | ` *` |
|        - |  4033 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4034 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4035 | ` * two elements.` |
|        - |  4036 | ` */` |
|        - |  4037 | `/* OP_BXOR * * *` |
|        - |  4038 | ` *` |
|        - |  4039 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4040 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4041 | ` * two elements.` |
|        - |  4042 | ` */` |
|       19 |  4043 | `case PH7_OP_BAND:` |
|        - |  4044 | `case PH7_OP_BOR:` |
|        - |  4045 | `case PH7_OP_BXOR:{` |
|       39 |  4046 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4047 | `	sxi64 a,b,r;` |
|        - |  4048 | `#ifdef UNTRUST` |
|        - |  4049 | `	if( pNos < pStack ){` |
|        - |  4050 | `		goto Abort;` |
|        - |  4051 | `	}` |
|        - |  4052 | `#endif` |
|        - |  4053 | `	/* Force the operands to be integer */` |
|       39 |  4054 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4055 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4056 | `	}` |
|       39 |  4057 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4058 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4059 | `	}` |
|        - |  4060 | `	/* Perform the requested operation */` |
|       39 |  4061 | `	a = pNos->x.iVal;` |
|       39 |  4062 | `	b = pTos->x.iVal;` |
|       39 |  4063 | `	switch(pInstr->iOp){` |
|        6 |  4064 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4065 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4066 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4067 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4068 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4069 | `	case PH7_OP_BAND:` |
|       15 |  4070 | `	default:          r = a&b; break;` |
|        - |  4071 | `	}` |
|        - |  4072 | `	/* Push the result */` |
|       39 |  4073 | `	pNos->x.iVal = r;` |
|       39 |  4074 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4075 | `	VmPopOperand(&pTos,1);` |
|       39 |  4076 | `	break;` |
|        - |  4077 | `				 }` |
|        - |  4078 | `/* OP_BAND_STORE * * *` |
|        - |  4079 | ` *` |
|        - |  4080 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4081 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4082 | ` * two elements.` |
|        - |  4083 | `*/` |
|        - |  4084 | `/* OP_BOR_STORE * * *` |
|        - |  4085 | ` *` |
|        - |  4086 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4087 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4088 | ` * two elements.` |
|        - |  4089 | ` */` |
|        - |  4090 | `/* OP_BXOR_STORE * * *` |
|        - |  4091 | ` *` |
|        - |  4092 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4093 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4094 | ` * two elements.` |
|        - |  4095 | ` */` |
|        7 |  4096 | `case PH7_OP_BAND_STORE:` |
|        - |  4097 | `case PH7_OP_BOR_STORE:` |
|        - |  4098 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4099 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4100 | `	ph7_value *pObj;` |
|        - |  4101 | `	sxi64 a,b,r;` |
|        - |  4102 | `#ifdef UNTRUST` |
|        - |  4103 | `	if( pNos < pStack ){` |
|        - |  4104 | `		goto Abort;` |
|        - |  4105 | `	}` |
|        - |  4106 | `#endif` |
|        - |  4107 | `	/* Force the operands to be integer */` |
|       15 |  4108 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4109 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4110 | `	}` |
|       15 |  4111 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4112 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4113 | `	}` |
|        - |  4114 | `	/* Perform the requested operation */` |
|       15 |  4115 | `	a = pTos->x.iVal;` |
|       15 |  4116 | `	b = pNos->x.iVal;` |
|       15 |  4117 | `	switch(pInstr->iOp){` |
|        2 |  4118 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4119 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4120 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4121 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4122 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4123 | `	case PH7_OP_BAND:` |
|        5 |  4124 | `	default:          r = a&b; break;` |
|        - |  4125 | `	}` |
|        - |  4126 | `	/* Push the result */` |
|       15 |  4127 | `	pNos->x.iVal = r;` |
|       15 |  4128 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4129 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4130 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4131 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4132 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4133 | `	}` |
|       15 |  4134 | `	VmPopOperand(&pTos,1);` |
|       15 |  4135 | `	break;` |
|        - |  4136 | `				 }` |
|        - |  4137 | `/* OP_SHL * * *` |
|        - |  4138 | ` *` |
|        - |  4139 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4140 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4141 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4142 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4143 | ` */` |
|        - |  4144 | `/* OP_SHR * * *` |
|        - |  4145 | ` *` |
|        - |  4146 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4147 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4148 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4149 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4150 | ` */` |
|        9 |  4151 | `case PH7_OP_SHL:` |
|        - |  4152 | `case PH7_OP_SHR: {` |
|       19 |  4153 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4154 | `	sxi64 a,r;` |
|        - |  4155 | `	sxi32 b;` |
|        - |  4156 | `#ifdef UNTRUST` |
|        - |  4157 | `	if( pNos < pStack ){` |
|        - |  4158 | `		goto Abort;` |
|        - |  4159 | `	}` |
|        - |  4160 | `#endif` |
|        - |  4161 | `	/* Force the operands to be integer */` |
|       19 |  4162 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4163 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4164 | `	}` |
|       19 |  4165 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4166 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4167 | `	}` |
|        - |  4168 | `	/* Perform the requested operation */` |
|       19 |  4169 | `	a = pNos->x.iVal;` |
|       19 |  4170 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4171 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4172 | `		r = a << b;` |
|        6 |  4173 | `	}else{` |
|        9 |  4174 | `		r = a >> b;` |
|        - |  4175 | `	}` |
|        - |  4176 | `	/* Push the result */` |
|       19 |  4177 | `	pNos->x.iVal = r;` |
|       19 |  4178 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4179 | `	VmPopOperand(&pTos,1);` |
|       19 |  4180 | `	break;` |
|        - |  4181 | `				 }` |
|        - |  4182 | `/*  OP_SHL_STORE * * *` |
|        - |  4183 | ` *` |
|        - |  4184 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4185 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4186 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4187 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4188 | ` */` |
|        - |  4189 | `/* OP_SHR_STORE * * *` |
|        - |  4190 | ` *` |
|        - |  4191 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4192 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4193 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4194 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4195 | ` */` |
|        7 |  4196 | `case PH7_OP_SHL_STORE:` |
|        - |  4197 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4198 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4199 | `	ph7_value *pObj;` |
|        - |  4200 | `	sxi64 a,r;` |
|        - |  4201 | `	sxi32 b;` |
|        - |  4202 | `#ifdef UNTRUST` |
|        - |  4203 | `	if( pNos < pStack ){` |
|        - |  4204 | `		goto Abort;` |
|        - |  4205 | `	}` |
|        - |  4206 | `#endif` |
|        - |  4207 | `	/* Force the operands to be integer */` |
|       15 |  4208 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4209 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4210 | `	}` |
|       15 |  4211 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4212 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4213 | `	}` |
|        - |  4214 | `	/* Perform the requested operation */` |
|       15 |  4215 | `	a = pTos->x.iVal;` |
|       15 |  4216 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4217 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4218 | `		r = a << b;` |
|        4 |  4219 | `	}else{` |
|        9 |  4220 | `		r = a >> b;` |
|        - |  4221 | `	}` |
|        - |  4222 | `	/* Push the result */` |
|       15 |  4223 | `	pNos->x.iVal = r;` |
|       15 |  4224 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4225 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4226 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4227 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4228 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4229 | `	}` |
|       15 |  4230 | `	VmPopOperand(&pTos,1);` |
|       15 |  4231 | `	break;` |
|        - |  4232 | `				 }` |
|        - |  4233 | `/* CAT:  P1 * *` |
|        - |  4234 | ` *` |
|        - |  4235 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4236 | ` * back.` |
|        - |  4237 | ` */` |
|    53411 |  4238 | `case PH7_OP_CAT:{` |
|        - |  4239 | `	ph7_value *pNos,*pCur;` |
|   106824 |  4240 | `	if( pInstr->iP1 < 1 ){` |
|    80172 |  4241 | `		pNos = &pTos[-1];` |
|    40087 |  4242 | `	}else{` |
|    26654 |  4243 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4244 | `	}` |
|        - |  4245 | `#ifdef UNTRUST` |
|        - |  4246 | `	if( pNos < pStack ){` |
|        - |  4247 | `		goto Abort;` |
|        - |  4248 | `	}` |
|        - |  4249 | `#endif` |
|        - |  4250 | `	/* Force a string cast */` |
|   106824 |  4251 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      602 |  4252 | `		PH7_MemObjToString(pNos);` |
|      300 |  4253 | `	}` |
|   106824 |  4254 | `	pCur = &pNos[1];` |
|   214942 |  4255 | `	while( pCur <= pTos ){` |
|   108120 |  4256 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50210 |  4257 | `			PH7_MemObjToString(pCur);` |
|    25104 |  4258 | `		}` |
|        - |  4259 | `		/* Perform the concatenation */` |
|   108120 |  4260 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   108082 |  4261 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    54040 |  4262 | `		}` |
|   108120 |  4263 | `		SyBlobRelease(&pCur->sBlob);` |
|   108120 |  4264 | `		pCur++;` |
|        2 |  4265 | `	}` |
|   106824 |  4266 | `	pTos = pNos;` |
|   106824 |  4267 | `	break;` |
|        - |  4268 | `				}` |
|        - |  4269 | `/*  CAT_STORE: * * *` |
|        - |  4270 | ` *` |
|        - |  4271 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4272 | ` * back.` |
|        - |  4273 | ` */` |
|     1808 |  4274 | `case PH7_OP_CAT_STORE:{` |
|     3617 |  4275 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4276 | `	ph7_value *pObj;` |
|        - |  4277 | `#ifdef UNTRUST` |
|        - |  4278 | `	if( pNos < pStack ){` |
|        - |  4279 | `		goto Abort;` |
|        - |  4280 | `	}` |
|        - |  4281 | `#endif` |
|     3617 |  4282 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4283 | `		/* Force a string cast */` |
|      ! 0 |  4284 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4285 | `	}` |
|     3617 |  4286 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4287 | `		/* Force a string cast */` |
|      ! 0 |  4288 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4289 | `	}` |
|        - |  4290 | `	/* Perform the concatenation (Reverse order) */` |
|     3617 |  4291 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     3617 |  4292 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     1808 |  4293 | `	}` |
|        - |  4294 | `	/* Perform the store operation */` |
|     3617 |  4295 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4296 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     3617 |  4297 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     3617 |  4298 | `		PH7_MemObjStore(pTos,pObj);` |
|     1808 |  4299 | `	}` |
|     3617 |  4300 | `	PH7_MemObjStore(pTos,pNos);` |
|     3617 |  4301 | `	VmPopOperand(&pTos,1);` |
|     3617 |  4302 | `	break;` |
|        - |  4303 | `				}` |
|        - |  4304 | `/* OP_AND: * * *` |
|        - |  4305 | ` *` |
|        - |  4306 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4307 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4308 | ` * stack.` |
|        - |  4309 | ` */` |
|        - |  4310 | `/* OP_OR: * * *` |
|        - |  4311 | ` *` |
|        - |  4312 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4313 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4314 | ` * stack.` |
|        - |  4315 | ` */` |
|    77945 |  4316 | `case PH7_OP_LAND:` |
|        - |  4317 | `case PH7_OP_LOR: {` |
|   155936 |  4318 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4319 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4320 | `#ifdef UNTRUST` |
|        - |  4321 | `	if( pNos < pStack ){` |
|        - |  4322 | `		goto Abort;` |
|        - |  4323 | `	}` |
|        - |  4324 | `#endif` |
|        - |  4325 | `	/* Force a boolean cast */` |
|   155936 |  4326 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4327 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4328 | `	}` |
|   155936 |  4329 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4330 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4331 | `	}` |
|   155936 |  4332 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   155936 |  4333 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   155936 |  4334 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4335 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    80404 |  4336 | `		v1 = and_logic[v1*3+v2];` |
|    40225 |  4337 | `	}else{` |
|        - |  4338 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    75534 |  4339 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4340 | `	}` |
|   155936 |  4341 | `	if( v1 == 2 ){` |
|      ! 0 |  4342 | `		v1 = 1;` |
|      ! 0 |  4343 | `	}` |
|   155936 |  4344 | `	VmPopOperand(&pTos,1);` |
|   155936 |  4345 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   155936 |  4346 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   155936 |  4347 | `	break;` |
|        - |  4348 | `				 }` |
|        - |  4349 | `/* OP_LXOR: * * *` |
|        - |  4350 | ` *` |
|        - |  4351 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4352 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4353 | ` * stack.` |
|        - |  4354 | ` * According to the PHP language reference manual:` |
|        - |  4355 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4356 | ` *  TRUE,but not both.` |
|        - |  4357 | ` */` |
|        5 |  4358 | `case PH7_OP_LXOR:{` |
|       11 |  4359 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4360 | `	sxi32 v = 0;` |
|        - |  4361 | `#ifdef UNTRUST` |
|        - |  4362 | `	if( pNos < pStack ){` |
|        - |  4363 | `		goto Abort;` |
|        - |  4364 | `	}` |
|        - |  4365 | `#endif` |
|        - |  4366 | `	/* Force a boolean cast */` |
|       11 |  4367 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4368 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4369 | `	}` |
|       11 |  4370 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4371 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4372 | `	}` |
|       11 |  4373 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4374 | `		v = 1;` |
|        3 |  4375 | `	}` |
|       11 |  4376 | `	VmPopOperand(&pTos,1);` |
|       11 |  4377 | `	pTos->x.iVal = v;` |
|       11 |  4378 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4379 | `	break;` |
|        - |  4380 | `				 }` |
|        - |  4381 | `/* OP_EQ P1 P2 P3` |
|        - |  4382 | ` *` |
|        - |  4383 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4384 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4385 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4386 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4387 | ` */` |
|        - |  4388 | `/* OP_NEQ P1 P2 P3` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4391 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4392 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4393 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4394 | ` */` |
|     3292 |  4395 | `case PH7_OP_EQ:` |
|        - |  4396 | `case PH7_OP_NEQ: {` |
|     6586 |  4397 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4398 | `	/* Perform the comparison and act accordingly */` |
|        - |  4399 | `#ifdef UNTRUST` |
|        - |  4400 | `	if( pNos < pStack ){` |
|        - |  4401 | `		goto Abort;` |
|        - |  4402 | `	}` |
|        - |  4403 | `#endif` |
|     6586 |  4404 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6586 |  4405 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4406 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     6581 |  4407 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     6550 |  4408 | `		rc = rc == 0;` |
|     3276 |  4409 | `	}else{` |
|       28 |  4410 | `		rc = rc != 0;` |
|        - |  4411 | `	}` |
|     6586 |  4412 | `	VmPopOperand(&pTos,1);` |
|     6586 |  4413 | `	if( !pInstr->iP2 ){` |
|        - |  4414 | `		/* Push comparison result without taking the jump */` |
|     6586 |  4415 | `		PH7_MemObjRelease(pTos);` |
|     6586 |  4416 | `		pTos->x.iVal = rc;` |
|        - |  4417 | `		/* Invalidate any prior representation */` |
|     6586 |  4418 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3294 |  4419 | `	}else{` |
|      ! 0 |  4420 | `		if( rc ){` |
|        - |  4421 | `			/* Jump to the desired location */` |
|      ! 0 |  4422 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4423 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4424 | `		}` |
|        - |  4425 | `	}` |
|     6586 |  4426 | `	break;` |
|        - |  4427 | `				 }` |
|        - |  4428 | `/* OP_TEQ P1 P2 *` |
|        - |  4429 | ` *` |
|        - |  4430 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4431 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4432 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4433 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4434 | ` */` |
|   100676 |  4435 | `case PH7_OP_TEQ: {` |
|   201354 |  4436 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4437 | `	/* Perform the comparison and act accordingly */` |
|        - |  4438 | `#ifdef UNTRUST` |
|        - |  4439 | `	if( pNos < pStack ){` |
|        - |  4440 | `		goto Abort;` |
|        - |  4441 | `	}` |
|        - |  4442 | `#endif` |
|   201354 |  4443 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   201354 |  4444 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4445 | `		rc = 0;` |
|        2 |  4446 | `	}else{` |
|   201352 |  4447 | `		rc = rc == 0;` |
|        - |  4448 | `	}` |
|   201354 |  4449 | `	VmPopOperand(&pTos,1);` |
|   201354 |  4450 | `	if( !pInstr->iP2 ){` |
|        - |  4451 | `		/* Push comparison result without taking the jump */` |
|   201354 |  4452 | `		PH7_MemObjRelease(pTos);` |
|   201354 |  4453 | `		pTos->x.iVal = rc;` |
|        - |  4454 | `		/* Invalidate any prior representation */` |
|   201354 |  4455 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100678 |  4456 | `	}else{` |
|      ! 0 |  4457 | `		if( rc ){` |
|        - |  4458 | `			/* Jump to the desired location */` |
|      ! 0 |  4459 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4460 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4461 | `		}` |
|        - |  4462 | `	}` |
|   201354 |  4463 | `	break;` |
|        - |  4464 | `				 }` |
|        - |  4465 | `/* OP_TNE P1 P2 *` |
|        - |  4466 | ` *` |
|        - |  4467 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4468 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4469 | ` * instruction.` |
|        - |  4470 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4471 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4472 | ` *` |
|        - |  4473 | ` */` |
|    79769 |  4474 | `case PH7_OP_TNE: {` |
|   159540 |  4475 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4476 | `	/* Perform the comparison and act accordingly */` |
|        - |  4477 | `#ifdef UNTRUST` |
|        - |  4478 | `	if( pNos < pStack ){` |
|        - |  4479 | `		goto Abort;` |
|        - |  4480 | `	}` |
|        - |  4481 | `#endif` |
|   159540 |  4482 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   159540 |  4483 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4484 | `		rc = 1;` |
|        2 |  4485 | `	}else{` |
|   159538 |  4486 | `		rc = rc != 0;` |
|        - |  4487 | `	}` |
|   159540 |  4488 | `	VmPopOperand(&pTos,1);` |
|   159540 |  4489 | `	if( !pInstr->iP2 ){` |
|        - |  4490 | `		/* Push comparison result without taking the jump */` |
|   159540 |  4491 | `		PH7_MemObjRelease(pTos);` |
|   159540 |  4492 | `		pTos->x.iVal = rc;` |
|        - |  4493 | `		/* Invalidate any prior representation */` |
|   159540 |  4494 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    79771 |  4495 | `	}else{` |
|      ! 0 |  4496 | `		if( rc ){` |
|        - |  4497 | `			/* Jump to the desired location */` |
|      ! 0 |  4498 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4499 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4500 | `		}` |
|        - |  4501 | `	}` |
|   159540 |  4502 | `	break;` |
|        - |  4503 | `				 }` |
|        - |  4504 | `/* OP_LT P1 P2 P3` |
|        - |  4505 | ` *` |
|        - |  4506 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4507 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4508 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4509 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4510 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4511 | ` *` |
|        - |  4512 | ` */` |
|        - |  4513 | `/* OP_LE P1 P2 P3` |
|        - |  4514 | ` *` |
|        - |  4515 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4516 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4517 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4518 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4519 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4520 | ` *` |
|        - |  4521 | ` */` |
|    89615 |  4522 | `case PH7_OP_LT:` |
|        - |  4523 | `case PH7_OP_LE: {` |
|   179276 |  4524 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4525 | `	/* Perform the comparison and act accordingly */` |
|        - |  4526 | `#ifdef UNTRUST` |
|        - |  4527 | `	if( pNos < pStack ){` |
|        - |  4528 | `		goto Abort;` |
|        - |  4529 | `	}` |
|        - |  4530 | `#endif` |
|   179276 |  4531 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   179276 |  4532 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4533 | `		rc = 0;` |
|   179272 |  4534 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4535 | `		rc = rc < 1;` |
|      198 |  4536 | `	}else{` |
|   178874 |  4537 | `		rc = rc < 0;` |
|        - |  4538 | `	}` |
|   179276 |  4539 | `	VmPopOperand(&pTos,1);` |
|   179276 |  4540 | `	if( !pInstr->iP2 ){` |
|        - |  4541 | `		/* Push comparison result without taking the jump */` |
|   179276 |  4542 | `		PH7_MemObjRelease(pTos);` |
|   179276 |  4543 | `		pTos->x.iVal = rc;` |
|        - |  4544 | `		/* Invalidate any prior representation */` |
|   179276 |  4545 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    89661 |  4546 | `	}else{` |
|      ! 0 |  4547 | `		if( rc ){` |
|        - |  4548 | `			/* Jump to the desired location */` |
|      ! 0 |  4549 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4550 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4551 | `		}` |
|        - |  4552 | `	}` |
|   179276 |  4553 | `	break;` |
|        - |  4554 | `				}` |
|        - |  4555 | `/* OP_GT P1 P2 P3` |
|        - |  4556 | ` *` |
|        - |  4557 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4558 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4559 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4560 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4561 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4562 | ` *` |
|        - |  4563 | ` */` |
|        - |  4564 | `/* OP_GE P1 P2 P3` |
|        - |  4565 | ` *` |
|        - |  4566 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4567 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4568 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4569 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4570 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4571 | ` *` |
|        - |  4572 | ` */` |
|    35494 |  4573 | `case PH7_OP_GT:` |
|        - |  4574 | `case PH7_OP_GE: {` |
|    70990 |  4575 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4576 | `	/* Perform the comparison and act accordingly */` |
|        - |  4577 | `#ifdef UNTRUST` |
|        - |  4578 | `	if( pNos < pStack ){` |
|        - |  4579 | `		goto Abort;` |
|        - |  4580 | `	}` |
|        - |  4581 | `#endif` |
|    70990 |  4582 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    70990 |  4583 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4584 | `		rc = 0;` |
|    70986 |  4585 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    70834 |  4586 | `		rc = rc >= 0;` |
|    35418 |  4587 | `	}else{` |
|      150 |  4588 | `		rc = rc > 0;` |
|        - |  4589 | `	}` |
|    70990 |  4590 | `	VmPopOperand(&pTos,1);` |
|    70990 |  4591 | `	if( !pInstr->iP2 ){` |
|        - |  4592 | `		/* Push comparison result without taking the jump */` |
|    70990 |  4593 | `		PH7_MemObjRelease(pTos);` |
|    70990 |  4594 | `		pTos->x.iVal = rc;` |
|        - |  4595 | `		/* Invalidate any prior representation */` |
|    70990 |  4596 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    35496 |  4597 | `	}else{` |
|      ! 0 |  4598 | `		if( rc ){` |
|        - |  4599 | `			/* Jump to the desired location */` |
|      ! 0 |  4600 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4601 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4602 | `		}` |
|        - |  4603 | `	}` |
|    70990 |  4604 | `	break;` |
|        - |  4605 | `				}` |
|        - |  4606 | `/* OP_SEQ P1 P2 *` |
|        - |  4607 | ` * Strict string comparison.` |
|        - |  4608 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4609 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4610 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4611 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4612 | ` * use PH7_OP_EQ.` |
|        - |  4613 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4614 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4615 | ` */` |
|        - |  4616 | `/* OP_SNE P1 P2 *` |
|        - |  4617 | ` * Strict string comparison.` |
|        - |  4618 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4619 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4620 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4621 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4622 | ` * use PH7_OP_EQ.` |
|        - |  4623 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4624 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4625 | ` */` |
|       18 |  4626 | `case PH7_OP_SEQ:` |
|        - |  4627 | `case PH7_OP_SNE: {` |
|       38 |  4628 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4629 | `	SyString s1,s2;` |
|        - |  4630 | `	/* Perform the comparison and act accordingly */` |
|        - |  4631 | `#ifdef UNTRUST` |
|        - |  4632 | `	if( pNos < pStack ){` |
|        - |  4633 | `		goto Abort;` |
|        - |  4634 | `	}` |
|        - |  4635 | `#endif` |
|        - |  4636 | `	/* Force a string cast */` |
|       38 |  4637 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4638 | `		PH7_MemObjToString(pTos);` |
|        2 |  4639 | `	}` |
|       38 |  4640 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4641 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4642 | `	}` |
|       38 |  4643 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4644 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4645 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4646 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4647 | `		rc = rc != 0;` |
|      ! 0 |  4648 | `	}else{` |
|       38 |  4649 | `		rc = rc == 0;` |
|        - |  4650 | `	}` |
|       38 |  4651 | `	VmPopOperand(&pTos,1);` |
|       38 |  4652 | `	if( !pInstr->iP2 ){` |
|        - |  4653 | `		/* Push comparison result without taking the jump */` |
|       38 |  4654 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4655 | `		pTos->x.iVal = rc;` |
|        - |  4656 | `		/* Invalidate any prior representation */` |
|       38 |  4657 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4658 | `	}else{` |
|      ! 0 |  4659 | `		if( rc ){` |
|        - |  4660 | `			/* Jump to the desired location */` |
|      ! 0 |  4661 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4662 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4663 | `		}` |
|        - |  4664 | `	}` |
|       38 |  4665 | `	break;` |
|        - |  4666 | `				 }` |
|        - |  4667 | `/*` |
|        - |  4668 | ` * OP_LOAD_REF * * *` |
|        - |  4669 | ` * Push the index of a referenced object on the stack.` |
|        - |  4670 | ` */` |
|       57 |  4671 | `case PH7_OP_LOAD_REF: {` |
|        - |  4672 | `	sxu32 nIdx;` |
|        - |  4673 | `#ifdef UNTRUST` |
|        - |  4674 | `	if( pTos < pStack ){` |
|        - |  4675 | `		goto Abort;` |
|        - |  4676 | `	}` |
|        - |  4677 | `#endif` |
|        - |  4678 | `	/* Extract memory object index */` |
|      115 |  4679 | `	nIdx = pTos->nIdx;` |
|      115 |  4680 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4681 | `		/* Nullify the object */` |
|       95 |  4682 | `		PH7_MemObjRelease(pTos);` |
|        - |  4683 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4684 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4685 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4686 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4687 | `	}` |
|      115 |  4688 | `	break;` |
|        - |  4689 | `					  }` |
|        - |  4690 | `/*` |
|        - |  4691 | ` * OP_STORE_REF * * P3` |
|        - |  4692 | ` * Perform an assignment operation by reference.` |
|        - |  4693 | ` */` |
|       14 |  4694 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4695 | `	 SyString sName = { 0 , 0 };` |
|        - |  4696 | `	 VmFrame *pFrameLocal;` |
|        - |  4697 | `	SyHashEntry *pEntry;` |
|        - |  4698 | `	sxu32 nIdx;` |
|        - |  4699 | `#ifdef UNTRUST` |
|        - |  4700 | `	if( pTos < pStack ){` |
|        - |  4701 | `		goto Abort;` |
|        - |  4702 | `	}` |
|        - |  4703 | `#endif` |
|       30 |  4704 | `	if( pInstr->p3 == 0 ){` |
|        - |  4705 | `		char *zName;` |
|        - |  4706 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4707 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4708 | `			/* Force a string cast */` |
|      ! 0 |  4709 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4710 | `		}` |
|      ! 0 |  4711 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4712 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4713 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4714 | `			if( zName ){` |
|      ! 0 |  4715 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4716 | `			}` |
|      ! 0 |  4717 | `		}` |
|      ! 0 |  4718 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4719 | `		pTos--;` |
|      ! 0 |  4720 | `	}else{` |
|       30 |  4721 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4722 | `	}` |
|       30 |  4723 | `	nIdx = pTos->nIdx;` |
|       30 |  4724 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4725 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4726 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4727 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4728 | `		}else{` |
|        - |  4729 | `			ph7_value *pObj;` |
|        - |  4730 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4731 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4732 | `			if( pObj == 0 ){` |
|      ! 0 |  4733 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4734 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4735 | `				goto Abort;` |
|        - |  4736 | `			}` |
|        - |  4737 | `			/* Perform the store operation */` |
|      ! 0 |  4738 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4739 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4740 | `		}` |
|       30 |  4741 | `	}else if( sName.nByte > 0){` |
|       30 |  4742 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4743 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4744 | `		}else{` |
|       30 |  4745 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4746 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4747 | `				/* Safely ignore the exception frame */` |
|       21 |  4748 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4749 | `			}` |
|        - |  4750 | `			/* Query the local frame */` |
|       30 |  4751 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4752 | `			if( pEntry ){` |
|      ! 0 |  4753 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4754 | `			}else{` |
|       30 |  4755 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4756 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4757 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4758 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4759 | `				}` |
|       30 |  4760 | `				if( rc == SXRET_OK ){` |
|       30 |  4761 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4762 | `				}` |
|        - |  4763 | `			}` |
|        - |  4764 | `		}` |
|       14 |  4765 | `	}` |
|       30 |  4766 | `	break;` |
|        - |  4767 | `				 }` |
|        - |  4768 | `/*` |
|        - |  4769 | ` * OP_UPLINK P1 * *` |
|        - |  4770 | ` * Link a variable to the top active VM frame.` |
|        - |  4771 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4772 | ` */` |
|       14 |  4773 | `case PH7_OP_UPLINK: {` |
|       29 |  4774 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4775 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4776 | `		SyString sName;` |
|        - |  4777 | `		/* Perform the link */` |
|       59 |  4778 | `		while( pLink <= pTos ){` |
|       31 |  4779 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4780 | `				/* Force a string cast */` |
|      ! 0 |  4781 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4782 | `			}` |
|       31 |  4783 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4784 | `			if( sName.nByte > 0 ){` |
|       31 |  4785 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4786 | `			}` |
|       31 |  4787 | `			pLink++;` |
|        1 |  4788 | `		}` |
|       14 |  4789 | `	}` |
|       29 |  4790 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4791 | `	break;` |
|        - |  4792 | `					}` |
|        - |  4793 | `/*` |
|        - |  4794 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4795 | ` * Push an exception in the corresponding container so that` |
|        - |  4796 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4797 | ` */` |
|       10 |  4798 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4799 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4800 | `	VmFrame *pFrameLocal;` |
|       22 |  4801 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4802 | `	/* Create the exception frame */` |
|       22 |  4803 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4804 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4805 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4806 | `		goto Abort;` |
|        - |  4807 | `	}` |
|        - |  4808 | `	/* Mark the special frame */` |
|       22 |  4809 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4810 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4811 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4812 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4813 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4814 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4815 | `	}` |
|       22 |  4816 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4817 | `	break;` |
|        - |  4818 | `							}` |
|        - |  4819 | `/*` |
|        - |  4820 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4821 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4822 | ` */` |
|        9 |  4823 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4824 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4825 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4826 | `		ph7_exception **apException;` |
|        - |  4827 | `		/* Pop the loaded exception */` |
|        7 |  4828 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4829 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4830 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4831 | `		}` |
|        3 |  4832 | `	}` |
|       20 |  4833 | `	pException->pFrame = 0;` |
|        - |  4834 | `	/* Leave the exception frame */` |
|       20 |  4835 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4836 | `	break;` |
|        - |  4837 | `							}` |
|        - |  4838 |  |
|        - |  4839 | `/*` |
|        - |  4840 | ` * OP_THROW * P2 *` |
|        - |  4841 | ` * Throw an user exception.` |
|        - |  4842 | ` */` |
|        8 |  4843 | `case PH7_OP_THROW: {` |
|       18 |  4844 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4845 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4846 | `#ifdef UNTRUST` |
|        - |  4847 | `	if( pTos < pStack ){` |
|        - |  4848 | `		goto Abort;` |
|        - |  4849 | `	}` |
|        - |  4850 | `#endif` |
|       24 |  4851 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4852 | `		/* Safely ignore the exception frame */` |
|        8 |  4853 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4854 | `	}` |
|        - |  4855 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4856 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4857 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4858 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4859 | `		ph7_class *pException;` |
|        - |  4860 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4861 | `		 */` |
|       18 |  4862 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4863 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4864 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4865 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4866 | `			if( rc == SXERR_ABORT ){` |
|        - |  4867 | `				/* Abort processing immediately */` |
|      ! 0 |  4868 | `				goto Abort;` |
|        - |  4869 | `			}` |
|      ! 0 |  4870 | `		}else{` |
|        - |  4871 | `			/* Throw the exception */` |
|       18 |  4872 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4873 | `			if( rc == SXERR_ABORT ){` |
|        - |  4874 | `				/* Abort processing immediately */` |
|        3 |  4875 | `				goto Abort;` |
|        - |  4876 | `			}` |
|        - |  4877 | `		}` |
|        9 |  4878 | `	}else{` |
|        - |  4879 | `		/* Expecting a class instance */` |
|      ! 0 |  4880 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4881 | `		if( rc == SXERR_ABORT ){` |
|        - |  4882 | `			/* Abort processing immediately */` |
|      ! 0 |  4883 | `			goto Abort;` |
|        - |  4884 | `		}` |
|        - |  4885 | `	}` |
|        - |  4886 | `	/* Pop the top entry */` |
|       16 |  4887 | `	VmPopOperand(&pTos,1);` |
|        - |  4888 | `	/* Perform an unconditional jump */` |
|       16 |  4889 | `	pc = nJump - 1;` |
|       16 |  4890 | `	break;` |
|        - |  4891 | `				   }` |
|        - |  4892 | `/*` |
|        - |  4893 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4894 | ` * Prepare a foreach step.` |
|        - |  4895 | ` */` |
|     3855 |  4896 | `case PH7_OP_FOREACH_INIT: {` |
|     7712 |  4897 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4898 | `	void *pName;` |
|        - |  4899 | `#ifdef UNTRUST` |
|        - |  4900 | `	if( pTos < pStack ){` |
|        - |  4901 | `		goto Abort;` |
|        - |  4902 | `	}` |
|        - |  4903 | `#endif` |
|     7712 |  4904 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4905 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4906 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4907 | `			/* Force a string cast */` |
|      ! 0 |  4908 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4909 | `		}` |
|        - |  4910 | `		/* Duplicate name */` |
|      ! 0 |  4911 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4912 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4913 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4914 | `		}` |
|      ! 0 |  4915 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4916 | `	}` |
|     7712 |  4917 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4918 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4919 | `			/* Force a string cast */` |
|      ! 0 |  4920 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4921 | `		}` |
|        - |  4922 | `		/* Duplicate name */` |
|      ! 0 |  4923 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4924 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4925 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4926 | `		}` |
|      ! 0 |  4927 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4928 | `	}` |
|        - |  4929 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     7712 |  4930 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4931 | `		/* Jump out of the loop */` |
|      ! 0 |  4932 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4933 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4934 | `		}` |
|      ! 0 |  4935 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4936 | `	}else{` |
|        - |  4937 | `		ph7_foreach_step *pStep;` |
|     7712 |  4938 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     7712 |  4939 | `		if( pStep == 0 ){` |
|      ! 0 |  4940 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4941 | `			/* Jump out of the loop */` |
|      ! 0 |  4942 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4943 | `		}else{` |
|        - |  4944 | `			/* Zero the structure */` |
|     7712 |  4945 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4946 | `			/* Prepare the step */` |
|     7712 |  4947 | `			pStep->iFlags = pInfo->iFlags;` |
|     7712 |  4948 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     7704 |  4949 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4950 | `				/* Reset the internal loop cursor */` |
|     7704 |  4951 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4952 | `				/* Mark the step */` |
|     7704 |  4953 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     7704 |  4954 | `				pStep->xIter.pMap = pMap;` |
|     7704 |  4955 | `				pMap->iRef++;` |
|     3853 |  4956 | `			}else{` |
|        9 |  4957 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4958 | `				/* Reset the loop cursor */` |
|        9 |  4959 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4960 | `				/* Mark the step */` |
|        9 |  4961 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4962 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4963 | `				pThis->iRef++;` |
|        - |  4964 | `			}` |
|        - |  4965 | `		}` |
|     7712 |  4966 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4967 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4968 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4969 | `			/* Jump out of the loop */` |
|      ! 0 |  4970 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4971 | `		}` |
|        - |  4972 | `	}` |
|     7712 |  4973 | `	VmPopOperand(&pTos,1);` |
|     7712 |  4974 | `	break;` |
|        - |  4975 | `						  }` |
|        - |  4976 | `/*` |
|        - |  4977 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4978 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4979 | ` */` |
|    63747 |  4980 | `case PH7_OP_FOREACH_STEP: {` |
|   127496 |  4981 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4982 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4983 | `	ph7_value *pValue;` |
|        - |  4984 | `	VmFrame *pFrameLocal;` |
|        - |  4985 | `	/* Peek the last step */` |
|   127496 |  4986 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   127496 |  4987 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   127496 |  4988 | `	pFrameLocal = pVm->pFrame;` |
|   132528 |  4989 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4990 | `		/* Safely ignore the exception frame */` |
|     5033 |  4991 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4992 | `	}` |
|   127496 |  4993 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   127472 |  4994 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4995 | `		ph7_hashmap_node *pNode;` |
|        - |  4996 | `		/* Extract the current node value */` |
|   127472 |  4997 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   127472 |  4998 | `		if( pNode == 0 ){` |
|        - |  4999 | `			/* No more entry to process */` |
|     7704 |  5000 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     7704 |  5001 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5002 | `				/* Break the reference with the last element */` |
|        5 |  5003 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5004 | `			}` |
|        - |  5005 | `			/* Automatically reset the loop cursor */` |
|     7704 |  5006 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5007 | `			/* Cleanup the mess left behind */` |
|     7704 |  5008 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     7704 |  5009 | `			SySetPop(&pInfo->aStep);` |
|     7704 |  5010 | `			PH7_HashmapUnref(pMap);` |
|     3853 |  5011 | `		}else{` |
|   119770 |  5012 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      139 |  5013 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      139 |  5014 | `				if( pKey ){` |
|      139 |  5015 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       69 |  5016 | `				}` |
|       69 |  5017 | `			}` |
|   119770 |  5018 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5019 | `				SyHashEntry *pEntry;` |
|        - |  5020 | `				/* Pass by reference */` |
|       13 |  5021 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5022 | `				if( pEntry ){` |
|       13 |  5023 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5024 | `				}else{` |
|      ! 0 |  5025 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5026 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5027 | `				}` |
|        7 |  5028 | `			}else{` |
|        - |  5029 | `				/* Make a copy of the entry value */` |
|   119758 |  5030 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   119758 |  5031 | `				if( pValue ){` |
|   119758 |  5032 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    59878 |  5033 | `				}` |
|        - |  5034 | `			}` |
|        - |  5035 | `		}` |
|    63737 |  5036 | `	}else{` |
|       25 |  5037 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5038 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5039 | `		SyHashEntry *pEntry;` |
|        - |  5040 | `		/* Point to the next attribute */` |
|       29 |  5041 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5042 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5043 | `			/* Check access permission */` |
|       31 |  5044 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5045 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5046 | `					break; /* Access is granted */` |
|        - |  5047 | `			}` |
|        1 |  5048 | `		}` |
|       25 |  5049 | `		if( pEntry == 0 ){` |
|        - |  5050 | `			/* Clean up the mess left behind */` |
|        9 |  5051 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5052 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5053 | `				/* Break the reference with the last element */` |
|        3 |  5054 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5055 | `			}` |
|        9 |  5056 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5057 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5058 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5059 | `		}else{` |
|       17 |  5060 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5061 | `			ph7_value *pAttrValue;` |
|       17 |  5062 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5063 | `				/* Fill with the current attribute name */` |
|       17 |  5064 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5065 | `				if( pKey ){` |
|       17 |  5066 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5067 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5068 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5069 | `				}` |
|        8 |  5070 | `			}` |
|        - |  5071 | `			/* Extract attribute value */` |
|       17 |  5072 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5073 | `			if( pAttrValue ){` |
|       17 |  5074 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5075 | `					/* Pass by reference */` |
|        3 |  5076 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5077 | `					if( pEntry ){` |
|        3 |  5078 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5079 | `					}else{` |
|      ! 0 |  5080 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5081 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5082 | `					}` |
|        2 |  5083 | `				}else{` |
|        - |  5084 | `					/* Make a copy of the attribute value */` |
|       15 |  5085 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5086 | `					if( pValue ){` |
|       15 |  5087 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5088 | `					}` |
|        - |  5089 | `				}` |
|        8 |  5090 | `			}` |
|        - |  5091 | `		}` |
|        - |  5092 | `	}` |
|   127496 |  5093 | `	break;` |
|        - |  5094 | `						  }` |
|        - |  5095 | `/*` |
|        - |  5096 | ` * OP_MEMBER P1 P2` |
|        - |  5097 | ` * Load class attribute/method on the stack.` |
|        - |  5098 | ` */` |
|      966 |  5099 | `case PH7_OP_MEMBER: {` |
|        - |  5100 | `	ph7_class_instance *pThis;` |
|        - |  5101 | `	ph7_value *pNos;` |
|        - |  5102 | `	SyString sName;` |
|     1934 |  5103 | `	if( !pInstr->iP1 ){` |
|     1876 |  5104 | `		pNos = &pTos[-1];` |
|        - |  5105 | `#ifdef UNTRUST` |
|        - |  5106 | `		if( pNos < pStack ){` |
|        - |  5107 | `			goto Abort;` |
|        - |  5108 | `		}` |
|        - |  5109 | `#endif` |
|     1876 |  5110 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5111 | `			ph7_class *pClass;` |
|        - |  5112 | `			/* Class already instantiated */` |
|     1876 |  5113 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5114 | `			/* Point to the instantiated class */` |
|     1876 |  5115 | `			pClass = pThis->pClass;` |
|        - |  5116 | `			/* Extract attribute name first */` |
|     1876 |  5117 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     1876 |  5118 | `			if( pInstr->iP2 ){` |
|        - |  5119 | `				/* Method call */` |
|      120 |  5120 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5121 | `				if( sName.nByte > 0 ){` |
|        - |  5122 | `					/* Extract the target method */` |
|      120 |  5123 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5124 | `				}` |
|      120 |  5125 | `				if( pMeth == 0 ){` |
|      ! 0 |  5126 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5127 | `						&pClass->sName,&sName` |
|        - |  5128 | `						);` |
|        - |  5129 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5130 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5131 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5132 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5133 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5134 | `				}else{` |
|        - |  5135 | `					/* Push method name on the stack */` |
|      120 |  5136 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5137 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5138 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5139 | `				}` |
|      120 |  5140 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5141 | `			}else{` |
|        - |  5142 | `				/* Attribute access */` |
|     1758 |  5143 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5144 | `				SyHashEntry *pEntry;` |
|        - |  5145 | `				/* Extract the target attribute */` |
|     1758 |  5146 | `				if( sName.nByte > 0 ){` |
|     1758 |  5147 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     1758 |  5148 | `					if( pEntry ){` |
|        - |  5149 | `						/* Point to the attribute value */` |
|     1756 |  5150 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      877 |  5151 | `					}` |
|      878 |  5152 | `				}` |
|     1758 |  5153 | `				if( pObjAttr == 0 ){` |
|        - |  5154 | `					/* No such attribute,load null */` |
|        4 |  5155 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5156 | `						&pClass->sName,&sName);` |
|        - |  5157 | `					/* Call the __get magic method if available */` |
|        3 |  5158 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5159 | `				}` |
|     1758 |  5160 | `				VmPopOperand(&pTos,1);` |
|        - |  5161 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5162 | `				 * This is due to the following case:` |
|        - |  5163 | `				 *     (new TestClass())->foo;` |
|        - |  5164 | `				 */` |
|     1758 |  5165 | `				pThis->iRef++;` |
|     1758 |  5166 | `				PH7_MemObjRelease(pTos);` |
|     1758 |  5167 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     1758 |  5168 | `				if( pObjAttr ){` |
|     1756 |  5169 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5170 | `					/* Check attribute access */` |
|     1756 |  5171 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5172 | `						/* Load attribute */` |
|     1756 |  5173 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     1756 |  5174 | `						if( pValue ){` |
|     1756 |  5175 | `							if( pThis->iRef < 2 ){` |
|        - |  5176 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5177 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5178 | `								 */` |
|        3 |  5179 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5180 | `							}else{` |
|        - |  5181 | `								/* Simple load */` |
|     1754 |  5182 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5183 | `							}` |
|     1756 |  5184 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     1754 |  5185 | `								if( pThis->iRef > 1 ){` |
|        - |  5186 | `									/* Load attribute index */` |
|     1752 |  5187 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      875 |  5188 | `								}` |
|      876 |  5189 | `							}` |
|      877 |  5190 | `						}` |
|      877 |  5191 | `					}` |
|      877 |  5192 | `				}` |
|        - |  5193 | `				/* Safely unreference the object */` |
|     1758 |  5194 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5195 | `			}` |
|      939 |  5196 | `		}else{` |
|      ! 0 |  5197 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5198 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5199 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5200 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5201 | `		}` |
|      939 |  5202 | `	}else{` |
|        - |  5203 | `		/* Static member access using class name */` |
|       59 |  5204 | `		pNos = pTos;` |
|       59 |  5205 | `		pThis = 0;` |
|       59 |  5206 | `		if( !pInstr->p3 ){` |
|       57 |  5207 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5208 | `			pNos--;` |
|        - |  5209 | `#ifdef UNTRUST` |
|        - |  5210 | `			if( pNos < pStack ){` |
|        - |  5211 | `				goto Abort;` |
|        - |  5212 | `			}` |
|        - |  5213 | `#endif` |
|       29 |  5214 | `		}else{` |
|        - |  5215 | `			/* Attribute name already computed */` |
|        3 |  5216 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5217 | `		}` |
|       59 |  5218 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5219 | `			ph7_class *pClass = 0;` |
|       59 |  5220 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5221 | `				/* Class already instantiated */` |
|      ! 0 |  5222 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5223 | `				pClass = pThis->pClass;` |
|      ! 0 |  5224 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5225 | `			}else{` |
|        - |  5226 | `				/* Try to extract the target class */` |
|       59 |  5227 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5228 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5229 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5230 | `				}` |
|        - |  5231 | `			}` |
|       59 |  5232 | `			if( pClass == 0 ){` |
|        - |  5233 | `				/* Undefined class */` |
|      ! 0 |  5234 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5235 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5236 | `					);` |
|      ! 0 |  5237 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5238 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5239 | `				}` |
|      ! 0 |  5240 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5241 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5242 | `			}else{` |
|       59 |  5243 | `				if( pInstr->iP2 ){` |
|        - |  5244 | `					/* Method call */` |
|       25 |  5245 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5246 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5247 | `						/* Extract the target method */` |
|       25 |  5248 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5249 | `					}` |
|       25 |  5250 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5251 | `						if( pMeth ){` |
|      ! 0 |  5252 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5253 | `								&pClass->sName,&sName` |
|        - |  5254 | `								);` |
|      ! 0 |  5255 | `						}else{` |
|      ! 0 |  5256 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5257 | `								&pClass->sName,&sName` |
|        - |  5258 | `								);` |
|        - |  5259 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5260 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5261 | `						}` |
|        - |  5262 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5263 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5264 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5265 | `						}` |
|      ! 0 |  5266 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5267 | `					}else{` |
|        - |  5268 | `						/* Push method name on the stack */` |
|       25 |  5269 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5270 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5271 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5272 | `					}` |
|       25 |  5273 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5274 | `				}else{` |
|        - |  5275 | `					/* Attribute access */` |
|       35 |  5276 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5277 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5278 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5279 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5280 | `						/* ::class returns the fully qualified class name */` |
|        - |  5281 | `						/* Pop the attribute name from the stack */` |
|       27 |  5282 | `						if( !pInstr->p3 ){` |
|       27 |  5283 | `							VmPopOperand(&pTos,1);` |
|       13 |  5284 | `						}` |
|       27 |  5285 | `						PH7_MemObjRelease(pTos);` |
|        - |  5286 | `						/* Load the class name */` |
|       27 |  5287 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5288 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5289 | `					}else{` |
|        - |  5290 | `						/* Extract the target attribute */` |
|        9 |  5291 | `						if( sName.nByte > 0 ){` |
|        9 |  5292 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5293 | `						}` |
|        9 |  5294 | `						if( pAttr == 0 ){` |
|        - |  5295 | `							/* No such attribute,load null */` |
|      ! 0 |  5296 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5297 | `								&pClass->sName,&sName);` |
|        - |  5298 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5299 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5300 | `						}` |
|        - |  5301 | `						/* Pop the attribute name from the stack */` |
|        9 |  5302 | `						if( !pInstr->p3 ){` |
|        7 |  5303 | `							VmPopOperand(&pTos,1);` |
|        3 |  5304 | `						}` |
|        9 |  5305 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5306 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5307 | `						if( pAttr ){` |
|        9 |  5308 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5309 | `								/* Access to a non static attribute */` |
|      ! 0 |  5310 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5311 | `									&pClass->sName,&pAttr->sName` |
|        - |  5312 | `									);` |
|      ! 0 |  5313 | `							}else{` |
|        - |  5314 | `								ph7_value *pValue;` |
|        - |  5315 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5316 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5317 | `									/* Load the desired attribute */` |
|        9 |  5318 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5319 | `									if( pValue ){` |
|        9 |  5320 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5321 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5322 | `											/* Load index number */` |
|        3 |  5323 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5324 | `										}` |
|        4 |  5325 | `									}` |
|        4 |  5326 | `								}` |
|        - |  5327 | `							}` |
|        4 |  5328 | `						}` |
|        - |  5329 | `					}` |
|        - |  5330 | `				}` |
|       59 |  5331 | `				if( pThis ){` |
|        - |  5332 | `					/* Safely unreference the object */` |
|      ! 0 |  5333 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5334 | `				}` |
|        - |  5335 | `			}` |
|       30 |  5336 | `		}else{` |
|        - |  5337 | `			/* Pop operands */` |
|      ! 0 |  5338 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5339 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5340 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5341 | `			}` |
|      ! 0 |  5342 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5343 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5344 | `		}` |
|        - |  5345 | `	}` |
|     1934 |  5346 | `	break;` |
|        - |  5347 | `					}` |
|        - |  5348 | `/*` |
|        - |  5349 | ` * OP_NEW P1 * * *` |
|        - |  5350 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5351 | ` */` |
|      248 |  5352 | `case PH7_OP_NEW: {` |
|      498 |  5353 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      498 |  5354 | `	ph7_class *pClass = 0;` |
|        - |  5355 | `	ph7_class_instance *pNew;` |
|      498 |  5356 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5357 | `		/* Try to extract the desired class */` |
|      746 |  5358 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      496 |  5359 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      248 |  5360 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5361 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5362 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5363 | `	}` |
|      498 |  5364 | `	if( pClass == 0 ){` |
|        - |  5365 | `		/* No such class */` |
|      ! 0 |  5366 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5367 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5368 | `			);` |
|      ! 0 |  5369 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5370 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5371 | `			/* Pop given arguments */` |
|      ! 0 |  5372 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5373 | `		}` |
|      ! 0 |  5374 | `	}else{` |
|        - |  5375 | `		ph7_class_method *pCons;` |
|        - |  5376 | `		/* Create a new class instance */` |
|      498 |  5377 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      498 |  5378 | `		if( pNew == 0 ){` |
|      ! 0 |  5379 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5380 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5381 | `				&pClass->sName` |
|        - |  5382 | `			);` |
|      ! 0 |  5383 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5384 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5385 | `				/* Pop given arguments */` |
|      ! 0 |  5386 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5387 | `			}` |
|      ! 0 |  5388 | `			break;` |
|        - |  5389 | `		}` |
|        - |  5390 | `		/* Check if a constructor is available */` |
|      498 |  5391 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      498 |  5392 | `		if( pCons == 0 ){` |
|      446 |  5393 | `			SyString *pName = &pClass->sName;` |
|        - |  5394 | `			/* Check for a constructor with the same base class name */` |
|      446 |  5395 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      222 |  5396 | `		}` |
|      498 |  5397 | `		if( pCons ){` |
|        - |  5398 | `			/* Call the class constructor */` |
|       54 |  5399 | `			SySetReset(&aArg);` |
|       96 |  5400 | `			while( pArg < pTos ){` |
|       44 |  5401 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5402 | `				pArg++;` |
|        2 |  5403 | `			}` |
|       54 |  5404 | `			if( pVm->bErrReport ){` |
|        - |  5405 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5406 | `				sxu32 n;` |
|       11 |  5407 | `				n = SySetUsed(&aArg);` |
|        - |  5408 | `				/* Emit a notice for missing arguments */` |
|       27 |  5409 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       17 |  5410 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       17 |  5411 | `					if( pFuncArg ){` |
|       17 |  5412 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5413 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5414 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5415 | `						}` |
|        8 |  5416 | `					}` |
|       17 |  5417 | `					n++;` |
|        1 |  5418 | `				}` |
|        5 |  5419 | `			}` |
|       54 |  5420 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5421 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5422 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5423 | `				pNew->iRef = 1;` |
|      ! 0 |  5424 | `			}` |
|       26 |  5425 | `		}` |
|      498 |  5426 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5427 | `			/* Pop given arguments */` |
|       38 |  5428 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5429 | `		}` |
|      498 |  5430 | `		PH7_MemObjRelease(pTos);` |
|      498 |  5431 | `		pTos->x.pOther = pNew;` |
|      498 |  5432 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5433 | `	}` |
|      498 |  5434 | `	break;` |
|        - |  5435 | `				 }` |
|        - |  5436 | `/*` |
|        - |  5437 | ` * OP_CLONE * * *` |
|        - |  5438 | ` * Perfome a clone operation.` |
|        - |  5439 | ` */` |
|       23 |  5440 | `case PH7_OP_CLONE: {` |
|        - |  5441 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5442 | `#ifdef UNTRUST` |
|        - |  5443 | `	if( pTos < pStack ){` |
|        - |  5444 | `		goto Abort;` |
|        - |  5445 | `	}` |
|        - |  5446 | `#endif` |
|        - |  5447 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5448 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5449 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5450 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5451 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5452 | `		break;` |
|        - |  5453 | `	}` |
|        - |  5454 | `	/* Point to the source */` |
|       44 |  5455 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5456 | `	/* Perform the clone operation */` |
|       44 |  5457 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5458 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5459 | `	if( pClone == 0 ){` |
|      ! 0 |  5460 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5461 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5462 | `	}else{` |
|        - |  5463 | `		/* Load the cloned object */` |
|       44 |  5464 | `		pTos->x.pOther = pClone;` |
|       44 |  5465 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5466 | `	}` |
|       44 |  5467 | `	break;` |
|        - |  5468 | `				   }` |
|        - |  5469 | `/*` |
|        - |  5470 | ` * OP_SWITCH * * P3` |
|        - |  5471 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5472 | ` */` |
|       18 |  5473 | `case PH7_OP_SWITCH: {` |
|       38 |  5474 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5475 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5476 | `	ph7_value sValue,sCaseValue;` |
|        - |  5477 | `	sxu32 n,nEntry;` |
|        - |  5478 | `#ifdef UNTRUST` |
|        - |  5479 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5480 | `		goto Abort;` |
|        - |  5481 | `	}` |
|        - |  5482 | `#endif` |
|        - |  5483 | `	/* Point to the case table  */` |
|       38 |  5484 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5485 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5486 | `	/* Select the appropriate case block to execute */` |
|       38 |  5487 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5488 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5489 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5490 | `		pCase = &aCase[n];` |
|       92 |  5491 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5492 | `		/* Execute the case expression first */` |
|       92 |  5493 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5494 | `		/* Compare the two expression */` |
|       92 |  5495 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5496 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5497 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5498 | `		if( rc == 0 ){` |
|        - |  5499 | `			/* Value match,jump to this block */` |
|       38 |  5500 | `			pc = pCase->nStart - 1;` |
|       38 |  5501 | `			break;` |
|        - |  5502 | `		}` |
|       29 |  5503 | `	}` |
|       38 |  5504 | `	VmPopOperand(&pTos,1);` |
|       38 |  5505 | `	if( n >= nEntry ){` |
|        - |  5506 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5507 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5508 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5509 | `		}else{` |
|        - |  5510 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5511 | `			pc = pSwitch->nOut - 1;` |
|        - |  5512 | `		}` |
|      ! 0 |  5513 | `	}` |
|       38 |  5514 | `	break;` |
|        - |  5515 | `					}` |
|        - |  5516 | `/*` |
|        - |  5517 | ` * OP_CALL P1 * *` |
|        - |  5518 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5519 | ` *  function on the stack.` |
|        - |  5520 | ` */` |
|   236270 |  5521 | `case PH7_OP_CALL: {` |
|   472586 |  5522 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5523 | `	SyHashEntry *pEntry;` |
|        - |  5524 | `	SyString sName;` |
|        - |  5525 | `	/* Extract function name */` |
|   472586 |  5526 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5527 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5528 | `			ph7_value sResult;` |
|      ! 0 |  5529 | `			SySetReset(&aArg);` |
|      ! 0 |  5530 | `			while( pArg < pTos ){` |
|      ! 0 |  5531 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5532 | `				pArg++;` |
|      ! 0 |  5533 | `			}` |
|      ! 0 |  5534 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5535 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5536 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5537 | `			SySetReset(&aArg);` |
|        - |  5538 | `			/* Pop given arguments */` |
|      ! 0 |  5539 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5540 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5541 | `			}` |
|        - |  5542 | `			/* Copy result */` |
|      ! 0 |  5543 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5544 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5545 | `		}else{` |
|        3 |  5546 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5547 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5548 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5549 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5550 | `			}else{` |
|        - |  5551 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5552 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5553 | `			}` |
|        - |  5554 | `			/* Pop given arguments */` |
|        3 |  5555 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5556 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5557 | `			}` |
|        - |  5558 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5559 | `			PH7_MemObjRelease(pTos);` |
|        - |  5560 | `		}` |
|   236178 |  5561 | `		break;` |
|        - |  5562 | `	}` |
|   472584 |  5563 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5564 | `	/* Check for a compiled function first */` |
|   472584 |  5565 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   472584 |  5566 | `	if( pEntry ){` |
|        - |  5567 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5568 | `		ph7_class_instance *pThis;` |
|        - |  5569 | `		ph7_value *pFrameStack;` |
|        - |  5570 | `		ph7_vm_func *pVmFunc;` |
|        - |  5571 | `		ph7_class *pSelf;` |
|        - |  5572 | `		VmFrame *pFrame;` |
|        - |  5573 | `		ph7_value *pObj;` |
|        - |  5574 | `		VmSlot sArg;` |
|        - |  5575 | `		sxu32 n;` |
|        - |  5576 | `		/* initialize fields */` |
|     9126 |  5577 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     9126 |  5578 | `		pThis = 0;` |
|     9126 |  5579 | `		pSelf = 0;` |
|     9126 |  5580 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5581 | `			ph7_class_method *pMeth;` |
|        - |  5582 | `			/* Class method call */` |
|      714 |  5583 | `			ph7_value *pTarget = &pTos[-1];` |
|      714 |  5584 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5585 | `				/* Extract the 'this' pointer */` |
|      714 |  5586 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5587 | `					/* Instance already loaded */` |
|      684 |  5588 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      684 |  5589 | `					pThis->iRef++;` |
|      684 |  5590 | `					pSelf = pThis->pClass;` |
|      341 |  5591 | `				}` |
|      714 |  5592 | `				if( pSelf == 0 ){` |
|       31 |  5593 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5594 | `						/* "Late Static Binding" class name */` |
|       37 |  5595 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5596 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5597 | `					}` |
|       31 |  5598 | `					if( pSelf == 0 ){` |
|        7 |  5599 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5600 | `					}` |
|       15 |  5601 | `				}` |
|      714 |  5602 | `				if( pThis == 0  ){` |
|       31 |  5603 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5604 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5605 | `						/* Safely ignore the exception frame */` |
|        3 |  5606 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5607 | `					}` |
|       31 |  5608 | `					if( pFrameLocal->pParent ){` |
|        - |  5609 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5610 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5611 | `						if( pThis ){` |
|       13 |  5612 | `							pThis->iRef++;` |
|        6 |  5613 | `						}` |
|        9 |  5614 | `					}` |
|       15 |  5615 | `				}` |
|      714 |  5616 | `				VmPopOperand(&pTos,1);` |
|      714 |  5617 | `				PH7_MemObjRelease(pTos);` |
|        - |  5618 | `				/* Synchronize pointers */` |
|      714 |  5619 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5620 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5621 | `				 * user have already computed the random generated unique class method name` |
|        - |  5622 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5623 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5624 | `				 */` |
|      714 |  5625 | `				while( pArg < pStack ){` |
|      ! 0 |  5626 | `					pArg++;` |
|      ! 0 |  5627 | `				}` |
|      714 |  5628 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5629 | `					/* Check if the call is allowed */` |
|      714 |  5630 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      714 |  5631 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5632 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5633 | `							/* Pop given arguments */` |
|      ! 0 |  5634 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5635 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5636 | `							}` |
|        - |  5637 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5638 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5639 | `							break;` |
|        - |  5640 | `						}` |
|        2 |  5641 | `					}` |
|      356 |  5642 | `				}` |
|      356 |  5643 | `			}` |
|      356 |  5644 | `		}` |
|        - |  5645 | `		/* Check The recursion limit */` |
|     9126 |  5646 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5647 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5648 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5649 | `				&pVmFunc->sName);` |
|        - |  5650 | `			/* Pop given arguments */` |
|        3 |  5651 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5652 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5653 | `			}` |
|        - |  5654 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5655 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5656 | `			break;` |
|        - |  5657 | `		}` |
|     9124 |  5658 | `		if( pVmFunc->pNextName ){` |
|        - |  5659 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5660 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5661 | `		}` |
|        - |  5662 | `		/* Extract the formal argument set */` |
|     9124 |  5663 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5664 | `		/* Create a new VM frame  */` |
|     9124 |  5665 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     9124 |  5666 | `		if( rc != SXRET_OK ){` |
|        - |  5667 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5668 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5669 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5670 | `				&pVmFunc->sName);` |
|        - |  5671 | `			/* Pop given arguments */` |
|      ! 0 |  5672 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5673 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5674 | `			}` |
|        - |  5675 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5676 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5677 | `			break;` |
|        - |  5678 | `		}` |
|     9124 |  5679 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5680 | `			/* Install the '$this' variable */` |
|        - |  5681 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      694 |  5682 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      694 |  5683 | `			if( pObj ){` |
|        - |  5684 | `				/* Reflect the change */` |
|      694 |  5685 | `				pObj->x.pOther = pThis;` |
|      694 |  5686 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      346 |  5687 | `			}` |
|      346 |  5688 | `		}` |
|     9124 |  5689 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5690 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5691 | `			/* Install static variables */` |
|      ! 0 |  5692 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5693 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5694 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5695 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5696 | `					/* Initialize the static variables */` |
|      ! 0 |  5697 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5698 | `					if( pObj ){` |
|        - |  5699 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5700 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5701 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5702 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5703 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5704 | `						}` |
|      ! 0 |  5705 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5706 | `					}else{` |
|      ! 0 |  5707 | `						continue;` |
|        - |  5708 | `					}` |
|      ! 0 |  5709 | `				}` |
|        - |  5710 | `				/* Install in the current frame */` |
|      ! 0 |  5711 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5712 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5713 | `			}` |
|      ! 0 |  5714 | `		}` |
|        - |  5715 | `		/* Push arguments in the local frame */` |
|     9124 |  5716 | `		n = 0;` |
|    25846 |  5717 | `		while( pArg < pTos ){` |
|    16724 |  5718 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    16624 |  5719 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5720 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5721 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5722 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5723 | `						goto Abort;` |
|        - |  5724 | `					}` |
|      ! 0 |  5725 | `				}` |
|        - |  5726 | `				/* Make sure the given arguments are of the correct type */` |
|    16624 |  5727 | `				if( aFormalArg[n].nType > 0 ){` |
|     1024 |  5728 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5729 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5730 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5731 | `						ph7_class *pClass;` |
|        - |  5732 | `						/* Try to extract the desired class */` |
|      ! 0 |  5733 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5734 | `						if( pClass ){` |
|      ! 0 |  5735 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5736 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5737 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5738 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5739 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5740 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5741 | `								}` |
|      ! 0 |  5742 | `							}else{` |
|        - |  5743 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5744 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5745 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5746 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5747 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5748 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5749 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5750 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5751 | `								}` |
|        - |  5752 | `							}` |
|      ! 0 |  5753 | `						}` |
|     1024 |  5754 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5755 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5756 | `						/* Cast to the desired type */` |
|      ! 0 |  5757 | `						xCast(pArg);` |
|      ! 0 |  5758 | `					}` |
|      511 |  5759 | `				}` |
|    16624 |  5760 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5761 | `					/* Pass by reference */` |
|       25 |  5762 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5763 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5764 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5765 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5766 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5767 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5768 | `						}` |
|        - |  5769 | `						/* Switch to pass by value */` |
|      ! 0 |  5770 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5771 | `					}else{` |
|        - |  5772 | `						SyHashEntry *pRefEntry;` |
|        - |  5773 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5774 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5775 | `						if( pRefEntry == 0 ){` |
|       37 |  5776 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5777 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5778 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5779 | `							sArg.pUserData = 0;` |
|       25 |  5780 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5781 | `						}` |
|       25 |  5782 | `						pObj = 0;` |
|        - |  5783 | `					}` |
|       13 |  5784 | `				}else{` |
|        - |  5785 | `					/* Pass by value,make a copy of the given argument */` |
|    16600 |  5786 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5787 | `				}` |
|     8313 |  5788 | `			}else{` |
|        - |  5789 | `				char zName[32];` |
|        - |  5790 | `				SyString sArgName;` |
|        - |  5791 | `				/* Set a dummy name */` |
|      101 |  5792 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5793 | `				sArgName.zString = zName;` |
|        - |  5794 | `				/* Annonymous argument */` |
|      101 |  5795 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5796 | `			}` |
|    16724 |  5797 | `			if( pObj ){` |
|    16700 |  5798 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5799 | `				/* Insert argument index  */` |
|    16700 |  5800 | `				sArg.nIdx = pObj->nIdx;` |
|    16700 |  5801 | `				sArg.pUserData = 0;` |
|    16700 |  5802 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     8349 |  5803 | `			}` |
|    16724 |  5804 | `			PH7_MemObjRelease(pArg);` |
|    16724 |  5805 | `			pArg++;` |
|    16724 |  5806 | `			++n;` |
|        2 |  5807 | `		}` |
|        - |  5808 | `		/* Set up closure environment */` |
|     9124 |  5809 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5810 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5811 | `			ph7_value *pValue;` |
|        - |  5812 | `			sxu32 iEnv;` |
|        9 |  5813 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5814 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5815 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5816 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5817 | `					/* Do not install null value */` |
|        9 |  5818 | `					continue;` |
|        - |  5819 | `				}` |
|        9 |  5820 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5821 | `				if( pValue == 0 ){` |
|      ! 0 |  5822 | `					continue;` |
|        - |  5823 | `				}` |
|        - |  5824 | `				/* Invalidate any prior representation */` |
|        9 |  5825 | `				PH7_MemObjRelease(pValue);` |
|        - |  5826 | `				/* Duplicate bound variable value */` |
|        9 |  5827 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5828 | `			}` |
|        4 |  5829 | `		}` |
|        - |  5830 | `		/* Process default values */` |
|    10322 |  5831 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1200 |  5832 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1190 |  5833 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1190 |  5834 | `				if( pObj ){` |
|        - |  5835 | `					/* Evaluate the default value and extract it's result */` |
|     1190 |  5836 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1190 |  5837 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5838 | `						goto Abort;` |
|        - |  5839 | `					}` |
|        - |  5840 | `					/* Insert argument index */` |
|     1190 |  5841 | `					sArg.nIdx = pObj->nIdx;` |
|     1190 |  5842 | `					sArg.pUserData = 0;` |
|     1190 |  5843 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5844 | `					/* Make sure the default argument is of the correct type */` |
|     1190 |  5845 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5846 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5847 | `						/* Cast to the desired type */` |
|      ! 0 |  5848 | `						xCast(pObj);` |
|      ! 0 |  5849 | `					}` |
|      594 |  5850 | `				}` |
|      594 |  5851 | `			}` |
|     1200 |  5852 | `			++n;` |
|        2 |  5853 | `		}` |
|        - |  5854 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5855 | `		 * does not return anything.` |
|        - |  5856 | `		 */` |
|     9124 |  5857 | `		PH7_MemObjRelease(pTos);` |
|     9124 |  5858 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5859 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     9124 |  5860 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     9124 |  5861 | `		if( pFrameStack == 0 ){` |
|        - |  5862 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5863 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5864 | `				&pVmFunc->sName);` |
|      ! 0 |  5865 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5866 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5867 | `			}` |
|      ! 0 |  5868 | `			break;` |
|        - |  5869 | `		}` |
|     9124 |  5870 | `		if( pSelf ){` |
|        - |  5871 | `			/* Push class name */` |
|      712 |  5872 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      355 |  5873 | `		}` |
|        - |  5874 | `		/* Increment nesting level */` |
|     9124 |  5875 | `		pVm->nRecursionDepth++;` |
|        - |  5876 | `		/* Execute function body */` |
|     9124 |  5877 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5878 | `		/* Decrement nesting level */` |
|     9124 |  5879 | `		pVm->nRecursionDepth--;` |
|     9124 |  5880 | `		if( pSelf ){` |
|        - |  5881 | `			/* Pop class name */` |
|      712 |  5882 | `			(void)SySetPop(&pVm->aSelf);` |
|      355 |  5883 | `		}` |
|        - |  5884 | `		/* Cleanup the mess left behind */` |
|     9124 |  5885 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5886 | `			/* Return by reference,reflect that */` |
|        9 |  5887 | `			if( n != SXU32_HIGH ){` |
|        9 |  5888 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5889 | `				sxu32 i;` |
|        - |  5890 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5891 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5892 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5893 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5894 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5895 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5896 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5897 | `								&pVmFunc->sName);` |
|      ! 0 |  5898 | `						}` |
|      ! 0 |  5899 | `						n = SXU32_HIGH;` |
|      ! 0 |  5900 | `						break;` |
|        - |  5901 | `					}` |
|        3 |  5902 | `				}` |
|        5 |  5903 | `			}else{` |
|      ! 0 |  5904 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5905 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5906 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5907 | `						&pVmFunc->sName);` |
|      ! 0 |  5908 | `				}` |
|        - |  5909 | `			}` |
|        9 |  5910 | `			pTos->nIdx = n;` |
|        4 |  5911 | `		}` |
|        - |  5912 | `		/* Cleanup the mess left behind */` |
|     9124 |  5913 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5914 | `			/* An exception was throw in this frame */` |
|        7 |  5915 | `			pFrame = pFrame->pParent;` |
|        7 |  5916 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5917 | `				/* Pop the resutlt */` |
|        5 |  5918 | `				VmPopOperand(&pTos,1);` |
|        - |  5919 | `				/* Jump to this destination */` |
|        5 |  5920 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5921 | `				rc = PH7_OK;` |
|        3 |  5922 | `			}else{` |
|        3 |  5923 | `				if( pFrame->pParent ){` |
|        3 |  5924 | `					rc = PH7_EXCEPTION;` |
|        2 |  5925 | `				}else{` |
|        - |  5926 | `					/* Continue normal execution */` |
|      ! 0 |  5927 | `					rc = PH7_OK;` |
|        - |  5928 | `				}` |
|        - |  5929 | `			}` |
|        3 |  5930 | `		}` |
|        - |  5931 | `		/* Free the operand stack */` |
|     9124 |  5932 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5933 | `		/* Leave the frame */` |
|     9124 |  5934 | `		VmLeaveFrame(&(*pVm));` |
|     9124 |  5935 | `		if( rc == PH7_ABORT ){` |
|        - |  5936 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5937 | `			goto Abort;` |
|     9124 |  5938 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5939 | `			goto Exception;` |
|        - |  5940 | `		}` |
|     4562 |  5941 | `	}else{` |
|        - |  5942 | `		ph7_user_func *pFunc;` |
|        - |  5943 | `		ph7_context sCtx;` |
|        - |  5944 | `		ph7_value sRet;` |
|        - |  5945 | `		/* Look for an installed foreign function */` |
|   463460 |  5946 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   463460 |  5947 | `		if( pEntry == 0 ){` |
|        - |  5948 | `			/* Call to undefined function */` |
|        5 |  5949 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5950 | `			/* Pop given arguments */` |
|        5 |  5951 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5952 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5953 | `			}` |
|        - |  5954 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5955 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5956 | `			break;` |
|        - |  5957 | `		}` |
|   463456 |  5958 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5959 | `		/* Start collecting function arguments */` |
|   463456 |  5960 | `		SySetReset(&aArg);` |
|  1242298 |  5961 | `		while( pArg < pTos ){` |
|   778844 |  5962 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   778844 |  5963 | `			pArg++;` |
|        2 |  5964 | `		}` |
|        - |  5965 | `		/* Assume a null return value */` |
|   463456 |  5966 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5967 | `		/* Init the call context */` |
|   463456 |  5968 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5969 | `		/* Call the foreign function */` |
|   463456 |  5970 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5971 | `		/* Release the call context */` |
|   463456 |  5972 | `		VmReleaseCallContext(&sCtx);` |
|   463456 |  5973 | `		if( rc == PH7_ABORT ){` |
|      185 |  5974 | `			goto Abort;` |
|   463272 |  5975 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5976 | `			goto Exception;` |
|        - |  5977 | `		}` |
|   463270 |  5978 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5979 | `			/* Pop function name and arguments */` |
|   447722 |  5980 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   223882 |  5981 | `		}` |
|        - |  5982 | `		/* Save foreign function return value */` |
|   463270 |  5983 | `		PH7_MemObjStore(&sRet,pTos);` |
|   463270 |  5984 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5985 | `	}` |
|   472390 |  5986 | `	break;` |
|        - |  5987 | `				  }` |
|        - |  5988 | `/*` |
|        - |  5989 | ` * OP_CONSUME: P1 * *` |
|        - |  5990 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5991 | ` */` |
|     8770 |  5992 | `case PH7_OP_CONSUME: {` |
|    17542 |  5993 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    17542 |  5994 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5995 |  |
|    17542 |  5996 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    17542 |  5997 | `	pCur = pOut;` |
|        - |  5998 | `	/* Start the consume process  */` |
|    35082 |  5999 | `	while( pOut <= pTos ){` |
|        - |  6000 | `		/* Force a string cast */` |
|    17542 |  6001 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       72 |  6002 | `			PH7_MemObjToString(pOut);` |
|       35 |  6003 | `		}` |
|    17542 |  6004 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6005 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6006 | `			/* Invoke the output consumer callback */` |
|     9368 |  6007 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     9368 |  6008 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6009 | `				/* Increment output length */` |
|     3664 |  6010 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1831 |  6011 | `			}` |
|     9368 |  6012 | `			SyBlobRelease(&pOut->sBlob);` |
|     9368 |  6013 | `			if( rc == SXERR_ABORT ){` |
|        - |  6014 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6015 | `				goto Abort;` |
|        - |  6016 | `			}` |
|     4683 |  6017 | `		}` |
|    17542 |  6018 | `		pOut++;` |
|        2 |  6019 | `	}` |
|    17542 |  6020 | `	pTos = &pCur[-1];` |
|    17540 |  6021 | `	break;` |
|        - |  6022 | `					 }` |
|        - |  6023 |  |
|        - |  6024 | `		} /* Switch() */` |
|  7959562 |  6025 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6026 | `	} /* For(;;) */` |
|    11369 |  6027 | `Done:` |
|    22740 |  6028 | `	SySetRelease(&aArg);` |
|    22740 |  6029 | `	return SXRET_OK;` |
|       93 |  6030 | `Abort:` |
|      187 |  6031 | `	SySetRelease(&aArg);` |
|      641 |  6032 | `	while( pTos >= pStack ){` |
|      455 |  6033 | `		PH7_MemObjRelease(pTos);` |
|      455 |  6034 | `		pTos--;` |
|        1 |  6035 | `	}` |
|      187 |  6036 | `	return PH7_ABORT;` |
|        2 |  6037 | `Exception:` |
|        5 |  6038 | `	SySetRelease(&aArg);` |
|        9 |  6039 | `	while( pTos >= pStack ){` |
|        5 |  6040 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6041 | `		pTos--;` |
|        1 |  6042 | `	}` |
|        5 |  6043 | `	return PH7_EXCEPTION;` |
|    11466 |  6044 |  |
|        - |  6045 | `/*` |
|        - |  6046 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6047 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6048 | ` * See block-comment on that function for additional information.` |
|        - |  6049 | ` */` |
|    11584 |  6050 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6051 |  |
|        - |  6052 | `	ph7_value *pStack;` |
|        - |  6053 | `	sxi32 rc;` |
|        - |  6054 | `	/* Allocate a new operand stack */` |
|    11586 |  6055 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    11586 |  6056 | `	if( pStack == 0 ){` |
|      ! 0 |  6057 | `		return SXERR_MEM;` |
|        - |  6058 | `	}` |
|        - |  6059 | `	/* Execute the program */` |
|    11586 |  6060 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6061 | `	/* Free the operand stack */` |
|    11586 |  6062 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6063 | `	/* Execution result */` |
|    11586 |  6064 | `	return rc;` |
|     5794 |  6065 |  |
|        - |  6066 | `/*` |
|        - |  6067 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6068 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6069 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6070 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6071 | ` * execution ends.` |
|        - |  6072 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6073 | ` * additional information.` |
|        - |  6074 | ` */` |
|     1296 |  6075 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6076 |  |
|        - |  6077 | `	VmShutdownCB *pEntry;` |
|        - |  6078 | `	ph7_value *apArg[10];` |
|        - |  6079 | `	sxu32 n,nEntry;` |
|        - |  6080 | `	int i;` |
|        - |  6081 | `	/* Point to the stack of registered callbacks */` |
|     1298 |  6082 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    14258 |  6083 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    12962 |  6084 | `		apArg[i] = 0;` |
|     6482 |  6085 | `	}` |
|     1300 |  6086 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6087 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6088 | `		if( pEntry ){` |
|        - |  6089 | `			/* Prepare callback arguments if any */` |
|        3 |  6090 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6091 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6092 | `					break;` |
|        - |  6093 | `				}` |
|      ! 0 |  6094 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6095 | `			}` |
|        - |  6096 | `			/* Invoke the callback */` |
|        3 |  6097 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6098 | `			/*` |
|        - |  6099 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6100 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6101 | `			 */` |
|        3 |  6102 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6103 | `			if( pEntry ){` |
|        3 |  6104 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6105 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6106 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6107 | `				}` |
|        1 |  6108 | `			}` |
|        1 |  6109 | `		}` |
|        2 |  6110 | `	}` |
|     1298 |  6111 | `	SySetReset(&pVm->aShutdown);` |
|     1298 |  6112 |  |
|        - |  6113 | `/*` |
|        - |  6114 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6115 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6116 | ` * See block-comment on that function for additional information.` |
|        - |  6117 | ` */` |
|     1304 |  6118 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6119 |  |
|        - |  6120 | `	/* Make sure we are ready to execute this program */` |
|     1306 |  6121 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6122 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6123 | `	}` |
|        - |  6124 | `	/* Set the execution magic number  */` |
|     1306 |  6125 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6126 | `	/* Execute the program */` |
|     1306 |  6127 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6128 | `	/* Invoke any shutdown callbacks */` |
|     1302 |  6129 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6130 | `	/*` |
|        - |  6131 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6132 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6133 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6134 | `	 */` |
|     1302 |  6135 | `	return SXRET_OK;` |
|      654 |  6136 |  |
|        - |  6137 | `/*` |
|        - |  6138 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6139 | ` * the desired message.` |
|        - |  6140 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6141 | ` * in 'api.c' for additional information.` |
|        - |  6142 | ` */` |
|      368 |  6143 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6144 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6145 | `	SyString *pString /* Message to output */` |
|        - |  6146 | `	)` |
|        2 |  6147 |  |
|      370 |  6148 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      370 |  6149 | `	sxi32 rc = SXRET_OK;` |
|        - |  6150 | `	/* Call the output consumer */` |
|      370 |  6151 | `	if( pString->nByte > 0 ){` |
|      370 |  6152 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      370 |  6153 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6154 | `			/* Increment output length */` |
|       17 |  6155 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6156 | `		}` |
|      184 |  6157 | `	}` |
|      370 |  6158 | `	return rc;` |
|        2 |  6159 |  |
|        - |  6160 | `/*` |
|        - |  6161 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6162 | ` * callback to consume the formatted message.` |
|        - |  6163 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6164 | ` * in 'api.c' for additional information.` |
|        - |  6165 | ` */` |
|        2 |  6166 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6167 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6168 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6169 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6170 | `	)` |
|        1 |  6171 |  |
|        3 |  6172 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6173 | `	sxi32 rc = SXRET_OK;` |
|        - |  6174 | `	SyBlob sWorker;` |
|        - |  6175 | `	/* Format the message and call the output consumer */` |
|        3 |  6176 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6177 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6178 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6179 | `		/* Consume the formatted message */` |
|        3 |  6180 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6181 | `	}` |
|        3 |  6182 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6183 | `		/* Increment output length */` |
|      ! 0 |  6184 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6185 | `	}` |
|        - |  6186 | `	/* Release the working buffer */` |
|        3 |  6187 | `	SyBlobRelease(&sWorker);` |
|        3 |  6188 | `	return rc;` |
|        1 |  6189 |  |
|        - |  6190 | `/*` |
|        - |  6191 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6192 | ` * This function never fail and always return a pointer` |
|        - |  6193 | ` * to a null terminated string.` |
|        - |  6194 | ` */` |
|       10 |  6195 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6196 |  |
|       11 |  6197 | `	const char *zOp = "Unknown     ";` |
|       11 |  6198 | `	switch(nOp){` |
|        3 |  6199 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6202 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6207 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6243 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6246 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6247 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6248 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6252 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6254 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6255 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6256 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6257 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6259 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6260 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6261 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6262 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6263 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6264 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6265 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6266 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6267 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6268 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6269 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6270 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6271 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6272 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6273 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6274 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6275 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6276 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6277 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6278 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6279 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6280 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6281 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6282 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6283 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6284 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6285 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6286 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6287 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6288 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6289 | `	default:` |
|      ! 0 |  6290 | `		break;` |
|        - |  6291 | `	}` |
|       11 |  6292 | `	return zOp;` |
|        1 |  6293 |  |
|        - |  6294 | `/*` |
|        - |  6295 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6296 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6297 | ` * is responsible of consuming the generated dump.` |
|        - |  6298 | ` */` |
|        2 |  6299 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6300 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6301 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6302 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6303 | `	)` |
|        1 |  6304 |  |
|        - |  6305 | `	sxi32 rc;` |
|        3 |  6306 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6307 | `	return rc;` |
|        1 |  6308 |  |
|        - |  6309 | `/*` |
|        - |  6310 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6311 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6312 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6313 | ` * in 'compile.c' for additional information.` |
|        - |  6314 | ` */` |
|        8 |  6315 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6316 |  |
|        9 |  6317 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6318 | `	/* Evaluate and expand constant value */` |
|        9 |  6319 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6320 |  |
|        - |  6321 | `/*` |
|        - |  6322 | ` * Section:` |
|        - |  6323 | ` *  Function handling functions.` |
|        - |  6324 | ` * Status:` |
|        - |  6325 | ` *    Stable.` |
|        - |  6326 | ` */` |
|        - |  6327 | `/*` |
|        - |  6328 | ` * int func_num_args(void)` |
|        - |  6329 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6330 | ` * Parameters` |
|        - |  6331 | ` *   None.` |
|        - |  6332 | ` * Return` |
|        - |  6333 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6334 | ` *  or -1 if called from the globe scope.` |
|        - |  6335 | ` */` |
|      796 |  6336 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6337 |  |
|        - |  6338 | `	VmFrame *pFrame;` |
|        - |  6339 | `	ph7_vm *pVm;` |
|        - |  6340 | `	/* Point to the target VM */` |
|      798 |  6341 | `	pVm = pCtx->pVm;` |
|        - |  6342 | `	/* Current frame */` |
|      798 |  6343 | `	pFrame = pVm->pFrame;` |
|      798 |  6344 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6345 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6346 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6347 | `	}` |
|      798 |  6348 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6349 | `		SXUNUSED(nArg);` |
|      ! 0 |  6350 | `		SXUNUSED(apArg);` |
|        - |  6351 | `		/* Global frame,return -1 */` |
|      ! 0 |  6352 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6353 | `		return SXRET_OK;` |
|        - |  6354 | `	}` |
|        - |  6355 | `	/* Total number of arguments passed to the enclosing function */` |
|      798 |  6356 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      798 |  6357 | `	ph7_result_int(pCtx,nArg);` |
|      798 |  6358 | `	return SXRET_OK;` |
|      400 |  6359 |  |
|        - |  6360 | `/*` |
|        - |  6361 | ` * value func_get_arg(int $arg_num)` |
|        - |  6362 | ` *   Return an item from the argument list.` |
|        - |  6363 | ` * Parameters` |
|        - |  6364 | ` *  Argument number(index start from zero).` |
|        - |  6365 | ` * Return` |
|        - |  6366 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6367 | ` */` |
|        6 |  6368 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6369 |  |
|        8 |  6370 | `	ph7_value *pObj = 0;` |
|        8 |  6371 | `	VmSlot *pSlot = 0;` |
|        - |  6372 | `	VmFrame *pFrame;` |
|        - |  6373 | `	ph7_vm *pVm;` |
|        - |  6374 | `	/* Point to the target VM */` |
|        8 |  6375 | `	pVm = pCtx->pVm;` |
|        - |  6376 | `	/* Current frame */` |
|        8 |  6377 | `	pFrame = pVm->pFrame;` |
|        8 |  6378 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6379 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6380 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6381 | `	}` |
|        8 |  6382 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6383 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6384 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6385 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6386 | `		return SXRET_OK;` |
|        - |  6387 | `	}` |
|        - |  6388 | `	/* Extract the desired index */` |
|        5 |  6389 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6390 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6391 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6392 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6393 | `		return SXRET_OK;` |
|        - |  6394 | `	}` |
|        - |  6395 | `	/* Extract the desired argument */` |
|        5 |  6396 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6397 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6398 | `			/* Return the desired argument */` |
|        5 |  6399 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6400 | `		}else{` |
|        - |  6401 | `			/* No such argument,return false */` |
|      ! 0 |  6402 | `			ph7_result_bool(pCtx,0);` |
|        - |  6403 | `		}` |
|        3 |  6404 | `	}else{` |
|        - |  6405 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6406 | `		ph7_result_bool(pCtx,0);` |
|        - |  6407 | `	}` |
|        5 |  6408 | `	return SXRET_OK;` |
|        5 |  6409 |  |
|        - |  6410 | `/*` |
|        - |  6411 | ` * array func_get_args_byref(void)` |
|        - |  6412 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6413 | ` * Parameters` |
|        - |  6414 | ` *  None.` |
|        - |  6415 | ` * Return` |
|        - |  6416 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6417 | ` *  member of the current user-defined function's argument list.` |
|        - |  6418 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6419 | ` * NOTE:` |
|        - |  6420 | ` *  Arguments are returned to the array by reference.` |
|        - |  6421 | ` */` |
|        2 |  6422 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6423 |  |
|        - |  6424 | `	ph7_value *pArray;` |
|        - |  6425 | `	VmFrame *pFrame;` |
|        - |  6426 | `	VmSlot *aSlot;` |
|        - |  6427 | `	sxu32 n;` |
|        - |  6428 | `	/* Point to the current frame */` |
|        3 |  6429 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6430 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6431 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6432 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6433 | `	}` |
|        3 |  6434 | `	if( pFrame->pParent == 0 ){` |
|        - |  6435 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6436 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6437 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6438 | `		return SXRET_OK;` |
|        - |  6439 | `	}` |
|        - |  6440 | `	/* Create a new array */` |
|        3 |  6441 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6442 | `	if( pArray == 0 ){` |
|      ! 0 |  6443 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6444 | `		SXUNUSED(apArg);` |
|      ! 0 |  6445 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6446 | `		return SXRET_OK;` |
|        - |  6447 | `	}` |
|        - |  6448 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6449 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6450 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6451 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6452 | `	}` |
|        - |  6453 | `	/* Return the freshly created array */` |
|        3 |  6454 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6455 | `	return SXRET_OK;` |
|        2 |  6456 |  |
|        - |  6457 | `/*` |
|        - |  6458 | ` * array func_get_args(void)` |
|        - |  6459 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6460 | ` * Parameters` |
|        - |  6461 | ` *  None.` |
|        - |  6462 | ` * Return` |
|        - |  6463 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6464 | ` *  member of the current user-defined function's argument list.` |
|        - |  6465 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6466 | ` */` |
|       46 |  6467 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6468 |  |
|       47 |  6469 | `	ph7_value *pObj = 0;` |
|        - |  6470 | `	ph7_value *pArray;` |
|        - |  6471 | `	VmFrame *pFrame;` |
|        - |  6472 | `	VmSlot *aSlot;` |
|        - |  6473 | `	sxu32 n;` |
|        - |  6474 | `	/* Point to the current frame */` |
|       47 |  6475 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6476 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6477 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6478 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6479 | `	}` |
|       47 |  6480 | `	if( pFrame->pParent == 0 ){` |
|        - |  6481 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6482 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6483 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6484 | `		return SXRET_OK;` |
|        - |  6485 | `	}` |
|        - |  6486 | `	/* Create a new array */` |
|       47 |  6487 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6488 | `	if( pArray == 0 ){` |
|      ! 0 |  6489 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6490 | `		SXUNUSED(apArg);` |
|      ! 0 |  6491 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6492 | `		return SXRET_OK;` |
|        - |  6493 | `	}` |
|        - |  6494 | `	/* Start filling the array with the given arguments */` |
|       47 |  6495 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6496 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6497 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6498 | `		if( pObj ){` |
|       97 |  6499 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6500 | `		}` |
|       49 |  6501 | `	}` |
|        - |  6502 | `	/* Return the freshly created array */` |
|       47 |  6503 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6504 | `	return SXRET_OK;` |
|       24 |  6505 |  |
|        - |  6506 | `/*` |
|        - |  6507 | ` * bool function_exists(string $name)` |
|        - |  6508 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6509 | ` * Parameters` |
|        - |  6510 | ` *  The name of the desired function.` |
|        - |  6511 | ` * Return` |
|        - |  6512 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6513 | ` */` |
|     1682 |  6514 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6515 |  |
|        - |  6516 | `	const char *zName;` |
|        - |  6517 | `	ph7_vm *pVm;` |
|        - |  6518 | `	int nLen;` |
|        - |  6519 | `	int res;` |
|     1684 |  6520 | `	if( nArg < 1 ){` |
|        - |  6521 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6522 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6523 | `		return SXRET_OK;` |
|        - |  6524 | `	}` |
|        - |  6525 | `	/* Point to the target VM */` |
|     1684 |  6526 | `	pVm = pCtx->pVm;` |
|        - |  6527 | `	/* Extract the function name */` |
|     1684 |  6528 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6529 | `	/* Assume the function is not defined */` |
|     1684 |  6530 | `	res = 0;` |
|        - |  6531 | `	/* Perform the lookup */` |
|     2523 |  6532 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  6533 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6534 | `			/* Function is defined */` |
|      212 |  6535 | `			res = 1;` |
|      105 |  6536 | `	}` |
|     1684 |  6537 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  6538 | `	return SXRET_OK;` |
|      843 |  6539 |  |
|        - |  6540 | `/* Forward declaration */` |
|        - |  6541 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6542 | `/*` |
|        - |  6543 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6544 | ` * [i.e: Whether it is callable or not].` |
|        - |  6545 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6546 | ` */` |
|    14508 |  6547 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6548 |  |
|    14510 |  6549 | `	int res = 0;` |
|    14510 |  6550 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6551 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6552 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6553 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6554 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6555 | `		if( pMethod && CallInvoke ){` |
|        - |  6556 | `			ph7_value sResult;` |
|        - |  6557 | `			sxi32 rc;` |
|        - |  6558 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6559 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6560 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6561 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6562 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6563 | `			}` |
|      ! 0 |  6564 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6565 | `		}` |
|    14510 |  6566 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       10 |  6567 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       10 |  6568 | `		if( pMap->nEntry > 1 ){` |
|        - |  6569 | `			ph7_class *pClass;` |
|        - |  6570 | `			ph7_value *pV;` |
|        - |  6571 | `			/* Extract the target class */` |
|       10 |  6572 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       10 |  6573 | `			if( pV ){` |
|       10 |  6574 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|       10 |  6575 | `				if( pClass ){` |
|        - |  6576 | `					ph7_class_method *pMethod;` |
|        - |  6577 | `					/* Extract the target method */` |
|        7 |  6578 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6579 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6580 | `						/* Perform the lookup */` |
|        7 |  6581 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6582 | `						if( pMethod ){` |
|        - |  6583 | `							/* Method is callable */` |
|        5 |  6584 | `							res = 1;` |
|        2 |  6585 | `						}` |
|        3 |  6586 | `					}` |
|        3 |  6587 | `				}` |
|        4 |  6588 | `			}` |
|        6 |  6589 | `		}` |
|    14506 |  6590 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6591 | `		const char *zName;` |
|        - |  6592 | `		int nLen;` |
|        - |  6593 | `		/* Extract the name */` |
|     4278 |  6594 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6595 | `		/* Perform the lookup */` |
|     4284 |  6596 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       12 |  6597 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6598 | `				/* Function is callable */` |
|     4272 |  6599 | `				res = 1;` |
|     2135 |  6600 | `		}` |
|     2138 |  6601 | `	}` |
|    14510 |  6602 | `	return res;` |
|        2 |  6603 |  |
|        - |  6604 | `/*` |
|        - |  6605 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6606 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6607 | ` * Parameters` |
|        - |  6608 | ` * $name` |
|        - |  6609 | ` *    The callback function to check` |
|        - |  6610 | ` * $syntax_only` |
|        - |  6611 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6612 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6613 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6614 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6615 | ` *    a string.` |
|        - |  6616 | ` * Return` |
|        - |  6617 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6618 | ` */` |
|       14 |  6619 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6620 |  |
|        - |  6621 | `	ph7_vm *pVm;` |
|        - |  6622 | `	int res;` |
|       15 |  6623 | `	if( nArg < 1 ){` |
|        - |  6624 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6625 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6626 | `		return SXRET_OK;` |
|        - |  6627 | `	}` |
|        - |  6628 | `	/* Point to the target VM */` |
|       15 |  6629 | `	pVm = pCtx->pVm;` |
|        - |  6630 | `	/* Perform the requested operation */` |
|       15 |  6631 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6632 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6633 | `	return SXRET_OK;` |
|        8 |  6634 |  |
|        - |  6635 | `/*` |
|        - |  6636 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6637 | ` * defined below.` |
|        - |  6638 | ` */` |
|     1046 |  6639 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6640 |  |
|     1047 |  6641 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6642 | `	ph7_value sName;` |
|        - |  6643 | `	sxi32 rc;` |
|        - |  6644 | `	/* Prepare the function name for insertion */` |
|     1047 |  6645 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1047 |  6646 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6647 | `	/* Perform the insertion */` |
|     1047 |  6648 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1047 |  6649 | `	PH7_MemObjRelease(&sName);` |
|     1047 |  6650 | `	return rc;` |
|        1 |  6651 |  |
|        - |  6652 | `/*` |
|        - |  6653 | ` * array get_defined_functions(void)` |
|        - |  6654 | ` *  Returns an array of all defined functions.` |
|        - |  6655 | ` * Parameter` |
|        - |  6656 | ` *  None.` |
|        - |  6657 | ` * Return` |
|        - |  6658 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6659 | ` *  both built-in (internal) and user-defined.` |
|        - |  6660 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6661 | ` *  defined ones using $arr["user"].` |
|        - |  6662 | ` * Note:` |
|        - |  6663 | ` *  NULL is returned on failure.` |
|        - |  6664 | ` */` |
|        2 |  6665 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6666 |  |
|        - |  6667 | `	ph7_value *pArray,*pEntry;` |
|        - |  6668 | `	/* NOTE:` |
|        - |  6669 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6670 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6671 | `	 */` |
|        3 |  6672 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6673 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6674 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6675 | `		SXUNUSED(apArg);` |
|        - |  6676 | `		/* Return NULL */` |
|      ! 0 |  6677 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6678 | `		return SXRET_OK;` |
|        - |  6679 | `	}` |
|        3 |  6680 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6681 | `	if( pEntry == 0 ){` |
|        - |  6682 | `		/* Return NULL */` |
|      ! 0 |  6683 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6684 | `		return SXRET_OK;` |
|        - |  6685 | `	}` |
|        - |  6686 | `	/* Fill with the appropriate information */` |
|        3 |  6687 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6688 | `	/* Create the 'internal' index */` |
|        3 |  6689 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6690 | `	/* Create the user-func array */` |
|        3 |  6691 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6692 | `	if( pEntry == 0 ){` |
|        - |  6693 | `		/* Return NULL */` |
|      ! 0 |  6694 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6695 | `		return SXRET_OK;` |
|        - |  6696 | `	}` |
|        - |  6697 | `	/* Fill with the appropriate information */` |
|        3 |  6698 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6699 | `	/* Create the 'user' index */` |
|        3 |  6700 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6701 | `	/* Return the multi-dimensional array */` |
|        3 |  6702 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6703 | `	return SXRET_OK;` |
|        2 |  6704 |  |
|        - |  6705 | `/*` |
|        - |  6706 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6707 | ` *  Register a function for execution on shutdown.` |
|        - |  6708 | ` * Note` |
|        - |  6709 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6710 | ` *  be called in the same order as they were registered.` |
|        - |  6711 | ` * Parameters` |
|        - |  6712 | ` *  $callback` |
|        - |  6713 | ` *   The shutdown callback to register.` |
|        - |  6714 | ` * $param` |
|        - |  6715 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6716 | ` * Return` |
|        - |  6717 | ` *  Nothing.` |
|        - |  6718 | ` */` |
|        2 |  6719 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6720 |  |
|        - |  6721 | `	VmShutdownCB sEntry;` |
|        - |  6722 | `	int i,j;` |
|        3 |  6723 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6724 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6725 | `		return PH7_OK;` |
|        - |  6726 | `	}` |
|        - |  6727 | `	/* Zero the Entry */` |
|        3 |  6728 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6729 | `	/* Initialize fields */` |
|        3 |  6730 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6731 | `	/* Save the callback name for later invocation name */` |
|        3 |  6732 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6733 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6734 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6735 | `	}` |
|        - |  6736 | `	/* Copy arguments */` |
|        3 |  6737 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6738 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6739 | `			/* Limit reached */` |
|      ! 0 |  6740 | `			break;` |
|        - |  6741 | `		}` |
|      ! 0 |  6742 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6743 | `	}` |
|        3 |  6744 | `	sEntry.nArg = j;` |
|        - |  6745 | `	/* Install the callback */` |
|        3 |  6746 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6747 | `	return PH7_OK;` |
|        2 |  6748 |  |
|        - |  6749 | `/*` |
|        - |  6750 | ` * Section:` |
|        - |  6751 | ` *  Class handling functions.` |
|        - |  6752 | ` * Status:` |
|        - |  6753 | ` *    Stable.` |
|        - |  6754 | ` */` |
|        - |  6755 | `/*` |
|        - |  6756 | ` * Extract the top active class. NULL is returned` |
|        - |  6757 | ` * if the class stack is empty.` |
|        - |  6758 | ` */` |
|      226 |  6759 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6760 |  |
|      228 |  6761 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6762 | `	ph7_class **apClass;` |
|      228 |  6763 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6764 | `		/* Empty stack,return NULL */` |
|       15 |  6765 | `		return 0;` |
|        - |  6766 | `	}` |
|        - |  6767 | `	/* Peek the last entry */` |
|      214 |  6768 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      214 |  6769 | `	return apClass[pSet->nUsed - 1];` |
|      115 |  6770 |  |
|        - |  6771 | `/*` |
|        - |  6772 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6773 | ` *   Get the class that declared the currently executing method.` |
|        - |  6774 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6775 | ` *` |
|        - |  6776 | ` * Parameters` |
|        - |  6777 | ` *   pVm: Target VM` |
|        - |  6778 | ` *` |
|        - |  6779 | ` * Return` |
|        - |  6780 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6781 | ` *   - Not executing within a class method` |
|        - |  6782 | ` *` |
|        - |  6783 | ` * Note` |
|        - |  6784 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6785 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6786 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6787 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6788 | ` *   declaring class.` |
|        - |  6789 | ` */` |
|       18 |  6790 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6791 |  |
|       19 |  6792 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6793 | `	ph7_vm_func *pVmFunc;` |
|        - |  6794 |  |
|        - |  6795 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6796 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6797 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6798 | `	}` |
|        - |  6799 |  |
|        - |  6800 | `	/* Check if we're in a method context */` |
|       19 |  6801 | `	if( pFrame->pParent ){` |
|       15 |  6802 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6803 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6804 | `			/* Return the declaring class */` |
|       15 |  6805 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6806 | `		}` |
|      ! 0 |  6807 | `	}` |
|        - |  6808 |  |
|        5 |  6809 | `	return 0;` |
|       10 |  6810 |  |
|        - |  6811 |  |
|        - |  6812 | `/*` |
|        - |  6813 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6814 | ` *   Returns the name of the class of an object` |
|        - |  6815 | ` * Parameters` |
|        - |  6816 | ` *  object` |
|        - |  6817 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6818 | ` * Return` |
|        - |  6819 | ` *  The name of the class of which object is an instance.` |
|        - |  6820 | ` *  Returns FALSE if object is not an object.` |
|        - |  6821 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6822 | ` */` |
|       18 |  6823 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6824 |  |
|        - |  6825 | `	ph7_class *pClass;` |
|        - |  6826 | `	SyString *pName;` |
|       20 |  6827 | `	if( nArg < 1 ){` |
|        - |  6828 | `		/* Check if we are inside a class */` |
|      ! 0 |  6829 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6830 | `		if( pClass ){` |
|        - |  6831 | `			/* Point to the class name */` |
|      ! 0 |  6832 | `			pName = &pClass->sName;` |
|      ! 0 |  6833 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6834 | `		}else{` |
|        - |  6835 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6836 | `			ph7_result_bool(pCtx,0);` |
|        - |  6837 | `		}` |
|      ! 0 |  6838 | `	}else{` |
|        - |  6839 | `		/* Extract the target class */` |
|       20 |  6840 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6841 | `		if( pClass ){` |
|       18 |  6842 | `			pName = &pClass->sName;` |
|        - |  6843 | `			/* Return the class name */` |
|       18 |  6844 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6845 | `		}else{` |
|        - |  6846 | `			/* Not a class instance,return FALSE */` |
|        3 |  6847 | `			ph7_result_bool(pCtx,0);` |
|        - |  6848 | `		}` |
|        - |  6849 | `	}` |
|       20 |  6850 | `	return PH7_OK;` |
|        2 |  6851 |  |
|        - |  6852 | `/*` |
|        - |  6853 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6854 | ` *   Returns the name of the parent class of an object` |
|        - |  6855 | ` * Parameters` |
|        - |  6856 | ` *  object` |
|        - |  6857 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6858 | ` * Return` |
|        - |  6859 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6860 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6861 | ` *  not have a parent.` |
|        - |  6862 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6863 | ` */` |
|        8 |  6864 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6865 |  |
|        - |  6866 | `	ph7_class *pClass;` |
|        - |  6867 | `	SyString *pName;` |
|        9 |  6868 | `	if( nArg < 1 ){` |
|        - |  6869 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6870 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6871 | `		if( pClass && pClass->pBase ){` |
|        - |  6872 | `			/* Point to the class name */` |
|        3 |  6873 | `			pName = &pClass->pBase->sName;` |
|        3 |  6874 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6875 | `		}else{` |
|        - |  6876 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6877 | `			ph7_result_bool(pCtx,0);` |
|        - |  6878 | `		}` |
|        2 |  6879 | `	}else{` |
|        - |  6880 | `		/* Extract the target class */` |
|        7 |  6881 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6882 | `		if( pClass ){` |
|        7 |  6883 | `			if( pClass->pBase ){` |
|        5 |  6884 | `				pName = &pClass->pBase->sName;` |
|        - |  6885 | `				/* Return the parent class name */` |
|        5 |  6886 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6887 | `			}else{` |
|        - |  6888 | `				/* Object does not have a parent class */` |
|        3 |  6889 | `				ph7_result_bool(pCtx,0);` |
|        - |  6890 | `			}` |
|        4 |  6891 | `		}else{` |
|        - |  6892 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6893 | `			ph7_result_bool(pCtx,0);` |
|        - |  6894 | `		}` |
|        - |  6895 | `	}` |
|        9 |  6896 | `	return PH7_OK;` |
|        1 |  6897 |  |
|        - |  6898 | `/*` |
|        - |  6899 | ` * string get_called_class(void)` |
|        - |  6900 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6901 | ` * Parameters` |
|        - |  6902 | ` *  None.` |
|        - |  6903 | ` * Return` |
|        - |  6904 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6905 | ` */` |
|        4 |  6906 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6907 |  |
|        - |  6908 | `	ph7_class *pClass;` |
|        - |  6909 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6910 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6911 | `	if( pClass ){` |
|        - |  6912 | `		SyString *pName;` |
|        - |  6913 | `		/* Point to the class name */` |
|        5 |  6914 | `		pName = &pClass->sName;` |
|        5 |  6915 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6916 | `	}else{` |
|      ! 0 |  6917 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6918 | `		SXUNUSED(apArg);` |
|        - |  6919 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6920 | `		ph7_result_bool(pCtx,0);` |
|        - |  6921 | `	}` |
|        5 |  6922 | `	return PH7_OK;` |
|        1 |  6923 |  |
|        - |  6924 | `/*` |
|        - |  6925 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6926 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6927 | ` * string which hold the class name.` |
|        - |  6928 | ` */` |
|       80 |  6929 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6930 |  |
|       82 |  6931 | `	ph7_class *pClass = 0;` |
|       82 |  6932 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6933 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6934 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       61 |  6935 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6936 | `		const char *zClass;` |
|        - |  6937 | `		int nLen;` |
|        - |  6938 | `		/* Extract class name */` |
|       38 |  6939 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       38 |  6940 | `		if( nLen > 0 ){` |
|        - |  6941 | `			SyHashEntry *pEntry;` |
|        - |  6942 | `			/* Perform a lookup */` |
|       38 |  6943 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       38 |  6944 | `			if( pEntry ){` |
|        - |  6945 | `				/* Point to the desired class */` |
|       31 |  6946 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6947 | `			}` |
|       18 |  6948 | `		}` |
|       18 |  6949 | `	}` |
|       82 |  6950 | `	return pClass;` |
|        2 |  6951 |  |
|        - |  6952 | `/*` |
|        - |  6953 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6954 | ` *   Checks if the object or class has a property.` |
|        - |  6955 | ` * Parameters` |
|        - |  6956 | ` *  class` |
|        - |  6957 | ` *   The class name or an object of the class to test for` |
|        - |  6958 | ` * property` |
|        - |  6959 | ` *  The name of the property` |
|        - |  6960 | ` * Return` |
|        - |  6961 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6962 | ` */` |
|       12 |  6963 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6964 |  |
|       13 |  6965 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6966 | `	if( nArg > 1 ){` |
|        - |  6967 | `		ph7_class *pClass;` |
|       13 |  6968 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6969 | `		if( pClass ){` |
|        - |  6970 | `			const char *zName;` |
|        - |  6971 | `			int nLen;` |
|        - |  6972 | `			/* Extract attribute name */` |
|       13 |  6973 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6974 | `			if( nLen > 0 ){` |
|        - |  6975 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6976 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6977 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6978 | `						/* property exists,flag that */` |
|       11 |  6979 | `						res = 1;` |
|        5 |  6980 | `				}` |
|        6 |  6981 | `			}` |
|        6 |  6982 | `		}` |
|        6 |  6983 | `	}` |
|       13 |  6984 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6985 | `	return PH7_OK;` |
|        1 |  6986 |  |
|        - |  6987 | `/*` |
|        - |  6988 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6989 | ` *   Checks if the given method is a class member.` |
|        - |  6990 | ` * Parameters` |
|        - |  6991 | ` *  class` |
|        - |  6992 | ` *   The class name or an object of the class to test for` |
|        - |  6993 | ` * property` |
|        - |  6994 | ` *  The name of the method` |
|        - |  6995 | ` * Return` |
|        - |  6996 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6997 | ` */` |
|        4 |  6998 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6999 |  |
|        5 |  7000 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  7001 | `	if( nArg > 1 ){` |
|        - |  7002 | `		ph7_class *pClass;` |
|        5 |  7003 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  7004 | `		if( pClass ){` |
|        - |  7005 | `			const char *zName;` |
|        - |  7006 | `			int nLen;` |
|        - |  7007 | `			/* Extract method name */` |
|        5 |  7008 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  7009 | `			if( nLen > 0 ){` |
|        - |  7010 | `				/* Perform the lookup in the method table */` |
|        5 |  7011 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7012 | `					/* method exists,flag that */` |
|        3 |  7013 | `					res = 1;` |
|        1 |  7014 | `				}` |
|        2 |  7015 | `			}` |
|        2 |  7016 | `		}` |
|        2 |  7017 | `	}` |
|        5 |  7018 | `	ph7_result_bool(pCtx,res);` |
|        5 |  7019 | `	return PH7_OK;` |
|        1 |  7020 |  |
|        - |  7021 | `/*` |
|        - |  7022 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7023 | ` *   Checks if the class has been defined.` |
|        - |  7024 | ` * Parameters` |
|        - |  7025 | ` *  class_name` |
|        - |  7026 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7027 | ` *   unlinke the standard PHP engine.` |
|        - |  7028 | ` *  autoload` |
|        - |  7029 | ` *   Whether or not to call __autoload by default.` |
|        - |  7030 | ` * Return` |
|        - |  7031 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7032 | ` */` |
|       12 |  7033 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7034 |  |
|       14 |  7035 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  7036 | `	if( nArg > 0 ){` |
|        - |  7037 | `		const char *zName;` |
|        - |  7038 | `		int nLen;` |
|        - |  7039 | `		/* Extract given name */` |
|       14 |  7040 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7041 | `		/* Perform a hashlookup */` |
|       14 |  7042 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7043 | `			/* class is available */` |
|       10 |  7044 | `			res = 1;` |
|        4 |  7045 | `		}` |
|        6 |  7046 | `	}` |
|       14 |  7047 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7048 | `	return PH7_OK;` |
|        2 |  7049 |  |
|        - |  7050 | `/*` |
|        - |  7051 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7052 | ` *   Checks if the interface has been defined.` |
|        - |  7053 | ` * Parameters` |
|        - |  7054 | ` *  class_name` |
|        - |  7055 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7056 | ` *   unlinke the standard PHP engine.` |
|        - |  7057 | ` *  autoload` |
|        - |  7058 | ` *   Whether or not to call __autoload by default.` |
|        - |  7059 | ` * Return` |
|        - |  7060 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7061 | ` */` |
|        6 |  7062 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7063 |  |
|        7 |  7064 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7065 | `	if( nArg > 0 ){` |
|        7 |  7066 | `		SyHashEntry *pEntry = 0;` |
|        - |  7067 | `		const char *zName;` |
|        - |  7068 | `		int nLen;` |
|        - |  7069 | `		/* Extract given name */` |
|        7 |  7070 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7071 | `		/* Perform a hashlookup */` |
|        7 |  7072 | `		if( nLen > 0 ){` |
|        7 |  7073 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7074 | `		}` |
|        7 |  7075 | `		if( pEntry ){` |
|        5 |  7076 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7077 | `			while( pClass ){` |
|        5 |  7078 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7079 | `					/* interface is available */` |
|        5 |  7080 | `					res = 1;` |
|        5 |  7081 | `					break;` |
|        - |  7082 | `				}` |
|        - |  7083 | `				/* Next with the same name */` |
|      ! 0 |  7084 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7085 | `			}` |
|        2 |  7086 | `		}` |
|        3 |  7087 | `	}` |
|        7 |  7088 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7089 | `	return PH7_OK;` |
|        1 |  7090 |  |
|        - |  7091 | `/*` |
|        - |  7092 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7093 | ` *   Creates an alias for a class.` |
|        - |  7094 | ` * Parameters` |
|        - |  7095 | ` *  original` |
|        - |  7096 | ` *    The original class.` |
|        - |  7097 | ` *  alias` |
|        - |  7098 | ` *   The alias name for the class.` |
|        - |  7099 | ` * Return` |
|        - |  7100 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7101 | ` */` |
|        2 |  7102 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7103 |  |
|        - |  7104 | `	const char *zOld,*zNew;` |
|        - |  7105 | `	int nOldLen,nNewLen;` |
|        - |  7106 | `	SyHashEntry *pEntry;` |
|        - |  7107 | `	ph7_class *pClass;` |
|        - |  7108 | `	char *zDup;` |
|        - |  7109 | `	sxi32 rc;` |
|        3 |  7110 | `	if( nArg < 2 ){` |
|        - |  7111 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7113 | `		return PH7_OK;` |
|        - |  7114 | `	}` |
|        - |  7115 | `	/* Extract old class name */` |
|        3 |  7116 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7117 | `	/* Extract alias name */` |
|        3 |  7118 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7119 | `	if( nNewLen < 1 ){` |
|        - |  7120 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7121 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7122 | `		return PH7_OK;` |
|        - |  7123 | `	}` |
|        - |  7124 | `	/* Perform a hash lookup */` |
|        3 |  7125 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7126 | `	if( pEntry ==  0 ){` |
|        - |  7127 | `		/* No such class,return FALSE */` |
|      ! 0 |  7128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7129 | `		return PH7_OK;` |
|        - |  7130 | `	}` |
|        - |  7131 | `	/* Point to the class */` |
|        3 |  7132 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7133 | `	/* Duplicate alias name */` |
|        3 |  7134 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7135 | `	if( zDup == 0 ){` |
|        - |  7136 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7137 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7138 | `		return PH7_OK;` |
|        - |  7139 | `	}` |
|        - |  7140 | `	/* Create the alias */` |
|        3 |  7141 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7142 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7143 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7144 | `	}` |
|        3 |  7145 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7146 | `	return PH7_OK;` |
|        2 |  7147 |  |
|        - |  7148 | `/*` |
|        - |  7149 | ` * array get_declared_classes(void)` |
|        - |  7150 | ` *   Returns an array with the name of the defined classes` |
|        - |  7151 | ` * Parameters` |
|        - |  7152 | ` *  None` |
|        - |  7153 | ` * Return` |
|        - |  7154 | ` *   Returns an array of the names of the declared classes` |
|        - |  7155 | ` *   in the current script.` |
|        - |  7156 | ` * Note:` |
|        - |  7157 | ` *   NULL is returned on failure.` |
|        - |  7158 | ` */` |
|        2 |  7159 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7160 |  |
|        - |  7161 | `	ph7_value *pName,*pArray;` |
|        - |  7162 | `	SyHashEntry *pEntry;` |
|        - |  7163 | `	/* Create a new array first */` |
|        3 |  7164 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7165 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7166 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7167 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7168 | `		SXUNUSED(apArg);` |
|        - |  7169 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7170 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7171 | `		return PH7_OK;` |
|        - |  7172 | `	}` |
|        - |  7173 | `	/* Fill the array with the defined classes */` |
|        3 |  7174 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7175 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7176 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7177 | `		/* Do not register classes defined as interfaces */` |
|       49 |  7178 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       43 |  7179 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7180 | `			/* insert class name */` |
|       43 |  7181 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7182 | `			/* Reset the cursor */` |
|       43 |  7183 | `			ph7_value_reset_string_cursor(pName);` |
|       21 |  7184 | `		}` |
|        1 |  7185 | `	}` |
|        - |  7186 | `	/* Return the created array */` |
|        3 |  7187 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7188 | `	return PH7_OK;` |
|        2 |  7189 |  |
|        - |  7190 | `/*` |
|        - |  7191 | ` * array get_declared_interfaces(void)` |
|        - |  7192 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7193 | ` * Parameters` |
|        - |  7194 | ` *  None` |
|        - |  7195 | ` * Return` |
|        - |  7196 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7197 | ` *   in the current script.` |
|        - |  7198 | ` * Note:` |
|        - |  7199 | ` *   NULL is returned on failure.` |
|        - |  7200 | ` */` |
|        2 |  7201 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7202 |  |
|        - |  7203 | `	ph7_value *pName,*pArray;` |
|        - |  7204 | `	SyHashEntry *pEntry;` |
|        - |  7205 | `	/* Create a new array first */` |
|        3 |  7206 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7207 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7208 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7209 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7210 | `		SXUNUSED(apArg);` |
|        - |  7211 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7212 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7213 | `		return PH7_OK;` |
|        - |  7214 | `	}` |
|        - |  7215 | `	/* Fill the array with the defined classes */` |
|        3 |  7216 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       54 |  7217 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       51 |  7218 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7219 | `		/* Register classes defined as interfaces only */` |
|       51 |  7220 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7221 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7222 | `			/* insert interface name */` |
|        9 |  7223 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7224 | `			/* Reset the cursor */` |
|        9 |  7225 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7226 | `		}` |
|        1 |  7227 | `	}` |
|        - |  7228 | `	/* Return the created array */` |
|        3 |  7229 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7230 | `	return PH7_OK;` |
|        2 |  7231 |  |
|        - |  7232 | `/*` |
|        - |  7233 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7234 | ` *   Returns an array with the name of the class methods` |
|        - |  7235 | ` * Parameters` |
|        - |  7236 | ` *  class_name` |
|        - |  7237 | ` *  The class name or class instance` |
|        - |  7238 | ` * Return` |
|        - |  7239 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7240 | ` *  In case of an error, it returns NULL.` |
|        - |  7241 | ` * Note:` |
|        - |  7242 | ` *   NULL is returned on failure.` |
|        - |  7243 | ` */` |
|        6 |  7244 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7245 |  |
|        - |  7246 | `	ph7_value *pName,*pArray;` |
|        - |  7247 | `	SyHashEntry *pEntry;` |
|        - |  7248 | `	ph7_class *pClass;` |
|        - |  7249 | `	/* Extract the target class first */` |
|        7 |  7250 | `	pClass = 0;` |
|        7 |  7251 | `	if( nArg > 0 ){` |
|        7 |  7252 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7253 | `	}` |
|        7 |  7254 | `	if( pClass == 0 ){` |
|        - |  7255 | `		/* No such class,return NULL */` |
|        3 |  7256 | `		ph7_result_null(pCtx);` |
|        3 |  7257 | `		return PH7_OK;` |
|        - |  7258 | `	}` |
|        - |  7259 | `	/* Create a new array  */` |
|        5 |  7260 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7261 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7262 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7263 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7264 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7265 | `		return PH7_OK;` |
|        - |  7266 | `	}` |
|        - |  7267 | `	/* Fill the array with the defined methods */` |
|        5 |  7268 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7269 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7270 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7271 | `		/* Insert method name */` |
|       13 |  7272 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7273 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7274 | `		/* Reset the cursor */` |
|       13 |  7275 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7276 | `	}` |
|        - |  7277 | `	/* Return the created array */` |
|        5 |  7278 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7279 | `	/*` |
|        - |  7280 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7281 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7282 | `	 */` |
|        5 |  7283 | `	return PH7_OK;` |
|        4 |  7284 |  |
|        - |  7285 | `/*` |
|        - |  7286 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7287 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7288 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7289 | ` */` |
|     1796 |  7290 | `static int VmClassMemberAccess(` |
|        - |  7291 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7292 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7293 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7294 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7295 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7296 | `	)` |
|        2 |  7297 |  |
|     1798 |  7298 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     1234 |  7299 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7300 | `		ph7_vm_func *pVmFunc;` |
|     1238 |  7301 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7302 | `			/* Safely ignore the exception frame */` |
|        5 |  7303 | `			pFrame = pFrame->pParent;` |
|        1 |  7304 | `		}` |
|     1234 |  7305 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1234 |  7306 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7307 | `			goto dis; /* Access is forbidden */` |
|        - |  7308 | `		}` |
|     1226 |  7309 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7310 | `			/* Must be the same instance */` |
|        7 |  7311 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7312 | `				goto dis; /* Access is forbidden */` |
|        - |  7313 | `			}` |
|        4 |  7314 | `		}else{` |
|        - |  7315 | `			/* Protected */` |
|     1220 |  7316 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7317 | `			/* Must be a derived class */` |
|     1220 |  7318 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7319 | `				goto dis; /* Access is forbidden */` |
|        - |  7320 | `			}` |
|        - |  7321 | `		}` |
|      612 |  7322 | `	}` |
|     1790 |  7323 | `	return 1; /* Access is granted */` |
|        4 |  7324 | `dis:` |
|        9 |  7325 | `	if( bLog ){` |
|      ! 0 |  7326 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7327 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7328 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7329 | `	}` |
|        9 |  7330 | `	return 0; /* Access is forbidden */` |
|      900 |  7331 |  |
|        - |  7332 | `/*` |
|        - |  7333 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7334 | ` *   Get the default properties of the class` |
|        - |  7335 | ` * Parameters` |
|        - |  7336 | ` *  class_name` |
|        - |  7337 | ` *   The class name or class instance` |
|        - |  7338 | ` * Return` |
|        - |  7339 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7340 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7341 | ` *  of varname => value.` |
|        - |  7342 | ` * Note:` |
|        - |  7343 | ` *   NULL is returned on failure.` |
|        - |  7344 | ` */` |
|        2 |  7345 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7346 |  |
|        - |  7347 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7348 | `	SyHashEntry *pEntry;` |
|        - |  7349 | `	ph7_class *pClass;` |
|        - |  7350 | `	/* Extract the target class first */` |
|        3 |  7351 | `	pClass = 0;` |
|        3 |  7352 | `	if( nArg > 0 ){` |
|        3 |  7353 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7354 | `	}` |
|        3 |  7355 | `	if( pClass == 0 ){` |
|        - |  7356 | `		/* No such class,return NULL */` |
|      ! 0 |  7357 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7358 | `		return PH7_OK;` |
|        - |  7359 | `	}` |
|        - |  7360 | `	/* Create a new array  */` |
|        3 |  7361 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7362 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7363 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7364 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7365 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7366 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7367 | `		return PH7_OK;` |
|        - |  7368 | `	}` |
|        - |  7369 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7370 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7371 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7372 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7373 | `		/* Check if the access is allowed */` |
|        5 |  7374 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7375 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7376 | `			ph7_value *pValue = 0;` |
|        5 |  7377 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7378 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7379 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7380 | `			}else{` |
|      ! 0 |  7381 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7382 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7383 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7384 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7385 | `					pValue = &sValue;` |
|      ! 0 |  7386 | `				}` |
|        - |  7387 | `			}` |
|        - |  7388 | `			/* Fill in the array */` |
|        5 |  7389 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7390 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7391 | `			/* Reset the cursor */` |
|        5 |  7392 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7393 | `		}` |
|        1 |  7394 | `	}` |
|        3 |  7395 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7396 | `	/* Return the created array */` |
|        3 |  7397 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7398 | `	/*` |
|        - |  7399 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7400 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7401 | `	 */` |
|        3 |  7402 | `	return PH7_OK;` |
|        2 |  7403 |  |
|        - |  7404 | `/*` |
|        - |  7405 | ` * array get_object_vars(object $this)` |
|        - |  7406 | ` *   Gets the properties of the given object` |
|        - |  7407 | ` * Parameters` |
|        - |  7408 | ` *  this` |
|        - |  7409 | ` *   A class instance` |
|        - |  7410 | ` * Return` |
|        - |  7411 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7412 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7413 | ` *  it will be returned with a NULL value.` |
|        - |  7414 | ` * Note:` |
|        - |  7415 | ` *   NULL is returned on failure.` |
|        - |  7416 | ` */` |
|        2 |  7417 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7418 |  |
|        3 |  7419 | `	ph7_class_instance *pThis = 0;` |
|        - |  7420 | `	ph7_value *pName,*pArray;` |
|        - |  7421 | `	SyHashEntry *pEntry;` |
|        3 |  7422 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7423 | `		/* Extract the target instance */` |
|        3 |  7424 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7425 | `	}` |
|        3 |  7426 | `	if( pThis == 0 ){` |
|        - |  7427 | `		/* No such instance,return NULL */` |
|      ! 0 |  7428 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7429 | `		return PH7_OK;` |
|        - |  7430 | `	}` |
|        - |  7431 | `	/* Create a new array  */` |
|        3 |  7432 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7433 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7434 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7435 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7436 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7437 | `		return PH7_OK;` |
|        - |  7438 | `	}` |
|        - |  7439 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7440 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7441 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7442 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7443 | `		SyString *pAttrName;` |
|        7 |  7444 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7445 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7446 | `			continue;` |
|        - |  7447 | `		}` |
|        7 |  7448 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7449 | `		/* Check if the access is allowed */` |
|        7 |  7450 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7451 | `			ph7_value *pValue = 0;` |
|        - |  7452 | `			/* Extract attribute */` |
|        3 |  7453 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7454 | `			if( pValue ){` |
|        - |  7455 | `				/* Insert attribute name in the array */` |
|        3 |  7456 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7457 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7458 | `			}` |
|        - |  7459 | `			/* Reset the cursor */` |
|        3 |  7460 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7461 | `		}` |
|        1 |  7462 | `	}` |
|        - |  7463 | `	/* Return the created array */` |
|        3 |  7464 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7465 | `	/*` |
|        - |  7466 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7467 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7468 | `	 */` |
|        3 |  7469 | `	return PH7_OK;` |
|        2 |  7470 |  |
|        - |  7471 | `/*` |
|        - |  7472 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7473 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7474 | ` */` |
|     2618 |  7475 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7476 |  |
|        - |  7477 | `	ph7_class **apInterface;` |
|        - |  7478 | `	sxu32 n;` |
|     2620 |  7479 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7480 | `		/* Empty interface container */` |
|     2618 |  7481 | `		return FALSE;` |
|        - |  7482 | `	}` |
|        - |  7483 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7484 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7485 | `	/* Perform the lookup */` |
|        3 |  7486 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7487 | `		if( apInterface[n] == pClass ){` |
|        3 |  7488 | `			return TRUE;` |
|        - |  7489 | `		}` |
|      ! 0 |  7490 | `	}` |
|      ! 0 |  7491 | `	return FALSE;` |
|     1311 |  7492 |  |
|        - |  7493 | `/*` |
|        - |  7494 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7495 | ` * is an instance of the main class (second argument).` |
|        - |  7496 | ` * Otherwise FALSE is returned.` |
|        - |  7497 | ` */` |
|     1270 |  7498 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7499 |  |
|        - |  7500 | `	ph7_class *pParent;` |
|        - |  7501 | `	sxi32 rc;` |
|     1272 |  7502 | `	if( pThis == pClass ){` |
|        - |  7503 | `		/* Instance of the same class */` |
|      140 |  7504 | `		return TRUE;` |
|        - |  7505 | `	}` |
|        - |  7506 | `	/* Check implemented interfaces */` |
|     1134 |  7507 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|     1134 |  7508 | `	if( rc ){` |
|        3 |  7509 | `		return TRUE;` |
|        - |  7510 | `	}` |
|        - |  7511 | `	/* Check parent classes */` |
|     1132 |  7512 | `	pParent = pThis->pBase;` |
|     2618 |  7513 | `	while( pParent ){` |
|     2616 |  7514 | `		if( pParent == pClass ){` |
|        - |  7515 | `			/* Same instance */` |
|     1130 |  7516 | `			return TRUE;` |
|        - |  7517 | `		}` |
|        - |  7518 | `		/* Check the implemented interfaces */` |
|     1488 |  7519 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|     1488 |  7520 | `		if( rc ){` |
|      ! 0 |  7521 | `			return TRUE;` |
|        - |  7522 | `		}` |
|        - |  7523 | `		/* Point to the parent class */` |
|     1488 |  7524 | `		pParent = pParent->pBase;` |
|        2 |  7525 | `	}` |
|        - |  7526 | `	/* Not an instance of the the given class */` |
|        3 |  7527 | `	return FALSE;` |
|      637 |  7528 |  |
|        - |  7529 | `/*` |
|        - |  7530 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7531 | ` * is a subclass of the main class (second argument).` |
|        - |  7532 | ` * Otherwise FALSE is returned.` |
|        - |  7533 | ` */` |
|        4 |  7534 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7535 |  |
|        5 |  7536 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7537 | `	SyHashEntry *pEntry;` |
|        - |  7538 | `	SyString *pName;` |
|        - |  7539 | `	sxi32 rc;` |
|        5 |  7540 | `	while( pClass ){` |
|        5 |  7541 | `		pName = &pClass->sName;` |
|        - |  7542 | `		/* Query the derived hashtable */` |
|        5 |  7543 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7544 | `		if( pEntry ){` |
|        5 |  7545 | `			return TRUE;` |
|        - |  7546 | `		}` |
|      ! 0 |  7547 | `		pClass = pClass->pBase;` |
|      ! 0 |  7548 | `	}` |
|      ! 0 |  7549 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7550 | `	if( rc ){` |
|      ! 0 |  7551 | `		return TRUE;` |
|        - |  7552 | `	}` |
|        - |  7553 | `	/* Not a subclass */` |
|      ! 0 |  7554 | `	return FALSE;` |
|        3 |  7555 |  |
|        - |  7556 | `/*` |
|        - |  7557 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7558 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7559 | ` * Parameters` |
|        - |  7560 | ` *  object` |
|        - |  7561 | ` *   The tested object` |
|        - |  7562 | ` * class_name` |
|        - |  7563 | ` *  The class name` |
|        - |  7564 | ` * Return` |
|        - |  7565 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7566 | ` *   parents, FALSE otherwise.` |
|        - |  7567 | ` */` |
|        2 |  7568 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7569 |  |
|        3 |  7570 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7571 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7572 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7573 | `		ph7_class *pClass;` |
|        - |  7574 | `		/* Extract the given class */` |
|        3 |  7575 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7576 | `		if( pClass ){` |
|        - |  7577 | `			/* Perform the query */` |
|        3 |  7578 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7579 | `		}` |
|        1 |  7580 | `	}` |
|        - |  7581 | `	/* Query result */` |
|        3 |  7582 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7583 | `	return PH7_OK;` |
|        1 |  7584 |  |
|        - |  7585 | `/*` |
|        - |  7586 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7587 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7588 | ` * Parameters` |
|        - |  7589 | ` *  object` |
|        - |  7590 | ` *   The tested object` |
|        - |  7591 | ` * class_name` |
|        - |  7592 | ` *  The class name` |
|        - |  7593 | ` * Return` |
|        - |  7594 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7595 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7596 | ` */` |
|        6 |  7597 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7598 |  |
|        7 |  7599 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7600 | `	if( nArg > 1 ){` |
|        - |  7601 | `		ph7_class *pClass,*pMain;` |
|        - |  7602 | `		/* Extract the given classes */` |
|        7 |  7603 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7604 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7605 | `		if( pClass && pMain ){` |
|        - |  7606 | `			/* Perform the query */` |
|        5 |  7607 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7608 | `		}` |
|        3 |  7609 | `	}` |
|        - |  7610 | `	/* Query result */` |
|        7 |  7611 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7612 | `	return PH7_OK;` |
|        1 |  7613 |  |
|        - |  7614 | `/*` |
|        - |  7615 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7616 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7617 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7618 | ` * return value indicates failure.` |
|        - |  7619 | ` */` |
|      570 |  7620 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7621 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7622 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7623 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7624 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7625 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7626 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7627 | `	)` |
|        2 |  7628 |  |
|        - |  7629 | `	ph7_value *aStack;` |
|        - |  7630 | `	VmInstr aInstr[2];` |
|        - |  7631 | `	int iCursor;` |
|        - |  7632 | `	int i;` |
|        - |  7633 | `	/* Create a new operand stack */` |
|      572 |  7634 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      572 |  7635 | `	if( aStack == 0 ){` |
|      ! 0 |  7636 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7637 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7638 | `		return SXERR_MEM;` |
|        - |  7639 | `	}` |
|        - |  7640 | `	/* Fill the operand stack with the given arguments */` |
|      828 |  7641 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      258 |  7642 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7643 | `		/*` |
|        - |  7644 | `		 * Symisc eXtension:` |
|        - |  7645 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7646 | `		 */` |
|      258 |  7647 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      130 |  7648 | `	}` |
|      572 |  7649 | `	iCursor = nArg + 1;` |
|      572 |  7650 | `	if( pThis ){` |
|        - |  7651 | `		/*` |
|        - |  7652 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7653 | `		 */` |
|      566 |  7654 | `		pThis->iRef++; /* Increment reference count */` |
|      566 |  7655 | `		aStack[i].x.pOther = pThis;` |
|      566 |  7656 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      282 |  7657 | `	}` |
|      572 |  7658 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      572 |  7659 | `	i++;` |
|        - |  7660 | `	/* Push method name */` |
|      572 |  7661 | `	SyBlobReset(&aStack[i].sBlob);` |
|      572 |  7662 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      572 |  7663 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      572 |  7664 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7665 | `	/* Emit the CALL istruction */` |
|      572 |  7666 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      572 |  7667 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      572 |  7668 | `	aInstr[0].iP2 = 0;` |
|      572 |  7669 | `	aInstr[0].p3  = 0;` |
|        - |  7670 | `	/* Emit the DONE instruction */` |
|      572 |  7671 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      572 |  7672 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      572 |  7673 | `	aInstr[1].iP2 = 0;` |
|      572 |  7674 | `	aInstr[1].p3  = 0;` |
|        - |  7675 | `	/* Execute the method body (if available) */` |
|      572 |  7676 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7677 | `	/* Clean up the mess left behind */` |
|      572 |  7678 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      572 |  7679 | `	return PH7_OK;` |
|      287 |  7680 |  |
|        - |  7681 | `/*` |
|        - |  7682 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7683 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7684 | ` * in the apArg[] array.` |
|        - |  7685 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7686 | ` * return value indicates failure.` |
|        - |  7687 | ` */` |
|      550 |  7688 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7689 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7690 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7691 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7692 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7693 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7694 | `	)` |
|        2 |  7695 |  |
|        - |  7696 | `	ph7_value *aStack;` |
|        - |  7697 | `	VmInstr aInstr[2];` |
|        - |  7698 | `	int i;` |
|      552 |  7699 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7700 | `		/* Don't bother processing,it's invalid anyway */` |
|      185 |  7701 | `		if( pResult ){` |
|        - |  7702 | `			/* Assume a null return value */` |
|      ! 0 |  7703 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7704 | `		}` |
|      185 |  7705 | `		return SXERR_INVALID;` |
|        - |  7706 | `	}` |
|      368 |  7707 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7708 | `		/* Class method */` |
|       11 |  7709 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7710 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7711 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7712 | `		ph7_class *pClass = 0;` |
|        - |  7713 | `		ph7_value *pValue;` |
|        - |  7714 | `		sxi32 rc;` |
|       11 |  7715 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7716 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7717 | `			if( pResult ){` |
|        - |  7718 | `				/* Assume a null return value */` |
|      ! 0 |  7719 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7720 | `			}` |
|      ! 0 |  7721 | `			return SXRET_OK;` |
|        - |  7722 | `		}` |
|        - |  7723 | `		/* Extract the class name or an instance of it */` |
|       11 |  7724 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7725 | `		if( pValue ){` |
|       11 |  7726 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7727 | `		}` |
|       11 |  7728 | `		if( pClass == 0 ){` |
|        - |  7729 | `			/* No such class,return NULL */` |
|      ! 0 |  7730 | `			if( pResult ){` |
|      ! 0 |  7731 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7732 | `			}` |
|      ! 0 |  7733 | `			return SXRET_OK;` |
|        - |  7734 | `		}` |
|       11 |  7735 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7736 | `			/* Point to the class instance */` |
|        5 |  7737 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7738 | `		}` |
|        - |  7739 | `		/* Try to extract the method */` |
|       11 |  7740 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7741 | `		if( pValue ){` |
|       11 |  7742 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7743 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7744 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7745 | `			}` |
|        5 |  7746 | `		}` |
|       11 |  7747 | `		if( pMethod == 0 ){` |
|        - |  7748 | `			/* No such method,return NULL */` |
|      ! 0 |  7749 | `			if( pResult ){` |
|      ! 0 |  7750 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7751 | `			}` |
|      ! 0 |  7752 | `			return SXRET_OK;` |
|        - |  7753 | `		}` |
|        - |  7754 | `		/* Call the class method */` |
|       11 |  7755 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7756 | `		return rc;` |
|        - |  7757 | `	}` |
|        - |  7758 | `	/* Create a new operand stack */` |
|      358 |  7759 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      358 |  7760 | `	if( aStack == 0 ){` |
|      ! 0 |  7761 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7762 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7763 | `		if( pResult ){` |
|        - |  7764 | `			/* Assume a null return value */` |
|      ! 0 |  7765 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7766 | `		}` |
|      ! 0 |  7767 | `		return SXERR_MEM;` |
|        - |  7768 | `	}` |
|        - |  7769 | `	/* Fill the operand stack with the given arguments */` |
|     1160 |  7770 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      804 |  7771 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7772 | `		/*` |
|        - |  7773 | `		 * Symisc eXtension:` |
|        - |  7774 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7775 | `		 */` |
|      804 |  7776 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      403 |  7777 | `	}` |
|        - |  7778 | `	/* Push the function name */` |
|      358 |  7779 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      358 |  7780 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7781 | `	/* Emit the CALL istruction */` |
|      358 |  7782 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      358 |  7783 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      358 |  7784 | `	aInstr[0].iP2 = 0;` |
|      358 |  7785 | `	aInstr[0].p3  = 0;` |
|        - |  7786 | `	/* Emit the DONE instruction */` |
|      358 |  7787 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      358 |  7788 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      358 |  7789 | `	aInstr[1].iP2 = 0;` |
|      358 |  7790 | `	aInstr[1].p3  = 0;` |
|        - |  7791 | `	/* Execute the function body (if available) */` |
|      358 |  7792 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7793 | `	/* Clean up the mess left behind */` |
|      358 |  7794 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      358 |  7795 | `	return PH7_OK;` |
|      277 |  7796 |  |
|        - |  7797 | `/*` |
|        - |  7798 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7799 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7800 | ` * parameter.` |
|        - |  7801 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7802 | ` * return value indicates failure.` |
|        - |  7803 | ` */` |
|      190 |  7804 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7805 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7806 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7807 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7808 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7809 | `	)` |
|        1 |  7810 |  |
|        - |  7811 | `	ph7_value *pArg;` |
|        - |  7812 | `	SySet aArg;` |
|        - |  7813 | `	va_list ap;` |
|        - |  7814 | `	sxi32 rc;` |
|      191 |  7815 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7816 | `	/* Copy arguments one after one */` |
|      191 |  7817 | `	va_start(ap,pResult);` |
|      319 |  7818 | `	for(;;){` |
|      639 |  7819 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7820 | `		if( pArg == 0 ){` |
|      191 |  7821 | `			break;` |
|        - |  7822 | `		}` |
|      449 |  7823 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7824 | `	}` |
|        - |  7825 | `	/* Call the core routine */` |
|      191 |  7826 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7827 | `	/* Cleanup */` |
|      191 |  7828 | `	SySetRelease(&aArg);` |
|      191 |  7829 | `	return rc;` |
|        1 |  7830 |  |
|        - |  7831 | `/*` |
|        - |  7832 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7833 | ` *  Call the callback given by the first parameter.` |
|        - |  7834 | ` * Parameter` |
|        - |  7835 | ` *  $callback` |
|        - |  7836 | ` *   The callable to be called.` |
|        - |  7837 | ` *  ...` |
|        - |  7838 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7839 | ` * Return` |
|        - |  7840 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7841 | ` */` |
|       14 |  7842 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7843 |  |
|        - |  7844 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7845 | `	sxi32 rc;` |
|       15 |  7846 | `	if( nArg < 1 ){` |
|        - |  7847 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7848 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7849 | `		return PH7_OK;` |
|        - |  7850 | `	}` |
|       15 |  7851 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7852 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7853 | `	/* Try to invoke the callback */` |
|       15 |  7854 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7855 | `	if( rc != SXRET_OK ){` |
|        - |  7856 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7857 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7858 | `	}else{` |
|        - |  7859 | `		/* Callback result */` |
|       15 |  7860 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7861 | `	}` |
|       15 |  7862 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7863 | `	return PH7_OK;` |
|        8 |  7864 |  |
|        - |  7865 | `/*` |
|        - |  7866 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7867 | ` *  Call a callback with an array of parameters.` |
|        - |  7868 | ` * Parameter` |
|        - |  7869 | ` *  $callback` |
|        - |  7870 | ` *   The callable to be called.` |
|        - |  7871 | ` * $param_arr` |
|        - |  7872 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7873 | ` * Return` |
|        - |  7874 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7875 | ` */` |
|       10 |  7876 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7877 |  |
|        - |  7878 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7879 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7880 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7881 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7882 | `	sxi32 rc;` |
|        - |  7883 | `	sxu32 n;` |
|       11 |  7884 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7885 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7886 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7887 | `		return PH7_OK;` |
|        - |  7888 | `	}` |
|       11 |  7889 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7890 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7891 | `	/* Initialize the arguments container */` |
|       11 |  7892 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7893 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7894 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7895 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7896 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7897 | `		/* Extract node value */` |
|       13 |  7898 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7899 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7900 | `		}` |
|        - |  7901 | `		/* Point to the next entry */` |
|       13 |  7902 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7903 | `	}` |
|        - |  7904 | `	/* Try to invoke the callback */` |
|       11 |  7905 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7906 | `	if( rc != SXRET_OK ){` |
|        - |  7907 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7908 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7909 | `	}else{` |
|        - |  7910 | `		/* Callback result */` |
|       11 |  7911 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7912 | `	}` |
|        - |  7913 | `	/* Cleanup the mess left behind */` |
|       11 |  7914 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7915 | `	SySetRelease(&aArg);` |
|       11 |  7916 | `	return PH7_OK;` |
|        6 |  7917 |  |
|        - |  7918 | `/*` |
|        - |  7919 | ` * bool defined(string $name)` |
|        - |  7920 | ` *  Checks whether a given named constant exists.` |
|        - |  7921 | ` * Parameter:` |
|        - |  7922 | ` *  Name of the desired constant.` |
|        - |  7923 | ` * Return` |
|        - |  7924 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7925 | ` */` |
|       14 |  7926 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7927 |  |
|        - |  7928 | `	const char *zName;` |
|       16 |  7929 | `	int nLen = 0;` |
|       16 |  7930 | `	int res = 0;` |
|       16 |  7931 | `	if( nArg < 1 ){` |
|        - |  7932 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7933 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7934 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7935 | `		return SXRET_OK;` |
|        - |  7936 | `	}` |
|        - |  7937 | `	/* Extract constant name */` |
|       16 |  7938 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7939 | `	/* Perform the lookup */` |
|       16 |  7940 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7941 | `		/* Already defined */` |
|       10 |  7942 | `		res = 1;` |
|        4 |  7943 | `	}` |
|       16 |  7944 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7945 | `	return SXRET_OK;` |
|        9 |  7946 |  |
|        - |  7947 | `/*` |
|        - |  7948 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7949 | ` * below.` |
|        - |  7950 | ` */` |
|        8 |  7951 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7952 |  |
|       10 |  7953 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7954 | `	/* Expand constant value */` |
|       10 |  7955 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7956 |  |
|        - |  7957 | `/*` |
|        - |  7958 | ` * bool define(string $constant_name,expression value)` |
|        - |  7959 | ` *  Defines a named constant at runtime.` |
|        - |  7960 | ` * Parameter:` |
|        - |  7961 | ` *  $constant_name` |
|        - |  7962 | ` *   The name of the constant` |
|        - |  7963 | ` *  $value` |
|        - |  7964 | ` *   Constant value` |
|        - |  7965 | ` * Return:` |
|        - |  7966 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7967 | ` */` |
|       10 |  7968 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7969 |  |
|        - |  7970 | `	const char *zName;  /* Constant name */` |
|        - |  7971 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7972 | `	int nLen = 0;       /* Name length */` |
|        - |  7973 | `	sxi32 rc;` |
|       12 |  7974 | `	if( nArg < 2 ){` |
|        - |  7975 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7976 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7977 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7978 | `		return SXRET_OK;` |
|        - |  7979 | `	}` |
|       12 |  7980 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7981 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7982 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7983 | `		return SXRET_OK;` |
|        - |  7984 | `	}` |
|        - |  7985 | `	/* Extract constant name */` |
|       12 |  7986 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7987 | `	if( nLen < 1 ){` |
|      ! 0 |  7988 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7989 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7990 | `		return SXRET_OK;` |
|        - |  7991 | `	}` |
|        - |  7992 | `	/* Duplicate constant value */` |
|       12 |  7993 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7994 | `	if( pValue == 0 ){` |
|      ! 0 |  7995 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7996 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7997 | `		return SXRET_OK;` |
|        - |  7998 | `	}` |
|        - |  7999 | `	/* Initialize the memory object */` |
|       12 |  8000 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8001 | `	/* Register the constant */` |
|       12 |  8002 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8003 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8004 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8005 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8006 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8007 | `		return SXRET_OK;` |
|        - |  8008 | `	}` |
|        - |  8009 | `	/* Duplicate constant value */` |
|       12 |  8010 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8011 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8012 | `		/* Lower case the constant name */` |
|      ! 0 |  8013 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8014 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8015 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8016 | `				/* UTF-8 stream */` |
|      ! 0 |  8017 | `				zCur++;` |
|      ! 0 |  8018 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8019 | `					zCur++;` |
|      ! 0 |  8020 | `				}` |
|      ! 0 |  8021 | `				continue;` |
|        - |  8022 | `			}` |
|      ! 0 |  8023 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8024 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8025 | `				zCur[0] = (char)c;` |
|      ! 0 |  8026 | `			}` |
|      ! 0 |  8027 | `			zCur++;` |
|      ! 0 |  8028 | `		}` |
|        - |  8029 | `		/* Finally,register the constant */` |
|      ! 0 |  8030 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8031 | `	}` |
|        - |  8032 | `	/* All done,return TRUE */` |
|       12 |  8033 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8034 | `	return SXRET_OK;` |
|        7 |  8035 |  |
|        - |  8036 | `/*` |
|        - |  8037 | ` * value constant(string $name)` |
|        - |  8038 | ` *  Returns the value of a constant` |
|        - |  8039 | ` * Parameter` |
|        - |  8040 | ` *  $name` |
|        - |  8041 | ` *    Name of the constant.` |
|        - |  8042 | ` * Return` |
|        - |  8043 | ` *  Constant value or NULL if not defined.` |
|        - |  8044 | ` */` |
|        8 |  8045 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8046 |  |
|        - |  8047 | `	SyHashEntry *pEntry;` |
|        - |  8048 | `	ph7_constant *pCons;` |
|        - |  8049 | `	const char *zName; /* Constant name */` |
|        - |  8050 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8051 | `	int nLen;` |
|       10 |  8052 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8053 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8054 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8055 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8056 | `		return SXRET_OK;` |
|        - |  8057 | `	}` |
|        - |  8058 | `	/* Extract the constant name */` |
|       10 |  8059 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8060 | `	/* Perform the query */` |
|       10 |  8061 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8062 | `	if( pEntry == 0 ){` |
|        3 |  8063 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8064 | `		ph7_result_null(pCtx);` |
|        3 |  8065 | `		return SXRET_OK;` |
|        - |  8066 | `	}` |
|        8 |  8067 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8068 | `	/* Point to the structure that describe the constant */` |
|        8 |  8069 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8070 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8071 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8072 | `	/* Return that value */` |
|        8 |  8073 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8074 | `	/* Cleanup */` |
|        8 |  8075 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8076 | `	return SXRET_OK;` |
|        6 |  8077 |  |
|        - |  8078 | `/*` |
|        - |  8079 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8080 | ` * defined below.` |
|        - |  8081 | ` */` |
|      414 |  8082 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8083 |  |
|      415 |  8084 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8085 | `	ph7_value sName;` |
|        - |  8086 | `	sxi32 rc;` |
|        - |  8087 | `	/* Prepare the constant name for insertion */` |
|      415 |  8088 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8089 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8090 | `	/* Perform the insertion */` |
|      415 |  8091 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8092 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8093 | `	return rc;` |
|        1 |  8094 |  |
|        - |  8095 | `/*` |
|        - |  8096 | ` * array get_defined_constants(void)` |
|        - |  8097 | ` *  Returns an associative array with the names of all defined` |
|        - |  8098 | ` *  constants.` |
|        - |  8099 | ` * Parameters` |
|        - |  8100 | ` *  NONE.` |
|        - |  8101 | ` * Returns` |
|        - |  8102 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8103 | ` */` |
|        2 |  8104 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8105 |  |
|        - |  8106 | `	ph7_value *pArray;` |
|        - |  8107 | `	/* Create the array first*/` |
|        3 |  8108 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8109 | `	if( pArray == 0 ){` |
|      ! 0 |  8110 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8111 | `		SXUNUSED(apArg);` |
|        - |  8112 | `		/* Return NULL */` |
|      ! 0 |  8113 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8114 | `		return SXRET_OK;` |
|        - |  8115 | `	}` |
|        - |  8116 | `	/* Fill the array with the defined constants */` |
|        3 |  8117 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8118 | `	/* Return the created array */` |
|        3 |  8119 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8120 | `	return SXRET_OK;` |
|        2 |  8121 |  |
|        - |  8122 | `/*` |
|        - |  8123 | ` * Section:` |
|        - |  8124 | ` *  Output Control (OB) functions.` |
|        - |  8125 | ` * Status:` |
|        - |  8126 | ` *    Stable.` |
|        - |  8127 | ` */` |
|        - |  8128 | `/* Forward declaration */` |
|        - |  8129 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8130 | `/*` |
|        - |  8131 | ` * void ob_clean(void)` |
|        - |  8132 | ` *  This function discards the contents of the output buffer.` |
|        - |  8133 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8134 | ` * Parameter` |
|        - |  8135 | ` *  None` |
|        - |  8136 | ` * Return` |
|        - |  8137 | ` *  No value is returned.` |
|        - |  8138 | ` */` |
|        2 |  8139 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8140 |  |
|        3 |  8141 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8142 | `	VmObEntry *pOb;` |
|        1 |  8143 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8144 | `	SXUNUSED(apArg);` |
|        - |  8145 | `	/* Peek the top most OB */` |
|        3 |  8146 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8147 | `	if( pOb ){` |
|        3 |  8148 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8149 | `	}` |
|        3 |  8150 | `	return PH7_OK;` |
|        1 |  8151 |  |
|        - |  8152 | `/*` |
|        - |  8153 | ` * bool ob_end_clean(void)` |
|        - |  8154 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8155 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8156 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8157 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8158 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8159 | ` * Parameter` |
|        - |  8160 | ` *  None` |
|        - |  8161 | ` * Return` |
|        - |  8162 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8163 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8164 | ` * (possible for special buffer)` |
|        - |  8165 | ` */` |
|     2830 |  8166 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8167 |  |
|     2832 |  8168 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8169 | `	VmObEntry *pOb;` |
|        - |  8170 | `	/* Pop the top most OB */` |
|     2832 |  8171 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2832 |  8172 | `	if( pOb == 0){` |
|        - |  8173 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8174 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8175 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8176 | `		SXUNUSED(apArg);` |
|      ! 0 |  8177 | `	}else{` |
|        - |  8178 | `		/* Release */` |
|     2832 |  8179 | `		VmObRestore(pVm,pOb);` |
|        - |  8180 | `		/* Return true */` |
|     2832 |  8181 | `		ph7_result_bool(pCtx,1);` |
|        - |  8182 | `	}` |
|     2832 |  8183 | `	return PH7_OK;` |
|        2 |  8184 |  |
|        - |  8185 | `/*` |
|        - |  8186 | ` * string ob_get_contents(void)` |
|        - |  8187 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8188 | ` * Parameter` |
|        - |  8189 | ` *  None` |
|        - |  8190 | ` * Return` |
|        - |  8191 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8192 | ` */` |
|        6 |  8193 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8194 |  |
|        7 |  8195 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8196 | `	VmObEntry *pOb;` |
|        - |  8197 | `	/* Peek the top most OB */` |
|        7 |  8198 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8199 | `	if( pOb == 0 ){` |
|        - |  8200 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8201 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8202 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8203 | `		SXUNUSED(apArg);` |
|      ! 0 |  8204 | `	}else{` |
|        - |  8205 | `		/* Return contents */` |
|        7 |  8206 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8207 | `	}` |
|        7 |  8208 | `	return PH7_OK;` |
|        1 |  8209 |  |
|        - |  8210 | `/*` |
|        - |  8211 | ` * string ob_get_clean(void)` |
|        - |  8212 | ` * string ob_get_flush(void)` |
|        - |  8213 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8214 | ` * Parameter` |
|        - |  8215 | ` *  None` |
|        - |  8216 | ` * Return` |
|        - |  8217 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8218 | ` */` |
|     4052 |  8219 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8220 |  |
|     4054 |  8221 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8222 | `	VmObEntry *pOb;` |
|        - |  8223 | `	/* Pop the top most OB */` |
|     4054 |  8224 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     4054 |  8225 | `	if( pOb == 0 ){` |
|        - |  8226 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8227 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8228 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8229 | `		SXUNUSED(apArg);` |
|      ! 0 |  8230 | `	}else{` |
|        - |  8231 | `		/* Return contents */` |
|     4054 |  8232 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8233 | `		/* Release */` |
|     4054 |  8234 | `		VmObRestore(pVm,pOb);` |
|        - |  8235 | `	}` |
|     4054 |  8236 | `	return PH7_OK;` |
|        2 |  8237 |  |
|        - |  8238 | `/*` |
|        - |  8239 | ` * int ob_get_length(void)` |
|        - |  8240 | ` *  Return the length of the output buffer.` |
|        - |  8241 | ` * Parameter` |
|        - |  8242 | ` *  None` |
|        - |  8243 | ` * Return` |
|        - |  8244 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8245 | ` */` |
|        2 |  8246 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8247 |  |
|        3 |  8248 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8249 | `	VmObEntry *pOb;` |
|        - |  8250 | `	/* Peek the top most OB */` |
|        3 |  8251 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8252 | `	if( pOb == 0 ){` |
|        - |  8253 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8254 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8255 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8256 | `		SXUNUSED(apArg);` |
|      ! 0 |  8257 | `	}else{` |
|        - |  8258 | `		/* Return OB length */` |
|        3 |  8259 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8260 | `	}` |
|        3 |  8261 | `	return PH7_OK;` |
|        1 |  8262 |  |
|        - |  8263 | `/*` |
|        - |  8264 | ` * int ob_get_level(void)` |
|        - |  8265 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8266 | ` * Parameter` |
|        - |  8267 | ` *  None` |
|        - |  8268 | ` * Return` |
|        - |  8269 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8270 | ` */` |
|        6 |  8271 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8272 |  |
|        7 |  8273 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8274 | `	int iNest;` |
|        3 |  8275 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8276 | `	SXUNUSED(apArg);` |
|        - |  8277 | `	/* Nesting level */` |
|        7 |  8278 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8279 | `	/* Return the nesting value */` |
|        7 |  8280 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8281 | `	return PH7_OK;` |
|        1 |  8282 |  |
|        - |  8283 | `/*` |
|        - |  8284 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8285 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8286 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8287 | ` */` |
|     6060 |  8288 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8289 |  |
|     6062 |  8290 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8291 | `	VmObEntry *pEntry;` |
|        - |  8292 | `	ph7_value sResult;` |
|        - |  8293 | `	/* Peek the top most entry */` |
|     6062 |  8294 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     6062 |  8295 | `	if( pEntry == 0 ){` |
|        - |  8296 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8297 | `		return PH7_OK;` |
|        - |  8298 | `	}` |
|     6062 |  8299 | `	PH7_MemObjInit(pVm,&sResult);` |
|     6062 |  8300 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8301 | `		ph7_value sArg,*apArg[2];` |
|        - |  8302 | `		/* Fill the first argument */` |
|      ! 0 |  8303 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8304 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8305 | `		apArg[0] = &sArg;` |
|        - |  8306 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8307 | `		pVm->nObDepth++;` |
|      ! 0 |  8308 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8309 | `		pVm->nObDepth--;` |
|      ! 0 |  8310 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8311 | `			/* Extract the function result */` |
|      ! 0 |  8312 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8313 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8314 | `		}` |
|      ! 0 |  8315 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8316 | `	}` |
|     6062 |  8317 | `	if( nDataLen > 0 ){` |
|        - |  8318 | `		/* Redirect the VM output to the internal buffer */` |
|     6062 |  8319 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     3030 |  8320 | `	}` |
|        - |  8321 | `	/* Release */` |
|     6062 |  8322 | `	PH7_MemObjRelease(&sResult);` |
|     6062 |  8323 | `	return PH7_OK;` |
|     3032 |  8324 |  |
|        - |  8325 | `/*` |
|        - |  8326 | ` * Restore the default consumer.` |
|        - |  8327 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8328 | ` * information.` |
|        - |  8329 | ` */` |
|     6884 |  8330 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8331 |  |
|     6886 |  8332 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6886 |  8333 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8334 | `		/* No more stackable OB */` |
|     6868 |  8335 | `		pCons->xConsumer = pCons->xDef;` |
|     6868 |  8336 | `		pCons->pUserData = pCons->pDefData;` |
|     3433 |  8337 | `	}` |
|        - |  8338 | `	/* Release OB data */` |
|     6886 |  8339 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6886 |  8340 | `	SyBlobRelease(&pEntry->sOB);` |
|     6886 |  8341 |  |
|        - |  8342 | `/*` |
|        - |  8343 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8344 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8345 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8346 | ` *  buffer.` |
|        - |  8347 | ` * Parameter` |
|        - |  8348 | ` *  $output_callback` |
|        - |  8349 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8350 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8351 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8352 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8353 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8354 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8355 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8356 | ` *   will return FALSE.` |
|        - |  8357 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8358 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8359 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8360 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8361 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8362 | ` * Return` |
|        - |  8363 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8364 | ` */` |
|     6884 |  8365 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8366 |  |
|     6886 |  8367 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8368 | `	VmObEntry sOb;` |
|        - |  8369 | `	sxi32 rc;` |
|        - |  8370 | `	/* Initialize the OB entry */` |
|     6886 |  8371 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6886 |  8372 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6886 |  8373 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8374 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8375 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8376 | `	}` |
|        - |  8377 | `	/* Push in the stack */` |
|     6886 |  8378 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6886 |  8379 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8380 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8381 | `	}else{` |
|     6886 |  8382 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8383 | `		/* Substitute the default VM consumer */` |
|     6886 |  8384 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6868 |  8385 | `			pCons->xDef = pCons->xConsumer;` |
|     6868 |  8386 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8387 | `			/* Install the new consumer */` |
|     6868 |  8388 | `			pCons->xConsumer = VmObConsumer;` |
|     6868 |  8389 | `			pCons->pUserData = pVm;` |
|     3433 |  8390 | `		}` |
|        - |  8391 | `	}` |
|     6886 |  8392 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6886 |  8393 | `	return PH7_OK;` |
|        2 |  8394 |  |
|        - |  8395 | `/*` |
|        - |  8396 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8397 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8398 | ` * information.` |
|        - |  8399 | ` */` |
|        4 |  8400 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8401 |  |
|        5 |  8402 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8403 | `	sxi32 rc;` |
|        - |  8404 | `	/* Flush contents */` |
|        5 |  8405 | `	rc = PH7_OK;` |
|        5 |  8406 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8407 | `		/* Call the VM output consumer */` |
|        5 |  8408 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8409 | `		/* Increment VM output counter */` |
|        5 |  8410 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8411 | `		if( rc != PH7_ABORT ){` |
|        5 |  8412 | `			rc = PH7_OK;` |
|        2 |  8413 | `		}` |
|        2 |  8414 | `	}` |
|        5 |  8415 | `	if( bRelease ){` |
|        3 |  8416 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8417 | `	}else{` |
|        - |  8418 | `		/* Reset the blob */` |
|        3 |  8419 | `		SyBlobReset(pBlob);` |
|        - |  8420 | `	}` |
|        5 |  8421 | `	return rc;` |
|        1 |  8422 |  |
|        - |  8423 | `/*` |
|        - |  8424 | ` * void ob_flush(void)` |
|        - |  8425 | ` * void flush(void)` |
|        - |  8426 | ` *  Flush (send) the output buffer.` |
|        - |  8427 | ` * Parameter` |
|        - |  8428 | ` *  None` |
|        - |  8429 | ` * Return` |
|        - |  8430 | ` *  No return value.` |
|        - |  8431 | ` */` |
|        2 |  8432 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8433 |  |
|        3 |  8434 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8435 | `	VmObEntry *pOb;` |
|        - |  8436 | `	sxi32 rc;` |
|        - |  8437 | `	/* Peek the top most OB entry */` |
|        3 |  8438 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8439 | `	if( pOb == 0 ){` |
|        - |  8440 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8441 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8442 | `		SXUNUSED(apArg);` |
|      ! 0 |  8443 | `		return PH7_OK;` |
|        - |  8444 | `	}` |
|        - |  8445 | `	/* Flush contents */` |
|        3 |  8446 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8447 | `	return rc;` |
|        2 |  8448 |  |
|        - |  8449 | `/*` |
|        - |  8450 | ` * bool ob_end_flush(void)` |
|        - |  8451 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8452 | ` * Parameter` |
|        - |  8453 | ` *  None` |
|        - |  8454 | ` * Return` |
|        - |  8455 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8456 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8457 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8458 | ` */` |
|        2 |  8459 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8460 |  |
|        3 |  8461 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8462 | `	VmObEntry *pOb;` |
|        - |  8463 | `	sxi32 rc;` |
|        - |  8464 | `	/* Pop the top most OB entry */` |
|        3 |  8465 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8466 | `	if( pOb == 0 ){` |
|        - |  8467 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8468 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8469 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8470 | `		SXUNUSED(apArg);` |
|      ! 0 |  8471 | `		return PH7_OK;` |
|        - |  8472 | `	}` |
|        - |  8473 | `	/* Flush contents */` |
|        3 |  8474 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8475 | `	/* Return true */` |
|        3 |  8476 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8477 | `	return rc;` |
|        2 |  8478 |  |
|        - |  8479 | `/*` |
|        - |  8480 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8481 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8482 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8483 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8484 | ` * Parameter` |
|        - |  8485 | ` *  $flag` |
|        - |  8486 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8487 | ` * Return` |
|        - |  8488 | ` *   Nothing` |
|        - |  8489 | ` */` |
|        4 |  8490 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8491 |  |
|        - |  8492 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8493 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8494 | `	 */` |
|        2 |  8495 | `	SXUNUSED(pCtx);` |
|        2 |  8496 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8497 | `	SXUNUSED(apArg);` |
|        5 |  8498 | `	return PH7_OK;` |
|        1 |  8499 |  |
|        - |  8500 | `/*` |
|        - |  8501 | ` * array ob_list_handlers(void)` |
|        - |  8502 | ` *  Lists all output handlers in use.` |
|        - |  8503 | ` * Parameter` |
|        - |  8504 | ` *  None` |
|        - |  8505 | ` * Return` |
|        - |  8506 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8507 | ` */` |
|        2 |  8508 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8509 |  |
|        3 |  8510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8511 | `	ph7_value *pArray;` |
|        - |  8512 | `	VmObEntry *aEntry;` |
|        - |  8513 | `	ph7_value sVal;` |
|        - |  8514 | `	sxu32 n;` |
|        3 |  8515 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8516 | `		/* Empty stack,return null */` |
|      ! 0 |  8517 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8518 | `		return PH7_OK;` |
|        - |  8519 | `	}` |
|        - |  8520 | `	/* Create a new array */` |
|        3 |  8521 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8522 | `	if( pArray == 0 ){` |
|        - |  8523 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8524 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8525 | `		SXUNUSED(apArg);` |
|      ! 0 |  8526 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8527 | `		return PH7_OK;` |
|        - |  8528 | `	}` |
|        3 |  8529 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8530 | `	/* Point to the installed OB entries */` |
|        3 |  8531 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8532 | `	/* Perform the requested operation */` |
|        5 |  8533 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8534 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8535 | `		/* Extract handler name */` |
|        3 |  8536 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8537 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8538 | `			/* Callback,dup it's name */` |
|      ! 0 |  8539 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8540 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8541 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8542 | `		}else{` |
|        3 |  8543 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8544 | `		}` |
|        3 |  8545 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8546 | `		/* Perform the insertion */` |
|        3 |  8547 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8548 | `	}` |
|        3 |  8549 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8550 | `	/* Return the freshly created array */` |
|        3 |  8551 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8552 | `	return PH7_OK;` |
|        2 |  8553 |  |
|        - |  8554 | `/*` |
|        - |  8555 | ` * Section:` |
|        - |  8556 | ` *  Random numbers/string generators.` |
|        - |  8557 | ` * Status:` |
|        - |  8558 | ` *    Stable.` |
|        - |  8559 | ` */` |
|        - |  8560 | `/*` |
|        - |  8561 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8562 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8563 | ` * used by te SQLite3 library.` |
|        - |  8564 | ` */` |
|     1377 |  8565 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8566 |  |
|        - |  8567 | `	sxu32 iNum;` |
|     1379 |  8568 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1379 |  8569 | `	return iNum;` |
|        2 |  8570 |  |
|        - |  8571 | `/*` |
|        - |  8572 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8573 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8574 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8575 | ` * by te SQLite3 library.` |
|        - |  8576 | ` */` |
|    45648 |  8577 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8578 |  |
|        - |  8579 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8580 | `	int i;` |
|        - |  8581 | `	/* Generate a binary string first */` |
|    45650 |  8582 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8583 | `	/* Turn the binary string into english based alphabet */` |
|   502298 |  8584 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   456650 |  8585 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   228326 |  8586 | `	 }` |
|    45650 |  8587 |  |
|        - |  8588 | `/*` |
|        - |  8589 | ` * int rand()` |
|        - |  8590 | ` * int mt_rand()` |
|        - |  8591 | ` * int rand(int $min,int $max)` |
|        - |  8592 | ` * int mt_rand(int $min,int $max)` |
|        - |  8593 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8594 | ` * Parameter` |
|        - |  8595 | ` *  $min` |
|        - |  8596 | ` *    The lowest value to return (default: 0)` |
|        - |  8597 | ` *  $max` |
|        - |  8598 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8599 | ` * Return` |
|        - |  8600 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8601 | ` * Note:` |
|        - |  8602 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8603 | ` *  by te SQLite3 library.` |
|        - |  8604 | ` */` |
|       20 |  8605 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8606 |  |
|        - |  8607 | `	sxu32 iNum;` |
|        - |  8608 | `	/* Generate the random number */` |
|       21 |  8609 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8610 | `	if( nArg > 1 ){` |
|        - |  8611 | `		sxu32 iMin,iMax;` |
|        3 |  8612 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8613 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8614 | `		if( iMin < iMax ){` |
|        3 |  8615 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8616 | `			if( iDiv > 0 ){` |
|        3 |  8617 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8618 | `			}` |
|        1 |  8619 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8620 | `			iNum %= iMax;` |
|      ! 0 |  8621 | `		}` |
|        1 |  8622 | `	}` |
|        - |  8623 | `	/* Return the number */` |
|       21 |  8624 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8625 | `	return SXRET_OK;` |
|        1 |  8626 |  |
|        - |  8627 | `/*` |
|        - |  8628 | ` * int getrandmax(void)` |
|        - |  8629 | ` * int mt_getrandmax(void)` |
|        - |  8630 | ` * int rc4_getrandmax(void)` |
|        - |  8631 | ` *   Show largest possible random value` |
|        - |  8632 | ` * Return` |
|        - |  8633 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8634 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8635 | ` * Note:` |
|        - |  8636 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8637 | ` *  by te SQLite3 library.` |
|        - |  8638 | ` */` |
|        4 |  8639 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8640 |  |
|        2 |  8641 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8642 | `	SXUNUSED(apArg);` |
|        5 |  8643 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8644 | `	return SXRET_OK;` |
|        1 |  8645 |  |
|        - |  8646 | `/*` |
|        - |  8647 | ` * string rand_str()` |
|        - |  8648 | ` * string rand_str(int $len)` |
|        - |  8649 | ` *  Generate a random string (English alphabet).` |
|        - |  8650 | ` * Parameter` |
|        - |  8651 | ` *  $len` |
|        - |  8652 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8653 | ` * Return` |
|        - |  8654 | ` *   A pseudo random string.` |
|        - |  8655 | ` * Note:` |
|        - |  8656 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8657 | ` *  by te SQLite3 library.` |
|        - |  8658 | ` *  This function is a symisc extension.` |
|        - |  8659 | ` */` |
|      120 |  8660 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8661 |  |
|        - |  8662 | `	char zString[1024];` |
|      122 |  8663 | `	int iLen = 0x10;` |
|      122 |  8664 | `	if( nArg > 0 ){` |
|        - |  8665 | `		/* Get the desired length */` |
|      122 |  8666 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8667 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8668 | `			/* Default length */` |
|        3 |  8669 | `			iLen = 0x10;` |
|        1 |  8670 | `		}` |
|       60 |  8671 | `	}` |
|        - |  8672 | `	/* Generate the random string */` |
|      122 |  8673 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8674 | `	/* Return the generated string */` |
|      122 |  8675 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8676 | `	return SXRET_OK;` |
|        2 |  8677 |  |
|        - |  8678 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8679 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8680 | `/* Unique ID private data */` |
|        - |  8681 | `struct unique_id_data` |
|        - |  8682 |  |
|        - |  8683 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8684 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8685 | `};` |
|        - |  8686 | `/*` |
|        - |  8687 | ` * Binary to hex consumer callback.` |
|        - |  8688 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8689 | ` * defined below.` |
|        - |  8690 | ` */` |
|      192 |  8691 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8692 |  |
|      193 |  8693 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8694 | `	sxu32 nBuflen;` |
|        - |  8695 | `	/* Extract result buffer length */` |
|      193 |  8696 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8697 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8698 | `			/*` |
|        - |  8699 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8700 | `			 * string will be 13 characters long` |
|        - |  8701 | `			 */` |
|       25 |  8702 | `		return SXERR_ABORT;` |
|        - |  8703 | `	}` |
|      169 |  8704 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8705 | `		return SXERR_ABORT;` |
|        - |  8706 | `	}` |
|        - |  8707 | `	/* Safely Consume the hex stream */` |
|      169 |  8708 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8709 | `	return SXRET_OK;` |
|       97 |  8710 |  |
|        - |  8711 | `/*` |
|        - |  8712 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8713 | ` *  Generate a unique ID` |
|        - |  8714 | ` * Parameter` |
|        - |  8715 | ` * $prefix` |
|        - |  8716 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8717 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8718 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8719 | ` * $more_entropy` |
|        - |  8720 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8721 | ` *  that the result will be unique.` |
|        - |  8722 | ` * Return` |
|        - |  8723 | ` *  Returns the unique identifier, as a string.` |
|        - |  8724 | ` */` |
|       24 |  8725 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8726 |  |
|        - |  8727 | `	struct unique_id_data sUniq;` |
|        - |  8728 | `	unsigned char zDigest[20];` |
|       25 |  8729 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8730 | `	const char *zPrefix;` |
|        - |  8731 | `	SHA1Context sCtx;` |
|        - |  8732 | `	char zRandom[7];` |
|        - |  8733 | `	int nPrefix;` |
|        - |  8734 | `	int entropy;` |
|        - |  8735 | `	/* Generate a random string first */` |
|       25 |  8736 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8737 | `	/* Initialize fields */` |
|       25 |  8738 | `	zPrefix = 0;` |
|       25 |  8739 | `	nPrefix = 0;` |
|       25 |  8740 | `	entropy = 0;` |
|       25 |  8741 | `	if( nArg > 0 ){` |
|        - |  8742 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8743 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8744 | `		if( nArg > 1 ){` |
|      ! 0 |  8745 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8746 | `		}` |
|      ! 0 |  8747 | `	}` |
|       25 |  8748 | `	SHA1Init(&sCtx);` |
|        - |  8749 | `	/* Generate the random ID */` |
|       25 |  8750 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8751 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8752 | `	}` |
|        - |  8753 | `	/* Append the random ID */` |
|       25 |  8754 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8755 | `	/* Append the random string */` |
|       25 |  8756 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8757 | `	/* Increment the number */` |
|       25 |  8758 | `	pVm->unique_id++;` |
|       25 |  8759 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8760 | `	/* Hexify the digest */` |
|       25 |  8761 | `	sUniq.pCtx = pCtx;` |
|       25 |  8762 | `	sUniq.entropy = entropy;` |
|       25 |  8763 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8764 | `	/* All done */` |
|       25 |  8765 | `	return PH7_OK;` |
|        1 |  8766 |  |
|        - |  8767 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8768 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8769 | `/*` |
|        - |  8770 | ` * Section:` |
|        - |  8771 | ` *  Language construct implementation as foreign functions.` |
|        - |  8772 | ` * Status:` |
|        - |  8773 | ` *    Stable.` |
|        - |  8774 | ` */` |
|        - |  8775 | `/*` |
|        - |  8776 | ` * void echo($string...)` |
|        - |  8777 | ` *  Output one or more messages.` |
|        - |  8778 | ` * Parameters` |
|        - |  8779 | ` *  $string` |
|        - |  8780 | ` *   Message to output.` |
|        - |  8781 | ` * Return` |
|        - |  8782 | ` *  NULL.` |
|        - |  8783 | ` */` |
|      ! 0 |  8784 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8785 |  |
|        - |  8786 | `	const char *zData;` |
|      ! 0 |  8787 | `	int nDataLen = 0;` |
|        - |  8788 | `	ph7_vm *pVm;` |
|        - |  8789 | `	int i,rc;` |
|        - |  8790 | `	/* Point to the target VM */` |
|      ! 0 |  8791 | `	pVm = pCtx->pVm;` |
|        - |  8792 | `	/* Output */` |
|      ! 0 |  8793 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8794 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8795 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8796 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8797 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8798 | `				/* Increment output length */` |
|      ! 0 |  8799 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8800 | `			}` |
|      ! 0 |  8801 | `			if( rc == SXERR_ABORT ){` |
|        - |  8802 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8803 | `				return PH7_ABORT;` |
|        - |  8804 | `			}` |
|      ! 0 |  8805 | `		}` |
|      ! 0 |  8806 | `	}` |
|      ! 0 |  8807 | `	return SXRET_OK;` |
|      ! 0 |  8808 |  |
|        - |  8809 | `/*` |
|        - |  8810 | ` * int print($string...)` |
|        - |  8811 | ` *  Output one or more messages.` |
|        - |  8812 | ` * Parameters` |
|        - |  8813 | ` *  $string` |
|        - |  8814 | ` *   Message to output.` |
|        - |  8815 | ` * Return` |
|        - |  8816 | ` *  1 always.` |
|        - |  8817 | ` */` |
|        2 |  8818 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8819 |  |
|        - |  8820 | `	const char *zData;` |
|        3 |  8821 | `	int nDataLen = 0;` |
|        - |  8822 | `	ph7_vm *pVm;` |
|        - |  8823 | `	int i,rc;` |
|        - |  8824 | `	/* Point to the target VM */` |
|        3 |  8825 | `	pVm = pCtx->pVm;` |
|        - |  8826 | `	/* Output */` |
|        5 |  8827 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8828 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8829 | `		if( nDataLen > 0 ){` |
|        3 |  8830 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8831 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8832 | `				/* Increment output length */` |
|        3 |  8833 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8834 | `			}` |
|        3 |  8835 | `			if( rc == SXERR_ABORT ){` |
|        - |  8836 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8837 | `				return PH7_ABORT;` |
|        - |  8838 | `			}` |
|        1 |  8839 | `		}` |
|        2 |  8840 | `	}` |
|        - |  8841 | `	/* Return 1 */` |
|        3 |  8842 | `	ph7_result_int(pCtx,1);` |
|        3 |  8843 | `	return SXRET_OK;` |
|        2 |  8844 |  |
|        - |  8845 | `/*` |
|        - |  8846 | ` * void exit(string $msg)` |
|        - |  8847 | ` * void exit(int $status)` |
|        - |  8848 | ` * void die(string $ms)` |
|        - |  8849 | ` * void die(int $status)` |
|        - |  8850 | ` *   Output a message and terminate program execution.` |
|        - |  8851 | ` * Parameter` |
|        - |  8852 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8853 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8854 | ` *  and not printed` |
|        - |  8855 | ` * Return` |
|        - |  8856 | ` *  NULL` |
|        - |  8857 | ` */` |
|      ! 0 |  8858 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8859 |  |
|      ! 0 |  8860 | `	if( nArg > 0 ){` |
|      ! 0 |  8861 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8862 | `			const char *zData;` |
|      ! 0 |  8863 | `			int iLen = 0;` |
|        - |  8864 | `			/* Print exit message */` |
|      ! 0 |  8865 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8866 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8867 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8868 | `			sxi32 iExitStatus;` |
|        - |  8869 | `			/* Record exit status code */` |
|      ! 0 |  8870 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8871 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8872 | `		}` |
|      ! 0 |  8873 | `	}` |
|        - |  8874 | `	/* Check if we are in an included file */` |
|      ! 0 |  8875 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8876 | `		/* Exit the entire process */` |
|      ! 0 |  8877 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8878 | `	}` |
|        - |  8879 | `	/* Abort processing immediately */` |
|      ! 0 |  8880 | `	return PH7_ABORT;` |
|      ! 0 |  8881 |  |
|        - |  8882 | `/*` |
|        - |  8883 | ` * bool isset($var,...)` |
|        - |  8884 | ` *  Finds out whether a variable is set.` |
|        - |  8885 | ` * Parameters` |
|        - |  8886 | ` *  One or more variable to check.` |
|        - |  8887 | ` * Return` |
|        - |  8888 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8889 | ` */` |
|    57168 |  8890 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8891 |  |
|        - |  8892 | `	ph7_value *pObj;` |
|    57170 |  8893 | `	int res = 0;` |
|        - |  8894 | `	int i;` |
|    57170 |  8895 | `	if( nArg < 1 ){` |
|        - |  8896 | `		/* Missing arguments,return false */` |
|      ! 0 |  8897 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8898 | `		return SXRET_OK;` |
|        - |  8899 | `	}` |
|        - |  8900 | `	/* Iterate over available arguments */` |
|    76404 |  8901 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    57170 |  8902 | `		pObj = apArg[i];` |
|    57170 |  8903 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    37730 |  8904 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8905 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8906 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8907 | `			}` |
|    18864 |  8908 | `		}` |
|    57170 |  8909 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    57170 |  8910 | `		if( !res ){` |
|        - |  8911 | `			/* Variable not set,return FALSE */` |
|    37936 |  8912 | `			ph7_result_bool(pCtx,0);` |
|    37936 |  8913 | `			return SXRET_OK;` |
|        - |  8914 | `		}` |
|     9619 |  8915 | `	}` |
|        - |  8916 | `	/* All given variable are set,return TRUE */` |
|    19236 |  8917 | `	ph7_result_bool(pCtx,1);` |
|    19236 |  8918 | `	return SXRET_OK;` |
|    28586 |  8919 |  |
|        - |  8920 | `/*` |
|        - |  8921 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8922 | ` * frame,the reference table and discard it's contents.` |
|        - |  8923 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8924 | ` */` |
|  2786194 |  8925 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8926 |  |
|        - |  8927 | `	ph7_value *pObj;` |
|        - |  8928 | `	VmRefObj *pRef;` |
|  2786196 |  8929 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2786196 |  8930 | `	if( pObj ){` |
|        - |  8931 | `		/* Release the object */` |
|  2786196 |  8932 | `		PH7_MemObjRelease(pObj);` |
|  1393097 |  8933 | `	}` |
|        - |  8934 | `	/* Remove old reference links */` |
|  2786196 |  8935 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2786196 |  8936 | `	if( pRef ){` |
|  2786176 |  8937 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8938 | `		/* Unlink from the reference table */` |
|  2786176 |  8939 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2786176 |  8940 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8941 | `			VmSlot sFree;` |
|        - |  8942 | `			/* Restore to the free list */` |
|  2786170 |  8943 | `			sFree.nIdx = nObjIdx;` |
|  2786170 |  8944 | `			sFree.pUserData = 0;` |
|  2786170 |  8945 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1393084 |  8946 | `		}` |
|  1393087 |  8947 | `	}` |
|  2786196 |  8948 | `	return SXRET_OK;` |
|        2 |  8949 |  |
|        - |  8950 | `/*` |
|        - |  8951 | ` * void unset($var,...)` |
|        - |  8952 | ` *   Unset one or more given variable.` |
|        - |  8953 | ` * Parameters` |
|        - |  8954 | ` *  One or more variable to unset.` |
|        - |  8955 | ` * Return` |
|        - |  8956 | ` *  Nothing.` |
|        - |  8957 | ` */` |
|     2832 |  8958 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8959 |  |
|        - |  8960 | `	ph7_value *pObj;` |
|        - |  8961 | `	ph7_vm *pVm;` |
|        - |  8962 | `	int i;` |
|        - |  8963 | `	/* Point to the target VM */` |
|     2834 |  8964 | `	pVm = pCtx->pVm;` |
|        - |  8965 | `	/* Iterate and unset */` |
|     8586 |  8966 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5754 |  8967 | `		pObj = apArg[i];` |
|     5754 |  8968 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      726 |  8969 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8970 | `				/* Throw an error */` |
|      ! 0 |  8971 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8972 | `			}` |
|      364 |  8973 | `		}else{` |
|     5029 |  8974 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8975 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5029 |  8976 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5023 |  8977 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2511 |  8978 | `			}` |
|        - |  8979 | `		}` |
|     2878 |  8980 | `	}` |
|     2834 |  8981 | `	return SXRET_OK;` |
|        2 |  8982 |  |
|        - |  8983 | `/*` |
|        - |  8984 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8985 | ` */` |
|      108 |  8986 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8987 |  |
|      109 |  8988 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8989 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8990 | `	ph7_value *pObj;` |
|        - |  8991 | `	sxu32 nIdx;` |
|        - |  8992 | `	/* Extract the memory object */` |
|      109 |  8993 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8994 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8995 | `	if( pObj ){` |
|      109 |  8996 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8997 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8998 | `				SyString sName;` |
|        - |  8999 | `				ph7_value sKey;` |
|        - |  9000 | `				/* Perform the insertion */` |
|      107 |  9001 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  9002 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  9003 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  9004 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  9005 | `			}` |
|       53 |  9006 | `		}` |
|       54 |  9007 | `	}` |
|      109 |  9008 | `	return SXRET_OK;` |
|        1 |  9009 |  |
|        - |  9010 | `/*` |
|        - |  9011 | ` * array get_defined_vars(void)` |
|        - |  9012 | ` *  Returns an array of all defined variables.` |
|        - |  9013 | ` * Parameter` |
|        - |  9014 | ` *  None` |
|        - |  9015 | ` * Return` |
|        - |  9016 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9017 | ` */` |
|        2 |  9018 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9019 |  |
|        3 |  9020 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9021 | `	ph7_value *pArray;` |
|        - |  9022 | `	/* Create a new array */` |
|        3 |  9023 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9024 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9025 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9026 | `		SXUNUSED(apArg);` |
|        - |  9027 | `		/* Return NULL */` |
|      ! 0 |  9028 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9029 | `		return SXRET_OK;` |
|        - |  9030 | `	}` |
|        - |  9031 | `	/* Superglobals first */` |
|        3 |  9032 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9033 | `	/* Then variable defined in the current frame */` |
|        3 |  9034 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9035 | `	/* Finally,return the created array */` |
|        3 |  9036 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9037 | `	return SXRET_OK;` |
|        2 |  9038 |  |
|        - |  9039 | `/*` |
|        - |  9040 | ` * bool gettype($var)` |
|        - |  9041 | ` *  Get the type of a variable` |
|        - |  9042 | ` * Parameters` |
|        - |  9043 | ` *   $var` |
|        - |  9044 | ` *    The variable being type checked.` |
|        - |  9045 | ` * Return` |
|        - |  9046 | ` *   String representation of the given variable type.` |
|        - |  9047 | ` */` |
|       28 |  9048 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9049 |  |
|       29 |  9050 | `	const char *zType = "Empty";` |
|       29 |  9051 | `	if( nArg > 0 ){` |
|       29 |  9052 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       14 |  9053 | `	}` |
|        - |  9054 | `	/* Return the variable type */` |
|       29 |  9055 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       29 |  9056 | `	return SXRET_OK;` |
|        1 |  9057 |  |
|        - |  9058 | `/*` |
|        - |  9059 | ` * string get_resource_type(resource $handle)` |
|        - |  9060 | ` *  This function gets the type of the given resource.` |
|        - |  9061 | ` * Parameters` |
|        - |  9062 | ` *  $handle` |
|        - |  9063 | ` *  The evaluated resource handle.` |
|        - |  9064 | ` * Return` |
|        - |  9065 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9066 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9067 | ` *  the return value will be the string Unknown.` |
|        - |  9068 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9069 | ` *  is not a resource.` |
|        - |  9070 | ` */` |
|        2 |  9071 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9072 |  |
|        3 |  9073 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9074 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9075 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9076 | `		return PH7_OK;` |
|        - |  9077 | `	}` |
|        3 |  9078 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9079 | `	return SXRET_OK;` |
|        2 |  9080 |  |
|        - |  9081 | `/*` |
|        - |  9082 | ` * void var_dump(expression,....)` |
|        - |  9083 | ` *   var_dump � Dumps information about a variable` |
|        - |  9084 | ` * Parameters` |
|        - |  9085 | ` *   One or more expression to dump.` |
|        - |  9086 | ` * Returns` |
|        - |  9087 | ` *  Nothing.` |
|        - |  9088 | ` */` |
|      236 |  9089 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9090 |  |
|        - |  9091 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9092 | `	int i;` |
|      238 |  9093 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9094 | `	/* Dump one or more expressions */` |
|      480 |  9095 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      244 |  9096 | `		ph7_value *pObj = apArg[i];` |
|        - |  9097 | `		/* Reset the working buffer */` |
|      244 |  9098 | `		SyBlobReset(&sDump);` |
|        - |  9099 | `		/* Dump the given expression */` |
|      244 |  9100 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9101 | `		/* Output */` |
|      244 |  9102 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      244 |  9103 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      121 |  9104 | `		}` |
|      123 |  9105 | `	}` |
|        - |  9106 | `	/* Release the working buffer */` |
|      238 |  9107 | `	SyBlobRelease(&sDump);` |
|      238 |  9108 | `	return SXRET_OK;` |
|        2 |  9109 |  |
|        - |  9110 | `/*` |
|        - |  9111 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9112 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9113 | ` * Parameters` |
|        - |  9114 | ` *   expression: Expression to dump` |
|        - |  9115 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9116 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9117 | ` *            print_r() will return the information rather than print it.` |
|        - |  9118 | ` * Return` |
|        - |  9119 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9120 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9121 | ` */` |
|       16 |  9122 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9123 |  |
|       17 |  9124 | `	int ret_string = 0;` |
|        - |  9125 | `	SyBlob sDump;` |
|       17 |  9126 | `	if( nArg < 1 ){` |
|        - |  9127 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9129 | `		return SXRET_OK;` |
|        - |  9130 | `	}` |
|       17 |  9131 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9132 | `	if ( nArg > 1 ){` |
|        - |  9133 | `		/* Where to redirect output */` |
|       11 |  9134 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9135 | `	}` |
|        - |  9136 | `	/* Generate dump */` |
|       17 |  9137 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9138 | `	if( !ret_string ){` |
|        - |  9139 | `		/* Output dump */` |
|        7 |  9140 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9141 | `		/* Return true */` |
|        7 |  9142 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9143 | `	}else{` |
|        - |  9144 | `		/* Generated dump as return value */` |
|       11 |  9145 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9146 | `	}` |
|        - |  9147 | `	/* Release the working buffer */` |
|       17 |  9148 | `	SyBlobRelease(&sDump);` |
|       17 |  9149 | `	return SXRET_OK;` |
|        9 |  9150 |  |
|        - |  9151 | `/*` |
|        - |  9152 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9153 | ` * Same job as print_r. (see coment above)` |
|        - |  9154 | ` */` |
|        2 |  9155 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9156 |  |
|        3 |  9157 | `	int ret_string = 0;` |
|        - |  9158 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9159 | `	if( nArg < 1 ){` |
|        - |  9160 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9161 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9162 | `		return SXRET_OK;` |
|        - |  9163 | `	}` |
|        3 |  9164 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9165 | `	if ( nArg > 1 ){` |
|        - |  9166 | `		/* Where to redirect output */` |
|        3 |  9167 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9168 | `	}` |
|        - |  9169 | `	/* Generate dump */` |
|        3 |  9170 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9171 | `	if( !ret_string ){` |
|        - |  9172 | `		/* Output dump */` |
|      ! 0 |  9173 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9174 | `		/* Return NULL */` |
|      ! 0 |  9175 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9176 | `	}else{` |
|        - |  9177 | `		/* Generated dump as return value */` |
|        3 |  9178 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9179 | `	}` |
|        - |  9180 | `	/* Release the working buffer */` |
|        3 |  9181 | `	SyBlobRelease(&sDump);` |
|        3 |  9182 | `	return SXRET_OK;` |
|        2 |  9183 |  |
|        - |  9184 | `/*` |
|        - |  9185 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9186 | ` *  Set/get the various assert flags.` |
|        - |  9187 | ` * Parameter` |
|        - |  9188 | ` * $what` |
|        - |  9189 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9190 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9191 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9192 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9193 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9194 | ` * $value` |
|        - |  9195 | ` *   An optional new value for the option.` |
|        - |  9196 | ` * Return` |
|        - |  9197 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9198 | ` */` |
|        8 |  9199 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9200 |  |
|        9 |  9201 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9202 | `	int iOld,iNew,iValue;` |
|        9 |  9203 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9204 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9205 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9206 | `		return PH7_OK;` |
|        - |  9207 | `	}` |
|        - |  9208 | `	/* Save old assertion flags */` |
|        9 |  9209 | `	iOld = pVm->iAssertFlags;` |
|        - |  9210 | `	/* Extract the new flags */` |
|        9 |  9211 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9212 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9213 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9214 | `		if( nArg > 1 ){` |
|        5 |  9215 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9216 | `			if( iValue ){` |
|        - |  9217 | `				/* Disable assertion */` |
|        3 |  9218 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9219 | `			}` |
|        3 |  9220 | `		}` |
|        6 |  9221 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9222 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9223 | `		if( nArg > 1 ){` |
|      ! 0 |  9224 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9225 | `			if( iValue ){` |
|        - |  9226 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9227 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9228 | `			}` |
|      ! 0 |  9229 | `		}` |
|        3 |  9230 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9231 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9232 | `		if( nArg > 1 ){` |
|        3 |  9233 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9234 | `			if( iValue ){` |
|        - |  9235 | `				/* Terminate execution on failed assertions */` |
|        3 |  9236 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9237 | `			}` |
|        2 |  9238 | `		}` |
|        1 |  9239 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9240 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9241 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9242 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9243 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9244 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9245 | `		}` |
|      ! 0 |  9246 | `	}` |
|        - |  9247 | `	/* Return the old flags */` |
|        9 |  9248 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9249 | `	return PH7_OK;` |
|        5 |  9250 |  |
|        - |  9251 | `/*` |
|        - |  9252 | ` * bool assert(mixed $assertion)` |
|        - |  9253 | ` *  Checks if assertion is FALSE.` |
|        - |  9254 | ` * Parameter` |
|        - |  9255 | ` *  $assertion` |
|        - |  9256 | ` *    The assertion to test.` |
|        - |  9257 | ` * Return` |
|        - |  9258 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9259 | ` */` |
|       14 |  9260 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9261 |  |
|       15 |  9262 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9263 | `	ph7_value *pAssert;` |
|        - |  9264 | `	int iFlags,iResult;` |
|       15 |  9265 | `	if( nArg < 1 ){` |
|        - |  9266 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9267 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9268 | `		return PH7_OK;` |
|        - |  9269 | `	}` |
|       15 |  9270 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9271 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9272 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9273 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9274 | `		return PH7_OK;` |
|        - |  9275 | `	}` |
|       15 |  9276 | `	pAssert = apArg[0];` |
|       15 |  9277 | `	iResult = 1; /* cc warning */` |
|       15 |  9278 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9279 | `		SyString sChunk;` |
|        7 |  9280 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  9281 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9282 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9283 | `			/* Extract evaluation result */` |
|        5 |  9284 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9285 | `		}else{` |
|        3 |  9286 | `			iResult = 0;` |
|        - |  9287 | `		}` |
|        4 |  9288 | `	}else{` |
|        - |  9289 | `		/* Perform a boolean cast */` |
|        9 |  9290 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9291 | `	}` |
|       15 |  9292 | `	if( !iResult ){` |
|        - |  9293 | `		/* Assertion failed */` |
|        9 |  9294 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9295 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9296 | `			ph7_value sFile,sLine;` |
|        - |  9297 | `			ph7_value *apCbArg[3];` |
|        - |  9298 | `			SyString *pFile;` |
|        - |  9299 | `			/* Extract the processed script */` |
|      ! 0 |  9300 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9301 | `			if( pFile == 0 ){` |
|      ! 0 |  9302 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9303 | `			}` |
|        - |  9304 | `			/* Invoke the callback */` |
|      ! 0 |  9305 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9306 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9307 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9308 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9309 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9310 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9311 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9312 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9313 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9314 | `		}` |
|        9 |  9315 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9316 | `			/* Emit a warning */` |
|        9 |  9317 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9318 | `		}` |
|        9 |  9319 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9320 | `			/* Abort VM execution immediately */` |
|        3 |  9321 | `			return PH7_ABORT;` |
|        - |  9322 | `		}` |
|        3 |  9323 | `	}` |
|        - |  9324 | `	/* Assertion result */` |
|       13 |  9325 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9326 | `	return PH7_OK;` |
|        8 |  9327 |  |
|        - |  9328 | `/*` |
|        - |  9329 | ` * Section:` |
|        - |  9330 | ` *  Error reporting functions.` |
|        - |  9331 | ` * Status:` |
|        - |  9332 | ` *    Stable.` |
|        - |  9333 | ` */` |
|        - |  9334 | `/*` |
|        - |  9335 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9336 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9337 | ` * Parameters` |
|        - |  9338 | ` *  $error_msg` |
|        - |  9339 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9340 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9341 | ` * $error_type` |
|        - |  9342 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9343 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9344 | ` * Return` |
|        - |  9345 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9346 | ` */` |
|       12 |  9347 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9348 |  |
|       14 |  9349 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9350 | `	int rc = PH7_OK;` |
|       14 |  9351 | `	if( nArg > 0 ){` |
|        - |  9352 | `		const char *zErr;` |
|        - |  9353 | `		int nLen;` |
|        - |  9354 | `		/* Extract the error message */` |
|       12 |  9355 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9356 | `		if( nArg > 1 ){` |
|        - |  9357 | `			/* Extract the error type */` |
|       12 |  9358 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9359 | `			switch( nErr ){` |
|        1 |  9360 | `			case 1:   /* E_ERROR */` |
|        - |  9361 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9362 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9363 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9364 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9365 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9366 | `				break;` |
|        1 |  9367 | `			case 2:   /* E_WARNING */` |
|        - |  9368 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9369 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9370 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9371 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9372 | `				break;` |
|        3 |  9373 | `			default:` |
|        8 |  9374 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9375 | `				break;` |
|        - |  9376 | `			}` |
|        5 |  9377 | `		}` |
|        - |  9378 | `		/* Report error */` |
|       12 |  9379 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9380 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9381 | `			return rc;` |
|        - |  9382 | `		}` |
|        - |  9383 | `		/* Return true */` |
|       12 |  9384 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9385 | `	}else{` |
|        - |  9386 | `		/* Missing arguments,return FALSE */` |
|        3 |  9387 | `		ph7_result_bool(pCtx,0);` |
|        - |  9388 | `	}` |
|       14 |  9389 | `	return rc;` |
|        8 |  9390 |  |
|        - |  9391 | `/*` |
|        - |  9392 | ` * int error_reporting([int $level])` |
|        - |  9393 | ` *  Sets which PHP errors are reported.` |
|        - |  9394 | ` * Parameters` |
|        - |  9395 | ` *  $level` |
|        - |  9396 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9397 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9398 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9399 | ` *   levels will not always behave as expected.` |
|        - |  9400 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9401 | ` *   in the predefined constants.` |
|        - |  9402 | ` * Return` |
|        - |  9403 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9404 | ` *   parameter is given.` |
|        - |  9405 | ` */` |
|       18 |  9406 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9407 |  |
|       19 |  9408 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9409 | `	int nOld;` |
|        - |  9410 | `	/* Extract the old reporting level */` |
|       19 |  9411 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9412 | `	if( nArg > 0 ){` |
|        - |  9413 | `		int nNew;` |
|        - |  9414 | `		/* Extract the desired error reporting level */` |
|       11 |  9415 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9416 | `		if( !nNew ){` |
|        - |  9417 | `			/* Do not report errors at all */` |
|        5 |  9418 | `			pVm->bErrReport = 0;` |
|        3 |  9419 | `		}else{` |
|        - |  9420 | `			/* Report all errors */` |
|        7 |  9421 | `			pVm->bErrReport = 1;` |
|        - |  9422 | `		}` |
|        5 |  9423 | `	}` |
|        - |  9424 | `	/* Return the old level */` |
|       19 |  9425 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9426 | `	return PH7_OK;` |
|        1 |  9427 |  |
|        - |  9428 | `/*` |
|        - |  9429 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9430 | ` *  Send an error message somewhere.` |
|        - |  9431 | ` * Parameter` |
|        - |  9432 | ` *  $message` |
|        - |  9433 | ` *   The error message that should be logged.` |
|        - |  9434 | ` *  $message_type` |
|        - |  9435 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9436 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9437 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9438 | ` *       This is the default option.` |
|        - |  9439 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9440 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9441 | ` *    2  No longer an option.` |
|        - |  9442 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9443 | ` *       to the end of the message string.` |
|        - |  9444 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9445 | ` *  $destination` |
|        - |  9446 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9447 | ` *  $extra_headers` |
|        - |  9448 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9449 | ` * Return` |
|        - |  9450 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9451 | ` * NOTE:` |
|        - |  9452 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9453 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9454 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9455 | ` *  Otherwise this function is no-op.` |
|        - |  9456 | ` */` |
|        4 |  9457 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9458 |  |
|        - |  9459 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9460 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9461 | `	int iType = 0;` |
|        5 |  9462 | `	if( nArg < 1 ){` |
|        - |  9463 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9464 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9465 | `		return PH7_OK;` |
|        - |  9466 | `	}` |
|        5 |  9467 | `	if( pVm->xErrLog  ){` |
|        - |  9468 | `		/* Invoke the user callback */` |
|      ! 0 |  9469 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9470 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9471 | `		if( nArg > 1 ){` |
|      ! 0 |  9472 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9473 | `			if( nArg > 2 ){` |
|      ! 0 |  9474 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9475 | `				if( nArg > 3 ){` |
|      ! 0 |  9476 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9477 | `				}` |
|      ! 0 |  9478 | `			}` |
|      ! 0 |  9479 | `		}` |
|      ! 0 |  9480 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9481 | `	}` |
|        - |  9482 | `	/* Retun TRUE */` |
|        5 |  9483 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9484 | `	return PH7_OK;` |
|        3 |  9485 |  |
|        - |  9486 | `/*` |
|        - |  9487 | ` * bool restore_exception_handler(void)` |
|        - |  9488 | ` *  Restores the previously defined exception handler function.` |
|        - |  9489 | ` * Parameter` |
|        - |  9490 | ` *  None` |
|        - |  9491 | ` * Return` |
|        - |  9492 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9493 | ` */` |
|        4 |  9494 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9495 |  |
|        5 |  9496 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9497 | `	ph7_value *pOld,*pNew;` |
|        - |  9498 | `	/* Point to the old and the new handler */` |
|        5 |  9499 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9500 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9501 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9502 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9503 | `		SXUNUSED(apArg);` |
|        - |  9504 | `		/* No installed handler,return FALSE */` |
|        5 |  9505 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9506 | `		return PH7_OK;` |
|        - |  9507 | `	}` |
|        - |  9508 | `	/* Copy the old handler */` |
|      ! 0 |  9509 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9510 | `	PH7_MemObjRelease(pOld);` |
|        - |  9511 | `	/* Return TRUE */` |
|      ! 0 |  9512 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9513 | `	return PH7_OK;` |
|        3 |  9514 |  |
|        - |  9515 | `/*` |
|        - |  9516 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9517 | ` *  Sets a user-defined exception handler function.` |
|        - |  9518 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9519 | ` * NOTE` |
|        - |  9520 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9521 | ` *  the satndard PHP engine.` |
|        - |  9522 | ` * Parameters` |
|        - |  9523 | ` *  $exception_handler` |
|        - |  9524 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9525 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9526 | ` *   that was thrown.` |
|        - |  9527 | ` *  Note:` |
|        - |  9528 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9529 | ` * Return` |
|        - |  9530 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9531 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9532 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9533 | ` */` |
|        4 |  9534 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9535 |  |
|        6 |  9536 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9537 | `	ph7_value *pOld,*pNew;` |
|        - |  9538 | `	/* Point to the old and the new handler */` |
|        6 |  9539 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9540 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9541 | `	/* Return the old handler */` |
|        6 |  9542 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9543 | `	if( nArg > 0 ){` |
|        6 |  9544 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9545 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9546 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9547 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9548 | `		}else{` |
|        6 |  9549 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9550 | `			/* Install the new handler */` |
|        6 |  9551 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9552 | `		}` |
|        2 |  9553 | `	}` |
|        6 |  9554 | `	return PH7_OK;` |
|        2 |  9555 |  |
|        - |  9556 | `/*` |
|        - |  9557 | ` * bool restore_error_handler(void)` |
|        - |  9558 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9559 | ` * Parameters:` |
|        - |  9560 | ` *  None.` |
|        - |  9561 | ` * Return` |
|        - |  9562 | ` *  Always TRUE.` |
|        - |  9563 | ` */` |
|        4 |  9564 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9565 |  |
|        5 |  9566 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9567 | `	ph7_value *pOld,*pNew;` |
|        - |  9568 | `	/* Point to the old and the new handler */` |
|        5 |  9569 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9570 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9571 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9572 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9573 | `		SXUNUSED(apArg);` |
|        - |  9574 | `		/* No installed callback,return FALSE */` |
|        5 |  9575 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9576 | `		return PH7_OK;` |
|        - |  9577 | `	}` |
|        - |  9578 | `	/* Copy the old callback */` |
|      ! 0 |  9579 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9580 | `	PH7_MemObjRelease(pOld);` |
|        - |  9581 | `	/* Return TRUE */` |
|      ! 0 |  9582 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9583 | `	return PH7_OK;` |
|        3 |  9584 |  |
|        - |  9585 | `/*` |
|        - |  9586 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9587 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9588 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9589 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9590 | ` *  Sets a user-defined error handler function.` |
|        - |  9591 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9592 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9593 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9594 | ` *  conditions (using trigger_error()).` |
|        - |  9595 | ` * Parameters` |
|        - |  9596 | ` *  $error_handler` |
|        - |  9597 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9598 | ` *   describing the error.` |
|        - |  9599 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9600 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9601 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9602 | ` *   The function can be shown as:` |
|        - |  9603 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9604 | ` *     errno` |
|        - |  9605 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9606 | ` *   errstr` |
|        - |  9607 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9608 | ` *   errfile` |
|        - |  9609 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9610 | ` *     was raised in, as a string.` |
|        - |  9611 | ` *  Note:` |
|        - |  9612 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9613 | ` * Return` |
|        - |  9614 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9615 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9616 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9617 | ` */` |
|     8082 |  9618 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9619 |  |
|     8084 |  9620 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9621 | `	ph7_value *pOld,*pNew;` |
|        - |  9622 | `	/* Point to the old and the new handler */` |
|     8084 |  9623 | `	pOld = &pVm->aErrCB[0];` |
|     8084 |  9624 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9625 | `	/* Return the old handler */` |
|     8084 |  9626 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8084 |  9627 | `	if( nArg > 0 ){` |
|     8084 |  9628 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9629 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4041 |  9630 | `			PH7_MemObjRelease(pNew);` |
|     4041 |  9631 | `			ph7_result_bool(pCtx,1);` |
|     2021 |  9632 | `		}else{` |
|     4044 |  9633 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9634 | `			/* Install the new handler */` |
|     4044 |  9635 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9636 | `		}` |
|     4041 |  9637 | `	}` |
|     8084 |  9638 | `	return PH7_OK;` |
|        2 |  9639 |  |
|        - |  9640 | `/*` |
|        - |  9641 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9642 | ` *  Generates a backtrace.` |
|        - |  9643 | ` * Paramaeter` |
|        - |  9644 | ` *  $options` |
|        - |  9645 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9646 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9647 | ` *   all the function/method arguments, to save memory.` |
|        - |  9648 | ` * $limit` |
|        - |  9649 | ` *   (Not Used)` |
|        - |  9650 | ` * Return` |
|        - |  9651 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9652 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9653 | ` *          Name        Type      Description` |
|        - |  9654 | ` *          ------      ------     -----------` |
|        - |  9655 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9656 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9657 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9658 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9659 | ` *          object      object    The current object.` |
|        - |  9660 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9661 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9662 | ` */` |
|      202 |  9663 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9664 |  |
|      204 |  9665 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9666 | `	ph7_value *pArray;` |
|        - |  9667 | `	ph7_class *pClass;` |
|        - |  9668 | `	ph7_value *pValue;` |
|        - |  9669 | `	SyString *pFile;` |
|        - |  9670 | `	/* Create a new array */` |
|      204 |  9671 | `	pArray = ph7_context_new_array(pCtx);` |
|      204 |  9672 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      204 |  9673 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9674 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9675 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9676 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9677 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9678 | `		SXUNUSED(apArg);` |
|      ! 0 |  9679 | `		return PH7_OK;` |
|        - |  9680 | `	}` |
|        - |  9681 | `	/* Dump running function name and it's arguments  */` |
|      204 |  9682 | `	if( pVm->pFrame->pParent ){` |
|      204 |  9683 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9684 | `		ph7_vm_func *pFunc;` |
|        - |  9685 | `		ph7_value *pArg;` |
|      204 |  9686 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9687 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9688 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9689 | `		}` |
|      204 |  9690 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      204 |  9691 | `		if( pFrame->pParent && pFunc ){` |
|      204 |  9692 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      204 |  9693 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      204 |  9694 | `			ph7_value_reset_string_cursor(pValue);` |
|      101 |  9695 | `		}` |
|        - |  9696 | `		/* Function arguments */` |
|      204 |  9697 | `		pArg = ph7_context_new_array(pCtx);` |
|      204 |  9698 | `		if( pArg  ){` |
|        - |  9699 | `			ph7_value *pObj;` |
|        - |  9700 | `			VmSlot *aSlot;` |
|        - |  9701 | `			sxu32 n;` |
|        - |  9702 | `			/* Start filling the array with the given arguments */` |
|      204 |  9703 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      802 |  9704 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      600 |  9705 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      600 |  9706 | `				if( pObj ){` |
|      600 |  9707 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      299 |  9708 | `				}` |
|      301 |  9709 | `			}` |
|        - |  9710 | `			/* Save the array */` |
|      204 |  9711 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      101 |  9712 | `		}` |
|      101 |  9713 | `	}` |
|      204 |  9714 | `	ph7_value_int(pValue,1);` |
|        - |  9715 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9716 | `	 * line numbers at run-time. )` |
|        - |  9717 | `	 */` |
|      204 |  9718 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9719 | `	/* Current processed script */` |
|      204 |  9720 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      204 |  9721 | `	if( pFile ){` |
|      204 |  9722 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      204 |  9723 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      204 |  9724 | `		ph7_value_reset_string_cursor(pValue);` |
|      101 |  9725 | `	}` |
|        - |  9726 | `	/* Top class */` |
|      204 |  9727 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      204 |  9728 | `	if( pClass ){` |
|      200 |  9729 | `		ph7_value_reset_string_cursor(pValue);` |
|      200 |  9730 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      200 |  9731 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|       99 |  9732 | `	}` |
|        - |  9733 | `	/* Return the freshly created array */` |
|      204 |  9734 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9735 | `	/*` |
|        - |  9736 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9737 | `	 * as soon we return from this function.` |
|        - |  9738 | `	 */` |
|      204 |  9739 | `	return PH7_OK;` |
|      103 |  9740 |  |
|        - |  9741 | `/*` |
|        - |  9742 | ` * Generate a small backtrace.` |
|        - |  9743 | ` * Store the generated dump in the given BLOB` |
|        - |  9744 | ` */` |
|        4 |  9745 | `static int VmMiniBacktrace(` |
|        - |  9746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9747 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9748 | `	)` |
|        1 |  9749 |  |
|        5 |  9750 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9751 | `	ph7_vm_func *pFunc;` |
|        - |  9752 | `	ph7_class *pClass;` |
|        - |  9753 | `	SyString *pFile;` |
|        - |  9754 | `	/* Called function */` |
|        5 |  9755 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9756 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9757 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9758 | `	}` |
|        5 |  9759 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9760 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9761 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9762 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9763 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9764 | `	}else{` |
|      ! 0 |  9765 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9766 | `	}` |
|        5 |  9767 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9768 | `	/* Current processed script */` |
|        5 |  9769 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9770 | `	if( pFile ){` |
|        5 |  9771 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9772 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9773 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9774 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9775 | `	}` |
|        - |  9776 | `	/* Top class */` |
|        5 |  9777 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9778 | `	if( pClass ){` |
|      ! 0 |  9779 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9780 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9781 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9782 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9783 | `	}` |
|        5 |  9784 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9785 | `	/* All done */` |
|        5 |  9786 | `	return SXRET_OK;` |
|        1 |  9787 |  |
|        - |  9788 | `/*` |
|        - |  9789 | ` * void debug_print_backtrace()` |
|        - |  9790 | ` *  Prints a backtrace` |
|        - |  9791 | ` * Parameters` |
|        - |  9792 | ` * None` |
|        - |  9793 | ` * Return` |
|        - |  9794 | ` * NULL` |
|        - |  9795 | ` */` |
|        2 |  9796 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9797 |  |
|        3 |  9798 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9799 | `	SyBlob sDump;` |
|        3 |  9800 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9801 | `	/* Generate the backtrace */` |
|        3 |  9802 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9803 | `	/* Output backtrace */` |
|        3 |  9804 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9805 | `	/* All done,cleanup */` |
|        3 |  9806 | `	SyBlobRelease(&sDump);` |
|        1 |  9807 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9808 | `	SXUNUSED(apArg);` |
|        3 |  9809 | `	return PH7_OK;` |
|        1 |  9810 |  |
|        - |  9811 | `/*` |
|        - |  9812 | ` * string debug_string_backtrace()` |
|        - |  9813 | ` *  Generate a backtrace` |
|        - |  9814 | ` * Parameters` |
|        - |  9815 | ` * None` |
|        - |  9816 | ` * Return` |
|        - |  9817 | ` *  A mini backtrace().` |
|        - |  9818 | ` * Note that this is a symisc extension.` |
|        - |  9819 | ` */` |
|        2 |  9820 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9821 |  |
|        3 |  9822 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9823 | `	SyBlob sDump;` |
|        3 |  9824 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9825 | `	/* Generate the backtrace */` |
|        3 |  9826 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9827 | `	/* Return the backtrace */` |
|        3 |  9828 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9829 | `	/* All done,cleanup */` |
|        3 |  9830 | `	SyBlobRelease(&sDump);` |
|        1 |  9831 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9832 | `	SXUNUSED(apArg);` |
|        3 |  9833 | `	return PH7_OK;` |
|        1 |  9834 |  |
|        - |  9835 | `/*` |
|        - |  9836 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9837 | ` * exception is triggered.` |
|        - |  9838 | ` */` |
|      186 |  9839 | `static sxi32 VmUncaughtException(` |
|        - |  9840 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9841 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9842 | `	)` |
|        1 |  9843 |  |
|        - |  9844 | `	ph7_value *apArg[2],sArg;` |
|      187 |  9845 | `	int nArg = 1;` |
|        - |  9846 | `	sxi32 rc;` |
|      187 |  9847 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9848 | `		/* Nesting limit reached */` |
|      ! 0 |  9849 | `		return SXRET_OK;` |
|        - |  9850 | `	}` |
|        - |  9851 | `	/* Call any exception handler if available */` |
|      187 |  9852 | `	PH7_MemObjInit(pVm,&sArg);` |
|      187 |  9853 | `	if( pThis ){` |
|        - |  9854 | `		/* Load the exception instance */` |
|      187 |  9855 | `		sArg.x.pOther = pThis;` |
|      187 |  9856 | `		pThis->iRef++;` |
|      187 |  9857 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|       94 |  9858 | `	}else{` |
|      ! 0 |  9859 | `		nArg = 0;` |
|        - |  9860 | `	}` |
|      187 |  9861 | `	apArg[0] = &sArg;` |
|        - |  9862 | `	/* Call the exception handler if available */` |
|      187 |  9863 | `	pVm->nExceptDepth++;` |
|      187 |  9864 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      187 |  9865 | `	pVm->nExceptDepth--;` |
|      187 |  9866 | `	if( rc != SXRET_OK ){` |
|        - |  9867 | `		SyBlob sMsgBuf;` |
|      185 |  9868 | `		const char *zClass = "Exception";` |
|      185 |  9869 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9870 | `		const char *zMsg;` |
|        - |  9871 | `		sxu32 nMsg;` |
|        - |  9872 | `		const char *zFuncName;` |
|        - |  9873 | `		int nFuncLen;` |
|      185 |  9874 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      185 |  9875 | `		if( pThis ){` |
|        - |  9876 | `			ph7_class_method *pGetMessage;` |
|        - |  9877 | `			ph7_value sMsg;` |
|        - |  9878 | `			const char *zTmp;` |
|        - |  9879 | `			int nTmp;` |
|      185 |  9880 | `			zClass = pThis->pClass->sName.zString;` |
|      185 |  9881 | `			nClass = pThis->pClass->sName.nByte;` |
|      185 |  9882 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      185 |  9883 | `			if( pGetMessage ){` |
|      185 |  9884 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      185 |  9885 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      185 |  9886 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      185 |  9887 | `					if( zTmp && nTmp > 0 ){` |
|      185 |  9888 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|       92 |  9889 | `					}` |
|       92 |  9890 | `				}` |
|      185 |  9891 | `				PH7_MemObjRelease(&sMsg);` |
|       92 |  9892 | `			}` |
|       92 |  9893 | `		}` |
|      185 |  9894 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9895 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9896 | `		}` |
|      185 |  9897 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      185 |  9898 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      185 |  9899 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      185 |  9900 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      185 |  9901 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9902 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      185 |  9903 | `		rc = SXERR_ABORT;` |
|       92 |  9904 | `	}` |
|      187 |  9905 | `	PH7_MemObjRelease(&sArg);` |
|      187 |  9906 | `	return rc;` |
|       94 |  9907 |  |
|        - |  9908 | `/*` |
|        - |  9909 | ` * Throw an user exception.` |
|        - |  9910 | ` */` |
|      200 |  9911 | `static sxi32 VmThrowException(` |
|        - |  9912 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9913 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9914 | `	)` |
|        2 |  9915 |  |
|        - |  9916 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9917 | `	ph7_exception **apException;` |
|        - |  9918 | `	ph7_exception *pException;` |
|        - |  9919 | `	/* Point to the stack of loaded exceptions */` |
|      202 |  9920 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      202 |  9921 | `	pException = 0;` |
|      202 |  9922 | `	pCatch = 0;` |
|      202 |  9923 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9924 | `		ph7_exception_block *aCatch;` |
|        - |  9925 | `		ph7_class *pClass;` |
|        - |  9926 | `		sxu32 j;` |
|        - |  9927 | `		/* Locate the appropriate block to execute */` |
|       16 |  9928 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9929 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9930 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9931 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9932 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9933 | `			/* Extract the target class */` |
|       16 |  9934 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9935 | `			if( pClass == 0 ){` |
|        - |  9936 | `				/* No such class */` |
|      ! 0 |  9937 | `				continue;` |
|        - |  9938 | `			}` |
|       16 |  9939 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9940 | `				/* Catch block found,break immeditaley */` |
|       16 |  9941 | `				pCatch = &aCatch[j];` |
|       16 |  9942 | `				break;` |
|        - |  9943 | `			}` |
|      ! 0 |  9944 | `		}` |
|        7 |  9945 | `	}` |
|        - |  9946 | `	/* Execute the cached block if available */` |
|      202 |  9947 | `	if( pCatch == 0 ){` |
|        - |  9948 | `		sxi32 rc;` |
|      187 |  9949 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      187 |  9950 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9951 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9952 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9953 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9954 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9955 | `			}` |
|      ! 0 |  9956 | `			if( pException->pFrame == pFrame ){` |
|        - |  9957 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9958 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9959 | `			}` |
|      ! 0 |  9960 | `		}` |
|      187 |  9961 | `		return rc;` |
|      ! 0 |  9962 | `	}else{` |
|       16 |  9963 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9964 | `		sxi32 rc;` |
|       24 |  9965 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9966 | `			/* Safely ignore the exception frame */` |
|       10 |  9967 | `			pFrame = pFrame->pParent;` |
|        2 |  9968 | `		}` |
|       16 |  9969 | `		if( pException->pFrame == pFrame ){` |
|        - |  9970 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9971 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9972 | `		}` |
|        - |  9973 | `		/* Create a private frame first */` |
|       16 |  9974 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9975 | `		if( rc == SXRET_OK ){` |
|        - |  9976 | `			/* Mark as catch frame */` |
|       16 |  9977 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9978 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9979 | `			if( pObj ){` |
|        - |  9980 | `				/* Install the exception instance */` |
|       16 |  9981 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9982 | `				pObj->x.pOther = pThis;` |
|       16 |  9983 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9984 | `			}` |
|        - |  9985 | `			/* Exceute the block */` |
|       16 |  9986 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9987 | `			/* Leave the frame */` |
|       16 |  9988 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9989 | `		}` |
|        - |  9990 | `	}` |
|        - |  9991 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9992 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9993 | `	 */` |
|       16 |  9994 | `	return SXRET_OK;` |
|      102 |  9995 |  |
|        - |  9996 | `/*` |
|        - |  9997 | ` * Section:` |
|        - |  9998 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9999 | ` * Status:` |
|        - | 10000 | ` *    Stable.` |
|        - | 10001 | ` */` |
|        - | 10002 | `/*` |
|        - | 10003 | ` * string ph7version(void)` |
|        - | 10004 | ` *  Returns the running version of the PH7 version.` |
|        - | 10005 | ` * Parameters` |
|        - | 10006 | ` *  None` |
|        - | 10007 | ` * Return` |
|        - | 10008 | ` * Current PH7 version.` |
|        - | 10009 | ` */` |
|        2 | 10010 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10011 |  |
|        1 | 10012 | `	SXUNUSED(nArg);` |
|        1 | 10013 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10014 | `	/* Current engine version */` |
|        3 | 10015 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10016 | `	return PH7_OK;` |
|        1 | 10017 |  |
|        - | 10018 | `/*` |
|        - | 10019 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10020 | ` */` |
|        - | 10021 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10022 | ` "<html><head>"\` |
|        - | 10023 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10024 | ` "<style type=\"text/css\">"\` |
|        - | 10025 | ` "div {"\` |
|        - | 10026 | `     "border: 1px solid #cccccc;"\` |
|        - | 10027 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10028 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10029 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10030 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10031 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10032 | `     "-o-border-radius: 10px;"\` |
|        - | 10033 | `     "border-radius: 10px;"\` |
|        - | 10034 | `     "padding-left: 2em;"\` |
|        - | 10035 | `     "background-color: white;"\` |
|        - | 10036 | `     "margin-left: auto;"\` |
|        - | 10037 | `     "font-family: verdana;"\` |
|        - | 10038 | `     "padding-right: 2em;"\` |
|        - | 10039 | `     "margin-right: auto;"\` |
|        - | 10040 | `     "}"\` |
|        - | 10041 | `     "body {"\` |
|        - | 10042 | `     "padding: 0.2em;"\` |
|        - | 10043 | `     "font-style: normal;"\` |
|        - | 10044 | `     "font-size: medium;"\` |
|        - | 10045 | `     "background-color: #f2f2f2;"\` |
|        - | 10046 | `     "}"\` |
|        - | 10047 | `     "hr {"\` |
|        - | 10048 | `     "border-style: solid none none;"\` |
|        - | 10049 | `     "border-width: 1px medium medium;"\` |
|        - | 10050 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10051 | `     "height: 1px;"\` |
|        - | 10052 | `     "}"\` |
|        - | 10053 | `     "a {"\` |
|        - | 10054 | `     "color: #3366cc;"\` |
|        - | 10055 | `     "text-decoration: none;"\` |
|        - | 10056 | `     "}"\` |
|        - | 10057 | `     "a:hover {"\` |
|        - | 10058 | `     "color: #999999;"\` |
|        - | 10059 | `     "}"\` |
|        - | 10060 | `     "a:active {"\` |
|        - | 10061 | `     "color: #663399;"\` |
|        - | 10062 | `     "}"\` |
|        - | 10063 | `     "h1 {"\` |
|        - | 10064 | `     "margin: 0;"\` |
|        - | 10065 | `     "padding: 0;"\` |
|        - | 10066 | `     "font-family: Verdana;"\` |
|        - | 10067 | `     "font-weight: bold;"\` |
|        - | 10068 | `     "font-style: normal;"\` |
|        - | 10069 | `     "font-size: medium;"\` |
|        - | 10070 | `     "text-transform: capitalize;"\` |
|        - | 10071 | `     "color: #0a328c;"\` |
|        - | 10072 | `     "}"\` |
|        - | 10073 | `     "p {"\` |
|        - | 10074 | `     "margin: 0 auto;"\` |
|        - | 10075 | `     "font-size: medium;"\` |
|        - | 10076 | `     "font-style: normal;"\` |
|        - | 10077 | `     "font-family: verdana;"\` |
|        - | 10078 | `     "}"\` |
|        - | 10079 | `"</style></head><body>"\` |
|        - | 10080 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10081 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10082 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10083 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10084 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10085 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10086 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10087 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10088 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10089 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10090 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10091 |  |
|        - | 10092 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10093 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10094 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10095 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10096 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10097 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10098 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10099 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10100 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10101 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10102 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10103 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10104 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10105 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10106 |  |
|        - | 10107 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10108 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10109 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10110 | `"&nbsp;*<br>"\` |
|        - | 10111 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10112 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10113 | `"&nbsp;* are met:<br>"\` |
|        - | 10114 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10115 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10116 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10117 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10118 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10119 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10120 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10121 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10122 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10123 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10124 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10125 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10126 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10127 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10128 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10129 | `"&nbsp;*<br>"\` |
|        - | 10130 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10131 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10132 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10133 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10134 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10135 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10136 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10137 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10138 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10139 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10140 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10141 | `"&nbsp;*/<br>"\` |
|        - | 10142 | `"</span></small></small></p>"\` |
|        - | 10143 | `"</div></body></html>"` |
|        - | 10144 | `/*` |
|        - | 10145 | ` * bool ph7credits(void)` |
|        - | 10146 | ` * bool ph7info(void)` |
|        - | 10147 | ` * bool ph7copyright(void)` |
|        - | 10148 | ` *  Prints out the credits for PH7 engine` |
|        - | 10149 | ` * Parameters` |
|        - | 10150 | ` *  None` |
|        - | 10151 | ` * Return` |
|        - | 10152 | ` *  Always TRUE` |
|        - | 10153 | ` */` |
|        2 | 10154 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10155 |  |
|        3 | 10156 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10157 | `	/* Expand the HTML page above*/` |
|        3 | 10158 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10159 | `	ph7_context_output_format(` |
|        1 | 10160 | `		pCtx,` |
|        - | 10161 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10162 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10163 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10164 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10165 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10166 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10167 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10168 | `#ifdef __WINNT__` |
|        - | 10169 | `		"Windows NT"` |
|        - | 10170 | `#elif defined(__UNIXES__)` |
|        - | 10171 | `		"UNIX-Like"` |
|        - | 10172 | `#else` |
|        - | 10173 | `		"Other OS"` |
|        - | 10174 | `#endif` |
|        - | 10175 | `		);` |
|        3 | 10176 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10177 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10178 | `	SXUNUSED(apArg);` |
|        - | 10179 | `	/* Return TRUE */` |
|        - | 10180 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10181 | `	return PH7_OK;` |
|        1 | 10182 |  |
|        - | 10183 | `/*` |
|        - | 10184 | ` * Section:` |
|        - | 10185 | ` *    URL related routines.` |
|        - | 10186 | ` * Status:` |
|        - | 10187 | ` *    Stable.` |
|        - | 10188 | ` */` |
|        - | 10189 | `/* Forward declaration */` |
|        - | 10190 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10191 | `/*` |
|        - | 10192 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10193 | ` *  Parse a URL and return its fields.` |
|        - | 10194 | ` * Parameters` |
|        - | 10195 | ` *  $url` |
|        - | 10196 | ` *   The URL to parse.` |
|        - | 10197 | ` * $component` |
|        - | 10198 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10199 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10200 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10201 | ` *  in which case the return value will be an integer).` |
|        - | 10202 | ` * Return` |
|        - | 10203 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10204 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10205 | ` *  this array are:` |
|        - | 10206 | ` *   scheme - e.g. http` |
|        - | 10207 | ` *   host` |
|        - | 10208 | ` *   port` |
|        - | 10209 | ` *   user` |
|        - | 10210 | ` *   pass` |
|        - | 10211 | ` *   path` |
|        - | 10212 | ` *   query - after the question mark ?` |
|        - | 10213 | ` *   fragment - after the hashmark #` |
|        - | 10214 | ` * Note:` |
|        - | 10215 | ` *  FALSE is returned on failure.` |
|        - | 10216 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10217 | ` *  with the standard PHP engine.` |
|        - | 10218 | ` */` |
|       28 | 10219 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10220 |  |
|        - | 10221 | `	const char *zStr; /* Input string */` |
|        - | 10222 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10223 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10224 | `	int nLen;` |
|        - | 10225 | `	sxi32 rc;` |
|       29 | 10226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10227 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10229 | `		return PH7_OK;` |
|        - | 10230 | `	}` |
|        - | 10231 | `	/* Extract the given URI */` |
|       29 | 10232 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10233 | `	if( nLen < 1 ){` |
|        - | 10234 | `		/* Nothing to process,return FALSE */` |
|        3 | 10235 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10236 | `		return PH7_OK;` |
|        - | 10237 | `	}` |
|        - | 10238 | `	/* Get a parse */` |
|       27 | 10239 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10240 | `	if( rc != SXRET_OK ){` |
|        - | 10241 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10242 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10243 | `		return PH7_OK;` |
|        - | 10244 | `	}` |
|       27 | 10245 | `	if( nArg > 1 ){` |
|      ! 0 | 10246 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10247 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10248 | `		switch(nComponent){` |
|      ! 0 | 10249 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10250 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10251 | `			if( pComp->nByte < 1 ){` |
|        - | 10252 | `				/* No available value,return NULL */` |
|      ! 0 | 10253 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10254 | `			}else{` |
|      ! 0 | 10255 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10256 | `			}` |
|      ! 0 | 10257 | `			break;` |
|      ! 0 | 10258 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10259 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10260 | `			if( pComp->nByte < 1 ){` |
|        - | 10261 | `				/* No available value,return NULL */` |
|      ! 0 | 10262 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10263 | `			}else{` |
|      ! 0 | 10264 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10265 | `			}` |
|      ! 0 | 10266 | `			break;` |
|      ! 0 | 10267 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10268 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10269 | `			if( pComp->nByte < 1 ){` |
|        - | 10270 | `				/* No available value,return NULL */` |
|      ! 0 | 10271 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10272 | `			}else{` |
|      ! 0 | 10273 | `				int iPort = 0;` |
|        - | 10274 | `				/* Cast the value to integer */` |
|      ! 0 | 10275 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10276 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10277 | `			}` |
|      ! 0 | 10278 | `			break;` |
|      ! 0 | 10279 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10280 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10281 | `			if( pComp->nByte < 1 ){` |
|        - | 10282 | `				/* No available value,return NULL */` |
|      ! 0 | 10283 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10284 | `			}else{` |
|      ! 0 | 10285 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10286 | `			}` |
|      ! 0 | 10287 | `			break;` |
|      ! 0 | 10288 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10289 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10290 | `			if( pComp->nByte < 1 ){` |
|        - | 10291 | `				/* No available value,return NULL */` |
|      ! 0 | 10292 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10293 | `			}else{` |
|      ! 0 | 10294 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10295 | `			}` |
|      ! 0 | 10296 | `			break;` |
|      ! 0 | 10297 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10298 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10299 | `			if( pComp->nByte < 1 ){` |
|        - | 10300 | `				/* No available value,return NULL */` |
|      ! 0 | 10301 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10302 | `			}else{` |
|      ! 0 | 10303 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10304 | `			}` |
|      ! 0 | 10305 | `			break;` |
|      ! 0 | 10306 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10307 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10308 | `			if( pComp->nByte < 1 ){` |
|        - | 10309 | `				/* No available value,return NULL */` |
|      ! 0 | 10310 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10311 | `			}else{` |
|      ! 0 | 10312 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10313 | `			}` |
|      ! 0 | 10314 | `			break;` |
|      ! 0 | 10315 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10316 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10317 | `			if( pComp->nByte < 1 ){` |
|        - | 10318 | `				/* No available value,return NULL */` |
|      ! 0 | 10319 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10320 | `			}else{` |
|      ! 0 | 10321 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10322 | `			}` |
|      ! 0 | 10323 | `			break;` |
|      ! 0 | 10324 | `		default:` |
|        - | 10325 | `			/* No such entry,return NULL */` |
|      ! 0 | 10326 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10327 | `			break;` |
|        - | 10328 | `		}` |
|      ! 0 | 10329 | `	}else{` |
|        - | 10330 | `		ph7_value *pArray,*pValue;` |
|        - | 10331 | `		/* Return an associative array */` |
|       27 | 10332 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10333 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10334 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10335 | `			/* Out of memory */` |
|      ! 0 | 10336 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10337 | `			/* Return false */` |
|      ! 0 | 10338 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10339 | `			return PH7_OK;` |
|        - | 10340 | `		}` |
|        - | 10341 | `		/* Fill the array */` |
|       27 | 10342 | `		pComp = &sURI.sScheme;` |
|       27 | 10343 | `		if( pComp->nByte > 0 ){` |
|       19 | 10344 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10345 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10346 | `		}` |
|        - | 10347 | `		/* Reset the string cursor */` |
|       27 | 10348 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10349 | `		pComp = &sURI.sHost;` |
|       27 | 10350 | `		if( pComp->nByte > 0 ){` |
|       25 | 10351 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10352 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10353 | `		}` |
|        - | 10354 | `		/* Reset the string cursor */` |
|       27 | 10355 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10356 | `		pComp = &sURI.sPort;` |
|       27 | 10357 | `		if( pComp->nByte > 0 ){` |
|       11 | 10358 | `			int iPort = 0;/* cc warning */` |
|        - | 10359 | `			/* Convert to integer */` |
|       11 | 10360 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10361 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10362 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10363 | `		}` |
|        - | 10364 | `		/* Reset the string cursor */` |
|       27 | 10365 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10366 | `		pComp = &sURI.sUser;` |
|       27 | 10367 | `		if( pComp->nByte > 0 ){` |
|        7 | 10368 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10369 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10370 | `		}` |
|        - | 10371 | `		/* Reset the string cursor */` |
|       27 | 10372 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10373 | `		pComp = &sURI.sPass;` |
|       27 | 10374 | `		if( pComp->nByte > 0 ){` |
|        7 | 10375 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10376 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10377 | `		}` |
|        - | 10378 | `		/* Reset the string cursor */` |
|       27 | 10379 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10380 | `		pComp = &sURI.sPath;` |
|       27 | 10381 | `		if( pComp->nByte > 0 ){` |
|       17 | 10382 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10383 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10384 | `		}` |
|        - | 10385 | `		/* Reset the string cursor */` |
|       27 | 10386 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10387 | `		pComp = &sURI.sQuery;` |
|       27 | 10388 | `		if( pComp->nByte > 0 ){` |
|        5 | 10389 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10390 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10391 | `		}` |
|        - | 10392 | `		/* Reset the string cursor */` |
|       27 | 10393 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10394 | `		pComp = &sURI.sFragment;` |
|       27 | 10395 | `		if( pComp->nByte > 0 ){` |
|        5 | 10396 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10397 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10398 | `		}` |
|        - | 10399 | `		/* Return the created array */` |
|       27 | 10400 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10401 | `		/* NOTE:` |
|        - | 10402 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10403 | `		 * automatically as soon we return from this function.` |
|        - | 10404 | `		 */` |
|        - | 10405 | `	}` |
|        - | 10406 | `	/* All done */` |
|       27 | 10407 | `	return PH7_OK;` |
|       15 | 10408 |  |
|        - | 10409 | `/*` |
|        - | 10410 | ` * Section:` |
|        - | 10411 | ` *   Array related routines.` |
|        - | 10412 | ` * Status:` |
|        - | 10413 | ` *    Stable.` |
|        - | 10414 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10415 | ` *  Array related functions that need access to the underlying` |
|        - | 10416 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10417 | ` */` |
|        - | 10418 | `/*` |
|        - | 10419 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10420 | ` * of the following structure.` |
|        - | 10421 | ` */` |
|        - | 10422 | `struct compact_data` |
|        - | 10423 |  |
|        - | 10424 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10425 | `	int nRecCount;      /* Recursion count */` |
|        - | 10426 | `};` |
|        - | 10427 | `/*` |
|        - | 10428 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10429 | ` */` |
|      ! 0 | 10430 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10431 |  |
|      ! 0 | 10432 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10433 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10434 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10435 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10436 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10437 | `		SyString sVar;` |
|      ! 0 | 10438 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10439 | `		if( sVar.nByte > 0 ){` |
|        - | 10440 | `			/* Query the current frame */` |
|      ! 0 | 10441 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10442 | `			/* ^` |
|        - | 10443 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10444 | `			 */` |
|      ! 0 | 10445 | `			if( pKey ){` |
|        - | 10446 | `				/* Perform the insertion */` |
|      ! 0 | 10447 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10448 | `			}` |
|      ! 0 | 10449 | `		}` |
|      ! 0 | 10450 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10451 | `		int rc;` |
|        - | 10452 | `		/* Recursively traverse this array */` |
|      ! 0 | 10453 | `		pData->nRecCount++;` |
|      ! 0 | 10454 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10455 | `		pData->nRecCount--;` |
|      ! 0 | 10456 | `		return rc;` |
|        - | 10457 | `	}` |
|      ! 0 | 10458 | `	return SXRET_OK;` |
|      ! 0 | 10459 |  |
|        - | 10460 | `/*` |
|        - | 10461 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10462 | ` *  Create array containing variables and their values.` |
|        - | 10463 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10464 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10465 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10466 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10467 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10468 | ` * Parameters` |
|        - | 10469 | ` *  $varname` |
|        - | 10470 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10471 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10472 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10473 | ` *   it recursively.` |
|        - | 10474 | ` * Return` |
|        - | 10475 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10476 | ` */` |
|        2 | 10477 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10478 |  |
|        - | 10479 | `	ph7_value *pArray,*pObj;` |
|        3 | 10480 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10481 | `	const char *zName;` |
|        - | 10482 | `	SyString sVar;` |
|        - | 10483 | `	int i,nLen;` |
|        3 | 10484 | `	if( nArg < 1 ){` |
|        - | 10485 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10486 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10487 | `		return PH7_OK;` |
|        - | 10488 | `	}` |
|        - | 10489 | `	/* Create the array */` |
|        3 | 10490 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10491 | `	if( pArray == 0 ){` |
|        - | 10492 | `		/* Out of memory */` |
|      ! 0 | 10493 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10494 | `		/* Return NULL */` |
|      ! 0 | 10495 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10496 | `		return PH7_OK;` |
|        - | 10497 | `	}` |
|        - | 10498 | `	/* Perform the requested operation */` |
|        7 | 10499 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10500 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10501 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10502 | `				struct compact_data sData;` |
|      ! 0 | 10503 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10504 | `				/* Recursively walk the array */` |
|      ! 0 | 10505 | `				sData.nRecCount = 0;` |
|      ! 0 | 10506 | `				sData.pArray = pArray;` |
|      ! 0 | 10507 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10508 | `			}` |
|      ! 0 | 10509 | `		}else{` |
|        - | 10510 | `			/* Extract variable name */` |
|        5 | 10511 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10512 | `			if( nLen > 0 ){` |
|        5 | 10513 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10514 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10515 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10516 | `				if( pObj ){` |
|        5 | 10517 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10518 | `				}` |
|        2 | 10519 | `			}` |
|        - | 10520 | `		}` |
|        3 | 10521 | `	}` |
|        - | 10522 | `	/* Return the array */` |
|        3 | 10523 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10524 | `	return PH7_OK;` |
|        2 | 10525 |  |
|        - | 10526 | `/*` |
|        - | 10527 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10528 | ` * of the following structure.` |
|        - | 10529 | ` */` |
|        - | 10530 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10531 | `struct extract_aux_data` |
|        - | 10532 |  |
|        - | 10533 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10534 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10535 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10536 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10537 | `	int iFlags;           /* Control flags */` |
|        - | 10538 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10539 | `};` |
|        - | 10540 | `/* Forward declaration */` |
|        - | 10541 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10542 | `/*` |
|        - | 10543 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10544 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10545 | ` * Parameters` |
|        - | 10546 | ` * $var_array` |
|        - | 10547 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10548 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10549 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10550 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10551 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10552 | ` * $extract_type` |
|        - | 10553 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10554 | ` *  It can be one of the following values:` |
|        - | 10555 | ` *   EXTR_OVERWRITE` |
|        - | 10556 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10557 | ` *   EXTR_SKIP` |
|        - | 10558 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10559 | ` *   EXTR_PREFIX_SAME` |
|        - | 10560 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10561 | ` *   EXTR_PREFIX_ALL` |
|        - | 10562 | ` *       Prefix all variable names with prefix.` |
|        - | 10563 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10564 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10565 | ` *   EXTR_IF_EXISTS` |
|        - | 10566 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10567 | ` *       otherwise do nothing.` |
|        - | 10568 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10569 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10570 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10571 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10572 | ` *      the current symbol table.` |
|        - | 10573 | ` * $prefix` |
|        - | 10574 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10575 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10576 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10577 | ` *  underscore character.` |
|        - | 10578 | ` * Return` |
|        - | 10579 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10580 | ` */` |
|        4 | 10581 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10582 |  |
|        - | 10583 | `	extract_aux_data sAux;` |
|        - | 10584 | `	ph7_hashmap *pMap;` |
|        5 | 10585 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10586 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10587 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10588 | `		return PH7_OK;` |
|        - | 10589 | `	}` |
|        - | 10590 | `	/* Point to the target hashmap */` |
|        5 | 10591 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10592 | `	if( pMap->nEntry < 1 ){` |
|        - | 10593 | `		/* Empty map,return  0 */` |
|      ! 0 | 10594 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10595 | `		return PH7_OK;` |
|        - | 10596 | `	}` |
|        - | 10597 | `	/* Prepare the aux data */` |
|        5 | 10598 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10599 | `	if( nArg > 1 ){` |
|        3 | 10600 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10601 | `		if( nArg > 2 ){` |
|      ! 0 | 10602 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10603 | `		}` |
|        1 | 10604 | `	}` |
|        5 | 10605 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10606 | `	/* Invoke the worker callback */` |
|        5 | 10607 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10608 | `	/* Number of variables successfully imported */` |
|        5 | 10609 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10610 | `	return PH7_OK;` |
|        3 | 10611 |  |
|        - | 10612 | `/*` |
|        - | 10613 | ` * Worker callback for the [extract()] function defined` |
|        - | 10614 | ` * below.` |
|        - | 10615 | ` */` |
|        8 | 10616 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10617 |  |
|        9 | 10618 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10619 | `	int iFlags = pAux->iFlags;` |
|        9 | 10620 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10621 | `	ph7_value *pObj;` |
|        - | 10622 | `	SyString sVar;` |
|        9 | 10623 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10624 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10625 | `	}` |
|        - | 10626 | `	/* Perform a string cast */` |
|        9 | 10627 | `	PH7_MemObjToString(pKey);` |
|        9 | 10628 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10629 | `		/* Unavailable variable name */` |
|      ! 0 | 10630 | `		return SXRET_OK;` |
|        - | 10631 | `	}` |
|        9 | 10632 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10633 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10634 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10635 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10636 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10637 | `			);` |
|      ! 0 | 10638 | `	}else{` |
|       13 | 10639 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10640 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10641 | `	}` |
|        9 | 10642 | `	sVar.zString = pAux->zWorker;` |
|        - | 10643 | `	/* Try to extract the variable */` |
|        9 | 10644 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10645 | `	if( pObj ){` |
|        - | 10646 | `		/* Collision */` |
|        3 | 10647 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10648 | `			return SXRET_OK;` |
|        - | 10649 | `		}` |
|        3 | 10650 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10651 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10652 | `				/* Already prefixed */` |
|      ! 0 | 10653 | `				return SXRET_OK;` |
|        - | 10654 | `			}` |
|      ! 0 | 10655 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10656 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10657 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10658 | `				);` |
|      ! 0 | 10659 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10660 | `		}` |
|        2 | 10661 | `	}else{` |
|        - | 10662 | `		/* Create the variable */` |
|        7 | 10663 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10664 | `	}` |
|        9 | 10665 | `	if( pObj ){` |
|        - | 10666 | `		/* Overwrite the old value */` |
|        9 | 10667 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10668 | `		/* Increment counter */` |
|        9 | 10669 | `		pAux->iCount++;` |
|        4 | 10670 | `	}` |
|        9 | 10671 | `	return SXRET_OK;` |
|        5 | 10672 |  |
|        - | 10673 | `/*` |
|        - | 10674 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10675 | ` * defined below.` |
|        - | 10676 | ` */` |
|        2 | 10677 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10678 |  |
|        3 | 10679 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10680 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10681 | `	ph7_value *pObj;` |
|        - | 10682 | `	SyString sVar;` |
|        - | 10683 | `	/* Perform a string cast */` |
|        3 | 10684 | `	PH7_MemObjToString(pKey);` |
|        3 | 10685 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10686 | `		/* Unavailable variable name */` |
|      ! 0 | 10687 | `		return SXRET_OK;` |
|        - | 10688 | `	}` |
|        3 | 10689 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10690 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10691 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10692 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10693 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10694 | `			);` |
|        2 | 10695 | `	}else{` |
|      ! 0 | 10696 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10697 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10698 | `	}` |
|        3 | 10699 | `	sVar.zString = pAux->zWorker;` |
|        - | 10700 | `	/* Extract the variable */` |
|        3 | 10701 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10702 | `	if( pObj ){` |
|        3 | 10703 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10704 | `	}` |
|        3 | 10705 | `	return SXRET_OK;` |
|        2 | 10706 |  |
|        - | 10707 | `/*` |
|        - | 10708 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10709 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10710 | ` * Parameters` |
|        - | 10711 | ` * $types` |
|        - | 10712 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10713 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10714 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10715 | ` *  POST includes the POST uploaded file information.` |
|        - | 10716 | ` *  Note:` |
|        - | 10717 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10718 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10719 | ` * $prefix` |
|        - | 10720 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10721 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10722 | ` *  variable named $pref_userid.` |
|        - | 10723 | ` * Return` |
|        - | 10724 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10725 | ` */` |
|        2 | 10726 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10727 |  |
|        - | 10728 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10729 | `	extract_aux_data sAux;` |
|        - | 10730 | `	int nLen,nPrefixLen;` |
|        - | 10731 | `	ph7_value *pSuper;` |
|        - | 10732 | `	ph7_vm *pVm;` |
|        - | 10733 | `	/* By default import only $_GET variables  */` |
|        3 | 10734 | `	zImport = "G";` |
|        3 | 10735 | `	nLen = (int)sizeof(char);` |
|        3 | 10736 | `	zPrefix = 0;` |
|        3 | 10737 | `	nPrefixLen = 0;` |
|        3 | 10738 | `	if( nArg > 0 ){` |
|        3 | 10739 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10740 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10741 | `		}` |
|        3 | 10742 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10743 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10744 | `		}` |
|        1 | 10745 | `	}` |
|        - | 10746 | `	/* Point to the underlying VM */` |
|        3 | 10747 | `	pVm = pCtx->pVm;` |
|        - | 10748 | `	/* Initialize the aux data */` |
|        3 | 10749 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10750 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10751 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10752 | `	sAux.pVm = pVm;` |
|        - | 10753 | `	/* Extract */` |
|        3 | 10754 | `	zEnd = &zImport[nLen];` |
|        5 | 10755 | `	while( zImport < zEnd ){` |
|        3 | 10756 | `		int c = zImport[0];` |
|        3 | 10757 | `		pSuper = 0;` |
|        3 | 10758 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10759 | `			/* Import $_GET variables */` |
|        3 | 10760 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10761 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10762 | `			/* Import $_POST variables */` |
|      ! 0 | 10763 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10764 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10765 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10766 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10767 | `		}` |
|        3 | 10768 | `		if( pSuper ){` |
|        - | 10769 | `			/* Iterate throw array entries */` |
|        3 | 10770 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10771 | `		}` |
|        - | 10772 | `		/* Advance the cursor */` |
|        3 | 10773 | `		zImport++;` |
|        1 | 10774 | `	}` |
|        - | 10775 | `	/* All done,return TRUE*/` |
|        3 | 10776 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10777 | `	return PH7_OK;` |
|        1 | 10778 |  |
|        - | 10779 | `/*` |
|        - | 10780 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10781 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10782 | ` * information.` |
|        - | 10783 | ` */` |
|     8444 | 10784 | `static sxi32 VmEvalChunk(` |
|        - | 10785 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10786 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10787 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10788 | `	int iFlags,         /* Compile flag */` |
|        - | 10789 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10790 | `	)` |
|        2 | 10791 |  |
|        - | 10792 | `	SySet *pByteCode,aByteCode;` |
|     8446 | 10793 | `	ProcConsumer xErr = 0;` |
|     8446 | 10794 | `	void *pErrData = 0;` |
|        - | 10795 | `	/* Initialize bytecode container */` |
|     8446 | 10796 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     8446 | 10797 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10798 | `	/* Reset the code generator */` |
|     8446 | 10799 | `	if( bTrueReturn ){` |
|        - | 10800 | `		/* Included file,log compile-time errors */` |
|     6865 | 10801 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6865 | 10802 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3432 | 10803 | `	}` |
|     8446 | 10804 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10805 | `	/* Swap bytecode container */` |
|     8446 | 10806 | `	pByteCode = pVm->pByteContainer;` |
|     8446 | 10807 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10808 | `	/* Compile the chunk */` |
|     8446 | 10809 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    12668 | 10810 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10811 | `		/* Compilation error,return false */` |
|        3 | 10812 | `		if( pCtx ){` |
|        3 | 10813 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10814 | `		}` |
|        2 | 10815 | `	}else{` |
|        - | 10816 | `		/* Mount any newly defined classes */` |
|        - | 10817 | `		SyHashEntry *pEntry;` |
|        - | 10818 | `		ph7_class *pClass;` |
|        - | 10819 | `		ph7_value sResult; /* Return value */` |
|        - | 10820 | `		sxi32 rc;` |
|     8444 | 10821 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   242949 | 10822 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   230286 | 10823 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10824 | `			/* Only mount classes that haven't been mounted yet */` |
|   230286 | 10825 | `			if( !pClass->bMounted ){` |
|    47160 | 10826 | `				rc = VmMountUserClass(pVm,pClass);` |
|    47160 | 10827 | `				if( rc != SXRET_OK ){` |
|        - | 10828 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10829 | `					if( pCtx ){` |
|      ! 0 | 10830 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10831 | `					}` |
|      ! 0 | 10832 | `					goto Cleanup;` |
|        - | 10833 | `				}` |
|    23579 | 10834 | `			}` |
|        2 | 10835 | `		}` |
|     8444 | 10836 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10837 | `			/* Out of memory */` |
|      ! 0 | 10838 | `			if( pCtx ){` |
|      ! 0 | 10839 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10840 | `			}` |
|      ! 0 | 10841 | `			goto Cleanup;` |
|        - | 10842 | `		}` |
|     8444 | 10843 | `		if( bTrueReturn ){` |
|        - | 10844 | `			/* Assume a boolean true return value */` |
|     6865 | 10845 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3433 | 10846 | `		}else{` |
|        - | 10847 | `			/* Assume a null return value */` |
|     1580 | 10848 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10849 | `		}` |
|        - | 10850 | `		/* Execute the compiled chunk */` |
|     8444 | 10851 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     8444 | 10852 | `		if( pCtx ){` |
|        - | 10853 | `			/* Set the execution result */` |
|     6882 | 10854 | `			ph7_result_value(pCtx,&sResult);` |
|     3440 | 10855 | `		}` |
|     8444 | 10856 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10857 | `	}` |
|     4222 | 10858 | `Cleanup:` |
|        - | 10859 | `	/* Cleanup the mess left behind */` |
|     8446 | 10860 | `	pVm->pByteContainer = pByteCode;` |
|     8446 | 10861 | `	SySetRelease(&aByteCode);` |
|     8446 | 10862 | `	return SXRET_OK;` |
|        2 | 10863 |  |
|        - | 10864 | `/*` |
|        - | 10865 | ` * value eval(string $code)` |
|        - | 10866 | ` *   Evaluate a string as PHP code.` |
|        - | 10867 | ` * Parameter` |
|        - | 10868 | ` *  code: PHP code to evaluate.` |
|        - | 10869 | ` * Return` |
|        - | 10870 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10871 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10872 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10873 | ` */` |
|       16 | 10874 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10875 |  |
|        - | 10876 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10877 | `	if( nArg < 1 ){` |
|        - | 10878 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10879 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10880 | `		return SXRET_OK;` |
|        - | 10881 | `	}` |
|        - | 10882 | `	/* Chunk to evaluate */` |
|       18 | 10883 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10884 | `	if( sChunk.nByte < 1 ){` |
|        - | 10885 | `		/* Empty string,return NULL */` |
|        3 | 10886 | `		ph7_result_null(pCtx);` |
|        3 | 10887 | `		return SXRET_OK;` |
|        - | 10888 | `	}` |
|        - | 10889 | `	/* Eval the chunk */` |
|       16 | 10890 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10891 | `	return SXRET_OK;` |
|       10 | 10892 |  |
|        - | 10893 | `/*` |
|        - | 10894 | ` * Check if a file path is already included.` |
|        - | 10895 | ` */` |
|    13724 | 10896 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10897 |  |
|        - | 10898 | `	SyString *aEntries;` |
|        - | 10899 | `	sxu32 n;` |
|    13725 | 10900 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10901 | `	/* Perform a linear search */` |
| 47076197 | 10902 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 47062479 | 10903 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10904 | `			/* Already included */` |
|        7 | 10905 | `			return TRUE;` |
|        - | 10906 | `		}` |
| 23531237 | 10907 | `	}` |
|    13719 | 10908 | `	return FALSE;` |
|     6863 | 10909 |  |
|        - | 10910 | `/*` |
|        - | 10911 | ` * Push a file path in the appropriate VM container.` |
|        - | 10912 | ` */` |
|    15278 | 10913 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10914 |  |
|        - | 10915 | `	SyString sPath;` |
|        - | 10916 | `	char *zDup;` |
|        - | 10917 | `#ifdef __WINNT__` |
|        - | 10918 | `	char *zCur;` |
|        - | 10919 | `#endif` |
|        - | 10920 | `	sxi32 rc;` |
|    15280 | 10921 | `	if( nLen < 0 ){` |
|     1556 | 10922 | `		nLen = SyStrlen(zPath);` |
|      777 | 10923 | `	}` |
|        - | 10924 | `	/* Duplicate the file path first */` |
|    15280 | 10925 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    15280 | 10926 | `	if( zDup == 0 ){` |
|      ! 0 | 10927 | `		return SXERR_MEM;` |
|        - | 10928 | `	}` |
|        - | 10929 | `#ifdef __WINNT__` |
|        - | 10930 | `	/* Normalize path on windows` |
|        - | 10931 | `	 * Example:` |
|        - | 10932 | `	 *    Path/To/File.php` |
|        - | 10933 | `	 * becomes` |
|        - | 10934 | `	 *   path\to\file.php` |
|        - | 10935 | `	 */` |
|        2 | 10936 | `	zCur = zDup;` |
|        2 | 10937 | `	while( zCur[0] != 0 ){` |
|        2 | 10938 | `		if( zCur[0] == '/' ){` |
|        2 | 10939 | `			zCur[0] = '\\';` |
|        2 | 10940 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10941 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10942 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10943 | `		}` |
|        2 | 10944 | `		zCur++;` |
|        2 | 10945 | `	}` |
|        - | 10946 | `#endif` |
|        - | 10947 | `	/* Install the file path */` |
|    15280 | 10948 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    15280 | 10949 | `	if( !bMain ){` |
|    13725 | 10950 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10951 | `			/* Already included */` |
|        7 | 10952 | `			*pNew = 0;` |
|        4 | 10953 | `		}else{` |
|        - | 10954 | `			/* Insert in the corresponding container */` |
|    13719 | 10955 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    13719 | 10956 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10957 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10958 | `				return rc;` |
|        - | 10959 | `			}` |
|    13719 | 10960 | `			*pNew = 1;` |
|        - | 10961 | `		}` |
|     6862 | 10962 | `	}` |
|    15280 | 10963 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    15280 | 10964 | `	return SXRET_OK;` |
|     7641 | 10965 |  |
|        - | 10966 | `/*` |
|        - | 10967 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10968 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10969 | ` * indicates failure.` |
|        - | 10970 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10971 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10972 | ` * operations.` |
|        - | 10973 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10974 | ` * this function is a no-op.` |
|        - | 10975 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10976 | ` * constructs for more information.` |
|        - | 10977 | ` */` |
|     6870 | 10978 | `static sxi32 VmExecIncludedFile(` |
|        - | 10979 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10980 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10981 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10982 | `	 )` |
|        2 | 10983 |  |
|        - | 10984 | `	sxi32 rc;` |
|        - | 10985 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10986 | `	const ph7_io_stream *pStream;` |
|        - | 10987 | `	SyBlob sContents;` |
|        - | 10988 | `	void *pHandle;` |
|        - | 10989 | `	ph7_vm *pVm;` |
|        - | 10990 | `	int isNew;` |
|        - | 10991 | `	/* Initialize fields */` |
|     6872 | 10992 | `	pVm = pCtx->pVm;` |
|     6872 | 10993 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6872 | 10994 | `	isNew = 0;` |
|        - | 10995 | `	/* Extract the associated stream */` |
|     6872 | 10996 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10997 | `	/*` |
|        - | 10998 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10999 | `	 * in a read-only mode.` |
|        - | 11000 | `	 */` |
|     6872 | 11001 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6872 | 11002 | `	if( pHandle == 0 ){` |
|        3 | 11003 | `		return SXERR_IO;` |
|        - | 11004 | `	}` |
|     6869 | 11005 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6869 | 11006 | `	if( IncludeOnce && !isNew ){` |
|        - | 11007 | `		/* Already included */` |
|        5 | 11008 | `		rc = SXERR_EXISTS;` |
|        3 | 11009 | `	}else{` |
|        - | 11010 | `		/* Read the whole file contents */` |
|     6865 | 11011 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6865 | 11012 | `		if( rc == SXRET_OK ){` |
|        - | 11013 | `			SyString sScript;` |
|        - | 11014 | `			/* Compile and execute the script */` |
|     6865 | 11015 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6865 | 11016 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3432 | 11017 | `		}` |
|        - | 11018 | `	}` |
|        - | 11019 | `	/* Pop from the set of included file */` |
|     6869 | 11020 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11021 | `	/* Close the handle */` |
|     6869 | 11022 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11023 | `	/* Release the working buffer */` |
|     6869 | 11024 | `	SyBlobRelease(&sContents);` |
|        - | 11025 | `#else` |
|        - | 11026 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11027 | `	SXUNUSED(pPath);` |
|        - | 11028 | `	SXUNUSED(IncludeOnce);` |
|        - | 11029 | `	rc = SXERR_IO;` |
|        - | 11030 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6869 | 11031 | `	return rc;` |
|     3437 | 11032 |  |
|        - | 11033 | `/*` |
|        - | 11034 | ` * string get_include_path(void)` |
|        - | 11035 | ` *  Gets the current include_path configuration option.` |
|        - | 11036 | ` * Parameter` |
|        - | 11037 | ` *  None` |
|        - | 11038 | ` * Return` |
|        - | 11039 | ` *  Included paths as a string` |
|        - | 11040 | ` */` |
|        2 | 11041 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11042 |  |
|        3 | 11043 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11044 | `	SyString *aEntry;` |
|        - | 11045 | `	int dir_sep;` |
|        - | 11046 | `	sxu32 n;` |
|        - | 11047 | `#ifdef __WINNT__` |
|        1 | 11048 | `	dir_sep = ';';` |
|        - | 11049 | `#else` |
|        - | 11050 | `	/* Assume UNIX path separator */` |
|        2 | 11051 | `	dir_sep = ':';` |
|        - | 11052 | `#endif` |
|        1 | 11053 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11054 | `	SXUNUSED(apArg);` |
|        - | 11055 | `	/* Point to the list of import paths */` |
|        3 | 11056 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11057 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11058 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11059 | `		if( n > 0 ){` |
|        - | 11060 | `			/* Append dir seprator */` |
|      ! 0 | 11061 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11062 | `		}` |
|        - | 11063 | `		/* Append path */` |
|        3 | 11064 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11065 | `	}` |
|        3 | 11066 | `	return PH7_OK;` |
|        1 | 11067 |  |
|        - | 11068 | `/*` |
|        - | 11069 | ` * string get_get_included_files(void)` |
|        - | 11070 | ` *  Gets the current include_path configuration option.` |
|        - | 11071 | ` * Parameter` |
|        - | 11072 | ` *  None` |
|        - | 11073 | ` * Return` |
|        - | 11074 | ` *  Included paths as a string` |
|        - | 11075 | ` */` |
|        2 | 11076 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11077 |  |
|        3 | 11078 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11079 | `	ph7_value *pArray,*pWorker;` |
|        - | 11080 | `	SyString *pEntry;` |
|        - | 11081 | `	int c,d;` |
|        - | 11082 | `	/* Create an array and a working value */` |
|        3 | 11083 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11084 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11085 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11086 | `		/* Out of memory,return null */` |
|      ! 0 | 11087 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11088 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11089 | `		SXUNUSED(apArg);` |
|      ! 0 | 11090 | `		return PH7_OK;` |
|        - | 11091 | `	}` |
|        3 | 11092 | `	c = d = '/';` |
|        - | 11093 | `#ifdef __WINNT__` |
|        1 | 11094 | `	d = '\\';` |
|        - | 11095 | `#endif` |
|        - | 11096 | `	/* Iterate throw entries */` |
|        3 | 11097 | `	SySetResetCursor(pFiles);` |
|     3079 | 11098 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11099 | `		const char *zBase,*zEnd;` |
|        - | 11100 | `		int iLen;` |
|        - | 11101 | `		/* reset the string cursor */` |
|     3077 | 11102 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11103 | `		/* Extract base name */` |
|     3077 | 11104 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11105 | `		/* Ignore trailing '/' */` |
|     4615 | 11106 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11107 | `			zEnd--;` |
|      ! 0 | 11108 | `		}` |
|     3077 | 11109 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    90061 | 11110 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    85447 | 11111 | `			zEnd--;` |
|        1 | 11112 | `		}` |
|     3077 | 11113 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3077 | 11114 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11115 | `		/* Copy entry name */` |
|     3077 | 11116 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11117 | `		/* Perform the insertion */` |
|     3077 | 11118 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11119 | `	}` |
|        - | 11120 | `	/* All done,return the created array */` |
|        3 | 11121 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11122 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11123 | `	 * by the engine as soon we return from this foreign` |
|        - | 11124 | `	 * function.` |
|        - | 11125 | `	 */` |
|        3 | 11126 | `	return PH7_OK;` |
|        2 | 11127 |  |
|        - | 11128 | `/*` |
|        - | 11129 | ` * include:` |
|        - | 11130 | ` * According to the PHP reference manual.` |
|        - | 11131 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11132 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11133 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11134 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11135 | ` *  and the current working directory before failing. The include()` |
|        - | 11136 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11137 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11138 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11139 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11140 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11141 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11142 | ` *  directory to find the requested file.` |
|        - | 11143 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11144 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11145 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11146 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11147 | ` */` |
|     6858 | 11148 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11149 |  |
|        - | 11150 | `	SyString sFile;` |
|        - | 11151 | `	sxi32 rc;` |
|     6860 | 11152 | `	if( nArg < 1 ){` |
|        - | 11153 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11154 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11155 | `		return SXRET_OK;` |
|        - | 11156 | `	}` |
|        - | 11157 | `	/* File to include */` |
|     6860 | 11158 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6860 | 11159 | `	if( sFile.nByte < 1 ){` |
|        - | 11160 | `		/* Empty string,return NULL */` |
|      ! 0 | 11161 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11162 | `		return SXRET_OK;` |
|        - | 11163 | `	}` |
|        - | 11164 | `	/* Open,compile and execute the desired script */` |
|     6860 | 11165 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6860 | 11166 | `	if( rc != SXRET_OK ){` |
|        - | 11167 | `		/* Emit a warning and return false */` |
|        3 | 11168 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11169 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11170 | `	}` |
|     6860 | 11171 | `	return SXRET_OK;` |
|     3431 | 11172 |  |
|        - | 11173 | `/*` |
|        - | 11174 | ` * include_once:` |
|        - | 11175 | ` *  According to the PHP reference manual.` |
|        - | 11176 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11177 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11178 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11179 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11180 | ` *   just once.` |
|        - | 11181 | ` */` |
|        4 | 11182 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11183 |  |
|        - | 11184 | `	SyString sFile;` |
|        - | 11185 | `	sxi32 rc;` |
|        5 | 11186 | `	if( nArg < 1 ){` |
|        - | 11187 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11188 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11189 | `		return SXRET_OK;` |
|        - | 11190 | `	}` |
|        - | 11191 | `	/* File to include */` |
|        5 | 11192 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11193 | `	if( sFile.nByte < 1 ){` |
|        - | 11194 | `		/* Empty string,return NULL */` |
|      ! 0 | 11195 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11196 | `		return SXRET_OK;` |
|        - | 11197 | `	}` |
|        - | 11198 | `	/* Open,compile and execute the desired script */` |
|        5 | 11199 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11200 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11201 | `		/* File already included,return TRUE */` |
|        3 | 11202 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11203 | `		return SXRET_OK;` |
|        - | 11204 | `	}` |
|        3 | 11205 | `	if( rc != SXRET_OK ){` |
|        - | 11206 | `		/* Emit a warning and return false */` |
|      ! 0 | 11207 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11208 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11209 | ` 	}` |
|        3 | 11210 | `	return SXRET_OK;` |
|        3 | 11211 |  |
|        - | 11212 | `/*` |
|        - | 11213 | ` * require.` |
|        - | 11214 | ` *  According to the PHP reference manual.` |
|        - | 11215 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11216 | ` *   also produce a fatal level error.` |
|        - | 11217 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11218 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11219 | ` */` |
|        4 | 11220 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11221 |  |
|        - | 11222 | `	SyString sFile;` |
|        - | 11223 | `	sxi32 rc;` |
|        5 | 11224 | `	if( nArg < 1 ){` |
|        - | 11225 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11226 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11227 | `		return SXRET_OK;` |
|        - | 11228 | `	}` |
|        - | 11229 | `	/* File to include */` |
|        5 | 11230 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11231 | `	if( sFile.nByte < 1 ){` |
|        - | 11232 | `		/* Empty string,return NULL */` |
|      ! 0 | 11233 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11234 | `		return SXRET_OK;` |
|        - | 11235 | `	}` |
|        - | 11236 | `	/* Open,compile and execute the desired script */` |
|        5 | 11237 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11238 | `	if( rc != SXRET_OK ){` |
|        - | 11239 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11240 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11241 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11242 | `		return PH7_ABORT;` |
|        - | 11243 | `	}` |
|        5 | 11244 | `	return SXRET_OK;` |
|        3 | 11245 |  |
|        - | 11246 | `/*` |
|        - | 11247 | ` * require_once:` |
|        - | 11248 | ` *  According to the PHP reference manual.` |
|        - | 11249 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11250 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11251 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11252 | ` *   and how it differs from its non _once siblings.` |
|        - | 11253 | ` */` |
|        4 | 11254 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11255 |  |
|        - | 11256 | `	SyString sFile;` |
|        - | 11257 | `	sxi32 rc;` |
|        5 | 11258 | `	if( nArg < 1 ){` |
|        - | 11259 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11260 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11261 | `		return SXRET_OK;` |
|        - | 11262 | `	}` |
|        - | 11263 | `	/* File to include */` |
|        5 | 11264 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11265 | `	if( sFile.nByte < 1 ){` |
|        - | 11266 | `		/* Empty string,return NULL */` |
|      ! 0 | 11267 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11268 | `		return SXRET_OK;` |
|        - | 11269 | `	}` |
|        - | 11270 | `	/* Open,compile and execute the desired script */` |
|        5 | 11271 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11272 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11273 | `		/* File already included,return TRUE */` |
|        3 | 11274 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11275 | `		return SXRET_OK;` |
|        - | 11276 | `	}` |
|        3 | 11277 | `	if( rc != SXRET_OK ){` |
|        - | 11278 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11279 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11280 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11281 | `		return PH7_ABORT;` |
|        - | 11282 | `	}` |
|        3 | 11283 | `	return SXRET_OK;` |
|        3 | 11284 |  |
|        - | 11285 | `/*` |
|        - | 11286 | ` * Section:` |
|        - | 11287 | ` *  Command line arguments processing.` |
|        - | 11288 | ` * Status:` |
|        - | 11289 | ` *    Stable.` |
|        - | 11290 | ` */` |
|        - | 11291 | `/*` |
|        - | 11292 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11293 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11294 | ` * NULL otherwise.` |
|        - | 11295 | ` */` |
|        6 | 11296 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11297 |  |
|      319 | 11298 | `	while( zIn < zEnd ){` |
|      313 | 11299 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11300 | `			/* Got one */` |
|      ! 0 | 11301 | `			return &zIn[1];` |
|        - | 11302 | `		}` |
|        - | 11303 | `		/* Advance the cursor */` |
|      313 | 11304 | `		zIn++;` |
|        1 | 11305 | `	}` |
|        - | 11306 | `	/* No such option */` |
|        7 | 11307 | `	return 0;` |
|        4 | 11308 |  |
|        - | 11309 | `/*` |
|        - | 11310 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11311 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11312 | ` * NULL otherwise.` |
|        - | 11313 | ` */` |
|      ! 0 | 11314 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11315 |  |
|        - | 11316 | `	const char *zOpt;` |
|      ! 0 | 11317 | `	while( zIn < zEnd ){` |
|      ! 0 | 11318 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11319 | `			zIn += 2;` |
|      ! 0 | 11320 | `			zOpt = zIn;` |
|      ! 0 | 11321 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11322 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11323 | `					break;` |
|        - | 11324 | `				}` |
|      ! 0 | 11325 | `				zIn++;` |
|      ! 0 | 11326 | `			}` |
|        - | 11327 | `			/* Test */` |
|      ! 0 | 11328 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11329 | `				/* Got one,return it's value */` |
|      ! 0 | 11330 | `				return zIn;` |
|        - | 11331 | `			}` |
|        - | 11332 |  |
|      ! 0 | 11333 | `		}else{` |
|      ! 0 | 11334 | `			zIn++;` |
|        - | 11335 | `		}` |
|      ! 0 | 11336 | `	}` |
|        - | 11337 | `	/* No such option */` |
|      ! 0 | 11338 | `	return 0;` |
|      ! 0 | 11339 |  |
|        - | 11340 | `/*` |
|        - | 11341 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11342 | ` */` |
|        - | 11343 | `struct getopt_long_opt` |
|        - | 11344 |  |
|        - | 11345 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11346 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11347 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11348 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11349 | `};` |
|        - | 11350 | `/* Forward declaration */` |
|        - | 11351 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11352 | `/*` |
|        - | 11353 | ` * Extract short or long argument option values.` |
|        - | 11354 | ` */` |
|      ! 0 | 11355 | `static void VmExtractOptArgValue(` |
|        - | 11356 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11357 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11358 | `	const char *zArg,   /* Argument stream */` |
|        - | 11359 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11360 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11361 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11362 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11363 |  |
|      ! 0 | 11364 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11365 | `	if( !need_val ){` |
|        - | 11366 | `		/*` |
|        - | 11367 | `		 * Option does not need arguments.` |
|        - | 11368 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11369 | `		 */` |
|      ! 0 | 11370 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11371 | `	}else{` |
|        - | 11372 | `		const char *zCur;` |
|        - | 11373 | `		/* Extract option argument */` |
|      ! 0 | 11374 | `		zArg++;` |
|      ! 0 | 11375 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11376 | `			zArg++;` |
|      ! 0 | 11377 | `		}` |
|      ! 0 | 11378 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11379 | `			zArg++;` |
|      ! 0 | 11380 | `		}` |
|      ! 0 | 11381 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11382 | `			/*` |
|        - | 11383 | `			 * Argument not found.` |
|        - | 11384 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11385 | `			 */` |
|      ! 0 | 11386 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11387 | `			return;` |
|        - | 11388 | `		}` |
|        - | 11389 | `		/* Delimit the value */` |
|      ! 0 | 11390 | `		zCur = zArg;` |
|      ! 0 | 11391 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11392 | `			int d = zArg[0];` |
|        - | 11393 | `			/* Delimt the argument */` |
|      ! 0 | 11394 | `			zArg++;` |
|      ! 0 | 11395 | `			zCur = zArg;` |
|      ! 0 | 11396 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11397 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11398 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11399 | `					break;` |
|        - | 11400 | `				}` |
|      ! 0 | 11401 | `				zArg++;` |
|      ! 0 | 11402 | `			}` |
|        - | 11403 | `			/* Save the value */` |
|      ! 0 | 11404 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11405 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11406 | `		}else{` |
|      ! 0 | 11407 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11408 | `				zArg++;` |
|      ! 0 | 11409 | `			}` |
|        - | 11410 | `			/* Save the value */` |
|      ! 0 | 11411 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11412 | `		}` |
|        - | 11413 | `		/*` |
|        - | 11414 | `		 * Check if we are dealing with multiple values.` |
|        - | 11415 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11416 | `		 */` |
|      ! 0 | 11417 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11418 | `			zArg++;` |
|      ! 0 | 11419 | `		}` |
|      ! 0 | 11420 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11421 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11422 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11423 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11424 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11425 | `			}else{` |
|        - | 11426 | `				/* Insert the first value */` |
|      ! 0 | 11427 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11428 | `				for(;;){` |
|      ! 0 | 11429 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11430 | `						/* No more value */` |
|      ! 0 | 11431 | `						break;` |
|        - | 11432 | `					}` |
|        - | 11433 | `					/* Delimit the value */` |
|      ! 0 | 11434 | `					zCur = zArg;` |
|      ! 0 | 11435 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11436 | `						zArg++;` |
|      ! 0 | 11437 | `						zCur = zArg;` |
|      ! 0 | 11438 | `					}` |
|      ! 0 | 11439 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11440 | `						zArg++;` |
|      ! 0 | 11441 | `					}` |
|        - | 11442 | `					/* Reset the string cursor */` |
|      ! 0 | 11443 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11444 | `					/* Save the value */` |
|      ! 0 | 11445 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11446 | `					/* Insert */` |
|      ! 0 | 11447 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11448 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11449 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11450 | `						zArg++;` |
|      ! 0 | 11451 | `					}` |
|      ! 0 | 11452 | `				}` |
|        - | 11453 | `				/* Insert the option arg array */` |
|      ! 0 | 11454 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11455 | `				/* Safely release */` |
|      ! 0 | 11456 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11457 | `			}` |
|      ! 0 | 11458 | `		}else{` |
|        - | 11459 | `			/* Single value */` |
|      ! 0 | 11460 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11461 | `		}` |
|        - | 11462 | `	}` |
|      ! 0 | 11463 |  |
|        - | 11464 | `/*` |
|        - | 11465 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11466 | ` *   Gets options from the command line argument list.` |
|        - | 11467 | ` * Parameters` |
|        - | 11468 | ` *  $options` |
|        - | 11469 | ` *   Each character in this string will be used as option characters` |
|        - | 11470 | ` *   and matched against options passed to the script starting with` |
|        - | 11471 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11472 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11473 | ` *  $longopts` |
|        - | 11474 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11475 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11476 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11477 | ` *   option --opt.` |
|        - | 11478 | ` * Return` |
|        - | 11479 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11480 | ` *  on failure.` |
|        - | 11481 | ` */` |
|        2 | 11482 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11483 |  |
|        - | 11484 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11485 | `	struct getopt_long_opt sLong;` |
|        - | 11486 | `	ph7_value *pArray,*pWorker;` |
|        - | 11487 | `	SyBlob *pArg;` |
|        - | 11488 | `	int nByte;` |
|        3 | 11489 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11490 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11491 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11492 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11493 | `		return PH7_OK;` |
|        - | 11494 | `	}` |
|        - | 11495 | `	/* Extract option arguments */` |
|        3 | 11496 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11497 | `	zEnd = &zIn[nByte];` |
|        - | 11498 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11499 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11500 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11501 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11502 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11503 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11504 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11505 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11506 | `		return PH7_OK;` |
|        - | 11507 | `	}` |
|        3 | 11508 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11509 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11510 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11511 | `		/* Everything will be released automatically when we return` |
|        - | 11512 | `		 * from this function.` |
|        - | 11513 | `		 */` |
|      ! 0 | 11514 | `		return PH7_OK;` |
|        - | 11515 | `	}` |
|        3 | 11516 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11517 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11518 | `	/* Fill the long option structure */` |
|        3 | 11519 | `	sLong.pArray = pArray;` |
|        3 | 11520 | `	sLong.pWorker = pWorker;` |
|        3 | 11521 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11522 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11523 | `	sLong.pCtx = pCtx;` |
|        - | 11524 | `	/* Start processing */` |
|        9 | 11525 | `	while( zIn < zEnd ){` |
|        7 | 11526 | `		int c = zIn[0];` |
|        7 | 11527 | `		int need_val = 0;` |
|        - | 11528 | `		/* Advance the stream cursor */` |
|        7 | 11529 | `		zIn++;` |
|        - | 11530 | `		/* Ignore non-alphanum characters */` |
|        7 | 11531 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11532 | `			continue;` |
|        - | 11533 | `		}` |
|        7 | 11534 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11535 | `			zIn++;` |
|        5 | 11536 | `			need_val = 1;` |
|        5 | 11537 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11538 | `				zIn++;` |
|      ! 0 | 11539 | `			}` |
|        2 | 11540 | `		}` |
|        - | 11541 | `		/* Find option */` |
|        7 | 11542 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11543 | `		if( zArg == 0 ){` |
|        - | 11544 | `			/* No such option */` |
|        7 | 11545 | `			continue;` |
|        - | 11546 | `		}` |
|        - | 11547 | `		/* Extract option argument value */` |
|      ! 0 | 11548 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11549 | `	}` |
|        3 | 11550 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11551 | `		/* Process long options */` |
|      ! 0 | 11552 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11553 | `	}` |
|        - | 11554 | `	/* Return the option array */` |
|        3 | 11555 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11556 | `	/*` |
|        - | 11557 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11558 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11559 | `	 */` |
|        3 | 11560 | `	return PH7_OK;` |
|        2 | 11561 |  |
|        - | 11562 | `/*` |
|        - | 11563 | ` * Array walker callback used for processing long options values.` |
|        - | 11564 | ` */` |
|      ! 0 | 11565 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11566 |  |
|      ! 0 | 11567 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11568 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11569 | `	int need_value = 0;` |
|        - | 11570 | `	int nByte;` |
|        - | 11571 | `	/* Value must be of type string */` |
|      ! 0 | 11572 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11573 | `		/* Simply ignore */` |
|      ! 0 | 11574 | `		return PH7_OK;` |
|        - | 11575 | `	}` |
|      ! 0 | 11576 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11577 | `	if( nByte < 1 ){` |
|        - | 11578 | `		/* Empty string,ignore */` |
|      ! 0 | 11579 | `		return PH7_OK;` |
|        - | 11580 | `	}` |
|      ! 0 | 11581 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11582 | `	if( zEnd[0] == ':' ){` |
|        - | 11583 | `		char *zTerm;` |
|        - | 11584 | `		/* Try to extract a value */` |
|      ! 0 | 11585 | `		need_value = 1;` |
|      ! 0 | 11586 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11587 | `			zEnd--;` |
|      ! 0 | 11588 | `		}` |
|      ! 0 | 11589 | `		if( zOpt >= zEnd ){` |
|        - | 11590 | `			/* Empty string,ignore */` |
|      ! 0 | 11591 | `			SXUNUSED(pKey);` |
|      ! 0 | 11592 | `			return PH7_OK;` |
|        - | 11593 | `		}` |
|      ! 0 | 11594 | `		zEnd++;` |
|      ! 0 | 11595 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11596 | `		zTerm[0] = 0;` |
|      ! 0 | 11597 | `	}else{` |
|      ! 0 | 11598 | `		zEnd = &zOpt[nByte];` |
|        - | 11599 | `	}` |
|        - | 11600 | `	/* Find the option */` |
|      ! 0 | 11601 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11602 | `	if( zArg == 0 ){` |
|        - | 11603 | `		/* No such option,return immediately */` |
|      ! 0 | 11604 | `		return PH7_OK;` |
|        - | 11605 | `	}` |
|        - | 11606 | `	/* Try to extract a value */` |
|      ! 0 | 11607 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11608 | `	return PH7_OK;` |
|      ! 0 | 11609 |  |
|        - | 11610 | `/*` |
|        - | 11611 | ` * Section:` |
|        - | 11612 | ` *  JSON encoding/decoding routines.` |
|        - | 11613 | ` * Status:` |
|        - | 11614 | ` *    Devel.` |
|        - | 11615 | ` */` |
|        - | 11616 | `/* Forward reference */` |
|        - | 11617 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11618 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11619 | `/*` |
|        - | 11620 | ` * JSON encoder state is stored in an instance` |
|        - | 11621 | ` * of the following structure.` |
|        - | 11622 | ` */` |
|        - | 11623 | `typedef struct json_private_data json_private_data;` |
|        - | 11624 | `struct json_private_data` |
|        - | 11625 |  |
|        - | 11626 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11627 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11628 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11629 | `	int nRecCount;     /* Recursion count */` |
|        - | 11630 | `};` |
|        - | 11631 | `/*` |
|        - | 11632 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11633 | ` * According to wikipedia` |
|        - | 11634 | ` * JSON's basic types are:` |
|        - | 11635 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11636 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11637 | ` *   Boolean (true or false)` |
|        - | 11638 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11639 | ` *    do not need to be of the same type)` |
|        - | 11640 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11641 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11642 | ` *     be distinct from each other)` |
|        - | 11643 | ` *   null (empty)` |
|        - | 11644 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11645 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11646 | ` */` |
|        8 | 11647 | `static sxi32 VmJsonEncode(` |
|        - | 11648 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11649 | `	json_private_data *pData /* Context data */` |
|        1 | 11650 | `	){` |
|        9 | 11651 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11652 | `		int iFlags = pData->iFlags;` |
|        - | 11653 | `		int nByte;` |
|        9 | 11654 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11655 | `			/* null */` |
|      ! 0 | 11656 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11657 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11658 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11659 | `			int iLen;` |
|        - | 11660 | `			/* true/false */` |
|      ! 0 | 11661 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11662 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11663 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11664 | `			const char *zNum;` |
|        - | 11665 | `			/* Get a string representation of the number */` |
|        7 | 11666 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11667 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11668 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11669 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11670 | `				const char *zNum;` |
|        - | 11671 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11672 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11673 | `				/* Get a string representation of the number */` |
|      ! 0 | 11674 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11675 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11676 | `			}else{` |
|        - | 11677 | `				const char *zIn,*zEnd;` |
|        - | 11678 | `				int c;` |
|        - | 11679 | `				/* Encode the string */` |
|      ! 0 | 11680 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11681 | `				zEnd = &zIn[nByte];` |
|        - | 11682 | `				/* Append the double quote */` |
|      ! 0 | 11683 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11684 | `				for(;;){` |
|      ! 0 | 11685 | `					if( zIn >= zEnd ){` |
|        - | 11686 | `						/* No more input to process */` |
|      ! 0 | 11687 | `						break;` |
|        - | 11688 | `					}` |
|      ! 0 | 11689 | `					c = zIn[0];` |
|        - | 11690 | `					/* Advance the stream cursor */` |
|      ! 0 | 11691 | `					zIn++;` |
|      ! 0 | 11692 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11693 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11694 | `						if( c == '<' ){` |
|      ! 0 | 11695 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11696 | `						}else{` |
|      ! 0 | 11697 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11698 | `						}` |
|      ! 0 | 11699 | `						continue;` |
|      ! 0 | 11700 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11701 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11702 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11703 | `						continue;` |
|      ! 0 | 11704 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11705 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11706 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11707 | `						continue;` |
|      ! 0 | 11708 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11709 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11710 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11711 | `						continue;` |
|        - | 11712 | `					}` |
|      ! 0 | 11713 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11714 | `						/* Unescape the character */` |
|      ! 0 | 11715 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11716 | `					}` |
|        - | 11717 | `					/* Append character verbatim */` |
|      ! 0 | 11718 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11719 | `				}` |
|        - | 11720 | `				/* Append the double quote */` |
|      ! 0 | 11721 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11722 | `			}` |
|        3 | 11723 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11724 | `			int c = '[',d = ']';` |
|        - | 11725 | `			/* Encode the array */` |
|        3 | 11726 | `			pData->isFirst = 1;` |
|        3 | 11727 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11728 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11729 | `				c = '{';` |
|      ! 0 | 11730 | `				d = '}';` |
|      ! 0 | 11731 | `			}` |
|        - | 11732 | `			/* Append the square bracket or curly braces */` |
|        3 | 11733 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11734 | `			/* Iterate throw array entries */` |
|        3 | 11735 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11736 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11737 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11738 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11739 | `			/* Encode the class instance */` |
|      ! 0 | 11740 | `			pData->isFirst = 1;` |
|        - | 11741 | `			/* Append the curly braces */` |
|      ! 0 | 11742 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11743 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11744 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11745 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11746 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11747 | `		}else{` |
|        - | 11748 | `			/* Can't happen */` |
|      ! 0 | 11749 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11750 | `		}` |
|        - | 11751 | `		/* All done */` |
|        9 | 11752 | `		return PH7_OK;` |
|        1 | 11753 |  |
|        - | 11754 | `/*` |
|        - | 11755 | ` * The following walker callback is invoked each time we need` |
|        - | 11756 | ` * to encode an array to JSON.` |
|        - | 11757 | ` */` |
|        6 | 11758 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11759 |  |
|        7 | 11760 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11761 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11762 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11763 | `		return PH7_OK;` |
|        - | 11764 | `	}` |
|        7 | 11765 | `	if( !pJson->isFirst ){` |
|        - | 11766 | `		/* Append the colon first */` |
|        5 | 11767 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11768 | `	}` |
|        7 | 11769 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11770 | `		/* Outputs an object rather than an array */` |
|        - | 11771 | `		const char *zKey;` |
|        - | 11772 | `		int nByte;` |
|        - | 11773 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11774 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11775 | `		/* Append the key and the double colon */` |
|      ! 0 | 11776 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11777 | `	}` |
|        - | 11778 | `	/* Encode the value */` |
|        7 | 11779 | `	pJson->nRecCount++;` |
|        7 | 11780 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11781 | `	pJson->nRecCount--;` |
|        7 | 11782 | `	pJson->isFirst = 0;` |
|        7 | 11783 | `	return PH7_OK;` |
|        4 | 11784 |  |
|        - | 11785 | `/*` |
|        - | 11786 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11787 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11788 | ` */` |
|      ! 0 | 11789 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11790 |  |
|      ! 0 | 11791 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11792 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11793 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11794 | `		return PH7_OK;` |
|        - | 11795 | `	}` |
|      ! 0 | 11796 | `	if( !pJson->isFirst ){` |
|        - | 11797 | `		/* Append the colon first */` |
|      ! 0 | 11798 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11799 | `	}` |
|        - | 11800 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11801 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11802 | `	/* Encode the value */` |
|      ! 0 | 11803 | `	pJson->nRecCount++;` |
|      ! 0 | 11804 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11805 | `	pJson->nRecCount--;` |
|      ! 0 | 11806 | `	pJson->isFirst = 0;` |
|      ! 0 | 11807 | `	return PH7_OK;` |
|      ! 0 | 11808 |  |
|        - | 11809 | `/*` |
|        - | 11810 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11811 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11812 | ` * Parameters` |
|        - | 11813 | ` *  $value` |
|        - | 11814 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11815 | ` * $options` |
|        - | 11816 | ` *  Bitmask consisting of:` |
|        - | 11817 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11818 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11819 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11820 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11821 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11822 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11823 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11824 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11825 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11826 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11827 | ` * Return` |
|        - | 11828 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11829 | ` */` |
|        2 | 11830 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11831 |  |
|        - | 11832 | `	json_private_data sJson;` |
|        3 | 11833 | `	if( nArg < 1 ){` |
|        - | 11834 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11835 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11836 | `		return PH7_OK;` |
|        - | 11837 | `	}` |
|        - | 11838 | `	/* Prepare the JSON data */` |
|        3 | 11839 | `	sJson.nRecCount = 0;` |
|        3 | 11840 | `	sJson.pCtx = pCtx;` |
|        3 | 11841 | `	sJson.isFirst = 1;` |
|        3 | 11842 | `	sJson.iFlags = 0;` |
|        3 | 11843 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11844 | `		/* Extract option flags */` |
|      ! 0 | 11845 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11846 | `	}` |
|        - | 11847 | `	/* Perform the encoding operation */` |
|        3 | 11848 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11849 | `	/* All done */` |
|        3 | 11850 | `	return PH7_OK;` |
|        2 | 11851 |  |
|        - | 11852 | `/*` |
|        - | 11853 | ` * int json_last_error(void)` |
|        - | 11854 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11855 | ` * Parameters` |
|        - | 11856 | ` *  None` |
|        - | 11857 | ` * Return` |
|        - | 11858 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11859 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11860 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11861 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11862 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11863 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11864 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11865 | ` */` |
|        8 | 11866 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11867 |  |
|       10 | 11868 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11869 | `	/* Return the error code */` |
|       10 | 11870 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11871 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11872 | `	SXUNUSED(apArg);` |
|       10 | 11873 | `	return PH7_OK;` |
|        2 | 11874 |  |
|        - | 11875 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11876 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11877 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11878 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11879 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11880 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11881 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11882 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11883 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11884 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11885 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11886 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11887 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11888 | `/*` |
|        - | 11889 | ` * Tokenize an entire JSON input.` |
|        - | 11890 | ` * Get a single low-level token from the input file.` |
|        - | 11891 | ` * Update the stream pointer so that it points to the first` |
|        - | 11892 | ` * character beyond the extracted token.` |
|        - | 11893 | ` */` |
|       60 | 11894 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11895 |  |
|       62 | 11896 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11897 | `	SyString *pStr;` |
|        - | 11898 | `	int c;` |
|        - | 11899 | `	/* Ignore leading white spaces */` |
|       66 | 11900 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11901 | `		/* Advance the stream cursor */` |
|        6 | 11902 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11903 | `			/* Update line counter */` |
|      ! 0 | 11904 | `			pStream->nLine++;` |
|      ! 0 | 11905 | `		}` |
|        6 | 11906 | `		pStream->zText++;` |
|        2 | 11907 | `	}` |
|       62 | 11908 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11909 | `		/* End of input reached */` |
|      ! 0 | 11910 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11911 | `		return SXERR_EOF;` |
|        - | 11912 | `	}` |
|        - | 11913 | `	/* Record token starting position and line */` |
|       62 | 11914 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11915 | `	pToken->pUserData = 0;` |
|       62 | 11916 | `	pStr = &pToken->sData;` |
|       62 | 11917 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11918 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11919 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11920 | `			/* Single character */` |
|       36 | 11921 | `			c = pStream->zText[0];` |
|        - | 11922 | `			/* Set token type */` |
|       36 | 11923 | `			switch(c){` |
|        5 | 11924 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11925 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11926 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11927 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11928 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11929 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11930 | `			default:` |
|      ! 0 | 11931 | `				break;` |
|        - | 11932 | `			}` |
|        - | 11933 | `			/* Advance the stream cursor */` |
|       36 | 11934 | `			pStream->zText++;` |
|       45 | 11935 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11936 | `		/* JSON string */` |
|       10 | 11937 | `		pStream->zText++;` |
|       10 | 11938 | `		pStr->zString++;` |
|        - | 11939 | `		/* Delimit the string */` |
|       32 | 11940 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11941 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11942 | `				break;` |
|        - | 11943 | `			}` |
|       24 | 11944 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11945 | `				/* Update line counter */` |
|      ! 0 | 11946 | `				pStream->nLine++;` |
|      ! 0 | 11947 | `			}` |
|       24 | 11948 | `			pStream->zText++;` |
|        2 | 11949 | `		}` |
|       10 | 11950 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11951 | `			/* Missing closing '"' */` |
|      ! 0 | 11952 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11953 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11954 | `		}else{` |
|       10 | 11955 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11956 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11957 | `		}` |
|       24 | 11958 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11959 | `		/* Number */` |
|       13 | 11960 | `		pStream->zText++;` |
|       13 | 11961 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11962 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11963 | `			pStream->zText++;` |
|      ! 0 | 11964 | `		}` |
|       13 | 11965 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11966 | `			c = pStream->zText[0];` |
|       13 | 11967 | `			if( c == '.' ){` |
|        - | 11968 | `					/* Real number */` |
|      ! 0 | 11969 | `					pStream->zText++;` |
|      ! 0 | 11970 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11971 | `						pStream->zText++;` |
|      ! 0 | 11972 | `					}` |
|      ! 0 | 11973 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11974 | `						c = pStream->zText[0];` |
|      ! 0 | 11975 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11976 | `							pStream->zText++;` |
|      ! 0 | 11977 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11978 | `								c = pStream->zText[0];` |
|      ! 0 | 11979 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11980 | `									pStream->zText++;` |
|      ! 0 | 11981 | `								}` |
|      ! 0 | 11982 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11983 | `									pStream->zText++;` |
|      ! 0 | 11984 | `								}` |
|      ! 0 | 11985 | `							}` |
|      ! 0 | 11986 | `						}` |
|      ! 0 | 11987 | `					}` |
|       13 | 11988 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11989 | `					/* Real number */` |
|      ! 0 | 11990 | `					pStream->zText++;` |
|      ! 0 | 11991 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11992 | `						c = pStream->zText[0];` |
|      ! 0 | 11993 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11994 | `							pStream->zText++;` |
|      ! 0 | 11995 | `						}` |
|      ! 0 | 11996 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11997 | `							pStream->zText++;` |
|      ! 0 | 11998 | `						}` |
|      ! 0 | 11999 | `					}` |
|      ! 0 | 12000 | `				}` |
|        7 | 12001 | `			}` |
|       17 | 12002 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 12003 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 12004 | `			/* boolean true */` |
|      ! 0 | 12005 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 12006 | `			/* Advance the stream cursor */` |
|      ! 0 | 12007 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 12008 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 12009 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 12010 | `			/* boolean false */` |
|      ! 0 | 12011 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 12012 | `			/* Advance the stream cursor */` |
|      ! 0 | 12013 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 12014 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 12015 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 12016 | `			/* NULL */` |
|      ! 0 | 12017 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 12018 | `			/* Advance the stream cursor */` |
|      ! 0 | 12019 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 12020 | `	}else{` |
|        - | 12021 | `		/* Unexpected token */` |
|        8 | 12022 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 12023 | `		/* Advance the stream cursor */` |
|        8 | 12024 | `		pStream->zText++;` |
|        8 | 12025 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 12026 | `		/* Abort processing immediatley */` |
|        8 | 12027 | `		return SXERR_ABORT;` |
|        - | 12028 | `	}` |
|        - | 12029 | `	/* record token length */` |
|       56 | 12030 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 12031 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 12032 | `		pStr->nByte--;` |
|        4 | 12033 | `	}` |
|        - | 12034 | `	/* Return to the lexer */` |
|       56 | 12035 | `	return SXRET_OK;` |
|       32 | 12036 |  |
|        - | 12037 | `/*` |
|        - | 12038 | ` * JSON decoded input consumer callback signature.` |
|        - | 12039 | ` */` |
|        - | 12040 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 12041 | `/*` |
|        - | 12042 | ` * JSON decoder state is kept in the following structure.` |
|        - | 12043 | ` */` |
|        - | 12044 | `typedef struct json_decoder json_decoder;` |
|        - | 12045 | `struct json_decoder` |
|        - | 12046 |  |
|        - | 12047 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12048 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 12049 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 12050 | `	int iFlags;        /* Configuration flags */` |
|        - | 12051 | `	SyToken *pIn;      /* Token stream */` |
|        - | 12052 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 12053 | `	int rec_depth;     /* Recursion limit */` |
|        - | 12054 | `	int rec_count;     /* Current nesting level */` |
|        - | 12055 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12056 | `};` |
|        - | 12057 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12058 | `/* Forward declaration */` |
|        - | 12059 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12060 | `/*` |
|        - | 12061 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12062 | ` * the result in the given ph7_value.` |
|        - | 12063 | ` */` |
|        8 | 12064 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12065 |  |
|       10 | 12066 | `	const char *zIn = pStr->zString;` |
|       10 | 12067 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12068 | `	const char *zCur;` |
|        - | 12069 | `	int c;` |
|        - | 12070 | `	/* Mark the value as a string */` |
|       10 | 12071 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12072 | `	for(;;){` |
|       10 | 12073 | `		zCur = zIn;` |
|       32 | 12074 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12075 | `			zIn++;` |
|        2 | 12076 | `		}` |
|       10 | 12077 | `		if( zIn > zCur ){` |
|        - | 12078 | `			/* Append chunk verbatim */` |
|       10 | 12079 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12080 | `		}` |
|       10 | 12081 | `		zIn++;` |
|       10 | 12082 | `		if( zIn >= zEnd ){` |
|        - | 12083 | `			/* End of the input reached */` |
|       10 | 12084 | `			break;` |
|        - | 12085 | `		}` |
|      ! 0 | 12086 | `		c = zIn[0];` |
|        - | 12087 | `		/* Unescape the character */` |
|      ! 0 | 12088 | `		switch(c){` |
|      ! 0 | 12089 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12090 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12091 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12092 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12093 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12094 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12095 | `		default:` |
|      ! 0 | 12096 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12097 | `			break;` |
|        - | 12098 | `		}` |
|        - | 12099 | `		/* Advance the stream cursor */` |
|      ! 0 | 12100 | `		zIn++;` |
|      ! 0 | 12101 | `	}` |
|       10 | 12102 |  |
|        - | 12103 | `/*` |
|        - | 12104 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12105 | ` * According to wikipedia` |
|        - | 12106 | ` * JSON's basic types are:` |
|        - | 12107 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12108 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12109 | ` *   Boolean (true or false)` |
|        - | 12110 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12111 | ` *    do not need to be of the same type)` |
|        - | 12112 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12113 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12114 | ` *     be distinct from each other)` |
|        - | 12115 | ` *   null (empty)` |
|        - | 12116 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12117 | ` */` |
|       24 | 12118 | `static sxi32 VmJsonDecode(` |
|        - | 12119 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12120 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12121 | `	){` |
|        - | 12122 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12123 | `	sxi32 rc;` |
|        - | 12124 | `	/* Check if we do not nest to much */` |
|       26 | 12125 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12126 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12127 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12128 | `		return SXERR_ABORT;` |
|        - | 12129 | `	}` |
|       26 | 12130 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12131 | `		/* Scalar value */` |
|       16 | 12132 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12133 | `		if( pWorker == 0 ){` |
|      ! 0 | 12134 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12135 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12136 | `			return SXERR_ABORT;` |
|        - | 12137 | `		}` |
|        - | 12138 | `		/* Reflect the JSON image */` |
|       16 | 12139 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12140 | `			/* Nullify the value.*/` |
|      ! 0 | 12141 | `			ph7_value_null(pWorker);` |
|       16 | 12142 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12143 | `			/* Boolean value */` |
|      ! 0 | 12144 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12145 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12146 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12147 | `			/*` |
|        - | 12148 | `			 * Numeric value.` |
|        - | 12149 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12150 | `			 * value.` |
|        - | 12151 | `			 */` |
|       13 | 12152 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12153 | `			/* Obtain a numeric representation */` |
|       13 | 12154 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12155 | `		}else{` |
|        - | 12156 | `			/* Dequote the string */` |
|        3 | 12157 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12158 | `		}` |
|        - | 12159 | `		/* Invoke the consumer callback */` |
|       16 | 12160 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12161 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12162 | `			return SXERR_ABORT;` |
|        - | 12163 | `		}` |
|        - | 12164 | `		/* All done,advance the stream cursor */` |
|       16 | 12165 | `		pDecoder->pIn++;` |
|       19 | 12166 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12167 | `		ProcJsonConsumer xOld;` |
|        - | 12168 | `		void *pOld;` |
|        - | 12169 | `		/* Array representation*/` |
|        5 | 12170 | `		pDecoder->pIn++;` |
|        - | 12171 | `		/* Create a working array */` |
|        5 | 12172 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12173 | `		if( pWorker == 0 ){` |
|      ! 0 | 12174 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12175 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12176 | `			return SXERR_ABORT;` |
|        - | 12177 | `		}` |
|        - | 12178 | `		/* Save the old consumer */` |
|        5 | 12179 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12180 | `		pOld = pDecoder->pUserData;` |
|        - | 12181 | `		/* Set the new consumer */` |
|        5 | 12182 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12183 | `		pDecoder->pUserData = pWorker;` |
|        - | 12184 | `		/* Decode the array */` |
|        7 | 12185 | `		for(;;){` |
|        - | 12186 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12187 | `			 * do this.` |
|        - | 12188 | `			 */` |
|       21 | 12189 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12190 | `				pDecoder->pIn++;` |
|        1 | 12191 | `			}` |
|       15 | 12192 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12193 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12194 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12195 | `				}` |
|        5 | 12196 | `				break;` |
|        - | 12197 | `			}` |
|        - | 12198 | `			/* Recurse and decode the entry */` |
|       11 | 12199 | `			pDecoder->rec_count++;` |
|       11 | 12200 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12201 | `			pDecoder->rec_count--;` |
|       11 | 12202 | `			if( rc == SXERR_ABORT ){` |
|        - | 12203 | `				/* Abort processing immediately */` |
|      ! 0 | 12204 | `				return SXERR_ABORT;` |
|        - | 12205 | `			}` |
|        - | 12206 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12207 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12208 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12209 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12210 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12211 | `					return SXERR_ABORT;` |
|        - | 12212 | `			}` |
|        1 | 12213 | `		}` |
|        - | 12214 | `		/* Restore the old consumer */` |
|        5 | 12215 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12216 | `		pDecoder->pUserData = pOld;` |
|        - | 12217 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12218 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12219 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12220 | `		ProcJsonConsumer xOld;` |
|        - | 12221 | `		ph7_value *pKey;` |
|        - | 12222 | `		void *pOld;` |
|        - | 12223 | `		/* Object representation*/` |
|        8 | 12224 | `		pDecoder->pIn++;` |
|        - | 12225 | `		/* Return the object as an associative array */` |
|        8 | 12226 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12227 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12228 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12229 | `				);` |
|        1 | 12230 | `		}` |
|        - | 12231 | `		/* Create a working array */` |
|        8 | 12232 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12233 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12234 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12235 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12236 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12237 | `			return SXERR_ABORT;` |
|        - | 12238 | `		}` |
|        - | 12239 | `		/* Save the old consumer */` |
|        8 | 12240 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12241 | `		pOld = pDecoder->pUserData;` |
|        - | 12242 | `		/* Set the new consumer */` |
|        8 | 12243 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12244 | `		pDecoder->pUserData = pWorker;` |
|        - | 12245 | `		/* Decode the object */` |
|        6 | 12246 | `		for(;;){` |
|        - | 12247 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12248 | `			 * do this.` |
|        - | 12249 | `			 */` |
|       16 | 12250 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12251 | `				pDecoder->pIn++;` |
|        1 | 12252 | `			}` |
|       14 | 12253 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12254 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12255 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12256 | `				}` |
|        8 | 12257 | `				break;` |
|        - | 12258 | `			}` |
|        6 | 12259 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12260 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12261 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12262 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12263 | `					return SXERR_ABORT;` |
|        - | 12264 | `			}` |
|        - | 12265 | `			/* Dequote the key */` |
|        8 | 12266 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12267 | `			/* Jump the key and the colon */` |
|        8 | 12268 | `			pDecoder->pIn += 2;` |
|        - | 12269 | `			/* Recurse and decode the value */` |
|        8 | 12270 | `			pDecoder->rec_count++;` |
|        8 | 12271 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12272 | `			pDecoder->rec_count--;` |
|        8 | 12273 | `			if( rc == SXERR_ABORT ){` |
|        - | 12274 | `				/* Abort processing immediately */` |
|      ! 0 | 12275 | `				return SXERR_ABORT;` |
|        - | 12276 | `			}` |
|        - | 12277 | `			/* Reset the internal buffer of the key */` |
|        8 | 12278 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12279 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12280 | `		}` |
|        - | 12281 | `		/* Restore the old consumer */` |
|        8 | 12282 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12283 | `		pDecoder->pUserData = pOld;` |
|        - | 12284 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12285 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12286 | `		/* Release the key */` |
|        8 | 12287 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12288 | `	}else{` |
|        - | 12289 | `		/* Unexpected token */` |
|      ! 0 | 12290 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12291 | `	}` |
|        - | 12292 | `	/* Release the worker variable */` |
|       26 | 12293 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12294 | `	return SXRET_OK;` |
|       14 | 12295 |  |
|        - | 12296 | `/*` |
|        - | 12297 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12298 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12299 | ` * is being decoded.` |
|        - | 12300 | ` */` |
|       16 | 12301 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12302 |  |
|       18 | 12303 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12304 | `	/* Insert the entry */` |
|       18 | 12305 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12306 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12307 | `	/* All done */` |
|       18 | 12308 | `	return SXRET_OK;` |
|        2 | 12309 |  |
|        - | 12310 | `/*` |
|        - | 12311 | ` * Standard JSON decoder callback.` |
|        - | 12312 | ` */` |
|        8 | 12313 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12314 |  |
|        - | 12315 | `	/* Return the value directly */` |
|       10 | 12316 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12317 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12318 | `	SXUNUSED(pUserData);` |
|        - | 12319 | `	/* All done */` |
|       10 | 12320 | `	return SXRET_OK;` |
|        2 | 12321 |  |
|        - | 12322 | `/*` |
|        - | 12323 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12324 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12325 | ` * Parameters` |
|        - | 12326 | ` *  $json` |
|        - | 12327 | ` *    The json string being decoded.` |
|        - | 12328 | ` * $assoc` |
|        - | 12329 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12330 | ` * $depth` |
|        - | 12331 | ` *   User specified recursion depth.` |
|        - | 12332 | ` * $options` |
|        - | 12333 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12334 | ` * (default is to cast large integers as floats)` |
|        - | 12335 | ` * Return` |
|        - | 12336 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12337 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12338 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12339 | ` */` |
|       16 | 12340 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12341 |  |
|       18 | 12342 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12343 | `	json_decoder sDecoder;` |
|        - | 12344 | `	const char *zIn;` |
|        - | 12345 | `	SySet sToken;` |
|        - | 12346 | `	SyLex sLex;` |
|        - | 12347 | `	int nByte;` |
|        - | 12348 | `	sxi32 rc;` |
|       18 | 12349 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12350 | `		/* Missing/Invalid arguments, return NULL */` |
|      ! 0 | 12351 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12352 | `		return PH7_OK;` |
|        - | 12353 | `	}` |
|        - | 12354 | `	/* Extract the JSON string */` |
|       18 | 12355 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       18 | 12356 | `	if( nByte < 1 ){` |
|        - | 12357 | `		/* Empty string,return NULL */` |
|        3 | 12358 | `		ph7_result_null(pCtx);` |
|        3 | 12359 | `		return PH7_OK;` |
|        - | 12360 | `	}` |
|        - | 12361 | `	/* Clear JSON error code */` |
|       16 | 12362 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12363 | `	/* Tokenize the input */` |
|       16 | 12364 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12365 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12366 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12367 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12368 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12369 | `		SyLexRelease(&sLex);` |
|        8 | 12370 | `		SySetRelease(&sToken);` |
|        - | 12371 | `		/* return NULL */` |
|        8 | 12372 | `		ph7_result_null(pCtx);` |
|        8 | 12373 | `		return PH7_OK;` |
|        - | 12374 | `	}` |
|        - | 12375 | `	/* Fill the decoder */` |
|       10 | 12376 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12377 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12378 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12379 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12380 | `	sDecoder.iFlags = 0;` |
|       10 | 12381 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12382 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12383 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12384 | `	}` |
|       10 | 12385 | `	sDecoder.rec_depth = 32;` |
|       10 | 12386 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12387 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12388 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12389 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12390 | `		}` |
|      ! 0 | 12391 | `	}` |
|       10 | 12392 | `	sDecoder.rec_count = 0;` |
|        - | 12393 | `	/* Set a default consumer */` |
|       10 | 12394 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12395 | `	sDecoder.pUserData = 0;` |
|        - | 12396 | `	/* Decode the raw JSON input */` |
|       10 | 12397 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12398 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12399 | `		/*` |
|        - | 12400 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12401 | `		 */` |
|      ! 0 | 12402 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12403 | `	}` |
|        - | 12404 | `	/* Clean-up the mess left behind */` |
|       10 | 12405 | `	SyLexRelease(&sLex);` |
|       10 | 12406 | `	SySetRelease(&sToken);` |
|        - | 12407 | `	/* All done */` |
|       10 | 12408 | `	return PH7_OK;` |
|       10 | 12409 |  |
|        - | 12410 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12411 | `/*` |
|        - | 12412 | ` * XML processing Functions.` |
|        - | 12413 | ` * Status:` |
|        - | 12414 | ` *    Devel.` |
|        - | 12415 | ` */` |
|        - | 12416 | `enum ph7_xml_handler_id{` |
|        - | 12417 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12418 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12419 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12420 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12421 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12422 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12423 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12424 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12425 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12426 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12427 | `};` |
|        - | 12428 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12429 | `/* An instance of the following structure describe a working` |
|        - | 12430 | ` * XML engine instance.` |
|        - | 12431 | ` */` |
|        - | 12432 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12433 | `struct ph7_xml_engine` |
|        - | 12434 |  |
|        - | 12435 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12436 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12437 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12438 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12439 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12440 | `							  * as the first argument to the user callbacks.` |
|        - | 12441 | `							  */` |
|        - | 12442 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12443 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12444 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12445 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12446 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12447 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12448 | `};` |
|        - | 12449 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12450 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12451 | `/*` |
|        - | 12452 | ` * Allocate and initialize an XML engine.` |
|        - | 12453 | ` */` |
|       84 | 12454 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12455 |  |
|        - | 12456 | `	ph7_xml_engine *pEngine;` |
|       85 | 12457 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12458 | `	ph7_value *pValue;` |
|        - | 12459 | `	sxu32 n;` |
|        - | 12460 | `	/* Allocate a new instance */` |
|       85 | 12461 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12462 | `	if( pEngine == 0 ){` |
|        - | 12463 | `		/* Out of memory */` |
|      ! 0 | 12464 | `		return 0;` |
|        - | 12465 | `	}` |
|        - | 12466 | `	/* Zero the structure */` |
|       85 | 12467 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12468 | `	/* Initialize fields */` |
|       85 | 12469 | `	pEngine->pVm = pVm;` |
|       85 | 12470 | `	pEngine->pCtx = 0;` |
|       85 | 12471 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12472 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12473 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12474 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12475 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12476 | `		pValue = &pEngine->aCB[n];` |
|        - | 12477 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12478 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12479 | `	}` |
|       85 | 12480 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12481 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12482 | `	/* Finally set the magic number */` |
|       85 | 12483 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12484 | `	return pEngine;` |
|       43 | 12485 |  |
|        - | 12486 | `/*` |
|        - | 12487 | ` * Release an XML engine.` |
|        - | 12488 | ` */` |
|       84 | 12489 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12490 |  |
|       85 | 12491 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12492 | `	ph7_value *pValue;` |
|        - | 12493 | `	sxu32 n;` |
|        - | 12494 | `	/* Release fields */` |
|       85 | 12495 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12496 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12497 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12498 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12499 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12500 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12501 | `	}` |
|       85 | 12502 | `	pEngine->nMagic = 0x2621;` |
|        - | 12503 | `	/* Finally,release the whole instance */` |
|       85 | 12504 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12505 |  |
|        - | 12506 | `/*` |
|        - | 12507 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12508 | ` *  Create an UTF-8 XML parser.` |
|        - | 12509 | ` * Parameter` |
|        - | 12510 | ` *  $encoding` |
|        - | 12511 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12512 | ` * Return` |
|        - | 12513 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12514 | ` */` |
|       80 | 12515 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12516 |  |
|        - | 12517 | `	ph7_xml_engine *pEngine;` |
|        - | 12518 | `	/* Allocate a new instance */` |
|       81 | 12519 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12520 | `	if( pEngine == 0 ){` |
|      ! 0 | 12521 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12522 | `		/* Return null */` |
|      ! 0 | 12523 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12524 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12525 | `		SXUNUSED(apArg);` |
|      ! 0 | 12526 | `		return PH7_OK;` |
|        - | 12527 | `	}` |
|        - | 12528 | `	/* Return the engine as a resource */` |
|       81 | 12529 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12530 | `	return PH7_OK;` |
|       41 | 12531 |  |
|        - | 12532 | `/*` |
|        - | 12533 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12534 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12535 | ` * Parameter` |
|        - | 12536 | ` *  $encoding` |
|        - | 12537 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12538 | ` *  $separtor` |
|        - | 12539 | ` *   Namespace separator (a single character)` |
|        - | 12540 | ` * Return` |
|        - | 12541 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12542 | ` */` |
|        4 | 12543 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12544 |  |
|        - | 12545 | `	ph7_xml_engine *pEngine;` |
|        5 | 12546 | `	int ns_sep = ':';` |
|        5 | 12547 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12548 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12549 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12550 | `			ns_sep = zSep[0];` |
|      ! 0 | 12551 | `		}` |
|      ! 0 | 12552 | `	}` |
|        - | 12553 | `	/* Allocate a new instance */` |
|        5 | 12554 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12555 | `	if( pEngine == 0 ){` |
|      ! 0 | 12556 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12557 | `		/* Return null */` |
|      ! 0 | 12558 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12559 | `		return PH7_OK;` |
|        - | 12560 | `	}` |
|        - | 12561 | `	/* Return the engine as a resource */` |
|        5 | 12562 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12563 | `	return PH7_OK;` |
|        3 | 12564 |  |
|        - | 12565 | `/*` |
|        - | 12566 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12567 | ` *  Release an XML engine.` |
|        - | 12568 | ` * Parameter` |
|        - | 12569 | ` *  $parser` |
|        - | 12570 | ` *   A reference to the XML parser to free.` |
|        - | 12571 | ` * Return` |
|        - | 12572 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12573 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12574 | ` */` |
|       84 | 12575 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12576 |  |
|        - | 12577 | `	ph7_xml_engine *pEngine;` |
|       85 | 12578 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12579 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12580 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12581 | `		return PH7_OK;` |
|        - | 12582 | `	}` |
|        - | 12583 | `	/* Point to the XML engine */` |
|       85 | 12584 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12585 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12586 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12587 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12588 | `		return PH7_OK;` |
|        - | 12589 | `	}` |
|        - | 12590 | `	/* Safely release the engine */` |
|       85 | 12591 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12592 | `	/* Return TRUE */` |
|       85 | 12593 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12594 | `	return PH7_OK;` |
|       43 | 12595 |  |
|        - | 12596 | `/*` |
|        - | 12597 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12598 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12599 | ` * are strings containing the names of functions.` |
|        - | 12600 | ` * Parameters` |
|        - | 12601 | ` *  $parser` |
|        - | 12602 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12603 | ` *  $start_element_handler` |
|        - | 12604 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12605 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12606 | ` *    $parser` |
|        - | 12607 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12608 | ` *   $name` |
|        - | 12609 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12610 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12611 | ` *  $attribs` |
|        - | 12612 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12613 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12614 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12615 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12616 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12617 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12618 | ` * $end_element_handler` |
|        - | 12619 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12620 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12621 | ` *    $parser` |
|        - | 12622 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12623 | ` *   $name` |
|        - | 12624 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12625 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12626 | ` *      letters.` |
|        - | 12627 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12628 | ` * Return` |
|        - | 12629 | ` * TRUE on success or FALSE on failure.` |
|        - | 12630 | ` */` |
|       66 | 12631 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12632 |  |
|        - | 12633 | `	ph7_xml_engine *pEngine;` |
|       67 | 12634 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12635 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12636 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12637 | `		return PH7_OK;` |
|        - | 12638 | `	}` |
|        - | 12639 | `	/* Point to the XML engine */` |
|       67 | 12640 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12641 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12642 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12643 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12644 | `		return PH7_OK;` |
|        - | 12645 | `	}` |
|       67 | 12646 | `	if( nArg > 1 ){` |
|        - | 12647 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12648 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12649 | `		if( nArg > 2 ){` |
|        - | 12650 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12651 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12652 | `		}` |
|       33 | 12653 | `	}` |
|        - | 12654 | `	/* All done,return TRUE */` |
|       67 | 12655 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12656 | `	return PH7_OK;` |
|       34 | 12657 |  |
|        - | 12658 | `/*` |
|        - | 12659 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12660 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12661 | ` * Parameters` |
|        - | 12662 | ` * $parser` |
|        - | 12663 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12664 | ` * $handler` |
|        - | 12665 | ` *  handler is a string containing the name of the callback.` |
|        - | 12666 | ` *  The function named by handler must accept two parameters:` |
|        - | 12667 | ` *   handler(resource $parser,string $data)` |
|        - | 12668 | ` *  $parser` |
|        - | 12669 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12670 | ` *  $data` |
|        - | 12671 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12672 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12673 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12674 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12675 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12676 | ` *   can also be supplied.` |
|        - | 12677 | ` * Return` |
|        - | 12678 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12679 | ` */` |
|       40 | 12680 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12681 |  |
|        - | 12682 | `	ph7_xml_engine *pEngine;` |
|       41 | 12683 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12684 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12685 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12686 | `		return PH7_OK;` |
|        - | 12687 | `	}` |
|        - | 12688 | `	/* Point to the XML engine */` |
|       41 | 12689 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12690 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12691 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12692 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12693 | `		return PH7_OK;` |
|        - | 12694 | `	}` |
|       41 | 12695 | `	if( nArg > 1 ){` |
|        - | 12696 | `		/* Save the user callback for later invocation */` |
|       41 | 12697 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12698 | `	}` |
|        - | 12699 | `	/* All done,return TRUE */` |
|       41 | 12700 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12701 | `	return PH7_OK;` |
|       21 | 12702 |  |
|        - | 12703 | `/*` |
|        - | 12704 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12705 | ` *  Set up default handler.` |
|        - | 12706 | ` * Parameters` |
|        - | 12707 | ` * $parser` |
|        - | 12708 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12709 | ` * $handler` |
|        - | 12710 | ` *  handler is a string containing the name of the callback.` |
|        - | 12711 | ` *  The function named by handler must accept two parameters:` |
|        - | 12712 | ` *   handler(resource $parser,string $data)` |
|        - | 12713 | ` *  $parser` |
|        - | 12714 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12715 | ` *  $data` |
|        - | 12716 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12717 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12718 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12719 | ` *   can also be supplied.` |
|        - | 12720 | ` * Return` |
|        - | 12721 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12722 | ` */` |
|        2 | 12723 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12724 |  |
|        - | 12725 | `	ph7_xml_engine *pEngine;` |
|        3 | 12726 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12727 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12728 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12729 | `		return PH7_OK;` |
|        - | 12730 | `	}` |
|        - | 12731 | `	/* Point to the XML engine */` |
|        3 | 12732 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12733 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12734 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12735 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12736 | `		return PH7_OK;` |
|        - | 12737 | `	}` |
|        3 | 12738 | `	if( nArg > 1 ){` |
|        - | 12739 | `		/* Save the user callback for later invocation */` |
|        3 | 12740 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12741 | `	}` |
|        - | 12742 | `	/* All done,return TRUE */` |
|        3 | 12743 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12744 | `	return PH7_OK;` |
|        2 | 12745 |  |
|        - | 12746 | `/*` |
|        - | 12747 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12748 | ` *  Set up end namespace declaration handler.` |
|        - | 12749 | ` * Parameters` |
|        - | 12750 | ` * $parser` |
|        - | 12751 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12752 | ` * $handler` |
|        - | 12753 | ` *  handler is a string containing the name of the callback.` |
|        - | 12754 | ` *  The function named by handler must accept two parameters:` |
|        - | 12755 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12756 | ` *  $parser` |
|        - | 12757 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12758 | ` *  $prefix` |
|        - | 12759 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12760 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12761 | ` *   can also be supplied.` |
|        - | 12762 | ` * Return` |
|        - | 12763 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12764 | ` */` |
|        2 | 12765 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12766 |  |
|        - | 12767 | `	ph7_xml_engine *pEngine;` |
|        3 | 12768 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12769 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12770 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12771 | `		return PH7_OK;` |
|        - | 12772 | `	}` |
|        - | 12773 | `	/* Point to the XML engine */` |
|        3 | 12774 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12775 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12776 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12777 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12778 | `		return PH7_OK;` |
|        - | 12779 | `	}` |
|        3 | 12780 | `	if( nArg > 1 ){` |
|        - | 12781 | `		/* Save the user callback for later invocation */` |
|        3 | 12782 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12783 | `	}` |
|        - | 12784 | `	/* All done,return TRUE */` |
|        3 | 12785 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12786 | `	return PH7_OK;` |
|        2 | 12787 |  |
|        - | 12788 | `/*` |
|        - | 12789 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12790 | ` *  Set up start namespace declaration handler.` |
|        - | 12791 | ` * Parameters` |
|        - | 12792 | ` * $parser` |
|        - | 12793 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12794 | ` * $handler` |
|        - | 12795 | ` *  handler is a string containing the name of the callback.` |
|        - | 12796 | ` *  The function named by handler must accept two parameters:` |
|        - | 12797 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12798 | ` *  $parser` |
|        - | 12799 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12800 | ` *  $prefix` |
|        - | 12801 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12802 | ` *  $uri` |
|        - | 12803 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12804 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12805 | ` *   can also be supplied.` |
|        - | 12806 | ` * Return` |
|        - | 12807 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12808 | ` */` |
|        2 | 12809 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12810 |  |
|        - | 12811 | `	ph7_xml_engine *pEngine;` |
|        3 | 12812 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12813 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12814 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12815 | `		return PH7_OK;` |
|        - | 12816 | `	}` |
|        - | 12817 | `	/* Point to the XML engine */` |
|        3 | 12818 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12819 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12820 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12821 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12822 | `		return PH7_OK;` |
|        - | 12823 | `	}` |
|        3 | 12824 | `	if( nArg > 1 ){` |
|        - | 12825 | `		/* Save the user callback for later invocation */` |
|        3 | 12826 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12827 | `	}` |
|        - | 12828 | `	/* All done,return TRUE */` |
|        3 | 12829 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12830 | `	return PH7_OK;` |
|        2 | 12831 |  |
|        - | 12832 | `/*` |
|        - | 12833 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12834 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12835 | ` * Parameters` |
|        - | 12836 | ` * $parser` |
|        - | 12837 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12838 | ` * $handler` |
|        - | 12839 | ` *  handler is a string containing the name of the callback.` |
|        - | 12840 | ` *  The function named by handler must accept three parameters:` |
|        - | 12841 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12842 | ` *  $parser` |
|        - | 12843 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12844 | ` *  $target` |
|        - | 12845 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12846 | ` *  $data` |
|        - | 12847 | `     The third parameter, data, contains the PI data.` |
|        - | 12848 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12849 | ` *   can also be supplied.` |
|        - | 12850 | ` * Return` |
|        - | 12851 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12852 | ` */` |
|        8 | 12853 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12854 |  |
|        - | 12855 | `	ph7_xml_engine *pEngine;` |
|        9 | 12856 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12857 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12858 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12859 | `		return PH7_OK;` |
|        - | 12860 | `	}` |
|        - | 12861 | `	/* Point to the XML engine */` |
|        9 | 12862 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12863 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12864 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12865 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12866 | `		return PH7_OK;` |
|        - | 12867 | `	}` |
|        9 | 12868 | `	if( nArg > 1 ){` |
|        - | 12869 | `		/* Save the user callback for later invocation */` |
|        9 | 12870 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12871 | `	}` |
|        - | 12872 | `	/* All done,return TRUE */` |
|        9 | 12873 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12874 | `	return PH7_OK;` |
|        5 | 12875 |  |
|        - | 12876 | `/*` |
|        - | 12877 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12878 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12879 | ` * Parameters` |
|        - | 12880 | ` * $parser` |
|        - | 12881 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12882 | ` * $handler` |
|        - | 12883 | ` *  handler is a string containing the name of the callback.` |
|        - | 12884 | ` *  The function named by handler must accept six parameters:` |
|        - | 12885 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12886 | ` *  $parser` |
|        - | 12887 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12888 | ` *  $entity_name` |
|        - | 12889 | ` *   The name of the entity that is about to be defined.` |
|        - | 12890 | ` *  $base` |
|        - | 12891 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12892 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12893 | ` *  $system_id` |
|        - | 12894 | ` *   System identifier for the external entity.` |
|        - | 12895 | ` *  $public_id` |
|        - | 12896 | ` *    Public identifier for the external entity.` |
|        - | 12897 | ` *  $notation_name` |
|        - | 12898 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12899 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12900 | ` *   can also be supplied.` |
|        - | 12901 | ` * Return` |
|        - | 12902 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12903 | ` */` |
|        2 | 12904 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12905 |  |
|        - | 12906 | `	ph7_xml_engine *pEngine;` |
|        3 | 12907 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12908 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12910 | `		return PH7_OK;` |
|        - | 12911 | `	}` |
|        - | 12912 | `	/* Point to the XML engine */` |
|        3 | 12913 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12914 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12915 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12916 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12917 | `		return PH7_OK;` |
|        - | 12918 | `	}` |
|        3 | 12919 | `	if( nArg > 1 ){` |
|        - | 12920 | `		/* Save the user callback for later invocation */` |
|        3 | 12921 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12922 | `	}` |
|        - | 12923 | `	/* All done,return TRUE */` |
|        3 | 12924 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12925 | `	return PH7_OK;` |
|        2 | 12926 |  |
|        - | 12927 | `/*` |
|        - | 12928 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12929 | ` *  Set up notation declaration handler.` |
|        - | 12930 | ` * Parameters` |
|        - | 12931 | ` * $parser` |
|        - | 12932 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12933 | ` * $handler` |
|        - | 12934 | ` *  handler is a string containing the name of the callback.` |
|        - | 12935 | ` *  The function named by handler must accept five parameters:` |
|        - | 12936 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12937 | ` *  $parser` |
|        - | 12938 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12939 | ` *  $entity_name` |
|        - | 12940 | ` *   The name of the entity that is about to be defined.` |
|        - | 12941 | ` *  $base` |
|        - | 12942 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12943 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12944 | ` *  $system_id` |
|        - | 12945 | ` *   System identifier for the external entity.` |
|        - | 12946 | ` *  $public_id` |
|        - | 12947 | ` *    Public identifier for the external entity.` |
|        - | 12948 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12949 | ` *  can also be supplied.` |
|        - | 12950 | ` * Return` |
|        - | 12951 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12952 | ` */` |
|        2 | 12953 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12954 |  |
|        - | 12955 | `	ph7_xml_engine *pEngine;` |
|        3 | 12956 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12957 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12958 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12959 | `		return PH7_OK;` |
|        - | 12960 | `	}` |
|        - | 12961 | `	/* Point to the XML engine */` |
|        3 | 12962 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12963 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12964 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12965 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12966 | `		return PH7_OK;` |
|        - | 12967 | `	}` |
|        3 | 12968 | `	if( nArg > 1 ){` |
|        - | 12969 | `		/* Save the user callback for later invocation */` |
|        3 | 12970 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12971 | `	}` |
|        - | 12972 | `	/* All done,return TRUE */` |
|        3 | 12973 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12974 | `	return PH7_OK;` |
|        2 | 12975 |  |
|        - | 12976 | `/*` |
|        - | 12977 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12978 | ` *  Set up external entity reference handler.` |
|        - | 12979 | ` * Parameters` |
|        - | 12980 | ` * $parser` |
|        - | 12981 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12982 | ` * $handler` |
|        - | 12983 | ` *  handler is a string containing the name of the callback.` |
|        - | 12984 | ` *  The function named by handler must accept five parameters:` |
|        - | 12985 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12986 | ` *  $parser` |
|        - | 12987 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12988 | ` *  $open_entity_names` |
|        - | 12989 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12990 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12991 | ` *  $base` |
|        - | 12992 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12993 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12994 | ` *  $system_id` |
|        - | 12995 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12996 | ` *  $public_id` |
|        - | 12997 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12998 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12999 | ` *   normalized as required by the XML spec.` |
|        - | 13000 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13001 | ` * can also be supplied.` |
|        - | 13002 | ` * Return` |
|        - | 13003 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13004 | ` */` |
|        2 | 13005 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13006 |  |
|        - | 13007 | `	ph7_xml_engine *pEngine;` |
|        3 | 13008 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13009 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13010 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13011 | `		return PH7_OK;` |
|        - | 13012 | `	}` |
|        - | 13013 | `	/* Point to the XML engine */` |
|        3 | 13014 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13015 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13016 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13017 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13018 | `		return PH7_OK;` |
|        - | 13019 | `	}` |
|        3 | 13020 | `	if( nArg > 1 ){` |
|        - | 13021 | `		/* Save the user callback for later invocation */` |
|        3 | 13022 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 13023 | `	}` |
|        - | 13024 | `	/* All done,return TRUE */` |
|        3 | 13025 | `	ph7_result_bool(pCtx,1);` |
|        3 | 13026 | `	return PH7_OK;` |
|        2 | 13027 |  |
|        - | 13028 | `/*` |
|        - | 13029 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 13030 | ` *  Gets the current line number for the given XML parser.` |
|        - | 13031 | ` * Parameters` |
|        - | 13032 | ` * $parser` |
|        - | 13033 | ` *   A reference to the XML parser.` |
|        - | 13034 | ` * Return` |
|        - | 13035 | ` *  This function returns FALSE if parser does not refer` |
|        - | 13036 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 13037 | ` *  is currently at in its data buffer.` |
|        - | 13038 | ` */` |
|        8 | 13039 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13040 |  |
|        - | 13041 | `	ph7_xml_engine *pEngine;` |
|        9 | 13042 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13043 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13044 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13045 | `		return PH7_OK;` |
|        - | 13046 | `	}` |
|        - | 13047 | `	/* Point to the XML engine */` |
|        9 | 13048 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 13049 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13050 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13051 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13052 | `		return PH7_OK;` |
|        - | 13053 | `	}` |
|        - | 13054 | `	/* Return the line number */` |
|        9 | 13055 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13056 | `	return PH7_OK;` |
|        5 | 13057 |  |
|        - | 13058 | `/*` |
|        - | 13059 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13060 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13061 | ` * Parameters` |
|        - | 13062 | ` * $parser` |
|        - | 13063 | ` *   A reference to the XML parser.` |
|        - | 13064 | ` * Return` |
|        - | 13065 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13066 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13067 | ` *  at in its data buffer (starting at 0).` |
|        - | 13068 | ` */` |
|        4 | 13069 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13070 |  |
|        - | 13071 | `	ph7_xml_engine *pEngine;` |
|        - | 13072 | `	SyStream *pStream;` |
|        - | 13073 | `	SyToken *pToken;` |
|        5 | 13074 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13075 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13076 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13077 | `		return PH7_OK;` |
|        - | 13078 | `	}` |
|        - | 13079 | `	/* Point to the XML engine */` |
|        5 | 13080 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13081 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13082 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13083 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13084 | `		return PH7_OK;` |
|        - | 13085 | `	}` |
|        - | 13086 | `	/* Point to the current processed token */` |
|        5 | 13087 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13088 | `	if( pToken == 0 ){` |
|        - | 13089 | `		/* Stream not yet processed */` |
|        3 | 13090 | `		ph7_result_int(pCtx,0);` |
|        3 | 13091 | `		return 0;` |
|        - | 13092 | `	}` |
|        - | 13093 | `	/* Point to the input stream */` |
|        3 | 13094 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13095 | `	/* Return the byte index */` |
|        3 | 13096 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13097 | `	return PH7_OK;` |
|        3 | 13098 |  |
|        - | 13099 | `/*` |
|        - | 13100 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13101 | ` *  Use XML Parser within an object.` |
|        - | 13102 | ` * NOTE` |
|        - | 13103 | ` *  This function is depreceated and is a no-op.` |
|        - | 13104 | ` * Parameters` |
|        - | 13105 | ` * $parser` |
|        - | 13106 | ` *   A reference to the XML parser.` |
|        - | 13107 | ` * $object` |
|        - | 13108 | ` *  The object where to use the XML parser.` |
|        - | 13109 | ` * Return` |
|        - | 13110 | ` * Always FALSE.` |
|        - | 13111 | ` */` |
|        2 | 13112 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13113 |  |
|        - | 13114 | `	ph7_xml_engine *pEngine;` |
|        3 | 13115 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13116 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13117 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13118 | `		return PH7_OK;` |
|        - | 13119 | `	}` |
|        - | 13120 | `	/* Point to the XML engine */` |
|      ! 0 | 13121 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13122 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13123 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13124 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13125 | `		return PH7_OK;` |
|        - | 13126 | `	}` |
|        - | 13127 | `	/*  Throw a notice and return */` |
|      ! 0 | 13128 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13129 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13130 | `		"containing an object reference and a method name."` |
|        - | 13131 | `		);` |
|        - | 13132 | `	/* Return FALSE */` |
|      ! 0 | 13133 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13134 | `	return PH7_OK;` |
|        2 | 13135 |  |
|        - | 13136 | `/*` |
|        - | 13137 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13138 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13139 | ` * Parameters` |
|        - | 13140 | ` * $parser` |
|        - | 13141 | ` *   A reference to the XML parser.` |
|        - | 13142 | ` * Return` |
|        - | 13143 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13144 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13145 | ` *  is currently at.` |
|        - | 13146 | ` */` |
|        4 | 13147 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13148 |  |
|        - | 13149 | `	ph7_xml_engine *pEngine;` |
|        - | 13150 | `	SyStream *pStream;` |
|        - | 13151 | `	SyToken *pToken;` |
|        5 | 13152 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13153 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13154 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13155 | `		return PH7_OK;` |
|        - | 13156 | `	}` |
|        - | 13157 | `	/* Point to the XML engine */` |
|        5 | 13158 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13159 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13160 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13161 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13162 | `		return PH7_OK;` |
|        - | 13163 | `	}` |
|        - | 13164 | `	/* Point to the current processed token */` |
|        5 | 13165 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13166 | `	if( pToken == 0 ){` |
|        - | 13167 | `		/* Stream not yet processed */` |
|      ! 0 | 13168 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13169 | `		return 0;` |
|        - | 13170 | `	}` |
|        - | 13171 | `	/* Point to the input stream */` |
|        5 | 13172 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13173 | `	/* Return the byte index */` |
|        5 | 13174 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13175 | `	return PH7_OK;` |
|        3 | 13176 |  |
|        - | 13177 | `/*` |
|        - | 13178 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13179 | ` *  Get XML parser error code.` |
|        - | 13180 | ` * Parameters` |
|        - | 13181 | ` * $parser` |
|        - | 13182 | ` *   A reference to the XML parser.` |
|        - | 13183 | ` * Return` |
|        - | 13184 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13185 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13186 | ` *  codes section.` |
|        - | 13187 | ` */` |
|       32 | 13188 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13189 |  |
|        - | 13190 | `	ph7_xml_engine *pEngine;` |
|       33 | 13191 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13192 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13193 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13194 | `		return PH7_OK;` |
|        - | 13195 | `	}` |
|        - | 13196 | `	/* Point to the XML engine */` |
|       33 | 13197 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13198 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13199 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13200 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13201 | `		return PH7_OK;` |
|        - | 13202 | `	}` |
|        - | 13203 | `	/* Return the error code if any */` |
|       33 | 13204 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13205 | `	return PH7_OK;` |
|       17 | 13206 |  |
|        - | 13207 | `/*` |
|        - | 13208 | ` * XML parser event callbacks` |
|        - | 13209 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13210 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13211 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13212 | ` */` |
|        - | 13213 | `/*` |
|        - | 13214 | ` * Create a scalar ph7_value holding the value` |
|        - | 13215 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13216 | ` */` |
|      148 | 13217 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13218 |  |
|        - | 13219 | `	ph7_value *pValue;` |
|        - | 13220 | `	/* Allocate a new scalar variable */` |
|      149 | 13221 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13222 | `	if( pValue == 0 ){` |
|      ! 0 | 13223 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13224 | `		return 0;` |
|        - | 13225 | `	}` |
|      149 | 13226 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13227 | `		/* Append namespace URI and the separator */` |
|        9 | 13228 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13229 | `	}` |
|        - | 13230 | `	/* Copy the tag value */` |
|      149 | 13231 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13232 | `	return pValue;` |
|       75 | 13233 |  |
|        - | 13234 | `/*` |
|        - | 13235 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13236 | ` * of an XML tag attributes.` |
|        - | 13237 | ` */` |
|       62 | 13238 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13239 |  |
|        - | 13240 | `	ph7_value *pArray;` |
|        - | 13241 | `	/* Create an empty array */` |
|       63 | 13242 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13243 | `	if( pArray == 0 ){` |
|      ! 0 | 13244 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13245 | `		return 0;` |
|        - | 13246 | `	}` |
|       63 | 13247 | `	if( nAttr > 0 ){` |
|        - | 13248 | `		ph7_value *pKey,*pValue;` |
|        - | 13249 | `		sxu32 n;` |
|        - | 13250 | `		/* Create worker variables */` |
|        5 | 13251 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13252 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13253 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13254 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13255 | `			return 0;` |
|        - | 13256 | `		}` |
|        - | 13257 | `		/* Copy attributes */` |
|        9 | 13258 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13259 | `			/* Reset string cursors */` |
|        5 | 13260 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13261 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13262 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13263 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13264 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13265 | `			/* Insert in the array */` |
|        5 | 13266 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13267 | `		}` |
|        - | 13268 | `		/* Release the worker variables */` |
|        5 | 13269 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13270 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13271 | `	}` |
|        - | 13272 | `	/* Return the freshly created array */` |
|       63 | 13273 | `	return pArray;` |
|       32 | 13274 |  |
|        - | 13275 | `/*` |
|        - | 13276 | ` * Start element handler.` |
|        - | 13277 | ` * The user defined callback must accept three parameters:` |
|        - | 13278 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13279 | ` *    $parser` |
|        - | 13280 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13281 | ` *    $name` |
|        - | 13282 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13283 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13284 | ` *    $attribs` |
|        - | 13285 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13286 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13287 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13288 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13289 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13290 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13291 | ` */` |
|       78 | 13292 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13293 |  |
|       79 | 13294 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13295 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13296 | `	/* Point to the target user defined callback */` |
|       79 | 13297 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13298 | `	/* Make sure the given callback is callable */` |
|       79 | 13299 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13300 | `		/* Not callable,return immediately*/` |
|       17 | 13301 | `		return SXRET_OK;` |
|        - | 13302 | `	}` |
|        - | 13303 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13304 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13305 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13306 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13307 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13308 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13309 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13310 | `		return SXRET_OK;` |
|        - | 13311 | `	}` |
|        - | 13312 | `	/* Invoke the user callback */` |
|       63 | 13313 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13314 | `	/* Clean-up the mess left behind */` |
|       63 | 13315 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13316 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13317 | `	return SXRET_OK;` |
|       40 | 13318 |  |
|        - | 13319 | `/*` |
|        - | 13320 | ` * End element handler.` |
|        - | 13321 | ` * The user defined callback must accept two parameters:` |
|        - | 13322 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13323 | ` *  $parser` |
|        - | 13324 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13325 | ` *  $name` |
|        - | 13326 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13327 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13328 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13329 | ` *   can also be supplied.` |
|        - | 13330 | ` */` |
|       62 | 13331 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13332 |  |
|       63 | 13333 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13334 | `	ph7_value *pCallback,*pTag;` |
|        - | 13335 | `	/* Point to the target user defined callback */` |
|       63 | 13336 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13337 | `	/* Make sure the given callback is callable */` |
|       63 | 13338 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13339 | `		/* Not callable,return immediately*/` |
|        9 | 13340 | `		return SXRET_OK;` |
|        - | 13341 | `	}` |
|        - | 13342 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13343 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13344 | `	if( pTag == 0  ){` |
|      ! 0 | 13345 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13346 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13347 | `		return SXRET_OK;` |
|        - | 13348 | `	}` |
|        - | 13349 | `	/* Invoke the user callback */` |
|       55 | 13350 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13351 | `	/* Clean-up the mess left behind */` |
|       55 | 13352 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13353 | `	return SXRET_OK;` |
|       32 | 13354 |  |
|        - | 13355 | `/*` |
|        - | 13356 | ` * Character data handler.` |
|        - | 13357 | ` *  The user defined callback must accept two parameters:` |
|        - | 13358 | ` *  handler(resource $parser,string $data)` |
|        - | 13359 | ` *  $parser` |
|        - | 13360 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13361 | ` *  $data` |
|        - | 13362 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13363 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13364 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13365 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13366 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13367 | ` */` |
|       28 | 13368 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13369 |  |
|       29 | 13370 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13371 | `	ph7_value *pCallback,*pData;` |
|        - | 13372 | `	/* Point to the target user defined callback */` |
|       29 | 13373 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13374 | `	/* Make sure the given callback is callable */` |
|       29 | 13375 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13376 | `		/* Not callable,return immediately*/` |
|       11 | 13377 | `		return SXRET_OK;` |
|        - | 13378 | `	}` |
|        - | 13379 | `	/* Create a ph7_value holding the data */` |
|       19 | 13380 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13381 | `	if( pData == 0  ){` |
|        - | 13382 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13383 | `		return SXRET_OK;` |
|        - | 13384 | `	}` |
|        - | 13385 | `	/* Invoke the user callback */` |
|       19 | 13386 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13387 | `	/* Clean-up the mess left behind */` |
|       19 | 13388 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13389 | `	return SXRET_OK;` |
|       15 | 13390 |  |
|        - | 13391 | `/*` |
|        - | 13392 | ` * Processing instruction (PI) handler.` |
|        - | 13393 | ` * The user defined callback must accept two parameters:` |
|        - | 13394 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13395 | ` *  $parser` |
|        - | 13396 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13397 | ` *  $target` |
|        - | 13398 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13399 | ` *  $data` |
|        - | 13400 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13401 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13402 | ` *    and a method name can also be supplied.` |
|        - | 13403 | ` */` |
|        8 | 13404 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13405 |  |
|        9 | 13406 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13407 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13408 | `	/* Point to the target user defined callback */` |
|        9 | 13409 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13410 | `	/* Make sure the given callback is callable */` |
|        9 | 13411 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13412 | `		/* Not callable,return immediately*/` |
|        5 | 13413 | `		return SXRET_OK;` |
|        - | 13414 | `	}` |
|        - | 13415 | `	/* Get a ph7_value holding the data */` |
|        5 | 13416 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13417 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13418 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13419 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13420 | `		return SXRET_OK;` |
|        - | 13421 | `	}` |
|        - | 13422 | `	/* Invoke the user callback */` |
|        5 | 13423 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13424 | `	/* Clean-up the mess left behind */` |
|        5 | 13425 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13426 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13427 | `	return SXRET_OK;` |
|        5 | 13428 |  |
|        - | 13429 | `/*` |
|        - | 13430 | ` * Namespace declaration handler.` |
|        - | 13431 | ` * The user defined callback must accept two parameters:` |
|        - | 13432 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13433 | ` * $parser` |
|        - | 13434 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13435 | ` * $prefix` |
|        - | 13436 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13437 | ` * $uri` |
|        - | 13438 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13439 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13440 | ` *   and a method name can also be supplied.` |
|        - | 13441 | ` */` |
|        4 | 13442 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13443 |  |
|        5 | 13444 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13445 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13446 | `	/* Point to the target user defined callback */` |
|        5 | 13447 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13448 | `	/* Make sure the given callback is callable */` |
|        5 | 13449 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13450 | `		/* Not callable,return immediately*/` |
|        3 | 13451 | `		return SXRET_OK;` |
|        - | 13452 | `	}` |
|        - | 13453 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13454 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13455 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13456 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13457 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13458 | `		return SXRET_OK;` |
|        - | 13459 | `	}` |
|        - | 13460 | `	/* Invoke the user callback */` |
|        3 | 13461 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13462 | `	/* Clean-up the mess left behind */` |
|        3 | 13463 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13464 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13465 | `	return SXRET_OK;` |
|        3 | 13466 |  |
|        - | 13467 | `/*` |
|        - | 13468 | ` * Namespace end declaration handler.` |
|        - | 13469 | ` * The user defined callback must accept two parameters:` |
|        - | 13470 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13471 | ` * $parser` |
|        - | 13472 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13473 | ` * $prefix` |
|        - | 13474 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13475 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13476 | ` *   and a method name can also be supplied.` |
|        - | 13477 | ` */` |
|        4 | 13478 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13479 |  |
|        5 | 13480 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13481 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13482 | `	/* Point to the target user defined callback */` |
|        5 | 13483 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13484 | `	/* Make sure the given callback is callable */` |
|        5 | 13485 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13486 | `		/* Not callable,return immediately*/` |
|        3 | 13487 | `		return SXRET_OK;` |
|        - | 13488 | `	}` |
|        - | 13489 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13490 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13491 | `	if( pPrefix == 0 ){` |
|        - | 13492 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13493 | `		return SXRET_OK;` |
|        - | 13494 | `	}` |
|        - | 13495 | `	/* Invoke the user callback */` |
|        3 | 13496 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13497 | `	/* Clean-up the mess left behind */` |
|        3 | 13498 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13499 | `	return SXRET_OK;` |
|        3 | 13500 |  |
|        - | 13501 | `/*` |
|        - | 13502 | ` * Error Message consumer handler.` |
|        - | 13503 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13504 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13505 | ` * underlying XML parser.` |
|        - | 13506 | ` */` |
|       34 | 13507 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13508 |  |
|       35 | 13509 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13510 | `	/* Save the error code */` |
|       35 | 13511 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13512 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13513 | `	if( pToken ){` |
|       35 | 13514 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13515 | `	}` |
|        - | 13516 | `	/* Abort XML processing immediately */` |
|       35 | 13517 | `	return SXERR_ABORT;` |
|        1 | 13518 |  |
|        - | 13519 | `/*` |
|        - | 13520 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13521 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13522 | ` *  as many times as necessary.` |
|        - | 13523 | ` * Parameters` |
|        - | 13524 | ` *  $parser` |
|        - | 13525 | ` *   A reference to the XML parser.` |
|        - | 13526 | ` *  $data` |
|        - | 13527 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13528 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13529 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13530 | ` * $is_final` |
|        - | 13531 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13532 | ` *   entirely loaded in memory.` |
|        - | 13533 | ` * Return` |
|        - | 13534 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13535 | ` */` |
|       74 | 13536 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13537 |  |
|        - | 13538 | `	ph7_xml_engine *pEngine;` |
|        - | 13539 | `	SyXMLParser *pParser;` |
|        - | 13540 | `	const char *zData;` |
|        - | 13541 | `	int nByte;` |
|       75 | 13542 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13543 | `		/* Missing/Ivalid arguments,return FALSE */` |
|      ! 0 | 13544 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13545 | `		return PH7_OK;` |
|        - | 13546 | `	}` |
|        - | 13547 | `	/* Point to the XML engine */` |
|       75 | 13548 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       75 | 13549 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13550 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13551 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13552 | `		return PH7_OK;` |
|        - | 13553 | `	}` |
|       75 | 13554 | `	if( pEngine->iNest > 0 ){` |
|        - | 13555 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13556 | `		 * in it's body which is forbidden.` |
|        - | 13557 | `		 */` |
|      ! 0 | 13558 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13559 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13560 | `			ph7_function_name(pCtx)` |
|        - | 13561 | `			);` |
|        - | 13562 | `		/* Return FALSE */` |
|      ! 0 | 13563 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13564 | `		return PH7_OK;` |
|        - | 13565 | `	}` |
|       75 | 13566 | `	pEngine->pCtx = pCtx;` |
|        - | 13567 | `	/* Point to the underlying XML parser */` |
|       75 | 13568 | `	pParser = &pEngine->sParser;` |
|        - | 13569 | `	/* Register elements handler */` |
|       75 | 13570 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13571 | `		VmXMLStartElementHandler,` |
|        - | 13572 | `		VmXMLTextHandler,` |
|        - | 13573 | `		VmXMLErrorHandler,` |
|        - | 13574 |  |
|        - | 13575 | `		VmXMLEndElementHandler,` |
|        - | 13576 | `		VmXMLPIHandler,` |
|        - | 13577 |  |
|        - | 13578 |  |
|        - | 13579 | `		VmXMLNSStartHandler,` |
|        - | 13580 | `		VmXMLNSEndHandler` |
|        - | 13581 | `		);` |
|       75 | 13582 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13583 | `	/* Extract the raw XML input */` |
|       75 | 13584 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13585 | `	/* Start the parse process */` |
|       75 | 13586 | `	pEngine->iNest++;` |
|       75 | 13587 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       75 | 13588 | `	pEngine->iNest--;` |
|        - | 13589 | `	/* Return the parse result */` |
|       75 | 13590 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       75 | 13591 | `	return PH7_OK;` |
|       38 | 13592 |  |
|        - | 13593 | `/*` |
|        - | 13594 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13595 | ` *  Sets an option in an XML parser.` |
|        - | 13596 | ` * Parameters` |
|        - | 13597 | ` *  $parser` |
|        - | 13598 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13599 | ` *  $option` |
|        - | 13600 | ` *    Which option to set. See below.` |
|        - | 13601 | ` *   The following options are available:` |
|        - | 13602 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13603 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13604 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13605 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13606 | ` * $value` |
|        - | 13607 | ` *   The option's new value.` |
|        - | 13608 | ` * Return` |
|        - | 13609 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13610 | ` * Note:` |
|        - | 13611 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13612 | ` *  function is a no-op.` |
|        - | 13613 | ` */` |
|        6 | 13614 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13615 |  |
|        - | 13616 | `	ph7_xml_engine *pEngine;` |
|        7 | 13617 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13618 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13619 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13620 | `		return PH7_OK;` |
|        - | 13621 | `	}` |
|        - | 13622 | `	/* Point to the XML engine */` |
|        7 | 13623 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13624 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13625 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13626 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13627 | `		return PH7_OK;` |
|        - | 13628 | `	}` |
|        - | 13629 | `	/* Always return FALSE */` |
|        7 | 13630 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13631 | `	return PH7_OK;` |
|        4 | 13632 |  |
|        - | 13633 | `/*` |
|        - | 13634 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13635 | ` *  Get options from an XML parser.` |
|        - | 13636 | ` * Parameters` |
|        - | 13637 | ` *  $parser` |
|        - | 13638 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13639 | ` * $option` |
|        - | 13640 | ` *   Which option to fetch.` |
|        - | 13641 | ` * Return` |
|        - | 13642 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13643 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13644 | ` */` |
|        2 | 13645 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13646 |  |
|        - | 13647 | `	ph7_xml_engine *pEngine;` |
|        - | 13648 | `	int nOp;` |
|        3 | 13649 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13650 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13651 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13652 | `		return PH7_OK;` |
|        - | 13653 | `	}` |
|        - | 13654 | `	/* Point to the XML engine */` |
|        3 | 13655 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13656 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13657 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13658 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13659 | `		return PH7_OK;` |
|        - | 13660 | `	}` |
|        - | 13661 | `	/* Extract the option */` |
|        3 | 13662 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13663 | `	switch(nOp){` |
|      ! 0 | 13664 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13665 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13666 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13667 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13668 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13669 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13670 | `		break;` |
|        1 | 13671 | `	default:` |
|        - | 13672 | `		/* Unknown option,return FALSE*/` |
|        3 | 13673 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13674 | `		break;` |
|        - | 13675 | `	}` |
|        3 | 13676 | `	return PH7_OK;` |
|        2 | 13677 |  |
|        - | 13678 | `/*` |
|        - | 13679 | ` * string xml_error_string(int $code)` |
|        - | 13680 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13681 | ` * Parameters` |
|        - | 13682 | ` *  $code` |
|        - | 13683 | ` *   An error code from xml_get_error_code().` |
|        - | 13684 | ` * Return` |
|        - | 13685 | ` *  Returns a string with a textual description of the error` |
|        - | 13686 | ` *  code, or FALSE if no description was found.` |
|        - | 13687 | ` */` |
|       30 | 13688 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13689 |  |
|       31 | 13690 | `	int nErr = -1;` |
|       31 | 13691 | `	if( nArg > 0 ){` |
|       31 | 13692 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13693 | `	}` |
|       31 | 13694 | `	switch(nErr){` |
|        1 | 13695 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13696 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13697 | `		break;` |
|      ! 0 | 13698 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13699 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13700 | `		break;` |
|      ! 0 | 13701 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13702 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13703 | `		break;` |
|        3 | 13704 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13705 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13706 | `		break;` |
|      ! 0 | 13707 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13708 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13709 | `		break;` |
|        1 | 13710 | `	case SXML_ERROR_NONE:` |
|        3 | 13711 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13712 | `		break;` |
|        1 | 13713 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13714 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13715 | `		break;` |
|      ! 0 | 13716 | `	case -1:` |
|      ! 0 | 13717 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13718 | `		break;` |
|        9 | 13719 | `	default:` |
|       19 | 13720 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13721 | `		break;` |
|        - | 13722 | `	}` |
|       31 | 13723 | `	return PH7_OK;` |
|        1 | 13724 |  |
|        - | 13725 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13726 | `/*` |
|        - | 13727 | ` * int utf8_encode(string $input)` |
|        - | 13728 | ` *  UTF-8 encoding.` |
|        - | 13729 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13730 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13731 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13732 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13733 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13734 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13735 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13736 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13737 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13738 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13739 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13740 | ` * Parameters` |
|        - | 13741 | ` * $input` |
|        - | 13742 | ` *   String to encode or NULL on failure.` |
|        - | 13743 | ` * Return` |
|        - | 13744 | ` *  An UTF-8 encoded string.` |
|        - | 13745 | ` */` |
|        2 | 13746 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13747 |  |
|        - | 13748 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13749 | `	int nByte,c,e;` |
|        3 | 13750 | `	if( nArg < 1 ){` |
|        - | 13751 | `		/* Missing arguments,return null */` |
|      ! 0 | 13752 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13753 | `		return PH7_OK;` |
|        - | 13754 | `	}` |
|        - | 13755 | `	/* Extract the target string */` |
|        3 | 13756 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13757 | `	if( nByte < 1 ){` |
|        - | 13758 | `		/* Empty string,return null */` |
|      ! 0 | 13759 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13760 | `		return PH7_OK;` |
|        - | 13761 | `	}` |
|        3 | 13762 | `	zEnd = &zIn[nByte];` |
|        - | 13763 | `	/* Start the encoding process */` |
|        2 | 13764 | `	for(;;){` |
|        5 | 13765 | `		if( zIn >= zEnd ){` |
|        - | 13766 | `			/* End of input */` |
|        3 | 13767 | `			break;` |
|        - | 13768 | `		}` |
|        3 | 13769 | `		c = zIn[0];` |
|        - | 13770 | `		/* Advance the stream cursor */` |
|        3 | 13771 | `		zIn++;` |
|        - | 13772 | `		/* Encode */` |
|        3 | 13773 | `		if( c<0x00080 ){` |
|      ! 0 | 13774 | `			e = (c&0xFF);` |
|      ! 0 | 13775 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13776 | `		}else if( c<0x00800 ){` |
|        3 | 13777 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13778 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13779 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13780 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13781 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13782 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13783 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13784 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13785 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13786 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13787 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13788 | `		}else{` |
|      ! 0 | 13789 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13790 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13791 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13792 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13793 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13794 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13795 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13796 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13797 | `		}` |
|        1 | 13798 | `	}` |
|        - | 13799 | `	/* All done */` |
|        3 | 13800 | `	return PH7_OK;` |
|        2 | 13801 |  |
|        - | 13802 | `/*` |
|        - | 13803 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13804 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13805 | ` * Status: Public Domain` |
|        - | 13806 | ` */` |
|        - | 13807 | `/*` |
|        - | 13808 | `** This lookup table is used to help decode the first byte of` |
|        - | 13809 | `** a multi-byte UTF8 character.` |
|        - | 13810 | `*/` |
|        - | 13811 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13812 |  |
|        - | 13813 |  |
|        - | 13814 |  |
|        - | 13815 |  |
|        - | 13816 |  |
|        - | 13817 |  |
|        - | 13818 |  |
|        - | 13819 |  |
|        - | 13820 | `};` |
|        - | 13821 | `/*` |
|        - | 13822 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13823 | `**` |
|        - | 13824 | `** During translation, assume that the byte that zTerm points` |
|        - | 13825 | `** is a 0x00.` |
|        - | 13826 | `**` |
|        - | 13827 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13828 | `**` |
|        - | 13829 | `** Notes On Invalid UTF-8:` |
|        - | 13830 | `**` |
|        - | 13831 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13832 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13833 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13834 | `**` |
|        - | 13835 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13836 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13837 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13838 | `**` |
|        - | 13839 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13840 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13841 | `**     and rendered as themselves even though they are technically` |
|        - | 13842 | `**     invalid characters.` |
|        - | 13843 | `**` |
|        - | 13844 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13845 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13846 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13847 | `*/` |
|        - | 13848 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13849 | `  c = *(zIn++);                                            \` |
|        - | 13850 | `  if( c>=0xc0 ){                                           \` |
|        - | 13851 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13852 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13853 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13854 | `    }                                                      \` |
|        - | 13855 | `    if( c<0x80                                             \` |
|        - | 13856 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13857 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13858 | `  }` |
|      150 | 13859 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13860 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13861 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13862 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13863 | `){` |
|        - | 13864 | `  int c;` |
|      153 | 13865 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13866 | `  *pzNext = z;` |
|      151 | 13867 | `  return c;` |
|        1 | 13868 |  |
|        - | 13869 | `/*` |
|        - | 13870 | ` * string utf8_decode(string $data)` |
|        - | 13871 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13872 | ` * Parameters` |
|        - | 13873 | ` * data` |
|        - | 13874 | ` *  An UTF-8 encoded string.` |
|        - | 13875 | ` * Return` |
|        - | 13876 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13877 | ` */` |
|        2 | 13878 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13879 |  |
|        - | 13880 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13881 | `	int nByte,c;` |
|        3 | 13882 | `	if( nArg < 1 ){` |
|        - | 13883 | `		/* Missing arguments,return null */` |
|      ! 0 | 13884 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13885 | `		return PH7_OK;` |
|        - | 13886 | `	}` |
|        - | 13887 | `	/* Extract the target string */` |
|        3 | 13888 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13889 | `	if( nByte < 1 ){` |
|        - | 13890 | `		/* Empty string,return null */` |
|      ! 0 | 13891 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13892 | `		return PH7_OK;` |
|        - | 13893 | `	}` |
|        3 | 13894 | `	zEnd = &zIn[nByte];` |
|        - | 13895 | `	/* Start the decoding process */` |
|        5 | 13896 | `	while( zIn < zEnd ){` |
|        3 | 13897 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13898 | `		if( c == 0x0 ){` |
|      ! 0 | 13899 | `			break;` |
|        - | 13900 | `		}` |
|        3 | 13901 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13902 | `	}` |
|        3 | 13903 | `	return PH7_OK;` |
|        2 | 13904 |  |
|        - | 13905 | `/* Table of built-in VM functions. */` |
|        - | 13906 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13907 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13908 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13909 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13910 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13911 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13912 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13913 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13914 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13915 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13916 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13917 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13918 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13919 | `	    /* Constants management */` |
|        - | 13920 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13921 | `	{ "define",   vm_builtin_define               },` |
|        - | 13922 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13923 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13924 | `	   /* Class/Object functions */` |
|        - | 13925 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13926 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13927 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13928 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13929 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13930 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13931 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13932 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13933 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13934 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13935 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13936 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13937 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13938 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13939 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13940 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13941 | `	   /* Random numbers/strings generators */` |
|        - | 13942 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13943 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13944 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13945 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13946 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13947 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13948 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13949 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13950 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13951 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13952 | `	   /* Language constructs functions */` |
|        - | 13953 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13954 | `	{ "print", vm_builtin_print                   },` |
|        - | 13955 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13956 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13957 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13958 | `	  /* Variable handling functions */` |
|        - | 13959 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13960 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13961 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13962 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13963 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13964 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13965 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13966 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13967 | `	  /* Ouput control functions */` |
|        - | 13968 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13969 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13970 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13971 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13972 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13973 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13974 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13975 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13976 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13977 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13978 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13979 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13980 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13981 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13982 | `	  /* Assertion functions */` |
|        - | 13983 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13984 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13985 | `	  /* Error reporting functions */` |
|        - | 13986 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13987 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13988 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13989 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13990 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13991 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13992 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13993 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13994 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13995 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13996 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13997 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13998 | `	  /* Release info */` |
|        - | 13999 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14000 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14001 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14002 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14003 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14004 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14005 | `	  /* hashmap */` |
|        - | 14006 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14007 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14008 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14009 | `	  /* URL related function */` |
|        - | 14010 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14011 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14012 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14013 | `	   /* XML processing functions */` |
|        - | 14014 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14015 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14016 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14017 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14018 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14019 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14020 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14021 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14022 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14023 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14024 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14025 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14026 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14027 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14028 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14029 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14030 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14031 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14032 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14033 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14034 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14035 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14036 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14037 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14038 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14039 | `	   /* Command line processing */` |
|        - | 14040 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14041 | `	   /* JSON encoding/decoding */` |
|        - | 14042 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14043 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14044 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14045 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14046 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14047 | `	   /* Files/URI inclusion facility */` |
|        - | 14048 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14049 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14050 | `	{ "include",      vm_builtin_include          },` |
|        - | 14051 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14052 | `	{ "require",      vm_builtin_require          },` |
|        - | 14053 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14054 | `};` |
|        - | 14055 | `/*` |
|        - | 14056 | ` * Register the built-in VM functions defined above.` |
|        - | 14057 | ` */` |
|     1304 | 14058 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14059 |  |
|        - | 14060 | `	sxi32 rc;` |
|        - | 14061 | `	sxu32 n;` |
|   163002 | 14062 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14063 | `		/* Note that these special functions have access` |
|        - | 14064 | `		 * to the underlying virtual machine as their` |
|        - | 14065 | `		 * private data.` |
|        - | 14066 | `		 */` |
|   161698 | 14067 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   161698 | 14068 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14069 | `			return rc;` |
|        - | 14070 | `		}` |
|    80850 | 14071 | `	}` |
|     1306 | 14072 | `	return SXRET_OK;` |
|      654 | 14073 |  |
|        - | 14074 | `/*` |
|        - | 14075 | ` * Check if the given name refer to an installed class.` |
|        - | 14076 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14077 | ` */` |
|     8682 | 14078 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14079 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14080 | `	const char *zName,  /* Name of the target class */` |
|        - | 14081 | `	sxu32 nByte,        /* zName length */` |
|        - | 14082 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14083 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14084 | `						 */` |
|        - | 14085 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14086 | `	)` |
|        2 | 14087 |  |
|        - | 14088 | `	SyHashEntry *pEntry;` |
|        - | 14089 | `	ph7_class *pClass;` |
|     4341 | 14090 | `		SXUNUSED(iNest);` |
|        - | 14091 | `	/* Perform a hash lookup */` |
|     8684 | 14092 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14093 |  |
|     8684 | 14094 | `	if( pEntry == 0 ){` |
|        - | 14095 | `		/* No such entry,return NULL */` |
|      ! 0 | 14096 | `		return 0;` |
|        - | 14097 | `	}` |
|     8684 | 14098 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     8684 | 14099 | `	if( !iLoadable ){` |
|        - | 14100 | `		/* Return the first class seen */` |
|     7974 | 14101 | `		return pClass;` |
|      ! 0 | 14102 | `	}else{` |
|        - | 14103 | `		/* Check the collision list */` |
|      712 | 14104 | `		while(pClass){` |
|      712 | 14105 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14106 | `				/* Class is loadable */` |
|      712 | 14107 | `				return pClass;` |
|        - | 14108 | `			}` |
|        - | 14109 | `			/* Point to the next entry */` |
|      ! 0 | 14110 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14111 | `		}` |
|        - | 14112 | `	}` |
|        - | 14113 | `	/* No such loadable class */` |
|      ! 0 | 14114 | `	return 0;` |
|     4343 | 14115 |  |
|        - | 14116 | `/*` |
|        - | 14117 | ` * Reference Table Implementation` |
|        - | 14118 | ` * Status: stable <chm@symisc.net>` |
|        - | 14119 | ` * Intro` |
|        - | 14120 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14121 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14122 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14123 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14124 | ` *  Refer to the official for more information on this powerful` |
|        - | 14125 | ` *  extension.` |
|        - | 14126 | ` */` |
|        - | 14127 | `/*` |
|        - | 14128 | ` * Allocate a new reference entry.` |
|        - | 14129 | ` */` |
|  2806180 | 14130 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14131 |  |
|        - | 14132 | `	VmRefObj *pRef;` |
|        - | 14133 | `	/* Allocate a new instance */` |
|  2806182 | 14134 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2806182 | 14135 | `	if( pRef == 0 ){` |
|      ! 0 | 14136 | `		return 0;` |
|        - | 14137 | `	}` |
|        - | 14138 | `	/* Zero the structure */` |
|  2806182 | 14139 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14140 | `	/* Initialize fields */` |
|  2806182 | 14141 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2806182 | 14142 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2806182 | 14143 | `	pRef->nIdx = nIdx;` |
|  2806182 | 14144 | `	return pRef;` |
|  1403092 | 14145 |  |
|        - | 14146 | `/*` |
|        - | 14147 | ` * Default hash function used by the reference table` |
|        - | 14148 | ` * for lookup/insertion operations.` |
|        - | 14149 | ` */` |
| 15810221 | 14150 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14151 |  |
|        - | 14152 | `	/* Calculate the hash based on the memory object index */` |
| 15810223 | 14153 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14154 |  |
|        - | 14155 | `/*` |
|        - | 14156 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14157 | ` * in the reference table.` |
|        - | 14158 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14159 | ` * otherwise.` |
|        - | 14160 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14161 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14162 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14163 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14164 | ` * Refer to the official for more information on this powerful` |
|        - | 14165 | ` * extension.` |
|        - | 14166 | ` */` |
|  8390210 | 14167 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14168 |  |
|        - | 14169 | `	VmRefObj *pRef;` |
|        - | 14170 | `	sxu32 nBucket;` |
|        - | 14171 | `	/* Point to the appropriate bucket */` |
|  8390212 | 14172 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14173 | `	/* Perform the lookup */` |
|  8390212 | 14174 | `	pRef = pVm->apRefObj[nBucket];` |
| 17351131 | 14175 | `	for(;;){` |
| 34701854 | 14176 | `		if( pRef == 0 ){` |
|  2864158 | 14177 | `			break;` |
|        - | 14178 | `		}` |
| 31837698 | 14179 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14180 | `			/* Entry found */` |
|  5526056 | 14181 | `			return pRef;` |
|        - | 14182 | `		}` |
|        - | 14183 | `		/* Point to the next entry */` |
| 26311644 | 14184 | `		pRef = pRef->pNextCollide;` |
|        2 | 14185 | `	}` |
|        - | 14186 | `	/* No such entry,return NULL */` |
|  2864158 | 14187 | `	return 0;` |
|  4195107 | 14188 |  |
|        - | 14189 | `/*` |
|        - | 14190 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14191 | ` *` |
|        - | 14192 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14193 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14194 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14195 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14196 | ` * Refer to the official for more information on this powerful` |
|        - | 14197 | ` * extension.` |
|        - | 14198 | ` */` |
|  2806180 | 14199 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14200 |  |
|        - | 14201 | `	sxu32 nBucket;` |
|  2806182 | 14202 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14203 | `		VmRefObj **apNew;` |
|        - | 14204 | `		sxu32 nNew;` |
|        - | 14205 | `		/* Allocate a larger table */` |
|     1854 | 14206 | `		nNew = pVm->nRefSize << 1;` |
|     1854 | 14207 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1854 | 14208 | `		if( apNew ){` |
|     1854 | 14209 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14210 | `			sxu32 n;` |
|        - | 14211 | `			/* Zero the structure */` |
|     1854 | 14212 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14213 | `			/* Rehash all referenced entries */` |
|  2815310 | 14214 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14215 | `				/* Remove old collision links */` |
|  2813458 | 14216 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14217 | `				/* Point to the appropriate bucket */` |
|  2813458 | 14218 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14219 | `				/* Insert the entry  */` |
|  2813458 | 14220 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2813458 | 14221 | `				if( apNew[nBucket] ){` |
|  2298042 | 14222 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149020 | 14223 | `				}` |
|  2813458 | 14224 | `				apNew[nBucket] = pEntry;` |
|        - | 14225 | `				/* Point to the next entry */` |
|  2813458 | 14226 | `				pEntry = pEntry->pNext;` |
|  1406730 | 14227 | `			}` |
|        - | 14228 | `			/* Release the old table */` |
|     1854 | 14229 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14230 | `			/* Install the new one */` |
|     1854 | 14231 | `			pVm->apRefObj = apNew;` |
|     1854 | 14232 | `			pVm->nRefSize = nNew;` |
|      926 | 14233 | `		}` |
|      926 | 14234 | `	}` |
|        - | 14235 | `	/* Point to the appropriate bucket */` |
|  2806182 | 14236 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14237 | `	/* Insert the entry */` |
|  2806182 | 14238 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2806182 | 14239 | `	if( pVm->apRefObj[nBucket] ){` |
|  2320344 | 14240 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1160011 | 14241 | `	}` |
|  2806182 | 14242 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2806182 | 14243 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2806182 | 14244 | `	pVm->nRefUsed++;` |
|  2806182 | 14245 | `	return SXRET_OK;` |
|        2 | 14246 |  |
|        - | 14247 | `/*` |
|        - | 14248 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14249 | ` * the reference table.` |
|        - | 14250 | ` * This function is invoked when the user perform an unset` |
|        - | 14251 | ` * call [i.e: unset($var); ].` |
|        - | 14252 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14253 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14254 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14255 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14256 | ` * Refer to the official for more information on this powerful` |
|        - | 14257 | ` * extension.` |
|        - | 14258 | ` */` |
|  2786174 | 14259 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14260 |  |
|        - | 14261 | `	ph7_hashmap_node **apNode;` |
|        - | 14262 | `	SyHashEntry **apEntry;` |
|        - | 14263 | `	sxu32 n;` |
|        - | 14264 | `	/* Point to the reference table */` |
|  2786176 | 14265 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2786176 | 14266 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14267 | `	/* Unlink the entry from the reference table */` |
|  2848588 | 14268 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    62414 | 14269 | `		if( apEntry[n] ){` |
|    62382 | 14270 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    31190 | 14271 | `		}` |
|    31208 | 14272 | `	}` |
|  5513050 | 14273 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2726876 | 14274 | `		if( apNode[n] ){` |
|     5125 | 14275 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2562 | 14276 | `		}` |
|  1363439 | 14277 | `	}` |
|  2786176 | 14278 | `	if( pRef->pPrevCollide ){` |
|   985801 | 14279 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   492830 | 14280 | `	}else{` |
|  1800377 | 14281 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14282 | `	}` |
|  2786176 | 14283 | `	if( pRef->pNextCollide ){` |
|  1519148 | 14284 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   759343 | 14285 | `	}` |
|  2786176 | 14286 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14287 | `	/* Release the node */` |
|  2786176 | 14288 | `	SySetRelease(&pRef->aReference);` |
|  2786176 | 14289 | `	SySetRelease(&pRef->aArrEntries);` |
|  2786176 | 14290 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2786176 | 14291 | `	pVm->nRefUsed--;` |
|  2786176 | 14292 | `	return SXRET_OK;` |
|        2 | 14293 |  |
|        - | 14294 | `/*` |
|        - | 14295 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14296 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14297 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14298 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14299 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14300 | ` * Refer to the official for more information on this powerful` |
|        - | 14301 | ` * extension.` |
|        - | 14302 | ` */` |
|  2824272 | 14303 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14304 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14305 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14306 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14307 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14308 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14309 | `	)` |
|        2 | 14310 |  |
|  2824274 | 14311 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14312 | `	VmRefObj *pRef;` |
|        - | 14313 | `	/* Check if the referenced object already exists */` |
|  2824274 | 14314 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2824274 | 14315 | `	if( pRef == 0 ){` |
|        - | 14316 | `		/* Create a new entry */` |
|  2806182 | 14317 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2806182 | 14318 | `		if( pRef == 0 ){` |
|      ! 0 | 14319 | `			return SXERR_MEM;` |
|        - | 14320 | `		}` |
|  2806182 | 14321 | `		pRef->iFlags = iFlags;` |
|        - | 14322 | `		/* Install the entry */` |
|  2806182 | 14323 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1403090 | 14324 | `	}` |
|  2829186 | 14325 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14326 | `		/* Safely ignore the exception frame */` |
|     4914 | 14327 | `		pFrame = pFrame->pParent;` |
|        2 | 14328 | `	}` |
|  2824274 | 14329 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14330 | `		VmSlot sRef;` |
|        - | 14331 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14332 | `		 * be deleted when we leave this frame.` |
|        - | 14333 | `		 */` |
|    57990 | 14334 | `		sRef.nIdx = nIdx;` |
|    57990 | 14335 | `		sRef.pUserData = pEntry;` |
|    57990 | 14336 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14337 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14338 | `		}` |
|    28994 | 14339 | `	}` |
|  2824274 | 14340 | `	if( pEntry ){` |
|        - | 14341 | `		/* Address of the hash-entry */` |
|    75916 | 14342 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    37957 | 14343 | `	}` |
|  2824274 | 14344 | `	if( pMapEntry ){` |
|        - | 14345 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2745336 | 14346 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1372667 | 14347 | `	}` |
|  2824274 | 14348 | `	return SXRET_OK;` |
|  1412138 | 14349 |  |
|        - | 14350 | `/*` |
|        - | 14351 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14352 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14353 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14354 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14355 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14356 | ` * Refer to the official for more information on this powerful` |
|        - | 14357 | ` * extension.` |
|        - | 14358 | ` */` |
|  2779744 | 14359 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14360 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14361 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14362 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14363 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14364 | `	)` |
|        2 | 14365 |  |
|        - | 14366 | `	VmRefObj *pRef;` |
|        - | 14367 | `	sxu32 n;` |
|        - | 14368 | `	/* Check if the referenced object already exists */` |
|  2779746 | 14369 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2779746 | 14370 | `	if( pRef == 0 ){` |
|        - | 14371 | `		/* Not such entry */` |
|    57958 | 14372 | `		return SXERR_NOTFOUND;` |
|        - | 14373 | `	}` |
|        - | 14374 | `	/* Remove the desired entry */` |
|  2721790 | 14375 | `	if( pEntry ){` |
|        - | 14376 | `		SyHashEntry **apEntry;` |
|       33 | 14377 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14378 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14379 | `			if( apEntry[n] == pEntry ){` |
|        - | 14380 | `				/* Nullify the entry */` |
|       33 | 14381 | `				apEntry[n] = 0;` |
|        - | 14382 | `				/*` |
|        - | 14383 | `				 * NOTE:` |
|        - | 14384 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14385 | `				 * we avoid wasting spaces.` |
|        - | 14386 | `				 */` |
|       16 | 14387 | `			}` |
|       49 | 14388 | `		}` |
|       16 | 14389 | `	}` |
|  2721790 | 14390 | `	if( pMapEntry ){` |
|        - | 14391 | `		ph7_hashmap_node **apNode;` |
|  2721758 | 14392 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5443602 | 14393 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2721846 | 14394 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14395 | `				/* nullify the entry */` |
|  2721758 | 14396 | `				apNode[n] = 0;` |
|  1360878 | 14397 | `			}` |
|  1360924 | 14398 | `		}` |
|  1360878 | 14399 | `	}` |
|  2721790 | 14400 | `	return SXRET_OK;` |
|  1389874 | 14401 |  |
|        - | 14402 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14403 | `/*` |
|        - | 14404 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14405 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14406 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14407 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14408 | ` * For more information on how to register IO stream devices,please` |
|        - | 14409 | ` * refer to the official documentation.` |
|        - | 14410 | ` */` |
|    19846 | 14411 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14412 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14413 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14414 | `	int nByte              /* *pzDevice length*/` |
|        - | 14415 | `	)` |
|        2 | 14416 |  |
|        - | 14417 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14418 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14419 | `	SyString sDev,sCur;` |
|        - | 14420 | `	sxu32 n,nEntry;` |
|        - | 14421 | `	int rc;` |
|        - | 14422 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    19848 | 14423 | `	zNext = zCur = zIn = *pzDevice;` |
|    19848 | 14424 | `	zEnd = &zIn[nByte];` |
|  1231691 | 14425 | `	while( zIn < zEnd ){` |
|  1211847 | 14426 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14427 | `			/* Got one */` |
|        3 | 14428 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14429 | `			break;` |
|        - | 14430 | `		}` |
|        - | 14431 | `		/* Advance the cursor */` |
|  1211845 | 14432 | `		zIn++;` |
|        2 | 14433 | `	}` |
|    19848 | 14434 | `	if( zIn >= zEnd ){` |
|        - | 14435 | `		/* No such scheme,return the default stream */` |
|    19846 | 14436 | `		return pVm->pDefStream;` |
|        - | 14437 | `	}` |
|        3 | 14438 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14439 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14440 | `	SyStringFullTrim(&sDev);` |
|        - | 14441 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14442 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14443 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14444 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14445 | `		pStream = apStream[n];` |
|        3 | 14446 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14447 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14448 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14449 | `		if( rc == 0 ){` |
|        - | 14450 | `			/* Stream device found */` |
|        3 | 14451 | `			*pzDevice = zNext;` |
|        3 | 14452 | `			return pStream;` |
|        - | 14453 | `		}` |
|      ! 0 | 14454 | `	}` |
|        - | 14455 | `	/* No such stream,return NULL */` |
|      ! 0 | 14456 | `	return 0;` |
|     9925 | 14457 |  |
|        - | 14458 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14459 | `/*` |
|        - | 14460 | ` * Section:` |
|        - | 14461 | ` *    HTTP/URI related routines.` |
|        - | 14462 | ` * Status:` |
|        - | 14463 | ` *    Stable.` |
|        - | 14464 | ` */` |
|        - | 14465 | ` /*` |
|        - | 14466 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14467 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14468 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14469 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14470 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14471 | `  * the input to its fields.` |
|        - | 14472 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14473 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14474 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14475 | `  * input is encountered.` |
|        - | 14476 | `  */` |
|       26 | 14477 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14478 | ` {` |
|       27 | 14479 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14480 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14481 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14482 | `	 const char *zCur;` |
|        - | 14483 | `	 SyString *pComp;` |
|       27 | 14484 | `	 sxu32 nPos = 0;` |
|        - | 14485 | `	 sxi32 rc;` |
|        - | 14486 | `	 /* Zero the structure first */` |
|       27 | 14487 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14488 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14489 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14490 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14491 | `	 /* Find the first '/' separator */` |
|       27 | 14492 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14493 | `	 if( rc != SXRET_OK ){` |
|        - | 14494 | `		 /* Assume a host name only */` |
|        7 | 14495 | `		 zCur = zEnd;` |
|        7 | 14496 | `		 bHostOnly = TRUE;` |
|        7 | 14497 | `		 goto ProcessHost;` |
|        - | 14498 | `	 }` |
|       21 | 14499 | `	 zCur = &zUri[nPos];` |
|       21 | 14500 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14501 | `		 /* Extract a scheme:` |
|        - | 14502 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14503 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14504 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14505 | `		  * fail.` |
|        - | 14506 | `		  */` |
|       19 | 14507 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14508 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14509 | `		SyStringLeftTrim(pComp);` |
|        9 | 14510 | `	 }` |
|       21 | 14511 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14512 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14513 | `		  /* No authority */` |
|      ! 0 | 14514 | `		  goto PathSplit;` |
|        - | 14515 | `		}` |
|        - | 14516 | `		 /* There is something here , we will assume its an authority` |
|        - | 14517 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14518 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14519 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14520 | `		  * and let the caller handle all the validation process.` |
|        - | 14521 | `		  */` |
|      ! 0 | 14522 | `		 goto ProcessHost;` |
|        - | 14523 | `	 }` |
|       21 | 14524 | `	 zUri = &zCur[2];` |
|       21 | 14525 | `	 zCur = zEnd;` |
|       21 | 14526 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14527 | `	 if( rc == SXRET_OK ){` |
|       17 | 14528 | `		 zCur = &zUri[nPos];` |
|        8 | 14529 | `	 }` |
|        2 | 14530 | ` ProcessHost:` |
|        - | 14531 | `	 /* Extract user information if present */` |
|       27 | 14532 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14533 | `	 if( rc == SXRET_OK ){` |
|        7 | 14534 | `		 if( nPos > 0 ){` |
|        - | 14535 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14536 | `			 pComp = &pOut->sUser;` |
|        7 | 14537 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14538 | `			 /* Extract the password if available */` |
|        7 | 14539 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14540 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14541 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14542 | `				 pComp = &pOut->sPass;` |
|        7 | 14543 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14544 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14545 | `			 }` |
|        - | 14546 | `			 /* Update the cursor */` |
|        7 | 14547 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14548 | `		 }else{` |
|      ! 0 | 14549 | `			 zUri++;` |
|        - | 14550 | `		 }` |
|        3 | 14551 | `	 }` |
|       27 | 14552 | `	 pComp = &pOut->sHost;` |
|       27 | 14553 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14554 | `		 zUri++;` |
|      ! 0 | 14555 | `	 }` |
|       27 | 14556 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14557 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14558 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14559 | `		  */` |
|        3 | 14560 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14561 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14562 | `			 zUri++; pComp->nByte++;` |
|        1 | 14563 | `		 }` |
|        3 | 14564 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14565 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14566 | `		 }` |
|        3 | 14567 | `		 zUri++;` |
|        3 | 14568 | `		 bIPv6 = TRUE;` |
|        1 | 14569 | `	 }` |
|        - | 14570 | `	 /* Extract a port number if available */` |
|       27 | 14571 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14572 | `	 if( rc == SXRET_OK ){` |
|       11 | 14573 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14574 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14575 | `		 }` |
|       11 | 14576 | `		 pComp = &pOut->sPort;` |
|       11 | 14577 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14578 | `	 }` |
|       27 | 14579 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14580 | `		 return SXRET_OK;` |
|        - | 14581 | `	 }` |
|       10 | 14582 | `PathSplit:` |
|       21 | 14583 | `	 zUri = zCur;` |
|       21 | 14584 | `	 pComp = &pOut->sPath;` |
|       21 | 14585 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14586 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14587 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14588 | `	 }` |
|       17 | 14589 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14590 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14591 | `		 pComp = &pOut->sQuery;` |
|        5 | 14592 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14593 | `	 }` |
|       17 | 14594 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14595 | `		 /* Update path or query length */` |
|        5 | 14596 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14597 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14598 | `		 }else{` |
|        5 | 14599 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14600 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14601 | `				 return SXERR_SYNTAX;` |
|        - | 14602 | `			 }` |
|        5 | 14603 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14604 | `		 }` |
|        5 | 14605 | `		 pComp = &pOut->sFragment;` |
|        5 | 14606 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14607 | `	 }` |
|       17 | 14608 | `	 return SXRET_OK;` |
|       14 | 14609 | ` }` |
|        - | 14610 | ` /*` |
|        - | 14611 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14612 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14613 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14614 | ` */` |
|      ! 0 | 14615 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14616 |  |
|        - | 14617 | `  	const char *zIn;` |
|        - | 14618 | `  	sxu32 nPos;` |
|        - | 14619 | `	/* Jump leading white spaces */` |
|      ! 0 | 14620 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14621 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14622 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14623 | `		return SXERR_EOF; /* End of input */` |
|        - | 14624 | `	}` |
|      ! 0 | 14625 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14626 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14627 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14628 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14629 | `		return SXERR_MORE;` |
|        - | 14630 | `	}` |
|      ! 0 | 14631 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14632 | `  	pCurrent->nByte	= nPos;` |
|        - | 14633 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14634 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14635 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14636 | `  	return SXRET_OK;` |
|      ! 0 | 14637 | ` }` |
|        - | 14638 | ` /*` |
|        - | 14639 | `  * Split a single MIME header into a name value pair.` |
|        - | 14640 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14641 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14642 | `  * is encountered.` |
|        - | 14643 | `  * Note: This function handle also mult-line headers.` |
|        - | 14644 | `  */` |
|      ! 0 | 14645 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14646 | ` {` |
|        - | 14647 | `	 SyString *pName;` |
|        - | 14648 | `	 sxu32 nPos;` |
|        - | 14649 | `	 sxi32 rc;` |
|      ! 0 | 14650 | `	 if( nLen < 1 ){` |
|      ! 0 | 14651 | `		 return SXERR_NEXT;` |
|        - | 14652 | `	 }` |
|        - | 14653 | `	 /* Check for multi-line header */` |
|      ! 0 | 14654 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14655 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14656 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14657 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14658 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14659 | `		}else{` |
|        - | 14660 | `			/* Update header value length */` |
|      ! 0 | 14661 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14662 | `		}` |
|        - | 14663 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14664 | `		 return SXERR_CONTINUE;` |
|        - | 14665 | `	 }` |
|        - | 14666 | `	/* Split the header */` |
|      ! 0 | 14667 | `	pName = &pHdr->sName;` |
|      ! 0 | 14668 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14669 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14670 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14671 | `	}` |
|      ! 0 | 14672 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14673 | `	SyStringFullTrim(pName);` |
|        - | 14674 | `	/* Extract a header value */` |
|      ! 0 | 14675 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14676 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14677 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14678 | `	return SXRET_OK;` |
|      ! 0 | 14679 | ` }` |
|        - | 14680 | ` /*` |
|        - | 14681 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14682 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14683 | `  * routine is called in order to extract MIME headers.` |
|        - | 14684 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14685 | `  * more inputs.` |
|        - | 14686 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14687 | `  */` |
|      ! 0 | 14688 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14689 | ` {` |
|      ! 0 | 14690 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14691 | `	 SyString sCurrent;` |
|        - | 14692 | `	 SyhttpHeader sHdr;` |
|        - | 14693 | `	 sxu8 bEol;` |
|        - | 14694 | `	 sxi32 rc;` |
|      ! 0 | 14695 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14696 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14697 | `	 }` |
|      ! 0 | 14698 | `	 bEol = FALSE;` |
|      ! 0 | 14699 | `	 for(;;){` |
|      ! 0 | 14700 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14701 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14702 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14703 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14704 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14705 | `				 break;` |
|        - | 14706 | `			 }` |
|      ! 0 | 14707 | `			 bEol = TRUE;` |
|      ! 0 | 14708 | `		 }` |
|        - | 14709 | `		 /* Process the header */` |
|      ! 0 | 14710 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14711 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14712 | `				 break;` |
|        - | 14713 | `			 }` |
|        - | 14714 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14715 | `			  * in case we face one of them.` |
|        - | 14716 | `			  */` |
|      ! 0 | 14717 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14718 | `		 }` |
|      ! 0 | 14719 | `		 if( bEol ){` |
|      ! 0 | 14720 | `			 break;` |
|        - | 14721 | `		 }` |
|      ! 0 | 14722 | `	 } /* for(;;) */` |
|      ! 0 | 14723 | `	 return SXRET_OK;` |
|      ! 0 | 14724 | ` }` |
|        - | 14725 | ` /*` |
|        - | 14726 | `  * Process the first line of a HTTP request.` |
|        - | 14727 | `  * This routine perform the following operations` |
|        - | 14728 | `  *  1) Extract the HTTP method.` |
|        - | 14729 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14730 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14731 | `  */` |
|      ! 0 | 14732 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14733 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14734 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14735 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14736 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14737 | `	 )` |
|      ! 0 | 14738 | ` {` |
|        - | 14739 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14740 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14741 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14742 | `	 SyString sLine;` |
|        - | 14743 | `	 sxu32 nLen;` |
|        - | 14744 | `	 sxi32 rc;` |
|        - | 14745 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14746 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14747 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14748 | `		 return rc;` |
|        - | 14749 | `	 }` |
|      ! 0 | 14750 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14751 | `		 /* Empty HTTP request */` |
|      ! 0 | 14752 | `		 return SXERR_EMPTY;` |
|        - | 14753 | `	 }` |
|        - | 14754 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14755 | `	 zIn = sLine.zString;` |
|      ! 0 | 14756 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14757 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14758 | `		 zIn++;` |
|      ! 0 | 14759 | `	 }` |
|        - | 14760 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14761 | `	 zPtr = zIn;` |
|      ! 0 | 14762 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14763 | `		 zIn++;` |
|      ! 0 | 14764 | `	 }` |
|      ! 0 | 14765 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14766 | `	 if( zIn > zPtr ){` |
|        - | 14767 | `		 sxu32 i;` |
|      ! 0 | 14768 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14769 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14770 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14771 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14772 | `				 break;` |
|        - | 14773 | `			 }` |
|      ! 0 | 14774 | `		 }` |
|      ! 0 | 14775 | `	 }` |
|        - | 14776 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14777 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14778 | `		 zIn++;` |
|      ! 0 | 14779 | `	 }` |
|        - | 14780 | `	  /* Extract the request URI */` |
|      ! 0 | 14781 | `	 zPtr = zIn;` |
|      ! 0 | 14782 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14783 | `		 zIn++;` |
|      ! 0 | 14784 | `	 }` |
|      ! 0 | 14785 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14786 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14787 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14788 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14789 | `	 }` |
|        - | 14790 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14791 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14792 | `		 zIn++;` |
|      ! 0 | 14793 | `	 }` |
|        - | 14794 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14795 | `	 zPtr = zIn;` |
|      ! 0 | 14796 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14797 | `		 zIn++;` |
|      ! 0 | 14798 | `	 }` |
|      ! 0 | 14799 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14800 | `	 rc = 1;` |
|      ! 0 | 14801 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14802 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14803 | `	 }` |
|      ! 0 | 14804 | `	 if( !rc ){` |
|      ! 0 | 14805 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14806 | `	 }` |
|      ! 0 | 14807 | `	 return SXRET_OK;` |
|      ! 0 | 14808 | ` }` |
|        - | 14809 | ` /*` |
|        - | 14810 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14811 | `  * into a name value pair.` |
|        - | 14812 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14813 | `  * After the tokenization process,register the decoded queries` |
|        - | 14814 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14815 | `  */` |
|      ! 0 | 14816 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14817 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14818 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14819 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14820 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14821 | `	 )` |
|      ! 0 | 14822 | ` {` |
|      ! 0 | 14823 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14824 | `	 const char *zIn = pQuery->zString;` |
|        - | 14825 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14826 | `	 SyString sName,sValue;` |
|        - | 14827 | `	 const char *zPtr;` |
|        - | 14828 | `	 sxu32 nBlobOfft;` |
|        - | 14829 | `	 /* Extract superglobals */` |
|      ! 0 | 14830 | `	 if( is_post ){` |
|        - | 14831 | `		 /* $_POST superglobal */` |
|      ! 0 | 14832 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14833 | `	 }else{` |
|        - | 14834 | `		 /* $_GET superglobal */` |
|      ! 0 | 14835 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14836 | `	 }` |
|      ! 0 | 14837 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14838 | `	 /* Split up the raw query */` |
|      ! 0 | 14839 | `	 for(;;){` |
|        - | 14840 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14841 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14842 | `			 zIn++;` |
|      ! 0 | 14843 | `		 }` |
|      ! 0 | 14844 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14845 | `			 break;` |
|        - | 14846 | `		 }` |
|      ! 0 | 14847 | `		 zPtr = zIn;` |
|      ! 0 | 14848 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14849 | `			 zPtr++;` |
|      ! 0 | 14850 | `		 }` |
|        - | 14851 | `		 /* Reset the working buffer */` |
|      ! 0 | 14852 | `		 SyBlobReset(pWorker);` |
|        - | 14853 | `		 /* Decode the entry */` |
|      ! 0 | 14854 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14855 | `		 /* Save the entry */` |
|      ! 0 | 14856 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14857 | `		 sValue.zString = 0;` |
|      ! 0 | 14858 | `		 sValue.nByte = 0;` |
|      ! 0 | 14859 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14860 | `			 zPtr++;` |
|      ! 0 | 14861 | `			 zIn = zPtr;` |
|        - | 14862 | `			 /* Store field value */` |
|      ! 0 | 14863 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14864 | `				 zPtr++;` |
|      ! 0 | 14865 | `			 }` |
|      ! 0 | 14866 | `			 if( zPtr > zIn ){` |
|        - | 14867 | `				 /* Decode the value */` |
|      ! 0 | 14868 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14869 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14870 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14871 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14872 |  |
|      ! 0 | 14873 | `			 }` |
|        - | 14874 | `			 /* Synchronize pointers */` |
|      ! 0 | 14875 | `			 zIn = zPtr;` |
|      ! 0 | 14876 | `		 }` |
|      ! 0 | 14877 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14878 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14879 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14880 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14881 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14882 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14883 | `				 );` |
|      ! 0 | 14884 | `		 }` |
|      ! 0 | 14885 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14886 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14887 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14888 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14889 | `					 );` |
|      ! 0 | 14890 | `		 }` |
|        - | 14891 | `		 /* Advance the pointer */` |
|      ! 0 | 14892 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14893 | `	 }` |
|        - | 14894 | `	/* All done*/` |
|      ! 0 | 14895 | `	return SXRET_OK;` |
|      ! 0 | 14896 | ` }` |
|        - | 14897 | ` /*` |
|        - | 14898 | `  * Extract MIME header value from the given set.` |
|        - | 14899 | `  * Return header value on success. NULL otherwise.` |
|        - | 14900 | `  */` |
|      ! 0 | 14901 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14902 | ` {` |
|        - | 14903 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14904 | `	 SyString sMime;` |
|        - | 14905 | `	 sxu32 n;` |
|      ! 0 | 14906 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14907 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14908 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14909 | `	 /* Perform the lookup */` |
|      ! 0 | 14910 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14911 | `		 pMime = &aMime[n];` |
|      ! 0 | 14912 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14913 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14914 | `			 return &pMime->sValue;` |
|        - | 14915 | `		 }` |
|      ! 0 | 14916 | `	 }` |
|        - | 14917 | `	 /* No such MIME header */` |
|      ! 0 | 14918 | `	 return 0;` |
|      ! 0 | 14919 | ` }` |
|        - | 14920 | ` /*` |
|        - | 14921 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14922 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14923 | `  */` |
|      ! 0 | 14924 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14925 | ` {` |
|      ! 0 | 14926 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14927 | `	 SyString sName,sValue;` |
|        - | 14928 | `	 ph7_value *pCookie;` |
|        - | 14929 | `	 sxu32 nOfft;` |
|        - | 14930 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14931 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14932 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14933 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14934 | `		 return SXERR_NOTFOUND;` |
|        - | 14935 | `	 }` |
|      ! 0 | 14936 | `	 for(;;){` |
|        - | 14937 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14938 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14939 | `			 zIn++;` |
|      ! 0 | 14940 | `		 }` |
|      ! 0 | 14941 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14942 | `			 break;` |
|        - | 14943 | `		 }` |
|        - | 14944 | `		  /* Reset the working buffer */` |
|      ! 0 | 14945 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14946 | `		 zDelimiter = zIn;` |
|        - | 14947 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14948 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14949 | `			 zDelimiter++;` |
|      ! 0 | 14950 | `		 }` |
|      ! 0 | 14951 | `		 zPtr = zIn;` |
|      ! 0 | 14952 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14953 | `			 zPtr++;` |
|      ! 0 | 14954 | `		 }` |
|        - | 14955 | `		 /* Decode the cookie */` |
|      ! 0 | 14956 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14957 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14958 | `		 zPtr++;` |
|      ! 0 | 14959 | `		 sValue.zString = 0;` |
|      ! 0 | 14960 | `		 sValue.nByte = 0;` |
|      ! 0 | 14961 | `		 if( zPtr < zDelimiter ){` |
|        - | 14962 | `			 /* Got a Cookie value */` |
|      ! 0 | 14963 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14964 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14965 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14966 | `		 }` |
|        - | 14967 | `		 /* Synchronize pointers */` |
|      ! 0 | 14968 | `		 zIn = &zDelimiter[1];` |
|        - | 14969 | `		 /* Perform the insertion */` |
|      ! 0 | 14970 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14971 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14972 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14973 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14974 | `			 );` |
|      ! 0 | 14975 | `	 }` |
|      ! 0 | 14976 | `	 return SXRET_OK;` |
|      ! 0 | 14977 | ` }` |
|        - | 14978 | ` /*` |
|        - | 14979 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14980 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14981 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14982 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14983 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14984 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14985 | `  * a malformed HTTP request.` |
|        - | 14986 | `  */` |
|      ! 0 | 14987 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14988 | ` {` |
|        - | 14989 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14990 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14991 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14992 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14993 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14994 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14995 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14996 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14997 | `	 sxi32 rc;` |
|      ! 0 | 14998 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14999 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 15000 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 15001 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 15002 | `	 SyStringFullTrim(&sRequest);` |
|        - | 15003 | `	 /* Process the first line */` |
|      ! 0 | 15004 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 15005 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 15006 | `		 return rc;` |
|        - | 15007 | `	 }` |
|        - | 15008 | `	 /* Process MIME headers */` |
|      ! 0 | 15009 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 15010 | `	 /*` |
|        - | 15011 | `	  * Setup $_SERVER environments` |
|        - | 15012 | `	  */` |
|        - | 15013 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 15014 | `	 ph7_vm_config(pVm,` |
|        - | 15015 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15016 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 15017 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 15018 | `		 sizeof("HTTP/1.1")-1` |
|        - | 15019 | `		 );` |
|        - | 15020 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 15021 | `	 ph7_vm_config(pVm,` |
|        - | 15022 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15023 | `		 "REQUEST_METHOD",` |
|      ! 0 | 15024 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 15025 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 15026 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 15027 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 15028 | `		 -1 /* Compute attribute length automatically */` |
|        - | 15029 | `		 );` |
|      ! 0 | 15030 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 15031 | `		 pValue = &sUri.sQuery;` |
|        - | 15032 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 15033 | `		 ph7_vm_config(pVm,` |
|        - | 15034 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15035 | `			 "QUERY_STRING",` |
|      ! 0 | 15036 | `			 pValue->zString,` |
|      ! 0 | 15037 | `			 pValue->nByte` |
|        - | 15038 | `			 );` |
|        - | 15039 | `		 /* Decoded the raw query */` |
|      ! 0 | 15040 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 15041 | `	 }` |
|        - | 15042 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 15043 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 15044 | `	 ph7_vm_config(pVm,` |
|        - | 15045 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15046 | `		 "REQUEST_URI",` |
|      ! 0 | 15047 | `		 pValue->zString,` |
|      ! 0 | 15048 | `		 pValue->nByte` |
|        - | 15049 | `		 );` |
|        - | 15050 | `	 /*` |
|        - | 15051 | `	  * 'PATH_INFO'` |
|        - | 15052 | `	  * 'ORIG_PATH_INFO'` |
|        - | 15053 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 15054 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 15055 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15056 | `	  * /some/stuff.` |
|        - | 15057 | `	  */` |
|      ! 0 | 15058 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15059 | `	 ph7_vm_config(pVm,` |
|        - | 15060 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15061 | `		 "PATH_INFO",` |
|      ! 0 | 15062 | `		 pValue->zString,` |
|      ! 0 | 15063 | `		 pValue->nByte` |
|        - | 15064 | `		 );` |
|      ! 0 | 15065 | `	 ph7_vm_config(pVm,` |
|        - | 15066 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15067 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15068 | `		 pValue->zString,` |
|      ! 0 | 15069 | `		 pValue->nByte` |
|        - | 15070 | `		 );` |
|        - | 15071 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15072 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15073 | `	 if( pValue ){` |
|      ! 0 | 15074 | `		 ph7_vm_config(pVm,` |
|        - | 15075 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15076 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15077 | `			 pValue->zString,` |
|      ! 0 | 15078 | `			 pValue->nByte` |
|        - | 15079 | `		 );` |
|      ! 0 | 15080 | `	 }` |
|        - | 15081 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15082 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15083 | `	 if( pValue ){` |
|      ! 0 | 15084 | `		 ph7_vm_config(pVm,` |
|        - | 15085 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15086 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15087 | `			 pValue->zString,` |
|      ! 0 | 15088 | `			 pValue->nByte` |
|        - | 15089 | `		 );` |
|      ! 0 | 15090 | `	 }` |
|        - | 15091 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15092 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15093 | `	 if( pValue ){` |
|      ! 0 | 15094 | `		 ph7_vm_config(pVm,` |
|        - | 15095 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15096 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15097 | `			 pValue->zString,` |
|      ! 0 | 15098 | `			 pValue->nByte` |
|        - | 15099 | `		 );` |
|      ! 0 | 15100 | `	 }` |
|        - | 15101 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15102 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15103 | `	 if( pValue ){` |
|      ! 0 | 15104 | `		 ph7_vm_config(pVm,` |
|        - | 15105 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15106 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15107 | `			 pValue->zString,` |
|      ! 0 | 15108 | `			 pValue->nByte` |
|        - | 15109 | `		 );` |
|      ! 0 | 15110 | `	 }` |
|        - | 15111 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15112 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15113 | `	 if( pValue ){` |
|      ! 0 | 15114 | `		 ph7_vm_config(pVm,` |
|        - | 15115 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15116 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15117 | `			 pValue->zString,` |
|      ! 0 | 15118 | `			 pValue->nByte` |
|        - | 15119 | `		 );` |
|      ! 0 | 15120 | `	 }` |
|        - | 15121 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15122 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15123 | `	 if( pValue ){` |
|      ! 0 | 15124 | `		 ph7_vm_config(pVm,` |
|        - | 15125 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15126 | `			 "HTTP_HOST",` |
|      ! 0 | 15127 | `			 pValue->zString,` |
|      ! 0 | 15128 | `			 pValue->nByte` |
|        - | 15129 | `		 );` |
|      ! 0 | 15130 | `	 }` |
|        - | 15131 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15132 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15133 | `	 if( pValue ){` |
|      ! 0 | 15134 | `		 ph7_vm_config(pVm,` |
|        - | 15135 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15136 | `			 "HTTP_REFERER",` |
|      ! 0 | 15137 | `			 pValue->zString,` |
|      ! 0 | 15138 | `			 pValue->nByte` |
|        - | 15139 | `		 );` |
|      ! 0 | 15140 | `	 }` |
|        - | 15141 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15142 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15143 | `	 if( pValue ){` |
|      ! 0 | 15144 | `		 ph7_vm_config(pVm,` |
|        - | 15145 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15146 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15147 | `			 pValue->zString,` |
|      ! 0 | 15148 | `			 pValue->nByte` |
|        - | 15149 | `		 );` |
|      ! 0 | 15150 | `	 }` |
|        - | 15151 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15152 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15153 | `	   */` |
|      ! 0 | 15154 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15155 | `	 if( pValue ){` |
|      ! 0 | 15156 | `		 ph7_vm_config(pVm,` |
|        - | 15157 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15158 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15159 | `			 pValue->zString,` |
|      ! 0 | 15160 | `			 pValue->nByte` |
|        - | 15161 | `		 );` |
|      ! 0 | 15162 | `		 ph7_vm_config(pVm,` |
|        - | 15163 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15164 | `			 "PHP_AUTH",` |
|      ! 0 | 15165 | `			 pValue->zString,` |
|      ! 0 | 15166 | `			 pValue->nByte` |
|        - | 15167 | `		 );` |
|      ! 0 | 15168 | `	 }` |
|        - | 15169 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15170 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15171 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15172 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15173 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15174 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15175 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15176 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15177 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15178 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15179 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15180 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15181 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15182 | `				 );` |
|      ! 0 | 15183 | `		 }` |
|      ! 0 | 15184 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15185 | `			 && pValue->nByte > 0){` |
|        - | 15186 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15187 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15188 | `		 }` |
|      ! 0 | 15189 | `	 }` |
|      ! 0 | 15190 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15191 | `		 /* Extract raw POST data */` |
|      ! 0 | 15192 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15193 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15194 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15195 | `				 /* Extract POST data length */` |
|      ! 0 | 15196 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15197 | `				 if( pValue ){` |
|      ! 0 | 15198 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15199 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15200 | `					 if( iLen > 0 ){` |
|        - | 15201 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15202 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15203 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15204 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15205 | `						 }` |
|        - | 15206 | `						 /* Decode POST data now */` |
|      ! 0 | 15207 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15208 | `					 }` |
|      ! 0 | 15209 | `				 }` |
|      ! 0 | 15210 | `		 }` |
|      ! 0 | 15211 | `	 }` |
|        - | 15212 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15213 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15214 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15215 | `	 return SXRET_OK;` |
|      ! 0 | 15216 | ` }` |
|        - | 15217 |  |
