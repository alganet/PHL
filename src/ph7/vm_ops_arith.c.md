# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 588/709 lines (82.93%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <math.h>  /* pow */` |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   11 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   12 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   13 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   14 | ` * Status:` |
|      - |   15 | ` *    Stable.` |
|      - |   16 | ` */` |
|      - |   17 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   18 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   19 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   20 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   21 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   22 | `#include "vm_dispatch.h"` |
|      - |   23 | `#define sState (*pState)` |
|      - |   24 |  |
|      - |   25 | `/*` |
|      - |   26 | ` * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of` |
|      - |   27 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   28 | ` */` |
|     54 |   29 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   30 | `{` |
|     57 |   31 | `	ph7_value *pTos = pState->pTos;` |
|     57 |   32 | `	ph7_value *pStack = pState->pStack;` |
|     57 |   33 | `	VmInstr *aInstr = pState->aInstr;` |
|     57 |   34 | `	sxi32 pc = pState->pc;` |
|      - |   35 | `	sxi32 rc;` |
|     27 |   36 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     57 |   37 | `	ph7_value *pNos = &pTos[-1];` |
|      - |   38 | `	ph7_value *pObj;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pNos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     57 |   45 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      - |   46 | ``		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a`` |
|      - |   47 | `		 * COAL entry targeting exactly THIS store (owner + pc identity — a` |
|      - |   48 | `		 * stale entry from an abandoned statement can never match, and nested` |
|      - |   49 | `		 * arms in the RHS were consumed/dropped above this one). Dispatch the` |
|      - |   50 | `		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=` |
|      - |   51 | `		 * residual: pre-fix the assign bypassed __set through the normal slot` |
|      - |   52 | `		 * path). The RHS stays as the expression result. */` |
|     13 |   53 | `		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|     12 |   54 | `		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr` |
|     12 |   55 | `		 && pTop->nPc == (sxu32)pc` |
|     13 |   56 | `		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK \|\| pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){` |
|     13 |   57 | `			VmHookRmw sPend = *pTop;` |
|     13 |   58 | `			(void)SySetPop(&pVm->aHookRmw);` |
|     13 |   59 | `			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){` |
|     11 |   60 | `				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);` |
|     11 |   61 | `				if( rcHs == PH7_ABORT ){` |
|    ! 0 |   62 | `					SyBlobRelease(&sPend.sName);` |
|    ! 0 |   63 | `					PH7_ClassInstanceUnref(sPend.pThis);` |
|    ! 0 |   64 | `					VM_EXIT_ABORT;` |
|      - |   65 | `				}` |
|      6 |   66 | `			}else{` |
|      - |   67 | `				SyString sSetName;` |
|      3 |   68 | `				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));` |
|      3 |   69 | `				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);` |
|      - |   70 | `			}` |
|     13 |   71 | `			SyBlobRelease(&sPend.sName);` |
|     13 |   72 | `			PH7_ClassInstanceUnref(sPend.pThis);` |
|     13 |   73 | `			PH7_MemObjStore(pTos,pNos);` |
|     13 |   74 | `			pNos->nIdx = SXU32_HIGH;` |
|     13 |   75 | `			VmPopOperand(&pTos,1);` |
|     13 |   76 | `			VM_EXIT_BREAK;` |
|      - |   77 | `		}` |
|    ! 0 |   78 | `	}` |
|      - |   79 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|      - |   80 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|      - |   81 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|     45 |   82 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|      5 |   83 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|      5 |   84 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |   85 | `			"offsetSet",sizeof("offsetSet")-1);` |
|      - |   86 | `		ph7_value *apArg[2];` |
|      5 |   87 | `		apArg[0] = &pVm->sCoalesceKey;` |
|      5 |   88 | `		apArg[1] = pTos;` |
|      5 |   89 | `		if( pSet ){` |
|      5 |   90 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|      2 |   91 | `		}` |
|      - |   92 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|      5 |   93 | `		PH7_MemObjStore(pTos,pNos);` |
|      5 |   94 | `		VmPopOperand(&pTos,1);` |
|      - |   95 | `		/* Disarm and release the cached instance ref + key. */` |
|      5 |   96 | `		VmCoalesceDisarm(pVm);` |
|      5 |   97 | `		VM_EXIT_BREAK;` |
|      - |   98 | `	}` |
|     40 |   99 | `	nIdx = pNos->nIdx;` |
|     40 |  100 | `	if( nIdx == SXU32_HIGH ){` |
|    ! 0 |  101 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  102 | `			"Cannot perform assignment on a constant class attribute");` |
|     40 |  103 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     40 |  104 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     40 |  105 | `		PH7_MemObjStore(pTos,pObj);` |
|     19 |  106 | `	}` |
|     40 |  107 | `	PH7_MemObjStore(pTos,pNos);` |
|     40 |  108 | `	VmPopOperand(&pTos,1);` |
|     40 |  109 | `	VM_EXIT_BREAK;` |
|    ! 0 |  110 | `	VM_EXIT_BREAK;` |
|     30 |  111 | `}` |
|      - |  112 |  |
|      - |  113 | `/*` |
|      - |  114 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|      - |  115 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  116 | ` */` |
|     94 |  117 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |  118 | `{` |
|     97 |  119 | `	ph7_value *pTos = pState->pTos;` |
|     97 |  120 | `	ph7_value *pStack = pState->pStack;` |
|     97 |  121 | `	VmInstr *aInstr = pState->aInstr;` |
|     97 |  122 | `	sxi32 pc = pState->pc;` |
|      - |  123 | `	sxi32 rc;` |
|     47 |  124 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     97 |  125 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  126 | `	{` |
|      - |  127 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  128 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  129 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  130 | `		SyBlob sArMsg;` |
|     97 |  131 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     97 |  132 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){` |
|      - |  133 | `			sxi32 rcAr;` |
|    ! 0 |  134 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  135 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  136 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  137 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  138 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  139 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  140 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  141 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  142 | `			rc = rcAr;` |
|    ! 0 |  143 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  144 | `		}` |
|     97 |  145 | `		SyBlobRelease(&sArMsg);` |
|      - |  146 | `	}` |
|      - |  147 | `	ph7_real a,b,r;` |
|      - |  148 | `#ifdef UNTRUST` |
|      - |  149 | `	if( pNos < pStack ){` |
|      - |  150 | `		VM_EXIT_ABORT;` |
|      - |  151 | `	}` |
|      - |  152 | `#endif` |
|      - |  153 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|      - |  154 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|      - |  155 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|      - |  156 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|      - |  157 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|     97 |  158 | `	PH7_MemObjToNumeric(pTos);` |
|     97 |  159 | `	PH7_MemObjToNumeric(pNos);` |
|     97 |  160 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     50 |  161 | `		sxi64 ia = pNos->x.iVal;` |
|     50 |  162 | `		sxi64 ib = pTos->x.iVal;` |
|     50 |  163 | `		if( ib == 0 ){` |
|      8 |  164 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      8 |  165 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     43 |  166 | `		}else if( ia % ib == 0 && !(ib == -1 && ia == SMALLEST_INT64) ){` |
|     19 |  167 | `			pNos->x.iVal = ia / ib;` |
|     19 |  168 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     19 |  169 | `			VmPopOperand(&pTos,1);` |
|     19 |  170 | `			VM_EXIT_BREAK;` |
|      - |  171 | `		}` |
|     12 |  172 | `	}` |
|      - |  173 | `	/* Force the operands to be real */` |
|     72 |  174 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     61 |  175 | `		PH7_MemObjToReal(pTos);` |
|     30 |  176 | `	}` |
|     72 |  177 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     25 |  178 | `		PH7_MemObjToReal(pNos);` |
|     12 |  179 | `	}` |
|      - |  180 | `	/* Perform the requested operation */` |
|     72 |  181 | `	a = pNos->rVal;` |
|     72 |  182 | `	b = pTos->rVal;` |
|     72 |  183 | `	if( b == 0 ){` |
|      - |  184 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  185 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      3 |  186 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      3 |  187 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  188 | `	}else{` |
|     70 |  189 | `		r = a/b;` |
|      - |  190 | `		/* Push the result */` |
|     70 |  191 | `		pNos->rVal = r;` |
|     70 |  192 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  193 | `	}` |
|     70 |  194 | `	VmPopOperand(&pTos,1);` |
|     70 |  195 | `	VM_EXIT_BREAK;` |
|    ! 0 |  196 | `	VM_EXIT_BREAK;` |
|     50 |  197 | `}` |
|      - |  198 |  |
|      - |  199 | `/*` |
|      - |  200 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|      - |  201 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  202 | ` */` |
|      8 |  203 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  204 | `{` |
|      9 |  205 | `	ph7_value *pTos = pState->pTos;` |
|      9 |  206 | `	ph7_value *pStack = pState->pStack;` |
|      9 |  207 | `	VmInstr *aInstr = pState->aInstr;` |
|      9 |  208 | `	sxi32 pc = pState->pc;` |
|      - |  209 | `	sxi32 rc;` |
|      4 |  210 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      9 |  211 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  212 | `	ph7_value *pObj;` |
|      - |  213 | `	sxi64 a,b,r;` |
|      - |  214 | `#ifdef UNTRUST` |
|      - |  215 | `	if( pNos < pStack ){` |
|      - |  216 | `		VM_EXIT_ABORT;` |
|      - |  217 | `	}` |
|      - |  218 | `#endif` |
|      - |  219 | `	{` |
|      - |  220 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  221 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  222 | `		SyBlob sArMsg;` |
|      9 |  223 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|      9 |  224 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  225 | `			sxi32 rcAr;` |
|    ! 0 |  226 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  227 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  228 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  229 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  230 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  231 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  232 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  233 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  234 | `			rc = rcAr;` |
|    ! 0 |  235 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  236 | `		}` |
|      9 |  237 | `		SyBlobRelease(&sArMsg);` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      9 |  240 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      9 |  241 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      9 |  242 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      9 |  243 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      9 |  244 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  245 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 |  246 | `	}` |
|      9 |  247 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  248 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  249 | `	}` |
|      - |  250 | `	/* Perform the requested operation */` |
|      9 |  251 | `	a = pTos->x.iVal;` |
|      9 |  252 | `	b = pNos->x.iVal;` |
|      9 |  253 | `	if( b == 0 ){` |
|      - |  254 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  255 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      5 |  256 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      5 |  257 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  258 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|      5 |  259 | `	}else if( b == -1 ){` |
|      - |  260 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|      - |  261 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|      3 |  262 | `		r = 0;` |
|      2 |  263 | `	}else{` |
|      3 |  264 | `		r = a%b;` |
|      - |  265 | `	}` |
|      - |  266 | `	/* Push the result */` |
|      5 |  267 | `	pNos->x.iVal = r;` |
|      5 |  268 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      5 |  269 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  270 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      5 |  271 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      5 |  272 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      5 |  273 | `		PH7_MemObjStore(pNos,pObj);` |
|      2 |  274 | `	}` |
|      5 |  275 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      5 |  276 | `	VmPopOperand(&pTos,1);` |
|      5 |  277 | `	VM_EXIT_BREAK;` |
|    ! 0 |  278 | `	VM_EXIT_BREAK;` |
|      5 |  279 | `}` |
|      - |  280 |  |
|      - |  281 | `/*` |
|      - |  282 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|      - |  283 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  284 | ` */` |
|   1050 |  285 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  286 | `{` |
|   1055 |  287 | `	ph7_value *pTos = pState->pTos;` |
|   1055 |  288 | `	ph7_value *pStack = pState->pStack;` |
|   1055 |  289 | `	VmInstr *aInstr = pState->aInstr;` |
|   1055 |  290 | `	sxi32 pc = pState->pc;` |
|      - |  291 | `	sxi32 rc;` |
|    525 |  292 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1055 |  293 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  294 | `	{` |
|      - |  295 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  296 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  297 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  298 | `		SyBlob sArMsg;` |
|   1055 |  299 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   1055 |  300 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  301 | `			sxi32 rcAr;` |
|      3 |  302 | `			VmPopOperand(&pTos,1);` |
|      3 |  303 | `			PH7_MemObjRelease(pTos);` |
|      3 |  304 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  305 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  306 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  307 | `				SyBlobLength(&sArMsg));` |
|      3 |  308 | `			SyBlobRelease(&sArMsg);` |
|      4 |  309 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  310 | `			rc = rcAr;` |
|      3 |  311 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  312 | `		}` |
|   1053 |  313 | `		SyBlobRelease(&sArMsg);` |
|      - |  314 | `	}` |
|      - |  315 | `	sxi64 a,b,r;` |
|      - |  316 | `#ifdef UNTRUST` |
|      - |  317 | `	if( pNos < pStack ){` |
|      - |  318 | `		VM_EXIT_ABORT;` |
|      - |  319 | `	}` |
|      - |  320 | `#endif` |
|      - |  321 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|   1053 |  322 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|   1053 |  323 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1053 |  324 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|   1053 |  325 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1053 |  326 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 |  327 | `		PH7_MemObjToInteger(pTos);` |
|      1 |  328 | `	}` |
|   1053 |  329 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  330 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  331 | `	}` |
|      - |  332 | `	/* Perform the requested operation */` |
|   1053 |  333 | `	a = pNos->x.iVal;` |
|   1053 |  334 | `	b = pTos->x.iVal;` |
|   1053 |  335 | `	if( b == 0 ){` |
|      - |  336 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  337 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|     10 |  338 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|     10 |  339 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  340 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|   1045 |  341 | `	}else if( b == -1 ){` |
|      - |  342 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|      - |  343 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|      - |  344 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|      - |  345 | `		 * remainder. php's result here is 0. */` |
|      5 |  346 | `		r = 0;` |
|      3 |  347 | `	}else{` |
|   1041 |  348 | `		r = a%b;` |
|      - |  349 | `	}` |
|      - |  350 | `	/* Push the result */` |
|   1045 |  351 | `	pNos->x.iVal = r;` |
|   1045 |  352 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|   1045 |  353 | `	VmPopOperand(&pTos,1);` |
|   1045 |  354 | `	VM_EXIT_BREAK;` |
|    ! 0 |  355 | `	VM_EXIT_BREAK;` |
|    530 |  356 | `}` |
|      - |  357 |  |
|      - |  358 | `/*` |
|      - |  359 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|      - |  360 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  361 | ` */` |
|     12 |  362 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 |  363 | `{` |
|     14 |  364 | `	ph7_value *pTos = pState->pTos;` |
|     14 |  365 | `	ph7_value *pStack = pState->pStack;` |
|     14 |  366 | `	VmInstr *aInstr = pState->aInstr;` |
|     14 |  367 | `	sxi32 pc = pState->pc;` |
|      - |  368 | `	sxi32 rc;` |
|      6 |  369 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     14 |  370 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  371 | `	ph7_value *pObj;` |
|      - |  372 | `#ifdef UNTRUST` |
|      - |  373 | `	if( pNos < pStack ){` |
|      - |  374 | `		VM_EXIT_ABORT;` |
|      - |  375 | `	}` |
|      - |  376 | `#endif` |
|      - |  377 | `	{` |
|      - |  378 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  379 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  380 | `		SyBlob sArMsg;` |
|     14 |  381 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     14 |  382 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  383 | `			sxi32 rcAr;` |
|      3 |  384 | `			VmPopOperand(&pTos,1);` |
|      3 |  385 | `			PH7_MemObjRelease(pTos);` |
|      3 |  386 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  387 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  388 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  389 | `				SyBlobLength(&sArMsg));` |
|      3 |  390 | `			SyBlobRelease(&sArMsg);` |
|      4 |  391 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  392 | `			rc = rcAr;` |
|      3 |  393 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  394 | `		}` |
|     12 |  395 | `		SyBlobRelease(&sArMsg);` |
|      - |  396 | `	}` |
|      - |  397 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     12 |  398 | `	PH7_MemObjToNumeric(pTos);` |
|     12 |  399 | `	PH7_MemObjToNumeric(pNos);` |
|     12 |  400 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  401 | `		/* Floating point arithemic */` |
|      - |  402 | `		ph7_real a,b,r;` |
|    ! 0 |  403 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  404 | `			PH7_MemObjToReal(pTos);` |
|    ! 0 |  405 | `		}` |
|    ! 0 |  406 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  407 | `			PH7_MemObjToReal(pNos);` |
|    ! 0 |  408 | `		}` |
|    ! 0 |  409 | `		a = pTos->rVal;` |
|    ! 0 |  410 | `		b = pNos->rVal;` |
|    ! 0 |  411 | `		r = a - b;` |
|      - |  412 | `		/* Push the result */` |
|    ! 0 |  413 | `		pNos->rVal = r;` |
|    ! 0 |  414 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  415 | `		/* Try to get an integer representation */` |
|    ! 0 |  416 | `		PH7_MemObjTryInteger(pNos);` |
|    ! 0 |  417 | `	}else{` |
|      - |  418 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  419 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  420 | `		sxi64 a,b,r;` |
|     12 |  421 | `		a = pTos->x.iVal;` |
|     12 |  422 | `		b = pNos->x.iVal;` |
|     12 |  423 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  424 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      3 |  425 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      3 |  426 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  427 | `#else` |
|      - |  428 | `			pNos->x.iVal = r;` |
|      - |  429 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  430 | `#endif` |
|      2 |  431 | `		}else{` |
|     10 |  432 | `			pNos->x.iVal = r;` |
|     10 |  433 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  434 | `		}` |
|      - |  435 | `	}` |
|     12 |  436 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  437 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     12 |  438 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     12 |  439 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     12 |  440 | `		PH7_MemObjStore(pNos,pObj);` |
|      5 |  441 | `	}` |
|     12 |  442 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     12 |  443 | `	VmPopOperand(&pTos,1);` |
|     12 |  444 | `	VM_EXIT_BREAK;` |
|    ! 0 |  445 | `	VM_EXIT_BREAK;` |
|      8 |  446 | `}` |
|      - |  447 |  |
|      - |  448 | `/*` |
|      - |  449 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|      - |  450 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  451 | ` */` |
|  24718 |  452 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  453 | `{` |
|  24723 |  454 | `	ph7_value *pTos = pState->pTos;` |
|  24723 |  455 | `	ph7_value *pStack = pState->pStack;` |
|  24723 |  456 | `	VmInstr *aInstr = pState->aInstr;` |
|  24723 |  457 | `	sxi32 pc = pState->pc;` |
|      - |  458 | `	sxi32 rc;` |
|  12416 |  459 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  24723 |  460 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  461 | `#ifdef UNTRUST` |
|      - |  462 | `	if( pNos < pStack ){` |
|      - |  463 | `		VM_EXIT_ABORT;` |
|      - |  464 | `	}` |
|      - |  465 | `#endif` |
|      - |  466 | `	{` |
|      - |  467 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  468 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  469 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  470 | `		SyBlob sArMsg;` |
|  24723 |  471 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|  24723 |  472 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  473 | `			sxi32 rcAr;` |
|    ! 0 |  474 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  475 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  476 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  477 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  478 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  479 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  480 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  481 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  482 | `			rc = rcAr;` |
|    ! 0 |  483 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  484 | `		}` |
|  24723 |  485 | `		SyBlobRelease(&sArMsg);` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|      - |  488 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|      - |  489 | `	 * quietly evaluated to 0. */` |
|  24723 |  490 | `	PH7_MemObjToNumeric(pTos);` |
|  24723 |  491 | `	PH7_MemObjToNumeric(pNos);` |
|  24723 |  492 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  493 | `		/* Floating point arithemic */` |
|      - |  494 | `		ph7_real a,b,r;` |
|    105 |  495 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      3 |  496 | `			PH7_MemObjToReal(pTos);` |
|      1 |  497 | `		}` |
|    105 |  498 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 |  499 | `			PH7_MemObjToReal(pNos);` |
|      2 |  500 | `		}` |
|    105 |  501 | `		a = pNos->rVal;` |
|    105 |  502 | `		b = pTos->rVal;` |
|    105 |  503 | `		r = a - b;` |
|      - |  504 | `		/* Push the result */` |
|    105 |  505 | `		pNos->rVal = r;` |
|    105 |  506 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  507 | `		/* Try to get an integer representation */` |
|    105 |  508 | `		PH7_MemObjTryInteger(pNos);` |
|     53 |  509 | `	}else{` |
|      - |  510 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  511 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  512 | `		sxi64 a,b,r;` |
|  24619 |  513 | `		a = pNos->x.iVal;` |
|  24619 |  514 | `		b = pTos->x.iVal;` |
|  24619 |  515 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  516 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      7 |  517 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      7 |  518 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  519 | `#else` |
|      - |  520 | `			pNos->x.iVal = r;` |
|      - |  521 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  522 | `#endif` |
|      4 |  523 | `		}else{` |
|  24613 |  524 | `			pNos->x.iVal = r;` |
|  24613 |  525 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  526 | `		}` |
|      - |  527 | `	}` |
|  24723 |  528 | `	VmPopOperand(&pTos,1);` |
|  24723 |  529 | `	VM_EXIT_BREAK;` |
|    ! 0 |  530 | `	VM_EXIT_BREAK;` |
|  12421 |  531 | `}` |
|      - |  532 |  |
|      - |  533 | `/*` |
|      - |  534 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|      - |  535 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  536 | ` */` |
|    136 |  537 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  538 | `{` |
|    137 |  539 | `	ph7_value *pTos = pState->pTos;` |
|    137 |  540 | `	ph7_value *pStack = pState->pStack;` |
|    137 |  541 | `	VmInstr *aInstr = pState->aInstr;` |
|    137 |  542 | `	sxi32 pc = pState->pc;` |
|      - |  543 | `	sxi32 rc;` |
|     68 |  544 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    137 |  545 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  546 | `	{` |
|      - |  547 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  548 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  549 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  550 | `		SyBlob sArMsg;` |
|    137 |  551 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    137 |  552 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){` |
|      - |  553 | `			sxi32 rcAr;` |
|      3 |  554 | `			VmPopOperand(&pTos,1);` |
|      3 |  555 | `			PH7_MemObjRelease(pTos);` |
|      3 |  556 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  557 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  558 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  559 | `				SyBlobLength(&sArMsg));` |
|      3 |  560 | `			SyBlobRelease(&sArMsg);` |
|      4 |  561 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  562 | `			rc = rcAr;` |
|      3 |  563 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  564 | `		}` |
|    135 |  565 | `		SyBlobRelease(&sArMsg);` |
|      - |  566 | `	}` |
|    135 |  567 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|      - |  568 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|      - |  569 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|      - |  570 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|      - |  571 | `	 */` |
|    135 |  572 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    135 |  573 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|      - |  574 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  575 | `	int bBothInt;` |
|    135 |  576 | `	int usedInt = 0;` |
|      - |  577 | `	ph7_real a, b, r;` |
|      - |  578 | `#endif` |
|    135 |  579 | `	sxi64 base_i = 0, exp_i = 0;` |
|      - |  580 | `#ifdef UNTRUST` |
|      - |  581 | `	if( pNos < pStack ){` |
|      - |  582 | `		VM_EXIT_ABORT;` |
|      - |  583 | `	}` |
|      - |  584 | `#endif` |
|    135 |  585 | `	PH7_MemObjToNumeric(pTos);` |
|    135 |  586 | `	PH7_MemObjToNumeric(pNos);` |
|      - |  587 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    265 |  588 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|    130 |  589 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|    135 |  590 | `	if( bBothInt ){` |
|    123 |  591 | `		base_i = pBase->x.iVal;` |
|    123 |  592 | `		exp_i  = pExp->x.iVal;` |
|     61 |  593 | `	}` |
|    135 |  594 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|    125 |  595 | `		PH7_MemObjToReal(pBase);` |
|     62 |  596 | `	}` |
|    135 |  597 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|    133 |  598 | `		PH7_MemObjToReal(pExp);` |
|     66 |  599 | `	}` |
|    135 |  600 | `	a = pBase->rVal;` |
|    135 |  601 | `	b = pExp->rVal;` |
|    135 |  602 | `	r = pow(a, b);` |
|      - |  603 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|      - |  604 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|      - |  605 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|      - |  606 | `	 * representable as double but not as signed int64. */` |
|    135 |  607 | `	if( bBothInt && exp_i >= 0 ){` |
|    117 |  608 | `		sxi64 result_i = 1;` |
|    117 |  609 | `		sxi64 cur_base = base_i;` |
|    117 |  610 | `		sxi64 cur_exp  = exp_i;` |
|    117 |  611 | `		int overflow = 0;` |
|    401 |  612 | `		while( cur_exp > 0 ){` |
|    289 |  613 | `			if( cur_exp & 1 ){` |
|    189 |  614 | `				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){` |
|      3 |  615 | `					overflow = 1;` |
|      3 |  616 | `					break;` |
|      - |  617 | `				}` |
|     93 |  618 | `			}` |
|    287 |  619 | `			cur_exp >>= 1;` |
|    287 |  620 | `			if( cur_exp > 0 ){` |
|    181 |  621 | `				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){` |
|      3 |  622 | `					overflow = 1;` |
|      3 |  623 | `					break;` |
|      - |  624 | `				}` |
|     89 |  625 | `			}` |
|      1 |  626 | `		}` |
|    117 |  627 | `		if( !overflow ){` |
|    113 |  628 | `			pNos->x.iVal = result_i;` |
|    113 |  629 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|    113 |  630 | `			usedInt = 1;` |
|     56 |  631 | `		}` |
|     58 |  632 | `	}` |
|    135 |  633 | `	if( !usedInt ){` |
|     23 |  634 | `		pNos->rVal = r;` |
|     23 |  635 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|     11 |  636 | `	}` |
|      - |  637 | `#else` |
|      - |  638 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|      - |  639 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|      - |  640 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|      - |  641 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|      - |  642 | `	 * represented. */` |
|      - |  643 | `	base_i = pBase->x.iVal;` |
|      - |  644 | `	exp_i  = pExp->x.iVal;` |
|      - |  645 | `	{` |
|      - |  646 | `		sxi64 result_i = 1;` |
|      - |  647 | `		sxi64 cur_base = base_i;` |
|      - |  648 | `		sxi64 cur_exp  = exp_i;` |
|      - |  649 | `		if( cur_exp < 0 ){` |
|      - |  650 | `			result_i = 0;` |
|      - |  651 | `		}else{` |
|      - |  652 | `			while( cur_exp > 0 ){` |
|      - |  653 | `				if( cur_exp & 1 ){` |
|      - |  654 | `					result_i *= cur_base;` |
|      - |  655 | `				}` |
|      - |  656 | `				cur_exp >>= 1;` |
|      - |  657 | `				if( cur_exp > 0 ){` |
|      - |  658 | `					cur_base *= cur_base;` |
|      - |  659 | `				}` |
|      - |  660 | `			}` |
|      - |  661 | `		}` |
|      - |  662 | `		pNos->x.iVal = result_i;` |
|      - |  663 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|      - |  664 | `	}` |
|      - |  665 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    135 |  666 | `	if( bStore ){` |
|      - |  667 | `		ph7_value *pObj;` |
|     23 |  668 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  669 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     23 |  670 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     23 |  671 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     23 |  672 | `			PH7_MemObjStore(pNos,pObj);` |
|     11 |  673 | `		}` |
|     11 |  674 | `	}` |
|    135 |  675 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    135 |  676 | `	VmPopOperand(&pTos,1);` |
|    135 |  677 | `	VM_EXIT_BREAK;` |
|    ! 0 |  678 | `	VM_EXIT_BREAK;` |
|     69 |  679 | `}` |
|      - |  680 |  |
|      - |  681 | `/*` |
|      - |  682 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|      - |  683 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  684 | ` */` |
|    426 |  685 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  686 | `{` |
|    427 |  687 | `	ph7_value *pTos = pState->pTos;` |
|    427 |  688 | `	ph7_value *pStack = pState->pStack;` |
|    427 |  689 | `	VmInstr *aInstr = pState->aInstr;` |
|    427 |  690 | `	sxi32 pc = pState->pc;` |
|      - |  691 | `	sxi32 rc;` |
|    213 |  692 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    427 |  693 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  694 | `#ifdef UNTRUST` |
|      - |  695 | `	if( pNos < pStack ){` |
|      - |  696 | `		VM_EXIT_ABORT;` |
|      - |  697 | `	}` |
|      - |  698 | `#endif` |
|    427 |  699 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    427 |  700 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      - |  701 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|      7 |  702 | `		rc = 1;` |
|      4 |  703 | `	}else{` |
|      - |  704 | `		/* Normalize to exactly -1, 0, or 1 */` |
|    421 |  705 | `		rc = (rc > 0) - (rc < 0);` |
|      - |  706 | `	}` |
|    427 |  707 | `	VmPopOperand(&pTos,1);` |
|    427 |  708 | `	PH7_MemObjRelease(pTos);` |
|    427 |  709 | `	pTos->x.iVal = rc;` |
|    427 |  710 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|    427 |  711 | `	VM_EXIT_BREAK;` |
|    ! 0 |  712 | `	VM_EXIT_BREAK;` |
|      1 |  713 | `}` |
|      - |  714 |  |
|      - |  715 | `/*` |
|      - |  716 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|      - |  717 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  718 | ` */` |
| 176631 |  719 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  720 | `{` |
| 176636 |  721 | `	ph7_value *pTos = pState->pTos;` |
| 176636 |  722 | `	ph7_value *pStack = pState->pStack;` |
| 176636 |  723 | `	VmInstr *aInstr = pState->aInstr;` |
| 176636 |  724 | `	sxi32 pc = pState->pc;` |
|      - |  725 | `	sxi32 rc;` |
|  88420 |  726 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 176636 |  727 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  728 | `	/* Perform the comparison and act accordingly */` |
|      - |  729 | `#ifdef UNTRUST` |
|      - |  730 | `	if( pNos < pStack ){` |
|      - |  731 | `		VM_EXIT_ABORT;` |
|      - |  732 | `	}` |
|      - |  733 | `#endif` |
| 176636 |  734 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 176636 |  735 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  736 | `		rc = 0;` |
| 176632 |  737 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
| 174410 |  738 | `		rc = rc >= 0;` |
|  87312 |  739 | `	}else{` |
|   2223 |  740 | `		rc = rc > 0;` |
|      - |  741 | `	}` |
| 176636 |  742 | `	VmPopOperand(&pTos,1);` |
| 176636 |  743 | `	if( !pInstr->iP2 ){` |
|      - |  744 | `		/* Push comparison result without taking the jump */` |
| 176636 |  745 | `		PH7_MemObjRelease(pTos);` |
| 176636 |  746 | `		pTos->x.iVal = rc;` |
|      - |  747 | `		/* Invalidate any prior representation */` |
| 176636 |  748 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  88425 |  749 | `	}else{` |
|    ! 0 |  750 | `		if( rc ){` |
|      - |  751 | `			/* Jump to the desired location */` |
|    ! 0 |  752 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  753 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  754 | `		}` |
|      - |  755 | `	}` |
| 176636 |  756 | `	VM_EXIT_BREAK;` |
|    ! 0 |  757 | `	VM_EXIT_BREAK;` |
|      5 |  758 | `}` |
|      - |  759 |  |
|      - |  760 | `/*` |
|      - |  761 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|      - |  762 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  763 | ` */` |
| 298058 |  764 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  765 | `{` |
| 298063 |  766 | `	ph7_value *pTos = pState->pTos;` |
| 298063 |  767 | `	ph7_value *pStack = pState->pStack;` |
| 298063 |  768 | `	VmInstr *aInstr = pState->aInstr;` |
| 298063 |  769 | `	sxi32 pc = pState->pc;` |
|      - |  770 | `	sxi32 rc;` |
| 149238 |  771 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 298063 |  772 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  773 | `	/* Perform the comparison and act accordingly */` |
|      - |  774 | `#ifdef UNTRUST` |
|      - |  775 | `	if( pNos < pStack ){` |
|      - |  776 | `		VM_EXIT_ABORT;` |
|      - |  777 | `	}` |
|      - |  778 | `#endif` |
| 298063 |  779 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 298063 |  780 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  781 | `		rc = 0;` |
| 298059 |  782 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|  48904 |  783 | `		rc = rc < 1;` |
|  24559 |  784 | `	}else{` |
| 249156 |  785 | `		rc = rc < 0;` |
|      - |  786 | `	}` |
| 298063 |  787 | `	VmPopOperand(&pTos,1);` |
| 298063 |  788 | `	if( !pInstr->iP2 ){` |
|      - |  789 | `		/* Push comparison result without taking the jump */` |
| 298063 |  790 | `		PH7_MemObjRelease(pTos);` |
| 298063 |  791 | `		pTos->x.iVal = rc;` |
|      - |  792 | `		/* Invalidate any prior representation */` |
| 298063 |  793 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 149243 |  794 | `	}else{` |
|    ! 0 |  795 | `		if( rc ){` |
|      - |  796 | `			/* Jump to the desired location */` |
|    ! 0 |  797 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  798 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  799 | `		}` |
|      - |  800 | `	}` |
| 298063 |  801 | `	VM_EXIT_BREAK;` |
|    ! 0 |  802 | `	VM_EXIT_BREAK;` |
|      5 |  803 | `}` |
|      - |  804 |  |
|      - |  805 | `/*` |
|      - |  806 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|      - |  807 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  808 | ` */` |
| 386093 |  809 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  810 | `{` |
| 386098 |  811 | `	ph7_value *pTos = pState->pTos;` |
| 386098 |  812 | `	ph7_value *pStack = pState->pStack;` |
| 386098 |  813 | `	VmInstr *aInstr = pState->aInstr;` |
| 386098 |  814 | `	sxi32 pc = pState->pc;` |
|      - |  815 | `	sxi32 rc;` |
| 193151 |  816 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 386098 |  817 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  818 | `	/* Perform the comparison and act accordingly */` |
|      - |  819 | `#ifdef UNTRUST` |
|      - |  820 | `	if( pNos < pStack ){` |
|      - |  821 | `		VM_EXIT_ABORT;` |
|      - |  822 | `	}` |
|      - |  823 | `#endif` |
| 386098 |  824 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 386098 |  825 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  826 | `		rc = 1;` |
|      2 |  827 | `	}else{` |
| 386096 |  828 | `		rc = rc != 0;` |
|      - |  829 | `	}` |
| 386098 |  830 | `	VmPopOperand(&pTos,1);` |
| 386098 |  831 | `	if( !pInstr->iP2 ){` |
|      - |  832 | `		/* Push comparison result without taking the jump */` |
| 386098 |  833 | `		PH7_MemObjRelease(pTos);` |
| 386098 |  834 | `		pTos->x.iVal = rc;` |
|      - |  835 | `		/* Invalidate any prior representation */` |
| 386098 |  836 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 193156 |  837 | `	}else{` |
|    ! 0 |  838 | `		if( rc ){` |
|      - |  839 | `			/* Jump to the desired location */` |
|    ! 0 |  840 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  841 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  842 | `		}` |
|      - |  843 | `	}` |
| 386098 |  844 | `	VM_EXIT_BREAK;` |
|    ! 0 |  845 | `	VM_EXIT_BREAK;` |
|      5 |  846 | `}` |
|      - |  847 |  |
|      - |  848 | `/*` |
|      - |  849 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|      - |  850 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  851 | ` */` |
| 462693 |  852 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  853 | `{` |
| 462698 |  854 | `	ph7_value *pTos = pState->pTos;` |
| 462698 |  855 | `	ph7_value *pStack = pState->pStack;` |
| 462698 |  856 | `	VmInstr *aInstr = pState->aInstr;` |
| 462698 |  857 | `	sxi32 pc = pState->pc;` |
|      - |  858 | `	sxi32 rc;` |
| 231451 |  859 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 462698 |  860 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  861 | `	/* Perform the comparison and act accordingly */` |
|      - |  862 | `#ifdef UNTRUST` |
|      - |  863 | `	if( pNos < pStack ){` |
|      - |  864 | `		VM_EXIT_ABORT;` |
|      - |  865 | `	}` |
|      - |  866 | `#endif` |
| 462698 |  867 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 462698 |  868 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  869 | `		rc = 0;` |
|      2 |  870 | `	}else{` |
| 462696 |  871 | `		rc = rc == 0;` |
|      - |  872 | `	}` |
| 462698 |  873 | `	VmPopOperand(&pTos,1);` |
| 462698 |  874 | `	if( !pInstr->iP2 ){` |
|      - |  875 | `		/* Push comparison result without taking the jump */` |
| 462698 |  876 | `		PH7_MemObjRelease(pTos);` |
| 462698 |  877 | `		pTos->x.iVal = rc;` |
|      - |  878 | `		/* Invalidate any prior representation */` |
| 462698 |  879 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 231456 |  880 | `	}else{` |
|    ! 0 |  881 | `		if( rc ){` |
|      - |  882 | `			/* Jump to the desired location */` |
|    ! 0 |  883 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  884 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  885 | `		}` |
|      - |  886 | `	}` |
| 462698 |  887 | `	VM_EXIT_BREAK;` |
|    ! 0 |  888 | `	VM_EXIT_BREAK;` |
|      5 |  889 | `}` |
|      - |  890 |  |
|      - |  891 | `/*` |
|      - |  892 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|      - |  893 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  894 | ` */` |
|  10898 |  895 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  896 | `{` |
|  10903 |  897 | `	ph7_value *pTos = pState->pTos;` |
|  10903 |  898 | `	ph7_value *pStack = pState->pStack;` |
|  10903 |  899 | `	VmInstr *aInstr = pState->aInstr;` |
|  10903 |  900 | `	sxi32 pc = pState->pc;` |
|      - |  901 | `	sxi32 rc;` |
|   5449 |  902 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  10903 |  903 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  904 | `	/* Perform the comparison and act accordingly */` |
|      - |  905 | `#ifdef UNTRUST` |
|      - |  906 | `	if( pNos < pStack ){` |
|      - |  907 | `		VM_EXIT_ABORT;` |
|      - |  908 | `	}` |
|      - |  909 | `#endif` |
|  10903 |  910 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|  10903 |  911 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|     24 |  912 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|  10892 |  913 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|  10829 |  914 | `		rc = rc == 0;` |
|   5417 |  915 | `	}else{` |
|     57 |  916 | `		rc = rc != 0;` |
|      - |  917 | `	}` |
|  10903 |  918 | `	VmPopOperand(&pTos,1);` |
|  10903 |  919 | `	if( !pInstr->iP2 ){` |
|      - |  920 | `		/* Push comparison result without taking the jump */` |
|  10903 |  921 | `		PH7_MemObjRelease(pTos);` |
|  10903 |  922 | `		pTos->x.iVal = rc;` |
|      - |  923 | `		/* Invalidate any prior representation */` |
|  10903 |  924 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   5454 |  925 | `	}else{` |
|    ! 0 |  926 | `		if( rc ){` |
|      - |  927 | `			/* Jump to the desired location */` |
|    ! 0 |  928 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  929 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  930 | `		}` |
|      - |  931 | `	}` |
|  10903 |  932 | `	VM_EXIT_BREAK;` |
|    ! 0 |  933 | `	VM_EXIT_BREAK;` |
|      5 |  934 | `}` |
|      - |  935 |  |
|      - |  936 | `/*` |
|      - |  937 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|      - |  938 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  939 | ` */` |
| 232887 |  940 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  941 | `{` |
| 232892 |  942 | `	ph7_value *pTos = pState->pTos;` |
| 232892 |  943 | `	ph7_value *pStack = pState->pStack;` |
| 232892 |  944 | `	VmInstr *aInstr = pState->aInstr;` |
| 232892 |  945 | `	sxi32 pc = pState->pc;` |
|      - |  946 | `	sxi32 rc;` |
| 116548 |  947 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 232892 |  948 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  949 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|      - |  950 | `#ifdef UNTRUST` |
|      - |  951 | `	if( pNos < pStack ){` |
|      - |  952 | `		VM_EXIT_ABORT;` |
|      - |  953 | `	}` |
|      - |  954 | `#endif` |
|      - |  955 | `	/* Force a boolean cast */` |
| 232892 |  956 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     25 |  957 | `		PH7_MemObjToBool(pTos);` |
|     12 |  958 | `	}` |
| 232892 |  959 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 |  960 | `		PH7_MemObjToBool(pNos);` |
|    ! 0 |  961 | `	}` |
| 232892 |  962 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
| 232892 |  963 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
| 232892 |  964 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|      - |  965 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|  53727 |  966 | `		v1 = and_logic[v1*3+v2];` |
|  26866 |  967 | `	}else{` |
|      - |  968 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
| 179170 |  969 | `		v1 = or_logic[v1*3+v2];` |
|      - |  970 | `	}` |
| 232892 |  971 | `	if( v1 == 2 ){` |
|    ! 0 |  972 | `		v1 = 1;` |
|    ! 0 |  973 | `	}` |
| 232892 |  974 | `	VmPopOperand(&pTos,1);` |
| 232892 |  975 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
| 232892 |  976 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 232892 |  977 | `	VM_EXIT_BREAK;` |
|    ! 0 |  978 | `	VM_EXIT_BREAK;` |
|      5 |  979 | `}` |
|      - |  980 |  |
|      - |  981 | `/*` |
|      - |  982 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|      - |  983 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  984 | ` */` |
|     18 |  985 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  986 | `{` |
|     19 |  987 | `	ph7_value *pTos = pState->pTos;` |
|     19 |  988 | `	ph7_value *pStack = pState->pStack;` |
|     19 |  989 | `	VmInstr *aInstr = pState->aInstr;` |
|     19 |  990 | `	sxi32 pc = pState->pc;` |
|      - |  991 | `	sxi32 rc;` |
|      9 |  992 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     19 |  993 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  994 | `	ph7_value *pObj;` |
|      - |  995 | `	sxi64 a,r;` |
|      - |  996 | `	sxi32 b;` |
|      - |  997 | `#ifdef UNTRUST` |
|      - |  998 | `	if( pNos < pStack ){` |
|      - |  999 | `		VM_EXIT_ABORT;` |
|      - | 1000 | `	}` |
|      - | 1001 | `#endif` |
|      - | 1002 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     19 | 1003 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     19 | 1004 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     19 | 1005 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     19 | 1006 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     19 | 1007 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1008 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 | 1009 | `	}` |
|     19 | 1010 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1011 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 | 1012 | `	}` |
|      - | 1013 | `	/* Perform the requested operation */` |
|     19 | 1014 | `	a = pTos->x.iVal;` |
|     19 | 1015 | `	b = (sxi32)pNos->x.iVal;` |
|     19 | 1016 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|      9 | 1017 | `		r = a << b;` |
|      5 | 1018 | `	}else{` |
|     11 | 1019 | `		r = a >> b;` |
|      - | 1020 | `	}` |
|      - | 1021 | `	/* Push the result */` |
|     19 | 1022 | `	pNos->x.iVal = r;` |
|     19 | 1023 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     19 | 1024 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1025 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     19 | 1026 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     19 | 1027 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     19 | 1028 | `		PH7_MemObjStore(pNos,pObj);` |
|      9 | 1029 | `	}` |
|     19 | 1030 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     19 | 1031 | `	VmPopOperand(&pTos,1);` |
|     19 | 1032 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1033 | `	VM_EXIT_BREAK;` |
|     10 | 1034 | `}` |
|      - | 1035 |  |
|      - | 1036 | `/*` |
|      - | 1037 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|      - | 1038 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1039 | ` */` |
|     58 | 1040 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 | 1041 | `{` |
|     60 | 1042 | `	ph7_value *pTos = pState->pTos;` |
|     60 | 1043 | `	ph7_value *pStack = pState->pStack;` |
|     60 | 1044 | `	VmInstr *aInstr = pState->aInstr;` |
|     60 | 1045 | `	sxi32 pc = pState->pc;` |
|      - | 1046 | `	sxi32 rc;` |
|     29 | 1047 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     60 | 1048 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1049 | `	sxi64 a,r;` |
|      - | 1050 | `	sxi32 b;` |
|      - | 1051 | `#ifdef UNTRUST` |
|      - | 1052 | `	if( pNos < pStack ){` |
|      - | 1053 | `		VM_EXIT_ABORT;` |
|      - | 1054 | `	}` |
|      - | 1055 | `#endif` |
|      - | 1056 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     60 | 1057 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     60 | 1058 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     60 | 1059 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     60 | 1060 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     60 | 1061 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1062 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 | 1063 | `	}` |
|     60 | 1064 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1065 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 | 1066 | `	}` |
|      - | 1067 | `	/* Perform the requested operation */` |
|     60 | 1068 | `	a = pNos->x.iVal;` |
|     60 | 1069 | `	b = (sxi32)pTos->x.iVal;` |
|     60 | 1070 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|     15 | 1071 | `		r = a << b;` |
|      8 | 1072 | `	}else{` |
|     46 | 1073 | `		r = a >> b;` |
|      - | 1074 | `	}` |
|      - | 1075 | `	/* Push the result */` |
|     60 | 1076 | `	pNos->x.iVal = r;` |
|     60 | 1077 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     60 | 1078 | `	VmPopOperand(&pTos,1);` |
|     60 | 1079 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1080 | `	VM_EXIT_BREAK;` |
|     31 | 1081 | `}` |
|      - | 1082 |  |
|      - | 1083 | `/*` |
|      - | 1084 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|      - | 1085 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1086 | ` */` |
|   3072 | 1087 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1088 | `{` |
|   3077 | 1089 | `	ph7_value *pTos = pState->pTos;` |
|   3077 | 1090 | `	ph7_value *pStack = pState->pStack;` |
|   3077 | 1091 | `	VmInstr *aInstr = pState->aInstr;` |
|   3077 | 1092 | `	sxi32 pc = pState->pc;` |
|      - | 1093 | `	sxi32 rc;` |
|   1536 | 1094 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   3077 | 1095 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1096 | `	{` |
|      - | 1097 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - | 1098 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - | 1099 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - | 1100 | `		SyBlob sArMsg;` |
|   3077 | 1101 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   3077 | 1102 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"*",&sArMsg) != SXRET_OK ){` |
|      - | 1103 | `			sxi32 rcAr;` |
|    ! 0 | 1104 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1105 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 | 1106 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 | 1107 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 | 1108 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 | 1109 | `				SyBlobLength(&sArMsg));` |
|    ! 0 | 1110 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 | 1111 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 | 1112 | `			rc = rcAr;` |
|    ! 0 | 1113 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1114 | `		}` |
|   3077 | 1115 | `		SyBlobRelease(&sArMsg);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* Force the operand to be numeric */` |
|      - | 1118 | `#ifdef UNTRUST` |
|      - | 1119 | `	if( pNos < pStack ){` |
|      - | 1120 | `		VM_EXIT_ABORT;` |
|      - | 1121 | `	}` |
|      - | 1122 | `#endif` |
|   3077 | 1123 | `	PH7_MemObjToNumeric(pTos);` |
|   3077 | 1124 | `	PH7_MemObjToNumeric(pNos);` |
|      - | 1125 | `	/* Perform the requested operation */` |
|   3077 | 1126 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - | 1127 | `		/* Floating point arithemic */` |
|      - | 1128 | `		ph7_real a,b,r;` |
|     17 | 1129 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      7 | 1130 | `			PH7_MemObjToReal(pTos);` |
|      3 | 1131 | `		}` |
|     17 | 1132 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 | 1133 | `			PH7_MemObjToReal(pNos);` |
|      2 | 1134 | `		}` |
|     17 | 1135 | `		a = pNos->rVal;` |
|     17 | 1136 | `		b = pTos->rVal;` |
|     17 | 1137 | `		r = a * b;` |
|      - | 1138 | `		/* Push the result */` |
|     17 | 1139 | `		pNos->rVal = r;` |
|     17 | 1140 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1141 | `		/* Try to get an integer representation */` |
|     17 | 1142 | `		PH7_MemObjTryInteger(pNos);` |
|      9 | 1143 | `	}else{` |
|      - | 1144 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|      - | 1145 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - | 1146 | `		sxi64 a,b,r;` |
|   3061 | 1147 | `		a = pNos->x.iVal;` |
|   3061 | 1148 | `		b = pTos->x.iVal;` |
|   3061 | 1149 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|      - | 1150 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     13 | 1151 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|     13 | 1152 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1153 | `#else` |
|      - | 1154 | `			pNos->x.iVal = r;` |
|      - | 1155 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1156 | `#endif` |
|      7 | 1157 | `		}else{` |
|   3049 | 1158 | `			pNos->x.iVal = r;` |
|   3049 | 1159 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1160 | `		}` |
|      - | 1161 | `	}` |
|   3077 | 1162 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|      - | 1163 | `		ph7_value *pObj;` |
|     43 | 1164 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1165 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     43 | 1166 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     43 | 1167 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     43 | 1168 | `			PH7_MemObjStore(pNos,pObj);` |
|     20 | 1169 | `		}` |
|     20 | 1170 | `	}` |
|   3077 | 1171 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|   3077 | 1172 | `	VmPopOperand(&pTos,1);` |
|   3077 | 1173 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1174 | `	VM_EXIT_BREAK;` |
|   1541 | 1175 | `}` |
|      - | 1176 |  |
