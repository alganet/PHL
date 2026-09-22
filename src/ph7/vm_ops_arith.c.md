# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 634/751 lines (84.42%)

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
|    100 |   29 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   30 | `{` |
|    103 |   31 | `	ph7_value *pTos = pState->pTos;` |
|    103 |   32 | `	ph7_value *pStack = pState->pStack;` |
|    103 |   33 | `	VmInstr *aInstr = pState->aInstr;` |
|    103 |   34 | `	sxi32 pc = pState->pc;` |
|      - |   35 | `	sxi32 rc;` |
|     50 |   36 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    103 |   37 | `	ph7_value *pNos = &pTos[-1];` |
|      - |   38 | `	ph7_value *pObj;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pNos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|    103 |   45 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
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
|     91 |   82 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
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
|     86 |   99 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|      - |  100 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|      - |  101 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|      - |  102 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|      - |  103 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|      - |  104 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|      - |  105 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|      - |  106 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|      - |  107 | `		 * offset the quiet peek let through raises at the store. */` |
|     43 |  108 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|     64 |  109 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|     42 |  110 | `			? (ph7_value *)SySetAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|     43 |  111 | `		sxi64 iOfft = 0;` |
|      - |  112 | `		SyBlob sTypeMsg;` |
|      - |  113 | `		int eOfft;` |
|     43 |  114 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|     43 |  115 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|      - |  116 | `			&iOfft,&sTypeMsg);` |
|     43 |  117 | `		if( eOfft == VM_STROFF_REJECT ){` |
|      - |  118 | `			sxi32 rcSo;` |
|      3 |  119 | `			VmPopOperand(&pTos,1);` |
|      3 |  120 | `			PH7_MemObjRelease(pTos);` |
|      3 |  121 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  122 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  123 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      3 |  124 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  125 | `			rc = rcSo;` |
|      3 |  126 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  127 | `		}` |
|     41 |  128 | `		SyBlobRelease(&sTypeMsg);` |
|      - |  129 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|      - |  130 | `		{` |
|     41 |  131 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     41 |  132 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      - |  133 | `		}` |
|     41 |  134 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|     41 |  135 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|      - |  136 | `				sxi32 rcEm;` |
|      5 |  137 | `				VmPopOperand(&pTos,1);` |
|      5 |  138 | `				PH7_MemObjRelease(pTos);` |
|      5 |  139 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  140 | `				pTos->nIdx = SXU32_HIGH;` |
|      5 |  141 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|      - |  142 | `					"Cannot assign an empty string to a string offset",` |
|      - |  143 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|      5 |  144 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  145 | `				rc = rcEm;` |
|      5 |  146 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  147 | `			}` |
|     18 |  148 | `		}` |
|      - |  149 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|      - |  150 | `		 * not free what the carrier owns, so release it here — every other exit` |
|      - |  151 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|     37 |  152 | `		VmFreeCoalStrOff(pCoalOff);` |
|     37 |  153 | `		pNos->x.pOther = 0;` |
|     37 |  154 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|      - |  155 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|     37 |  156 | `		PH7_MemObjStore(pTos,pNos);` |
|     37 |  157 | `		pNos->nIdx = SXU32_HIGH;` |
|     37 |  158 | `		VmPopOperand(&pTos,1);` |
|     37 |  159 | `		VM_EXIT_BREAK;` |
|      - |  160 | `	}` |
|     44 |  161 | `	nIdx = pNos->nIdx;` |
|     44 |  162 | `	if( nIdx == SXU32_HIGH ){` |
|    ! 0 |  163 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  164 | `			"Cannot perform assignment on a constant class attribute");` |
|     44 |  165 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     44 |  166 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     44 |  167 | `		PH7_MemObjStore(pTos,pObj);` |
|     21 |  168 | `	}` |
|     44 |  169 | `	PH7_MemObjStore(pTos,pNos);` |
|     44 |  170 | `	VmPopOperand(&pTos,1);` |
|     44 |  171 | `	VM_EXIT_BREAK;` |
|    ! 0 |  172 | `	VM_EXIT_BREAK;` |
|     53 |  173 | `}` |
|      - |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|      - |  177 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  178 | ` */` |
|    136 |  179 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      4 |  180 | `{` |
|    140 |  181 | `	ph7_value *pTos = pState->pTos;` |
|    140 |  182 | `	ph7_value *pStack = pState->pStack;` |
|    140 |  183 | `	VmInstr *aInstr = pState->aInstr;` |
|    140 |  184 | `	sxi32 pc = pState->pc;` |
|      - |  185 | `	sxi32 rc;` |
|     68 |  186 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    140 |  187 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  188 | `	{` |
|      - |  189 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  190 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  191 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  192 | `		SyBlob sArMsg;` |
|    140 |  193 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    140 |  194 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){` |
|      - |  195 | `			sxi32 rcAr;` |
|    ! 0 |  196 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  197 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  198 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  199 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  200 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  201 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  202 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  203 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  204 | `			rc = rcAr;` |
|    ! 0 |  205 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  206 | `		}` |
|    140 |  207 | `		SyBlobRelease(&sArMsg);` |
|      - |  208 | `	}` |
|      - |  209 | `	ph7_real a,b,r;` |
|      - |  210 | `#ifdef UNTRUST` |
|      - |  211 | `	if( pNos < pStack ){` |
|      - |  212 | `		VM_EXIT_ABORT;` |
|      - |  213 | `	}` |
|      - |  214 | `#endif` |
|      - |  215 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|      - |  216 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|      - |  217 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|      - |  218 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|      - |  219 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|    140 |  220 | `	PH7_MemObjToNumeric(pTos);` |
|    140 |  221 | `	PH7_MemObjToNumeric(pNos);` |
|    140 |  222 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     77 |  223 | `		sxi64 ia = pNos->x.iVal;` |
|     77 |  224 | `		sxi64 ib = pTos->x.iVal;` |
|     77 |  225 | `		sxi64 iQuot = 0;` |
|     77 |  226 | `		int bExact = 0;` |
|     77 |  227 | `		if( ib == 0 ){` |
|      8 |  228 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|     12 |  229 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     70 |  230 | `		}else if( ib == -1 ){` |
|      - |  231 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|      - |  232 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|      - |  233 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|      - |  234 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|      - |  235 | `			 * alongside the remainder, so testing the remainder first trapped` |
|      - |  236 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|      - |  237 | `			 * OP_MOD screens the same hazard the same way. */` |
|      - |  238 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|      - |  239 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|      - |  240 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|      - |  241 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|      - |  242 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|      - |  243 | `			bExact = 1;` |
|      - |  244 | `#else` |
|     19 |  245 | `			if( ia != SMALLEST_INT64 ){` |
|     15 |  246 | `				iQuot = -ia;` |
|     15 |  247 | `				bExact = 1;` |
|      8 |  248 | `			}` |
|      - |  249 | `#endif` |
|     61 |  250 | `		}else if( ia % ib == 0 ){` |
|     22 |  251 | `			iQuot = ia / ib;` |
|     22 |  252 | `			bExact = 1;` |
|     10 |  253 | `		}` |
|     70 |  254 | `		if( bExact ){` |
|     36 |  255 | `			pNos->x.iVal = iQuot;` |
|     36 |  256 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     36 |  257 | `			VmPopOperand(&pTos,1);` |
|     36 |  258 | `			VM_EXIT_BREAK;` |
|      - |  259 | `		}` |
|     17 |  260 | `	}` |
|      - |  261 | `	/* Force the operands to be real */` |
|     98 |  262 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     87 |  263 | `		PH7_MemObjToReal(pTos);` |
|     43 |  264 | `	}` |
|     98 |  265 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     35 |  266 | `		PH7_MemObjToReal(pNos);` |
|     17 |  267 | `	}` |
|      - |  268 | `	/* Perform the requested operation */` |
|     98 |  269 | `	a = pNos->rVal;` |
|     98 |  270 | `	b = pTos->rVal;` |
|     98 |  271 | `	if( b == 0 ){` |
|      - |  272 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  273 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      3 |  274 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      3 |  275 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  276 | `	}else{` |
|     96 |  277 | `		r = a/b;` |
|      - |  278 | `		/* Push the result */` |
|     96 |  279 | `		pNos->rVal = r;` |
|     96 |  280 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  281 | `	}` |
|     96 |  282 | `	VmPopOperand(&pTos,1);` |
|     96 |  283 | `	VM_EXIT_BREAK;` |
|    ! 0 |  284 | `	VM_EXIT_BREAK;` |
|     72 |  285 | `}` |
|      - |  286 |  |
|      - |  287 | `/*` |
|      - |  288 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|      - |  289 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  290 | ` */` |
|      8 |  291 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  292 | `{` |
|      9 |  293 | `	ph7_value *pTos = pState->pTos;` |
|      9 |  294 | `	ph7_value *pStack = pState->pStack;` |
|      9 |  295 | `	VmInstr *aInstr = pState->aInstr;` |
|      9 |  296 | `	sxi32 pc = pState->pc;` |
|      - |  297 | `	sxi32 rc;` |
|      4 |  298 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      9 |  299 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  300 | `	ph7_value *pObj;` |
|      - |  301 | `	sxi64 a,b,r;` |
|      - |  302 | `#ifdef UNTRUST` |
|      - |  303 | `	if( pNos < pStack ){` |
|      - |  304 | `		VM_EXIT_ABORT;` |
|      - |  305 | `	}` |
|      - |  306 | `#endif` |
|      - |  307 | `	{` |
|      - |  308 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  309 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  310 | `		SyBlob sArMsg;` |
|      9 |  311 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|      9 |  312 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  313 | `			sxi32 rcAr;` |
|    ! 0 |  314 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  315 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  316 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  317 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  318 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  319 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  320 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  321 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  322 | `			rc = rcAr;` |
|    ! 0 |  323 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  324 | `		}` |
|      9 |  325 | `		SyBlobRelease(&sArMsg);` |
|      - |  326 | `	}` |
|      - |  327 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|      9 |  328 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|      9 |  329 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      9 |  330 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|      9 |  331 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|      9 |  332 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  333 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 |  334 | `	}` |
|      9 |  335 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  336 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  337 | `	}` |
|      - |  338 | `	/* Perform the requested operation */` |
|      9 |  339 | `	a = pTos->x.iVal;` |
|      9 |  340 | `	b = pNos->x.iVal;` |
|      9 |  341 | `	if( b == 0 ){` |
|      - |  342 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  343 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      5 |  344 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      5 |  345 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  346 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|      5 |  347 | `	}else if( b == -1 ){` |
|      - |  348 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|      - |  349 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|      3 |  350 | `		r = 0;` |
|      2 |  351 | `	}else{` |
|      3 |  352 | `		r = a%b;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Push the result */` |
|      5 |  355 | `	pNos->x.iVal = r;` |
|      5 |  356 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      5 |  357 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  358 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      5 |  359 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      5 |  360 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      5 |  361 | `		PH7_MemObjStore(pNos,pObj);` |
|      2 |  362 | `	}` |
|      5 |  363 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      5 |  364 | `	VmPopOperand(&pTos,1);` |
|      5 |  365 | `	VM_EXIT_BREAK;` |
|    ! 0 |  366 | `	VM_EXIT_BREAK;` |
|      5 |  367 | `}` |
|      - |  368 |  |
|      - |  369 | `/*` |
|      - |  370 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|      - |  371 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  372 | ` */` |
|   1266 |  373 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  374 | `{` |
|   1271 |  375 | `	ph7_value *pTos = pState->pTos;` |
|   1271 |  376 | `	ph7_value *pStack = pState->pStack;` |
|   1271 |  377 | `	VmInstr *aInstr = pState->aInstr;` |
|   1271 |  378 | `	sxi32 pc = pState->pc;` |
|      - |  379 | `	sxi32 rc;` |
|    633 |  380 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1271 |  381 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  382 | `	{` |
|      - |  383 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  384 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  385 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  386 | `		SyBlob sArMsg;` |
|   1271 |  387 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   1271 |  388 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  389 | `			sxi32 rcAr;` |
|      3 |  390 | `			VmPopOperand(&pTos,1);` |
|      3 |  391 | `			PH7_MemObjRelease(pTos);` |
|      3 |  392 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  393 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  394 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  395 | `				SyBlobLength(&sArMsg));` |
|      3 |  396 | `			SyBlobRelease(&sArMsg);` |
|      4 |  397 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  398 | `			rc = rcAr;` |
|      5 |  399 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  400 | `		}` |
|   1269 |  401 | `		SyBlobRelease(&sArMsg);` |
|      - |  402 | `	}` |
|      - |  403 | `	sxi64 a,b,r;` |
|      - |  404 | `#ifdef UNTRUST` |
|      - |  405 | `	if( pNos < pStack ){` |
|      - |  406 | `		VM_EXIT_ABORT;` |
|      - |  407 | `	}` |
|      - |  408 | `#endif` |
|      - |  409 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|   1269 |  410 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|   1269 |  411 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1269 |  412 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|   1269 |  413 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1269 |  414 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 |  415 | `		PH7_MemObjToInteger(pTos);` |
|      1 |  416 | `	}` |
|   1269 |  417 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  418 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  419 | `	}` |
|      - |  420 | `	/* Perform the requested operation */` |
|   1269 |  421 | `	a = pNos->x.iVal;` |
|   1269 |  422 | `	b = pTos->x.iVal;` |
|   1269 |  423 | `	if( b == 0 ){` |
|      - |  424 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  425 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|     10 |  426 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|     18 |  427 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  428 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|   1261 |  429 | `	}else if( b == -1 ){` |
|      - |  430 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|      - |  431 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|      - |  432 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|      - |  433 | `		 * remainder. php's result here is 0. */` |
|      5 |  434 | `		r = 0;` |
|      3 |  435 | `	}else{` |
|   1257 |  436 | `		r = a%b;` |
|      - |  437 | `	}` |
|      - |  438 | `	/* Push the result */` |
|   1261 |  439 | `	pNos->x.iVal = r;` |
|   1261 |  440 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|   1261 |  441 | `	VmPopOperand(&pTos,1);` |
|   1261 |  442 | `	VM_EXIT_BREAK;` |
|    ! 0 |  443 | `	VM_EXIT_BREAK;` |
|    638 |  444 | `}` |
|      - |  445 |  |
|      - |  446 | `/*` |
|      - |  447 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|      - |  448 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  449 | ` */` |
|     12 |  450 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 |  451 | `{` |
|     14 |  452 | `	ph7_value *pTos = pState->pTos;` |
|     14 |  453 | `	ph7_value *pStack = pState->pStack;` |
|     14 |  454 | `	VmInstr *aInstr = pState->aInstr;` |
|     14 |  455 | `	sxi32 pc = pState->pc;` |
|      - |  456 | `	sxi32 rc;` |
|      6 |  457 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     14 |  458 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  459 | `	ph7_value *pObj;` |
|      - |  460 | `#ifdef UNTRUST` |
|      - |  461 | `	if( pNos < pStack ){` |
|      - |  462 | `		VM_EXIT_ABORT;` |
|      - |  463 | `	}` |
|      - |  464 | `#endif` |
|      - |  465 | `	{` |
|      - |  466 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  467 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  468 | `		SyBlob sArMsg;` |
|     14 |  469 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     14 |  470 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  471 | `			sxi32 rcAr;` |
|      3 |  472 | `			VmPopOperand(&pTos,1);` |
|      3 |  473 | `			PH7_MemObjRelease(pTos);` |
|      3 |  474 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  475 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  476 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  477 | `				SyBlobLength(&sArMsg));` |
|      3 |  478 | `			SyBlobRelease(&sArMsg);` |
|      4 |  479 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  480 | `			rc = rcAr;` |
|      3 |  481 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  482 | `		}` |
|     12 |  483 | `		SyBlobRelease(&sArMsg);` |
|      - |  484 | `	}` |
|      - |  485 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     12 |  486 | `	PH7_MemObjToNumeric(pTos);` |
|     12 |  487 | `	PH7_MemObjToNumeric(pNos);` |
|     12 |  488 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  489 | `		/* Floating point arithemic */` |
|      - |  490 | `		ph7_real a,b,r;` |
|    ! 0 |  491 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  492 | `			PH7_MemObjToReal(pTos);` |
|    ! 0 |  493 | `		}` |
|    ! 0 |  494 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  495 | `			PH7_MemObjToReal(pNos);` |
|    ! 0 |  496 | `		}` |
|    ! 0 |  497 | `		a = pTos->rVal;` |
|    ! 0 |  498 | `		b = pNos->rVal;` |
|    ! 0 |  499 | `		r = a - b;` |
|      - |  500 | `		/* Push the result */` |
|    ! 0 |  501 | `		pNos->rVal = r;` |
|    ! 0 |  502 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  503 | `		/* Try to get an integer representation */` |
|    ! 0 |  504 | `		PH7_MemObjTryInteger(pNos);` |
|    ! 0 |  505 | `	}else{` |
|      - |  506 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  507 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  508 | `		sxi64 a,b,r;` |
|     12 |  509 | `		a = pTos->x.iVal;` |
|     12 |  510 | `		b = pNos->x.iVal;` |
|     12 |  511 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  512 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      3 |  513 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      3 |  514 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  515 | `#else` |
|      - |  516 | `			pNos->x.iVal = r;` |
|      - |  517 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  518 | `#endif` |
|      2 |  519 | `		}else{` |
|     10 |  520 | `			pNos->x.iVal = r;` |
|     10 |  521 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  522 | `		}` |
|      - |  523 | `	}` |
|     12 |  524 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  525 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     12 |  526 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     12 |  527 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     12 |  528 | `		PH7_MemObjStore(pNos,pObj);` |
|      5 |  529 | `	}` |
|     12 |  530 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     12 |  531 | `	VmPopOperand(&pTos,1);` |
|     12 |  532 | `	VM_EXIT_BREAK;` |
|    ! 0 |  533 | `	VM_EXIT_BREAK;` |
|      8 |  534 | `}` |
|      - |  535 |  |
|      - |  536 | `/*` |
|      - |  537 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|      - |  538 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  539 | ` */` |
|  45029 |  540 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  541 | `{` |
|  45034 |  542 | `	ph7_value *pTos = pState->pTos;` |
|  45034 |  543 | `	ph7_value *pStack = pState->pStack;` |
|  45034 |  544 | `	VmInstr *aInstr = pState->aInstr;` |
|  45034 |  545 | `	sxi32 pc = pState->pc;` |
|      - |  546 | `	sxi32 rc;` |
|  22665 |  547 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  45034 |  548 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  549 | `#ifdef UNTRUST` |
|      - |  550 | `	if( pNos < pStack ){` |
|      - |  551 | `		VM_EXIT_ABORT;` |
|      - |  552 | `	}` |
|      - |  553 | `#endif` |
|      - |  554 | `	{` |
|      - |  555 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  556 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  557 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  558 | `		SyBlob sArMsg;` |
|  45034 |  559 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|  45034 |  560 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  561 | `			sxi32 rcAr;` |
|    ! 0 |  562 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  563 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  564 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  565 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  566 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  567 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  568 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  569 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  570 | `			rc = rcAr;` |
|    ! 0 |  571 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  572 | `		}` |
|  45034 |  573 | `		SyBlobRelease(&sArMsg);` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|      - |  576 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|      - |  577 | `	 * quietly evaluated to 0. */` |
|  45034 |  578 | `	PH7_MemObjToNumeric(pTos);` |
|  45034 |  579 | `	PH7_MemObjToNumeric(pNos);` |
|  45034 |  580 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  581 | `		/* Floating point arithemic */` |
|      - |  582 | `		ph7_real a,b,r;` |
|    121 |  583 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     19 |  584 | `			PH7_MemObjToReal(pTos);` |
|      9 |  585 | `		}` |
|    121 |  586 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 |  587 | `			PH7_MemObjToReal(pNos);` |
|      2 |  588 | `		}` |
|    121 |  589 | `		a = pNos->rVal;` |
|    121 |  590 | `		b = pTos->rVal;` |
|    121 |  591 | `		r = a - b;` |
|      - |  592 | `		/* Push the result */` |
|    121 |  593 | `		pNos->rVal = r;` |
|    121 |  594 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  595 | `		/* Try to get an integer representation */` |
|    121 |  596 | `		PH7_MemObjTryInteger(pNos);` |
|     61 |  597 | `	}else{` |
|      - |  598 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  599 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  600 | `		sxi64 a,b,r;` |
|  44914 |  601 | `		a = pNos->x.iVal;` |
|  44914 |  602 | `		b = pTos->x.iVal;` |
|  44914 |  603 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  604 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      7 |  605 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      7 |  606 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  607 | `#else` |
|      - |  608 | `			pNos->x.iVal = r;` |
|      - |  609 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  610 | `#endif` |
|      4 |  611 | `		}else{` |
|  44908 |  612 | `			pNos->x.iVal = r;` |
|  44908 |  613 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  614 | `		}` |
|      - |  615 | `	}` |
|  45034 |  616 | `	VmPopOperand(&pTos,1);` |
|  45034 |  617 | `	VM_EXIT_BREAK;` |
|    ! 0 |  618 | `	VM_EXIT_BREAK;` |
|  22670 |  619 | `}` |
|      - |  620 |  |
|      - |  621 | `/*` |
|      - |  622 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|      - |  623 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  624 | ` */` |
|    152 |  625 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  626 | `{` |
|    153 |  627 | `	ph7_value *pTos = pState->pTos;` |
|    153 |  628 | `	ph7_value *pStack = pState->pStack;` |
|    153 |  629 | `	VmInstr *aInstr = pState->aInstr;` |
|    153 |  630 | `	sxi32 pc = pState->pc;` |
|      - |  631 | `	sxi32 rc;` |
|     76 |  632 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    153 |  633 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  634 | `	{` |
|      - |  635 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  636 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  637 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  638 | `		SyBlob sArMsg;` |
|    153 |  639 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    153 |  640 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){` |
|      - |  641 | `			sxi32 rcAr;` |
|      3 |  642 | `			VmPopOperand(&pTos,1);` |
|      3 |  643 | `			PH7_MemObjRelease(pTos);` |
|      3 |  644 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  645 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  646 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  647 | `				SyBlobLength(&sArMsg));` |
|      3 |  648 | `			SyBlobRelease(&sArMsg);` |
|      4 |  649 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  650 | `			rc = rcAr;` |
|      3 |  651 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  652 | `		}` |
|    151 |  653 | `		SyBlobRelease(&sArMsg);` |
|      - |  654 | `	}` |
|    151 |  655 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|      - |  656 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|      - |  657 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|      - |  658 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|      - |  659 | `	 */` |
|    151 |  660 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    151 |  661 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|      - |  662 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  663 | `	int bBothInt;` |
|    151 |  664 | `	int usedInt = 0;` |
|      - |  665 | `	ph7_real a, b, r;` |
|      - |  666 | `#endif` |
|    151 |  667 | `	sxi64 base_i = 0, exp_i = 0;` |
|      - |  668 | `#ifdef UNTRUST` |
|      - |  669 | `	if( pNos < pStack ){` |
|      - |  670 | `		VM_EXIT_ABORT;` |
|      - |  671 | `	}` |
|      - |  672 | `#endif` |
|    151 |  673 | `	PH7_MemObjToNumeric(pTos);` |
|    151 |  674 | `	PH7_MemObjToNumeric(pNos);` |
|      - |  675 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    297 |  676 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|    146 |  677 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|    151 |  678 | `	if( bBothInt ){` |
|    123 |  679 | `		base_i = pBase->x.iVal;` |
|    123 |  680 | `		exp_i  = pExp->x.iVal;` |
|     61 |  681 | `	}` |
|    151 |  682 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|    125 |  683 | `		PH7_MemObjToReal(pBase);` |
|     62 |  684 | `	}` |
|    151 |  685 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|    149 |  686 | `		PH7_MemObjToReal(pExp);` |
|     74 |  687 | `	}` |
|    151 |  688 | `	a = pBase->rVal;` |
|    151 |  689 | `	b = pExp->rVal;` |
|    151 |  690 | `	r = pow(a, b);` |
|      - |  691 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|      - |  692 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|      - |  693 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|      - |  694 | `	 * representable as double but not as signed int64. */` |
|    151 |  695 | `	if( bBothInt && exp_i >= 0 ){` |
|    117 |  696 | `		sxi64 result_i = 1;` |
|    117 |  697 | `		sxi64 cur_base = base_i;` |
|    117 |  698 | `		sxi64 cur_exp  = exp_i;` |
|    117 |  699 | `		int overflow = 0;` |
|    401 |  700 | `		while( cur_exp > 0 ){` |
|    289 |  701 | `			if( cur_exp & 1 ){` |
|    189 |  702 | `				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){` |
|      3 |  703 | `					overflow = 1;` |
|      3 |  704 | `					break;` |
|      - |  705 | `				}` |
|     93 |  706 | `			}` |
|    287 |  707 | `			cur_exp >>= 1;` |
|    287 |  708 | `			if( cur_exp > 0 ){` |
|    181 |  709 | `				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){` |
|      3 |  710 | `					overflow = 1;` |
|      3 |  711 | `					break;` |
|      - |  712 | `				}` |
|     89 |  713 | `			}` |
|      1 |  714 | `		}` |
|    117 |  715 | `		if( !overflow ){` |
|    113 |  716 | `			pNos->x.iVal = result_i;` |
|    113 |  717 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|    113 |  718 | `			usedInt = 1;` |
|     56 |  719 | `		}` |
|     58 |  720 | `	}` |
|    151 |  721 | `	if( !usedInt ){` |
|     39 |  722 | `		pNos->rVal = r;` |
|     39 |  723 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|     19 |  724 | `	}` |
|      - |  725 | `#else` |
|      - |  726 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|      - |  727 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|      - |  728 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|      - |  729 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|      - |  730 | `	 * represented. */` |
|      - |  731 | `	base_i = pBase->x.iVal;` |
|      - |  732 | `	exp_i  = pExp->x.iVal;` |
|      - |  733 | `	{` |
|      - |  734 | `		sxi64 result_i = 1;` |
|      - |  735 | `		sxi64 cur_base = base_i;` |
|      - |  736 | `		sxi64 cur_exp  = exp_i;` |
|      - |  737 | `		if( cur_exp < 0 ){` |
|      - |  738 | `			result_i = 0;` |
|      - |  739 | `		}else{` |
|      - |  740 | `			while( cur_exp > 0 ){` |
|      - |  741 | `				if( cur_exp & 1 ){` |
|      - |  742 | `					result_i *= cur_base;` |
|      - |  743 | `				}` |
|      - |  744 | `				cur_exp >>= 1;` |
|      - |  745 | `				if( cur_exp > 0 ){` |
|      - |  746 | `					cur_base *= cur_base;` |
|      - |  747 | `				}` |
|      - |  748 | `			}` |
|      - |  749 | `		}` |
|      - |  750 | `		pNos->x.iVal = result_i;` |
|      - |  751 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|      - |  752 | `	}` |
|      - |  753 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    151 |  754 | `	if( bStore ){` |
|      - |  755 | `		ph7_value *pObj;` |
|     23 |  756 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  757 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     23 |  758 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     23 |  759 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     23 |  760 | `			PH7_MemObjStore(pNos,pObj);` |
|     11 |  761 | `		}` |
|     11 |  762 | `	}` |
|    151 |  763 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    151 |  764 | `	VmPopOperand(&pTos,1);` |
|    151 |  765 | `	VM_EXIT_BREAK;` |
|    ! 0 |  766 | `	VM_EXIT_BREAK;` |
|     77 |  767 | `}` |
|      - |  768 |  |
|      - |  769 | `/*` |
|      - |  770 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|      - |  771 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  772 | ` */` |
|    536 |  773 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |  774 | `{` |
|    539 |  775 | `	ph7_value *pTos = pState->pTos;` |
|    539 |  776 | `	ph7_value *pStack = pState->pStack;` |
|    539 |  777 | `	VmInstr *aInstr = pState->aInstr;` |
|    539 |  778 | `	sxi32 pc = pState->pc;` |
|      - |  779 | `	sxi32 rc;` |
|    268 |  780 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    539 |  781 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  782 | `#ifdef UNTRUST` |
|      - |  783 | `	if( pNos < pStack ){` |
|      - |  784 | `		VM_EXIT_ABORT;` |
|      - |  785 | `	}` |
|      - |  786 | `#endif` |
|    539 |  787 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    539 |  788 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      - |  789 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|      7 |  790 | `		rc = 1;` |
|      4 |  791 | `	}else{` |
|      - |  792 | `		/* Normalize to exactly -1, 0, or 1 */` |
|    533 |  793 | `		rc = (rc > 0) - (rc < 0);` |
|      - |  794 | `	}` |
|    539 |  795 | `	VmPopOperand(&pTos,1);` |
|    539 |  796 | `	PH7_MemObjRelease(pTos);` |
|    539 |  797 | `	pTos->x.iVal = rc;` |
|    539 |  798 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|    539 |  799 | `	VM_EXIT_BREAK;` |
|    ! 0 |  800 | `	VM_EXIT_BREAK;` |
|      3 |  801 | `}` |
|      - |  802 |  |
|      - |  803 | `/*` |
|      - |  804 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|      - |  805 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  806 | ` */` |
| 248648 |  807 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  808 | `{` |
| 248653 |  809 | `	ph7_value *pTos = pState->pTos;` |
| 248653 |  810 | `	ph7_value *pStack = pState->pStack;` |
| 248653 |  811 | `	VmInstr *aInstr = pState->aInstr;` |
| 248653 |  812 | `	sxi32 pc = pState->pc;` |
|      - |  813 | `	sxi32 rc;` |
| 124526 |  814 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 248653 |  815 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  816 | `	/* Perform the comparison and act accordingly */` |
|      - |  817 | `#ifdef UNTRUST` |
|      - |  818 | `	if( pNos < pStack ){` |
|      - |  819 | `		VM_EXIT_ABORT;` |
|      - |  820 | `	}` |
|      - |  821 | `#endif` |
| 248653 |  822 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 248653 |  823 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  824 | `		rc = 0;` |
| 248649 |  825 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
| 237781 |  826 | `		rc = rc >= 0;` |
| 119095 |  827 | `	}else{` |
|  10869 |  828 | `		rc = rc > 0;` |
|      - |  829 | `	}` |
| 248653 |  830 | `	VmPopOperand(&pTos,1);` |
| 248653 |  831 | `	if( !pInstr->iP2 ){` |
|      - |  832 | `		/* Push comparison result without taking the jump */` |
| 248653 |  833 | `		PH7_MemObjRelease(pTos);` |
| 248653 |  834 | `		pTos->x.iVal = rc;` |
|      - |  835 | `		/* Invalidate any prior representation */` |
| 248653 |  836 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 124531 |  837 | `	}else{` |
|    ! 0 |  838 | `		if( rc ){` |
|      - |  839 | `			/* Jump to the desired location */` |
|    ! 0 |  840 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  841 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  842 | `		}` |
|      - |  843 | `	}` |
| 248653 |  844 | `	VM_EXIT_BREAK;` |
|    ! 0 |  845 | `	VM_EXIT_BREAK;` |
|      5 |  846 | `}` |
|      - |  847 |  |
|      - |  848 | `/*` |
|      - |  849 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|      - |  850 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  851 | ` */` |
| 537834 |  852 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  853 | `{` |
| 537839 |  854 | `	ph7_value *pTos = pState->pTos;` |
| 537839 |  855 | `	ph7_value *pStack = pState->pStack;` |
| 537839 |  856 | `	VmInstr *aInstr = pState->aInstr;` |
| 537839 |  857 | `	sxi32 pc = pState->pc;` |
|      - |  858 | `	sxi32 rc;` |
| 269317 |  859 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 537839 |  860 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  861 | `	/* Perform the comparison and act accordingly */` |
|      - |  862 | `#ifdef UNTRUST` |
|      - |  863 | `	if( pNos < pStack ){` |
|      - |  864 | `		VM_EXIT_ABORT;` |
|      - |  865 | `	}` |
|      - |  866 | `#endif` |
| 537839 |  867 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 537839 |  868 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  869 | `		rc = 0;` |
| 537835 |  870 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|  70601 |  871 | `		rc = rc < 1;` |
|  35501 |  872 | `	}else{` |
| 467235 |  873 | `		rc = rc < 0;` |
|      - |  874 | `	}` |
| 537839 |  875 | `	VmPopOperand(&pTos,1);` |
| 537839 |  876 | `	if( !pInstr->iP2 ){` |
|      - |  877 | `		/* Push comparison result without taking the jump */` |
| 537839 |  878 | `		PH7_MemObjRelease(pTos);` |
| 537839 |  879 | `		pTos->x.iVal = rc;` |
|      - |  880 | `		/* Invalidate any prior representation */` |
| 537839 |  881 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 269322 |  882 | `	}else{` |
|    ! 0 |  883 | `		if( rc ){` |
|      - |  884 | `			/* Jump to the desired location */` |
|    ! 0 |  885 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  886 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  887 | `		}` |
|      - |  888 | `	}` |
| 537839 |  889 | `	VM_EXIT_BREAK;` |
|    ! 0 |  890 | `	VM_EXIT_BREAK;` |
|      5 |  891 | `}` |
|      - |  892 |  |
|      - |  893 | `/*` |
|      - |  894 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|      - |  895 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  896 | ` */` |
| 506288 |  897 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  898 | `{` |
| 506293 |  899 | `	ph7_value *pTos = pState->pTos;` |
| 506293 |  900 | `	ph7_value *pStack = pState->pStack;` |
| 506293 |  901 | `	VmInstr *aInstr = pState->aInstr;` |
| 506293 |  902 | `	sxi32 pc = pState->pc;` |
|      - |  903 | `	sxi32 rc;` |
| 253346 |  904 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 506293 |  905 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  906 | `	/* Perform the comparison and act accordingly */` |
|      - |  907 | `#ifdef UNTRUST` |
|      - |  908 | `	if( pNos < pStack ){` |
|      - |  909 | `		VM_EXIT_ABORT;` |
|      - |  910 | `	}` |
|      - |  911 | `#endif` |
| 506293 |  912 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 506293 |  913 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  914 | `		rc = 1;` |
|      2 |  915 | `	}else{` |
| 506291 |  916 | `		rc = rc != 0;` |
|      - |  917 | `	}` |
| 506293 |  918 | `	VmPopOperand(&pTos,1);` |
| 506293 |  919 | `	if( !pInstr->iP2 ){` |
|      - |  920 | `		/* Push comparison result without taking the jump */` |
| 506293 |  921 | `		PH7_MemObjRelease(pTos);` |
| 506293 |  922 | `		pTos->x.iVal = rc;` |
|      - |  923 | `		/* Invalidate any prior representation */` |
| 506293 |  924 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 253351 |  925 | `	}else{` |
|    ! 0 |  926 | `		if( rc ){` |
|      - |  927 | `			/* Jump to the desired location */` |
|    ! 0 |  928 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  929 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  930 | `		}` |
|      - |  931 | `	}` |
| 506293 |  932 | `	VM_EXIT_BREAK;` |
|    ! 0 |  933 | `	VM_EXIT_BREAK;` |
|      5 |  934 | `}` |
|      - |  935 |  |
|      - |  936 | `/*` |
|      - |  937 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|      - |  938 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  939 | ` */` |
| 583630 |  940 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  941 | `{` |
| 583635 |  942 | `	ph7_value *pTos = pState->pTos;` |
| 583635 |  943 | `	ph7_value *pStack = pState->pStack;` |
| 583635 |  944 | `	VmInstr *aInstr = pState->aInstr;` |
| 583635 |  945 | `	sxi32 pc = pState->pc;` |
|      - |  946 | `	sxi32 rc;` |
| 292017 |  947 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 583635 |  948 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  949 | `	/* Perform the comparison and act accordingly */` |
|      - |  950 | `#ifdef UNTRUST` |
|      - |  951 | `	if( pNos < pStack ){` |
|      - |  952 | `		VM_EXIT_ABORT;` |
|      - |  953 | `	}` |
|      - |  954 | `#endif` |
| 583635 |  955 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 583635 |  956 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  957 | `		rc = 0;` |
|      2 |  958 | `	}else{` |
| 583633 |  959 | `		rc = rc == 0;` |
|      - |  960 | `	}` |
| 583635 |  961 | `	VmPopOperand(&pTos,1);` |
| 583635 |  962 | `	if( !pInstr->iP2 ){` |
|      - |  963 | `		/* Push comparison result without taking the jump */` |
| 583635 |  964 | `		PH7_MemObjRelease(pTos);` |
| 583635 |  965 | `		pTos->x.iVal = rc;` |
|      - |  966 | `		/* Invalidate any prior representation */` |
| 583635 |  967 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 292022 |  968 | `	}else{` |
|    ! 0 |  969 | `		if( rc ){` |
|      - |  970 | `			/* Jump to the desired location */` |
|    ! 0 |  971 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  972 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  973 | `		}` |
|      - |  974 | `	}` |
| 583635 |  975 | `	VM_EXIT_BREAK;` |
|    ! 0 |  976 | `	VM_EXIT_BREAK;` |
|      5 |  977 | `}` |
|      - |  978 |  |
|      - |  979 | `/*` |
|      - |  980 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|      - |  981 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  982 | ` */` |
|  12046 |  983 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  984 | `{` |
|  12051 |  985 | `	ph7_value *pTos = pState->pTos;` |
|  12051 |  986 | `	ph7_value *pStack = pState->pStack;` |
|  12051 |  987 | `	VmInstr *aInstr = pState->aInstr;` |
|  12051 |  988 | `	sxi32 pc = pState->pc;` |
|      - |  989 | `	sxi32 rc;` |
|   6023 |  990 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  12051 |  991 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  992 | `	/* Perform the comparison and act accordingly */` |
|      - |  993 | `#ifdef UNTRUST` |
|      - |  994 | `	if( pNos < pStack ){` |
|      - |  995 | `		VM_EXIT_ABORT;` |
|      - |  996 | `	}` |
|      - |  997 | `#endif` |
|  12051 |  998 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|  12051 |  999 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|     26 | 1000 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|  12039 | 1001 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|  10849 | 1002 | `		rc = rc == 0;` |
|   5427 | 1003 | `	}else{` |
|   1183 | 1004 | `		rc = rc != 0;` |
|      - | 1005 | `	}` |
|  12051 | 1006 | `	VmPopOperand(&pTos,1);` |
|  12051 | 1007 | `	if( !pInstr->iP2 ){` |
|      - | 1008 | `		/* Push comparison result without taking the jump */` |
|  12051 | 1009 | `		PH7_MemObjRelease(pTos);` |
|  12051 | 1010 | `		pTos->x.iVal = rc;` |
|      - | 1011 | `		/* Invalidate any prior representation */` |
|  12051 | 1012 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   6028 | 1013 | `	}else{` |
|    ! 0 | 1014 | `		if( rc ){` |
|      - | 1015 | `			/* Jump to the desired location */` |
|    ! 0 | 1016 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 | 1017 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1018 | `		}` |
|      - | 1019 | `	}` |
|  12051 | 1020 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1021 | `	VM_EXIT_BREAK;` |
|      5 | 1022 | `}` |
|      - | 1023 |  |
|      - | 1024 | `/*` |
|      - | 1025 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|      - | 1026 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1027 | ` */` |
| 303280 | 1028 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1029 | `{` |
| 303285 | 1030 | `	ph7_value *pTos = pState->pTos;` |
| 303285 | 1031 | `	ph7_value *pStack = pState->pStack;` |
| 303285 | 1032 | `	VmInstr *aInstr = pState->aInstr;` |
| 303285 | 1033 | `	sxi32 pc = pState->pc;` |
|      - | 1034 | `	sxi32 rc;` |
| 151842 | 1035 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 303285 | 1036 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1037 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|      - | 1038 | `#ifdef UNTRUST` |
|      - | 1039 | `	if( pNos < pStack ){` |
|      - | 1040 | `		VM_EXIT_ABORT;` |
|      - | 1041 | `	}` |
|      - | 1042 | `#endif` |
|      - | 1043 | `	/* Force a boolean cast */` |
| 303285 | 1044 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     25 | 1045 | `		PH7_MemObjToBool(pTos);` |
|     12 | 1046 | `	}` |
| 303285 | 1047 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 | 1048 | `		PH7_MemObjToBool(pNos);` |
|    ! 0 | 1049 | `	}` |
| 303285 | 1050 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
| 303285 | 1051 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
| 303285 | 1052 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|      - | 1053 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|  58975 | 1054 | `		v1 = and_logic[v1*3+v2];` |
|  29490 | 1055 | `	}else{` |
|      - | 1056 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
| 244315 | 1057 | `		v1 = or_logic[v1*3+v2];` |
|      - | 1058 | `	}` |
| 303285 | 1059 | `	if( v1 == 2 ){` |
|    ! 0 | 1060 | `		v1 = 1;` |
|    ! 0 | 1061 | `	}` |
| 303285 | 1062 | `	VmPopOperand(&pTos,1);` |
| 303285 | 1063 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
| 303285 | 1064 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 303285 | 1065 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1066 | `	VM_EXIT_BREAK;` |
|      5 | 1067 | `}` |
|      - | 1068 |  |
|      - | 1069 | `/*` |
|      - | 1070 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|      - | 1071 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1072 | ` */` |
|     32 | 1073 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 | 1074 | `{` |
|     33 | 1075 | `	ph7_value *pTos = pState->pTos;` |
|     33 | 1076 | `	ph7_value *pStack = pState->pStack;` |
|     33 | 1077 | `	VmInstr *aInstr = pState->aInstr;` |
|     33 | 1078 | `	sxi32 pc = pState->pc;` |
|      - | 1079 | `	sxi32 rc;` |
|     16 | 1080 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     33 | 1081 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1082 | `	ph7_value *pObj;` |
|      - | 1083 | `	sxi64 a,r;` |
|      - | 1084 | `#ifdef UNTRUST` |
|      - | 1085 | `	if( pNos < pStack ){` |
|      - | 1086 | `		VM_EXIT_ABORT;` |
|      - | 1087 | `	}` |
|      - | 1088 | `#endif` |
|      - | 1089 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|      - | 1090 | `	 * this handler, beside the other eleven compound stores.) */` |
|     36 | 1091 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|      - | 1092 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     27 | 1093 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     27 | 1094 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     27 | 1095 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     27 | 1096 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     27 | 1097 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1098 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 | 1099 | `	}` |
|     27 | 1100 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1101 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 | 1102 | `	}` |
|      - | 1103 | `	/* Perform the requested operation */` |
|     27 | 1104 | `	a = pTos->x.iVal;` |
|     29 | 1105 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|      - | 1106 | `	/* Push the result */` |
|     23 | 1107 | `	pNos->x.iVal = r;` |
|     23 | 1108 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     23 | 1109 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1110 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     23 | 1111 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     23 | 1112 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     23 | 1113 | `		PH7_MemObjStore(pNos,pObj);` |
|     11 | 1114 | `	}` |
|     23 | 1115 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     23 | 1116 | `	VmPopOperand(&pTos,1);` |
|     23 | 1117 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1118 | `	VM_EXIT_BREAK;` |
|     17 | 1119 | `}` |
|      - | 1120 |  |
|      - | 1121 | `/*` |
|      - | 1122 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|      - | 1123 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1124 | ` */` |
|    134 | 1125 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 | 1126 | `{` |
|    136 | 1127 | `	ph7_value *pTos = pState->pTos;` |
|    136 | 1128 | `	ph7_value *pStack = pState->pStack;` |
|    136 | 1129 | `	VmInstr *aInstr = pState->aInstr;` |
|    136 | 1130 | `	sxi32 pc = pState->pc;` |
|      - | 1131 | `	sxi32 rc;` |
|     67 | 1132 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    136 | 1133 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1134 | `	sxi64 a,r;` |
|      - | 1135 | `#ifdef UNTRUST` |
|      - | 1136 | `	if( pNos < pStack ){` |
|      - | 1137 | `		VM_EXIT_ABORT;` |
|      - | 1138 | `	}` |
|      - | 1139 | `#endif` |
|    145 | 1140 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|      - | 1141 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    118 | 1142 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    118 | 1143 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    118 | 1144 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    118 | 1145 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    118 | 1146 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1147 | `		PH7_MemObjToInteger(pTos);` |
|      4 | 1148 | `	}` |
|    118 | 1149 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1150 | `		PH7_MemObjToInteger(pNos);` |
|      4 | 1151 | `	}` |
|      - | 1152 | `	/* Perform the requested operation */` |
|    118 | 1153 | `	a = pNos->x.iVal;` |
|    118 | 1154 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|      - | 1155 | `	/* Push the result */` |
|    108 | 1156 | `	pNos->x.iVal = r;` |
|    108 | 1157 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    108 | 1158 | `	VmPopOperand(&pTos,1);` |
|    108 | 1159 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1160 | `	VM_EXIT_BREAK;` |
|     69 | 1161 | `}` |
|      - | 1162 |  |
|      - | 1163 | `/*` |
|      - | 1164 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|      - | 1165 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1166 | ` */` |
|   3130 | 1167 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1168 | `{` |
|   3135 | 1169 | `	ph7_value *pTos = pState->pTos;` |
|   3135 | 1170 | `	ph7_value *pStack = pState->pStack;` |
|   3135 | 1171 | `	VmInstr *aInstr = pState->aInstr;` |
|   3135 | 1172 | `	sxi32 pc = pState->pc;` |
|      - | 1173 | `	sxi32 rc;` |
|   1565 | 1174 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   3135 | 1175 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1176 | `	{` |
|      - | 1177 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - | 1178 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - | 1179 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - | 1180 | `		SyBlob sArMsg;` |
|   3135 | 1181 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   3135 | 1182 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"*",&sArMsg) != SXRET_OK ){` |
|      - | 1183 | `			sxi32 rcAr;` |
|    ! 0 | 1184 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1185 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 | 1186 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 | 1187 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 | 1188 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 | 1189 | `				SyBlobLength(&sArMsg));` |
|    ! 0 | 1190 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 | 1191 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 | 1192 | `			rc = rcAr;` |
|    ! 0 | 1193 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1194 | `		}` |
|   3135 | 1195 | `		SyBlobRelease(&sArMsg);` |
|      - | 1196 | `	}` |
|      - | 1197 | `	/* Force the operand to be numeric */` |
|      - | 1198 | `#ifdef UNTRUST` |
|      - | 1199 | `	if( pNos < pStack ){` |
|      - | 1200 | `		VM_EXIT_ABORT;` |
|      - | 1201 | `	}` |
|      - | 1202 | `#endif` |
|   3135 | 1203 | `	PH7_MemObjToNumeric(pTos);` |
|   3135 | 1204 | `	PH7_MemObjToNumeric(pNos);` |
|      - | 1205 | `	/* Perform the requested operation */` |
|   3135 | 1206 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - | 1207 | `		/* Floating point arithemic */` |
|      - | 1208 | `		ph7_real a,b,r;` |
|     33 | 1209 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     23 | 1210 | `			PH7_MemObjToReal(pTos);` |
|     11 | 1211 | `		}` |
|     33 | 1212 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 | 1213 | `			PH7_MemObjToReal(pNos);` |
|      2 | 1214 | `		}` |
|     33 | 1215 | `		a = pNos->rVal;` |
|     33 | 1216 | `		b = pTos->rVal;` |
|     33 | 1217 | `		r = a * b;` |
|      - | 1218 | `		/* Push the result */` |
|     33 | 1219 | `		pNos->rVal = r;` |
|     33 | 1220 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1221 | `		/* Try to get an integer representation */` |
|     33 | 1222 | `		PH7_MemObjTryInteger(pNos);` |
|     17 | 1223 | `	}else{` |
|      - | 1224 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|      - | 1225 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - | 1226 | `		sxi64 a,b,r;` |
|   3103 | 1227 | `		a = pNos->x.iVal;` |
|   3103 | 1228 | `		b = pTos->x.iVal;` |
|   3103 | 1229 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|      - | 1230 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     13 | 1231 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|     13 | 1232 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1233 | `#else` |
|      - | 1234 | `			pNos->x.iVal = r;` |
|      - | 1235 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1236 | `#endif` |
|      7 | 1237 | `		}else{` |
|   3091 | 1238 | `			pNos->x.iVal = r;` |
|   3091 | 1239 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1240 | `		}` |
|      - | 1241 | `	}` |
|   3135 | 1242 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|      - | 1243 | `		ph7_value *pObj;` |
|     42 | 1244 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1245 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     42 | 1246 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     42 | 1247 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     42 | 1248 | `			PH7_MemObjStore(pNos,pObj);` |
|     20 | 1249 | `		}` |
|     20 | 1250 | `	}` |
|   3135 | 1251 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|   3135 | 1252 | `	VmPopOperand(&pTos,1);` |
|   3135 | 1253 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1254 | `	VM_EXIT_BREAK;` |
|   1570 | 1255 | `}` |
|      - | 1256 |  |
