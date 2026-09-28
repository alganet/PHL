# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 727/790 lines (92.03%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <math.h>  /* pow */` |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|       - |   11 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|       - |   12 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|       - |   13 | ` *    its labels (same idiom as VmCallFinish).` |
|       - |   14 | ` * Status:` |
|       - |   15 | ` *    Stable.` |
|       - |   16 | ` */` |
|       - |   17 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   18 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   19 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   20 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   21 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   22 | `#include "vm_dispatch.h"` |
|       - |   23 | `#define sState (*pState)` |
|       - |   24 |  |
|       - |   25 | `/*` |
|       - |   26 | ` * A native compare handler REFUSED the pair the operator just asked about` |
|       - |   27 | ` * (php throws DateException comparing two different KINDS of DateTimeZone).` |
|       - |   28 | ` * PH7_MemObjCmp only recorded it -- it has no throw boundary of its own -- so` |
|       - |   29 | ` * the operator raises it here, where the expression's value would have landed:` |
|       - |   30 | ` * both operands go, a null stands in for the result the way every other` |
|       - |   31 | ` * mid-expression throw leaves one, and the status routes to the catching try.` |
|       - |   32 | ` * Used by the four LOOSE comparison arms; the strict pair never asks a handler.` |
|       - |   33 | ` */` |
|       - |   34 | `#define VM_CMP_REFUSAL_ROUTE() \` |
|       - |   35 | `	if( PH7_CmpRefusalPending(pVm) ){ \` |
|       - |   36 | `		sxi32 _rcCmp; \` |
|       - |   37 | `		VmPopOperand(&pTos,1); \` |
|       - |   38 | `		PH7_MemObjRelease(pTos); \` |
|       - |   39 | `		MemObjSetType(pTos,MEMOBJ_NULL); \` |
|       - |   40 | `		pTos->nIdx = SXU32_HIGH; \` |
|       - |   41 | `		_rcCmp = PH7_CmpRefusalRaise(pVm); \` |
|       - |   42 | `		if( _rcCmp == SXERR_ABORT ){ VM_EXIT_ABORT; } \` |
|       - |   43 | `		PH7_THROW_ROUTE_MIDEXPR(_rcCmp) \` |
|       - |   44 | `	}` |
|       - |   45 |  |
|       - |   46 | `/*` |
|       - |   47 | ` * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of` |
|       - |   48 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   49 | ` */` |
|     112 |   50 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |   51 | `{` |
|     115 |   52 | `	ph7_value *pTos = pState->pTos;` |
|     115 |   53 | `	ph7_value *pStack = pState->pStack;` |
|     115 |   54 | `	VmInstr *aInstr = pState->aInstr;` |
|     115 |   55 | `	sxi32 pc = pState->pc;` |
|       - |   56 | `	sxi32 rc;` |
|      56 |   57 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     115 |   58 | `	ph7_value *pNos = &pTos[-1];` |
|       - |   59 | `	ph7_value *pObj;` |
|       - |   60 | `	sxu32 nIdx;` |
|       - |   61 | `#ifdef UNTRUST` |
|       - |   62 | `	if( pNos < pStack ){` |
|       - |   63 | `		VM_EXIT_ABORT;` |
|       - |   64 | `	}` |
|       - |   65 | `#endif` |
|     115 |   66 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|       - |   67 | ``		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a`` |
|       - |   68 | `		 * COAL entry targeting exactly THIS store (owner + pc identity — a` |
|       - |   69 | `		 * stale entry from an abandoned statement can never match, and nested` |
|       - |   70 | `		 * arms in the RHS were consumed/dropped above this one). Dispatch the` |
|       - |   71 | `		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=` |
|       - |   72 | `		 * residual: pre-fix the assign bypassed __set through the normal slot` |
|       - |   73 | `		 * path). The RHS stays as the expression result. */` |
|      13 |   74 | `		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      12 |   75 | `		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr` |
|      12 |   76 | `		 && pTop->nPc == (sxu32)pc` |
|      13 |   77 | `		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK \|\| pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){` |
|      13 |   78 | `			VmHookRmw sPend = *pTop;` |
|      13 |   79 | `			(void)SySetPop(&pVm->aHookRmw);` |
|      13 |   80 | `			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){` |
|      11 |   81 | `				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);` |
|      11 |   82 | `				if( rcHs == PH7_ABORT ){` |
|     ! 0 |   83 | `					SyBlobRelease(&sPend.sName);` |
|     ! 0 |   84 | `					PH7_ClassInstanceUnref(sPend.pThis);` |
|     ! 0 |   85 | `					VM_EXIT_ABORT;` |
|       - |   86 | `				}` |
|       6 |   87 | `			}else{` |
|       - |   88 | `				SyString sSetName;` |
|       3 |   89 | `				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));` |
|       3 |   90 | `				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);` |
|       - |   91 | `			}` |
|      13 |   92 | `			SyBlobRelease(&sPend.sName);` |
|      13 |   93 | `			PH7_ClassInstanceUnref(sPend.pThis);` |
|      13 |   94 | `			PH7_MemObjStore(pTos,pNos);` |
|      13 |   95 | `			pNos->nIdx = SXU32_HIGH;` |
|      13 |   96 | `			VmPopOperand(&pTos,1);` |
|      13 |   97 | `			VM_EXIT_BREAK;` |
|       - |   98 | `		}` |
|     ! 0 |   99 | `	}` |
|       - |  100 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|       - |  101 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|       - |  102 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|     103 |  103 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|      10 |  104 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|      10 |  105 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  106 | `			"offsetSet",sizeof("offsetSet")-1);` |
|       - |  107 | `		ph7_value *apArg[2];` |
|      10 |  108 | `		apArg[0] = &pVm->sCoalesceKey;` |
|      10 |  109 | `		apArg[1] = pTos;` |
|      10 |  110 | `		if( pSet == 0 ){` |
|       - |  111 | `			/* A container that answered the READ and has nowhere to put the write:` |
|       - |  112 | `			 * a class carrying a native dimension handler (ph7_class::xDim) and no` |
|       - |  113 | `			 * ArrayAccess, which is php's DOMNodeList. The store is the same one` |
|       - |  114 | ``			 * `$list[9] = 'x'` performs, so it takes the same Error. */`` |
|       - |  115 | `			char zMsg[256];` |
|       4 |  116 | `			sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,PH7_NATIVE_DIM_WRITE,` |
|       1 |  117 | `				zMsg,sizeof(zMsg));` |
|       3 |  118 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|       3 |  119 | `			VmPopOperand(&pTos,1);` |
|       3 |  120 | `			PH7_MemObjRelease(pTos);` |
|       3 |  121 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  122 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  123 | `			VmCoalesceDisarm(pVm);` |
|       3 |  124 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  125 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  126 | `		}` |
|       8 |  127 | `		PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|       - |  128 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|       8 |  129 | `		PH7_MemObjStore(pTos,pNos);` |
|       8 |  130 | `		VmPopOperand(&pTos,1);` |
|       - |  131 | `		/* Disarm and release the cached instance ref + key. */` |
|       8 |  132 | `		VmCoalesceDisarm(pVm);` |
|       8 |  133 | `		VM_EXIT_BREAK;` |
|       - |  134 | `	}` |
|      94 |  135 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|       - |  136 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|       - |  137 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|       - |  138 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|       - |  139 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|       - |  140 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|       - |  141 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|       - |  142 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|       - |  143 | `		 * offset the quiet peek let through raises at the store. */` |
|      43 |  144 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|      64 |  145 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|      42 |  146 | `			? (ph7_value *)SySetAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|      43 |  147 | `		sxi64 iOfft = 0;` |
|       - |  148 | `		SyBlob sTypeMsg;` |
|       - |  149 | `		int eOfft;` |
|      43 |  150 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|      43 |  151 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|       - |  152 | `			&iOfft,&sTypeMsg);` |
|      43 |  153 | `		if( eOfft == VM_STROFF_REJECT ){` |
|       - |  154 | `			sxi32 rcSo;` |
|       3 |  155 | `			VmPopOperand(&pTos,1);` |
|       3 |  156 | `			PH7_MemObjRelease(pTos);` |
|       3 |  157 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  158 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  159 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|       3 |  160 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  161 | `			rc = rcSo;` |
|       3 |  162 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  163 | `		}` |
|      41 |  164 | `		SyBlobRelease(&sTypeMsg);` |
|       - |  165 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|       - |  166 | `		{` |
|      41 |  167 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|      41 |  168 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       - |  169 | `		}` |
|      41 |  170 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|      41 |  171 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|       - |  172 | `				sxi32 rcEm;` |
|       5 |  173 | `				VmPopOperand(&pTos,1);` |
|       5 |  174 | `				PH7_MemObjRelease(pTos);` |
|       5 |  175 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 |  176 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 |  177 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  178 | `					"Cannot assign an empty string to a string offset",` |
|       - |  179 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|       5 |  180 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  181 | `				rc = rcEm;` |
|       5 |  182 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  183 | `			}` |
|      18 |  184 | `		}` |
|       - |  185 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|       - |  186 | `		 * not free what the carrier owns, so release it here — every other exit` |
|       - |  187 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|      37 |  188 | `		VmFreeCoalStrOff(pCoalOff);` |
|      37 |  189 | `		pNos->x.pOther = 0;` |
|      37 |  190 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|       - |  191 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|      37 |  192 | `		PH7_MemObjStore(pTos,pNos);` |
|      37 |  193 | `		pNos->nIdx = SXU32_HIGH;` |
|      37 |  194 | `		VmPopOperand(&pTos,1);` |
|      37 |  195 | `		VM_EXIT_BREAK;` |
|       - |  196 | `	}` |
|      52 |  197 | `	nIdx = pNos->nIdx;` |
|      52 |  198 | `	if( nIdx == SXU32_HIGH ){` |
|       - |  199 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] .= "x"`, `mk()->p += 1`):`` |
|       - |  200 | `		 * php computes it, drops it with the temporary and stays silent. Every case` |
|       - |  201 | `		 * that IS a refusal — a class constant, a hooked or handler-backed property —` |
|       - |  202 | `		 * is decided before the VM sees it. */` |
|      52 |  203 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      52 |  204 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      50 |  205 | `		PH7_MemObjStore(pTos,pObj);` |
|      24 |  206 | `	}` |
|      50 |  207 | `	PH7_MemObjStore(pTos,pNos);` |
|      50 |  208 | `	VmPopOperand(&pTos,1);` |
|      50 |  209 | `	VM_EXIT_BREAK;` |
|     ! 0 |  210 | `	VM_EXIT_BREAK;` |
|      59 |  211 | `}` |
|       - |  212 |  |
|       - |  213 | `/*` |
|       - |  214 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|       - |  215 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  216 | ` */` |
|     956 |  217 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |  218 | `{` |
|       - |  219 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  220 | `	int rcNa;` |
|     959 |  221 | `	const char *zArCls = "TypeError";` |
|     959 |  222 | `	ph7_value *pTos = pState->pTos;` |
|     959 |  223 | `	ph7_value *pStack = pState->pStack;` |
|     959 |  224 | `	VmInstr *aInstr = pState->aInstr;` |
|     959 |  225 | `	sxi32 pc = pState->pc;` |
|       - |  226 | `	sxi32 rc;` |
|     478 |  227 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     959 |  228 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  229 | `	{` |
|       - |  230 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  231 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  232 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  233 | `		SyBlob sArMsg;` |
|     959 |  234 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     959 |  235 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"/",pNos,&zArCls,&sArMsg);` |
|     959 |  236 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  237 | `			sxi32 rcAr;` |
|     161 |  238 | `			VmPopOperand(&pTos,1);` |
|     161 |  239 | `			PH7_MemObjRelease(pTos);` |
|     161 |  240 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     161 |  241 | `			pTos->nIdx = SXU32_HIGH;` |
|     241 |  242 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  243 | `				SyBlobLength(&sArMsg));` |
|     161 |  244 | `			SyBlobRelease(&sArMsg);` |
|     241 |  245 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     161 |  246 | `			rc = rcAr;` |
|     161 |  247 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  248 | `		}` |
|     799 |  249 | `		SyBlobRelease(&sArMsg);` |
|       - |  250 | `	}` |
|     799 |  251 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  252 | `		VmPopOperand(&pTos,1);` |
|       3 |  253 | `		VM_EXIT_BREAK;` |
|       - |  254 | `	}` |
|       - |  255 | `	ph7_real a,b,r;` |
|       - |  256 | `#ifdef UNTRUST` |
|       - |  257 | `	if( pNos < pStack ){` |
|       - |  258 | `		VM_EXIT_ABORT;` |
|       - |  259 | `	}` |
|       - |  260 | `#endif` |
|       - |  261 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|       - |  262 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|       - |  263 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|       - |  264 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|       - |  265 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|     797 |  266 | `	PH7_MemObjToNumeric(pTos);` |
|     797 |  267 | `	PH7_MemObjToNumeric(pNos);` |
|     797 |  268 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     729 |  269 | `		sxi64 ia = pNos->x.iVal;` |
|     729 |  270 | `		sxi64 ib = pTos->x.iVal;` |
|     729 |  271 | `		sxi64 iQuot = 0;` |
|     729 |  272 | `		int bExact = 0;` |
|     729 |  273 | `		if( ib == 0 ){` |
|      74 |  274 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      78 |  275 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     656 |  276 | `		}else if( ib == -1 ){` |
|       - |  277 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|       - |  278 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|       - |  279 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|       - |  280 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|       - |  281 | `			 * alongside the remainder, so testing the remainder first trapped` |
|       - |  282 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|       - |  283 | `			 * OP_MOD screens the same hazard the same way. */` |
|       - |  284 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  285 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|       - |  286 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|       - |  287 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|       - |  288 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|       - |  289 | `			bExact = 1;` |
|       - |  290 | `#else` |
|      19 |  291 | `			if( ia != SMALLEST_INT64 ){` |
|      15 |  292 | `				iQuot = -ia;` |
|      15 |  293 | `				bExact = 1;` |
|       8 |  294 | `			}` |
|       - |  295 | `#endif` |
|     647 |  296 | `		}else if( ia % ib == 0 ){` |
|     128 |  297 | `			iQuot = ia / ib;` |
|     128 |  298 | `			bExact = 1;` |
|      63 |  299 | `		}` |
|     656 |  300 | `		if( bExact ){` |
|     142 |  301 | `			pNos->x.iVal = iQuot;` |
|     142 |  302 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     142 |  303 | `			VmPopOperand(&pTos,1);` |
|     142 |  304 | `			VM_EXIT_BREAK;` |
|       - |  305 | `		}` |
|     257 |  306 | `	}` |
|       - |  307 | `	/* Force the operands to be real */` |
|     584 |  308 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     567 |  309 | `		PH7_MemObjToReal(pTos);` |
|     283 |  310 | `	}` |
|     584 |  311 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     515 |  312 | `		PH7_MemObjToReal(pNos);` |
|     257 |  313 | `	}` |
|       - |  314 | `	/* Perform the requested operation */` |
|     584 |  315 | `	a = pNos->rVal;` |
|     584 |  316 | `	b = pTos->rVal;` |
|     584 |  317 | `	if( b == 0 ){` |
|       - |  318 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  319 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|       3 |  320 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       3 |  321 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  322 | `	}else{` |
|     582 |  323 | `		r = a/b;` |
|       - |  324 | `		/* Push the result */` |
|     582 |  325 | `		pNos->rVal = r;` |
|     582 |  326 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  327 | `	}` |
|     582 |  328 | `	VmPopOperand(&pTos,1);` |
|     582 |  329 | `	VM_EXIT_BREAK;` |
|     ! 0 |  330 | `	VM_EXIT_BREAK;` |
|     481 |  331 | `}` |
|       - |  332 |  |
|       - |  333 | `/*` |
|       - |  334 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|       - |  335 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  336 | ` */` |
|     406 |  337 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 |  338 | `{` |
|       - |  339 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  340 | `	int rcNa;` |
|     407 |  341 | `	const char *zArCls = "TypeError";` |
|     407 |  342 | `	ph7_value *pTos = pState->pTos;` |
|     407 |  343 | `	ph7_value *pStack = pState->pStack;` |
|     407 |  344 | `	VmInstr *aInstr = pState->aInstr;` |
|     407 |  345 | `	sxi32 pc = pState->pc;` |
|       - |  346 | `	sxi32 rc;` |
|     203 |  347 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     407 |  348 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  349 | `	ph7_value *pObj;` |
|       - |  350 | `	sxi64 a,b,r;` |
|       - |  351 | `#ifdef UNTRUST` |
|       - |  352 | `	if( pNos < pStack ){` |
|       - |  353 | `		VM_EXIT_ABORT;` |
|       - |  354 | `	}` |
|       - |  355 | `#endif` |
|       - |  356 | `	{` |
|       - |  357 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  358 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  359 | `		SyBlob sArMsg;` |
|     407 |  360 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     407 |  361 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"%",pNos,&zArCls,&sArMsg);` |
|     407 |  362 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  363 | `			sxi32 rcAr;` |
|     151 |  364 | `			VmPopOperand(&pTos,1);` |
|     151 |  365 | `			PH7_MemObjRelease(pTos);` |
|     151 |  366 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     151 |  367 | `			pTos->nIdx = SXU32_HIGH;` |
|     226 |  368 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      75 |  369 | `				SyBlobLength(&sArMsg));` |
|     151 |  370 | `			SyBlobRelease(&sArMsg);` |
|     226 |  371 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     151 |  372 | `			rc = rcAr;` |
|     151 |  373 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  374 | `		}` |
|     257 |  375 | `		SyBlobRelease(&sArMsg);` |
|       - |  376 | `	}` |
|     257 |  377 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  378 | `		goto mod_store_write;` |
|       - |  379 | `	}` |
|       - |  380 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     257 |  381 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     257 |  382 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  383 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     257 |  384 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  385 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     113 |  386 | `		PH7_MemObjToInteger(pTos);` |
|      56 |  387 | `	}` |
|     257 |  388 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     111 |  389 | `		PH7_MemObjToInteger(pNos);` |
|      55 |  390 | `	}` |
|       - |  391 | `	/* Perform the requested operation */` |
|     257 |  392 | `	a = pTos->x.iVal;` |
|     257 |  393 | `	b = pNos->x.iVal;` |
|     257 |  394 | `	if( b == 0 ){` |
|       - |  395 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  396 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      71 |  397 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      71 |  398 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  399 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|     187 |  400 | `	}else if( b == -1 ){` |
|       - |  401 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|       - |  402 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|       3 |  403 | `		r = 0;` |
|       2 |  404 | `	}else{` |
|     185 |  405 | `		r = a%b;` |
|       - |  406 | `	}` |
|       - |  407 | `	/* Push the result */` |
|     187 |  408 | `	pNos->x.iVal = r;` |
|     187 |  409 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      93 |  410 | `mod_store_write:` |
|     187 |  411 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  412 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     186 |  413 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     185 |  414 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     185 |  415 | `		PH7_MemObjStore(pNos,pObj);` |
|      92 |  416 | `	}` |
|     187 |  417 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     187 |  418 | `	VmPopOperand(&pTos,1);` |
|     187 |  419 | `	VM_EXIT_BREAK;` |
|     ! 0 |  420 | `	VM_EXIT_BREAK;` |
|     204 |  421 | `}` |
|       - |  422 |  |
|       - |  423 | `/*` |
|       - |  424 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|       - |  425 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  426 | ` */` |
|    2170 |  427 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  428 | `{` |
|       - |  429 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  430 | `	int rcNa;` |
|    2175 |  431 | `	const char *zArCls = "TypeError";` |
|    2175 |  432 | `	ph7_value *pTos = pState->pTos;` |
|    2175 |  433 | `	ph7_value *pStack = pState->pStack;` |
|    2175 |  434 | `	VmInstr *aInstr = pState->aInstr;` |
|    2175 |  435 | `	sxi32 pc = pState->pc;` |
|       - |  436 | `	sxi32 rc;` |
|    1085 |  437 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    2175 |  438 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  439 | `	{` |
|       - |  440 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  441 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  442 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  443 | `		SyBlob sArMsg;` |
|    2175 |  444 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    2175 |  445 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"%",pNos,&zArCls,&sArMsg);` |
|    2175 |  446 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  447 | `			sxi32 rcAr;` |
|     163 |  448 | `			VmPopOperand(&pTos,1);` |
|     163 |  449 | `			PH7_MemObjRelease(pTos);` |
|     163 |  450 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     163 |  451 | `			pTos->nIdx = SXU32_HIGH;` |
|     244 |  452 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      81 |  453 | `				SyBlobLength(&sArMsg));` |
|     163 |  454 | `			SyBlobRelease(&sArMsg);` |
|     244 |  455 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     163 |  456 | `			rc = rcAr;` |
|     165 |  457 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  458 | `		}` |
|    2013 |  459 | `		SyBlobRelease(&sArMsg);` |
|       - |  460 | `	}` |
|    2013 |  461 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  462 | `		VmPopOperand(&pTos,1);` |
|       3 |  463 | `		VM_EXIT_BREAK;` |
|       - |  464 | `	}` |
|       - |  465 | `	sxi64 a,b,r;` |
|       - |  466 | `#ifdef UNTRUST` |
|       - |  467 | `	if( pNos < pStack ){` |
|       - |  468 | `		VM_EXIT_ABORT;` |
|       - |  469 | `	}` |
|       - |  470 | `#endif` |
|       - |  471 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    2011 |  472 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    2011 |  473 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    1997 |  474 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    1997 |  475 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    1995 |  476 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     117 |  477 | `		PH7_MemObjToInteger(pTos);` |
|      58 |  478 | `	}` |
|    1995 |  479 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     122 |  480 | `		PH7_MemObjToInteger(pNos);` |
|      60 |  481 | `	}` |
|       - |  482 | `	/* Perform the requested operation */` |
|    1995 |  483 | `	a = pNos->x.iVal;` |
|    1995 |  484 | `	b = pTos->x.iVal;` |
|    1995 |  485 | `	if( b == 0 ){` |
|       - |  486 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  487 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      76 |  488 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      84 |  489 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  490 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|    1921 |  491 | `	}else if( b == -1 ){` |
|       - |  492 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|       - |  493 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|       - |  494 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|       - |  495 | `		 * remainder. php's result here is 0. */` |
|       5 |  496 | `		r = 0;` |
|       3 |  497 | `	}else{` |
|    1917 |  498 | `		r = a%b;` |
|       - |  499 | `	}` |
|       - |  500 | `	/* Push the result */` |
|    1921 |  501 | `	pNos->x.iVal = r;` |
|    1921 |  502 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    1921 |  503 | `	VmPopOperand(&pTos,1);` |
|    1921 |  504 | `	VM_EXIT_BREAK;` |
|     ! 0 |  505 | `	VM_EXIT_BREAK;` |
|    1090 |  506 | `}` |
|       - |  507 |  |
|       - |  508 | `/*` |
|       - |  509 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|       - |  510 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  511 | ` */` |
|     408 |  512 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  513 | `{` |
|       - |  514 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  515 | `	int rcNa;` |
|     410 |  516 | `	const char *zArCls = "TypeError";` |
|     410 |  517 | `	ph7_value *pTos = pState->pTos;` |
|     410 |  518 | `	ph7_value *pStack = pState->pStack;` |
|     410 |  519 | `	VmInstr *aInstr = pState->aInstr;` |
|     410 |  520 | `	sxi32 pc = pState->pc;` |
|       - |  521 | `	sxi32 rc;` |
|     204 |  522 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     410 |  523 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  524 | `	ph7_value *pObj;` |
|       - |  525 | `#ifdef UNTRUST` |
|       - |  526 | `	if( pNos < pStack ){` |
|       - |  527 | `		VM_EXIT_ABORT;` |
|       - |  528 | `	}` |
|       - |  529 | `#endif` |
|       - |  530 | `	{` |
|       - |  531 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  532 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  533 | `		SyBlob sArMsg;` |
|     410 |  534 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     410 |  535 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"-",pNos,&zArCls,&sArMsg);` |
|     410 |  536 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  537 | `			sxi32 rcAr;` |
|     153 |  538 | `			VmPopOperand(&pTos,1);` |
|     153 |  539 | `			PH7_MemObjRelease(pTos);` |
|     153 |  540 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     153 |  541 | `			pTos->nIdx = SXU32_HIGH;` |
|     229 |  542 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      76 |  543 | `				SyBlobLength(&sArMsg));` |
|     153 |  544 | `			SyBlobRelease(&sArMsg);` |
|     229 |  545 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     153 |  546 | `			rc = rcAr;` |
|     153 |  547 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  548 | `		}` |
|     258 |  549 | `		SyBlobRelease(&sArMsg);` |
|       - |  550 | `	}` |
|     258 |  551 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  552 | `		goto sub_store_write;` |
|       - |  553 | `	}` |
|       - |  554 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     258 |  555 | `	PH7_MemObjToNumeric(pTos);` |
|     258 |  556 | `	PH7_MemObjToNumeric(pNos);` |
|     386 |  557 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  558 | `		/* Floating point arithemic */` |
|       - |  559 | `		ph7_real a,b,r;` |
|     ! 0 |  560 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  561 | `			PH7_MemObjToReal(pTos);` |
|     ! 0 |  562 | `		}` |
|     ! 0 |  563 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  564 | `			PH7_MemObjToReal(pNos);` |
|     ! 0 |  565 | `		}` |
|     ! 0 |  566 | `		a = pTos->rVal;` |
|     ! 0 |  567 | `		b = pNos->rVal;` |
|     ! 0 |  568 | `		r = a - b;` |
|       - |  569 | `		/* Push the result */` |
|     ! 0 |  570 | `		pNos->rVal = r;` |
|     ! 0 |  571 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  572 | `		/* Try to get an integer representation */` |
|     ! 0 |  573 | `		PH7_MemObjTryInteger(pNos);` |
|     ! 0 |  574 | `	}else{` |
|       - |  575 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  576 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  577 | `		sxi64 a,b,r;` |
|     258 |  578 | `		a = pTos->x.iVal;` |
|     258 |  579 | `		b = pNos->x.iVal;` |
|     258 |  580 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  581 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       7 |  582 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|       7 |  583 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  584 | `#else` |
|       - |  585 | `			pNos->x.iVal = r;` |
|       - |  586 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  587 | `#endif` |
|       4 |  588 | `		}else{` |
|     252 |  589 | `			pNos->x.iVal = r;` |
|     252 |  590 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  591 | `		}` |
|       - |  592 | `	}` |
|     128 |  593 | `sub_store_write:` |
|     258 |  594 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  595 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     258 |  596 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     258 |  597 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     258 |  598 | `		PH7_MemObjStore(pNos,pObj);` |
|     128 |  599 | `	}` |
|     258 |  600 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     258 |  601 | `	VmPopOperand(&pTos,1);` |
|     258 |  602 | `	VM_EXIT_BREAK;` |
|     ! 0 |  603 | `	VM_EXIT_BREAK;` |
|     206 |  604 | `}` |
|       - |  605 |  |
|       - |  606 | `/*` |
|       - |  607 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|       - |  608 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  609 | ` */` |
|   54464 |  610 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  611 | `{` |
|       - |  612 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  613 | `	int rcNa;` |
|   54469 |  614 | `	const char *zArCls = "TypeError";` |
|   54469 |  615 | `	ph7_value *pTos = pState->pTos;` |
|   54469 |  616 | `	ph7_value *pStack = pState->pStack;` |
|   54469 |  617 | `	VmInstr *aInstr = pState->aInstr;` |
|   54469 |  618 | `	sxi32 pc = pState->pc;` |
|       - |  619 | `	sxi32 rc;` |
|   27398 |  620 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   54469 |  621 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  622 | `#ifdef UNTRUST` |
|       - |  623 | `	if( pNos < pStack ){` |
|       - |  624 | `		VM_EXIT_ABORT;` |
|       - |  625 | `	}` |
|       - |  626 | `#endif` |
|       - |  627 | `	{` |
|       - |  628 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  629 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  630 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  631 | `		SyBlob sArMsg;` |
|   54469 |  632 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   54469 |  633 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"-",pNos,&zArCls,&sArMsg);` |
|   54469 |  634 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  635 | `			sxi32 rcAr;` |
|     162 |  636 | `			VmPopOperand(&pTos,1);` |
|     162 |  637 | `			PH7_MemObjRelease(pTos);` |
|     162 |  638 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     162 |  639 | `			pTos->nIdx = SXU32_HIGH;` |
|     242 |  640 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  641 | `				SyBlobLength(&sArMsg));` |
|     162 |  642 | `			SyBlobRelease(&sArMsg);` |
|     242 |  643 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     162 |  644 | `			rc = rcAr;` |
|     162 |  645 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  646 | `		}` |
|   54309 |  647 | `		SyBlobRelease(&sArMsg);` |
|       - |  648 | `	}` |
|   54309 |  649 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 |  650 | `		VmPopOperand(&pTos,1);` |
|       5 |  651 | `		VM_EXIT_BREAK;` |
|       - |  652 | `	}` |
|       - |  653 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|       - |  654 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|       - |  655 | `	 * quietly evaluated to 0. */` |
|   54305 |  656 | `	PH7_MemObjToNumeric(pTos);` |
|   54305 |  657 | `	PH7_MemObjToNumeric(pNos);` |
|   54305 |  658 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  659 | `		/* Floating point arithemic */` |
|       - |  660 | `		ph7_real a,b,r;` |
|     156 |  661 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      21 |  662 | `			PH7_MemObjToReal(pTos);` |
|      10 |  663 | `		}` |
|     156 |  664 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       9 |  665 | `			PH7_MemObjToReal(pNos);` |
|       4 |  666 | `		}` |
|     156 |  667 | `		a = pNos->rVal;` |
|     156 |  668 | `		b = pTos->rVal;` |
|     156 |  669 | `		r = a - b;` |
|       - |  670 | `		/* Push the result */` |
|     156 |  671 | `		pNos->rVal = r;` |
|     156 |  672 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  673 | `		/* Try to get an integer representation */` |
|     156 |  674 | `		PH7_MemObjTryInteger(pNos);` |
|      79 |  675 | `	}else{` |
|       - |  676 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  677 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  678 | `		sxi64 a,b,r;` |
|   54151 |  679 | `		a = pNos->x.iVal;` |
|   54151 |  680 | `		b = pTos->x.iVal;` |
|   54151 |  681 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  682 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 |  683 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      11 |  684 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  685 | `#else` |
|       - |  686 | `			pNos->x.iVal = r;` |
|       - |  687 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  688 | `#endif` |
|       6 |  689 | `		}else{` |
|   54141 |  690 | `			pNos->x.iVal = r;` |
|   54141 |  691 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  692 | `		}` |
|       - |  693 | `	}` |
|   54305 |  694 | `	VmPopOperand(&pTos,1);` |
|   54305 |  695 | `	VM_EXIT_BREAK;` |
|     ! 0 |  696 | `	VM_EXIT_BREAK;` |
|   27403 |  697 | `}` |
|       - |  698 |  |
|       - |  699 | `/*` |
|       - |  700 | `` * php's `**`, as a value operation.`` |
|       - |  701 | ` *` |
|       - |  702 | ` * In php pow() IS the exponentiation operator -- both compile to the same` |
|       - |  703 | ` * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and` |
|       - |  704 | ` * every edge value have to come from ONE place here too. This is that place:` |
|       - |  705 | ` * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call` |
|       - |  706 | ` * it, after the caller has run VmArithOperandCheck() over the two operands (the` |
|       - |  707 | ` * two sites word their throw differently -- one settles an operand stack first,` |
|       - |  708 | ` * the other is inside a C builtin -- so the CHECK stays with the caller and only` |
|       - |  709 | ` * the arithmetic is shared).` |
|       - |  710 | ` *` |
|       - |  711 | ` * pBase and pExp are converted in place, which is what the opcode arm already` |
|       - |  712 | ` * did to its stack slots; pOut may alias either of them and is written last.` |
|       - |  713 | ` */` |
|     790 |  714 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)` |
|       3 |  715 | `{` |
|       - |  716 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - |  717 | `	int bBothInt;` |
|     793 |  718 | `	int usedInt = 0;` |
|       - |  719 | `	ph7_real a, b, r;` |
|       - |  720 | `#endif` |
|     793 |  721 | `	sxi64 base_i = 0, exp_i = 0;` |
|     793 |  722 | `	PH7_MemObjToNumeric(pBase);` |
|     793 |  723 | `	PH7_MemObjToNumeric(pExp);` |
|       - |  724 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    1547 |  725 | `	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&` |
|     754 |  726 | `	           ((pExp->iFlags & MEMOBJ_REAL) == 0);` |
|     793 |  727 | `	if( bBothInt ){` |
|     749 |  728 | `		base_i = pBase->x.iVal;` |
|     749 |  729 | `		exp_i  = pExp->x.iVal;` |
|     373 |  730 | `	}` |
|     793 |  731 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|     757 |  732 | `		PH7_MemObjToReal(pBase);` |
|     377 |  733 | `	}` |
|     793 |  734 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|     785 |  735 | `		PH7_MemObjToReal(pExp);` |
|     391 |  736 | `	}` |
|     793 |  737 | `	a = pBase->rVal;` |
|     793 |  738 | `	b = pExp->rVal;` |
|     793 |  739 | `	r = pow(a, b);` |
|       - |  740 | `	/* int ** non-negative int is php's OWN loop, not a call to pow(), and the` |
|       - |  741 | `	 * difference is visible in the answer twice over. php multiplies in` |
|       - |  742 | ``	 * `pow_function_base`'s doubling loop and, the moment a step OVERFLOWS, finishes`` |
|       - |  743 | ``	 * in DOUBLE space FROM THERE — `dval * pow(l2, i)` with whatever exponent is`` |
|       - |  744 | `	 * left — rather than re-computing pow(base, exp) from the original operands.` |
|       - |  745 | ``	 * That carries the accumulated SIGN, so `(-3) ** PHP_INT_MAX` is -INF where`` |
|       - |  746 | ``	 * pow() answers +INF, and it rounds differently, so `3 ** 100` is`` |
|       - |  747 | ``	 * 5.1537752073201141e+47 where pow() gives ...132e+47 and `10 ** 64` is`` |
|       - |  748 | `	 * 1.0000000000000002e+64 where pow() gives exactly 1e+64. 33 rows of a 380-row` |
|       - |  749 | `	 * base×exponent sweep were wrong, sign included.` |
|       - |  750 | `	 * The overflowing product is the DOUBLE product of the two operands, which is` |
|       - |  751 | `	 * what php's ZEND_SIGNED_MULTIPLY_LONG hands back wherever it detects the` |
|       - |  752 | `	 * overflow with a builtin (gcc/clang) or with _mul128 (MSVC) — the two` |
|       - |  753 | `	 * platforms PHL builds on. */` |
|     793 |  754 | `	if( bBothInt && exp_i >= 0 ){` |
|     689 |  755 | `		sxi64 l1 = 1, l2 = base_i, i = exp_i, iProd;` |
|     689 |  756 | `		if( i == 0 ){` |
|       - |  757 | `			/* Anything to the 0 is int 1 — php answers before it looks at the base. */` |
|     149 |  758 | `			pOut->x.iVal = 1;` |
|     149 |  759 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|     149 |  760 | `			usedInt = 1;` |
|     615 |  761 | `		}else if( l2 == 0 ){` |
|      93 |  762 | `			pOut->x.iVal = 0;` |
|      93 |  763 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|      93 |  764 | `			usedInt = 1;` |
|      47 |  765 | `		}else{` |
|    3475 |  766 | `			while( i >= 1 ){` |
|    3475 |  767 | `				if( i % 2 ){` |
|    1775 |  768 | `					--i;` |
|    1775 |  769 | `					if( PH7_MUL_OVERFLOW64(l1, l2, &iProd) ){` |
|      53 |  770 | `						r = ((ph7_real)l1 * (ph7_real)l2) * pow((ph7_real)l2,(ph7_real)i);` |
|      53 |  771 | `						break;` |
|       - |  772 | `					}` |
|    1723 |  773 | `					l1 = iProd;` |
|     863 |  774 | `				}else{` |
|    1703 |  775 | `					i /= 2;` |
|    1703 |  776 | `					if( PH7_MUL_OVERFLOW64(l2, l2, &iProd) ){` |
|      45 |  777 | `						r = (ph7_real)l1 * pow((ph7_real)l2 * (ph7_real)l2,(ph7_real)i);` |
|      45 |  778 | `						break;` |
|       - |  779 | `					}` |
|    1659 |  780 | `					l2 = iProd;` |
|       - |  781 | `				}` |
|    3379 |  782 | `				if( i == 0 ){` |
|     353 |  783 | `					pOut->x.iVal = l1;` |
|     353 |  784 | `					MemObjSetType(pOut, MEMOBJ_INT);` |
|     353 |  785 | `					usedInt = 1;` |
|     353 |  786 | `					break;` |
|       - |  787 | `				}` |
|       3 |  788 | `			}` |
|       - |  789 | `		}` |
|     343 |  790 | `	}` |
|     793 |  791 | `	if( !usedInt ){` |
|     201 |  792 | `		pOut->rVal = r;` |
|     201 |  793 | `		MemObjSetType(pOut, MEMOBJ_REAL);` |
|     100 |  794 | `	}` |
|       - |  795 | `#else` |
|       - |  796 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|       - |  797 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|       - |  798 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|       - |  799 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|       - |  800 | `	 * represented. */` |
|       - |  801 | `	base_i = pBase->x.iVal;` |
|       - |  802 | `	exp_i  = pExp->x.iVal;` |
|       - |  803 | `	{` |
|       - |  804 | `		sxi64 result_i = 1;` |
|       - |  805 | `		sxi64 cur_base = base_i;` |
|       - |  806 | `		sxi64 cur_exp  = exp_i;` |
|       - |  807 | `		if( cur_exp < 0 ){` |
|       - |  808 | `			result_i = 0;` |
|       - |  809 | `		}else{` |
|       - |  810 | `			while( cur_exp > 0 ){` |
|       - |  811 | `				if( cur_exp & 1 ){` |
|       - |  812 | `					result_i *= cur_base;` |
|       - |  813 | `				}` |
|       - |  814 | `				cur_exp >>= 1;` |
|       - |  815 | `				if( cur_exp > 0 ){` |
|       - |  816 | `					cur_base *= cur_base;` |
|       - |  817 | `				}` |
|       - |  818 | `			}` |
|       - |  819 | `		}` |
|       - |  820 | `		pOut->x.iVal = result_i;` |
|       - |  821 | `		MemObjSetType(pOut, MEMOBJ_INT);` |
|       - |  822 | `	}` |
|       - |  823 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     793 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|       - |  827 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  828 | ` */` |
|    1068 |  829 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  830 | `{` |
|       - |  831 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  832 | `	int rcNa;` |
|    1070 |  833 | `	const char *zArCls = "TypeError";` |
|    1070 |  834 | `	ph7_value *pTos = pState->pTos;` |
|    1070 |  835 | `	ph7_value *pStack = pState->pStack;` |
|    1070 |  836 | `	VmInstr *aInstr = pState->aInstr;` |
|    1070 |  837 | `	sxi32 pc = pState->pc;` |
|       - |  838 | `	sxi32 rc;` |
|     534 |  839 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    1070 |  840 | `	ph7_value *pNos = &pTos[-1];` |
|    1070 |  841 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|       - |  842 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|       - |  843 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|       - |  844 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|       - |  845 | `	 */` |
|    1070 |  846 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    1070 |  847 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|       - |  848 | `	{` |
|       - |  849 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  850 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  851 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - |  852 | `		 * The message names the operands in SOURCE order, so it takes the same` |
|       - |  853 | ``		 * base/exponent convention as the operation: `$x **= "abc"` is`` |
|       - |  854 | ``		 * `int ** string`, the way `$x ** "abc"` is. */`` |
|       - |  855 | `		SyBlob sArMsg;` |
|    1070 |  856 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    1070 |  857 | `		rcNa = VmArithOperandStep(&(*pVm),pBase,pExp,"**",pNos,&zArCls,&sArMsg);` |
|    1070 |  858 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  859 | `			sxi32 rcAr;` |
|     322 |  860 | `			VmPopOperand(&pTos,1);` |
|     322 |  861 | `			PH7_MemObjRelease(pTos);` |
|     322 |  862 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     322 |  863 | `			pTos->nIdx = SXU32_HIGH;` |
|     482 |  864 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     160 |  865 | `				SyBlobLength(&sArMsg));` |
|     322 |  866 | `			SyBlobRelease(&sArMsg);` |
|     482 |  867 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     322 |  868 | `			rc = rcAr;` |
|     326 |  869 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  870 | `		}` |
|     749 |  871 | `		SyBlobRelease(&sArMsg);` |
|       - |  872 | `	}` |
|       - |  873 | `#ifdef UNTRUST` |
|       - |  874 | `	if( pNos < pStack ){` |
|       - |  875 | `		VM_EXIT_ABORT;` |
|       - |  876 | `	}` |
|       - |  877 | `#endif` |
|     749 |  878 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|     745 |  879 | `		PH7_MemObjPow(pBase,pExp,pNos);` |
|     372 |  880 | `	}` |
|     749 |  881 | `	if( bStore ){` |
|       - |  882 | `		ph7_value *pObj;` |
|     275 |  883 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  884 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     274 |  885 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     273 |  886 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     273 |  887 | `			PH7_MemObjStore(pNos,pObj);` |
|     136 |  888 | `		}` |
|     137 |  889 | `	}` |
|     749 |  890 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     749 |  891 | `	VmPopOperand(&pTos,1);` |
|     749 |  892 | `	VM_EXIT_BREAK;` |
|     ! 0 |  893 | `	VM_EXIT_BREAK;` |
|     536 |  894 | `}` |
|       - |  895 |  |
|       - |  896 | `/*` |
|       - |  897 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|       - |  898 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  899 | ` */` |
|     778 |  900 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 |  901 | `{` |
|     782 |  902 | `	ph7_value *pTos = pState->pTos;` |
|     782 |  903 | `	ph7_value *pStack = pState->pStack;` |
|     782 |  904 | `	VmInstr *aInstr = pState->aInstr;` |
|     782 |  905 | `	sxi32 pc = pState->pc;` |
|       - |  906 | `	sxi32 rc;` |
|     389 |  907 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     782 |  908 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  909 | `#ifdef UNTRUST` |
|       - |  910 | `	if( pNos < pStack ){` |
|       - |  911 | `		VM_EXIT_ABORT;` |
|       - |  912 | `	}` |
|       - |  913 | `#endif` |
|       - |  914 | `	/* php answers the UNORDERED comparison -- a NaN, or two arrays neither of` |
|       - |  915 | `	 * which contains the other -- with 1 whichever way round it is asked, and` |
|       - |  916 | `	 * PH7_MemObjCmp does the same, so the spaceship needs no case of its own. */` |
|     782 |  917 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     784 |  918 | `	VM_CMP_REFUSAL_ROUTE()` |
|     774 |  919 | `	rc = (rc > 0) - (rc < 0);   /* normalize to exactly -1, 0 or 1 */` |
|     774 |  920 | `	VmPopOperand(&pTos,1);` |
|     774 |  921 | `	PH7_MemObjRelease(pTos);` |
|     774 |  922 | `	pTos->x.iVal = rc;` |
|     774 |  923 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     774 |  924 | `	VM_EXIT_BREAK;` |
|     ! 0 |  925 | `	VM_EXIT_BREAK;` |
|     393 |  926 | `}` |
|       - |  927 |  |
|       - |  928 | `/*` |
|       - |  929 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|       - |  930 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  931 | ` */` |
|  279334 |  932 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  933 | `{` |
|  279339 |  934 | `	ph7_value *pTos = pState->pTos;` |
|  279339 |  935 | `	ph7_value *pStack = pState->pStack;` |
|  279339 |  936 | `	VmInstr *aInstr = pState->aInstr;` |
|  279339 |  937 | `	sxi32 pc = pState->pc;` |
|       - |  938 | `	sxi32 rc;` |
|  139893 |  939 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  279339 |  940 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  941 | `	/* Perform the comparison and act accordingly */` |
|       - |  942 | `#ifdef UNTRUST` |
|       - |  943 | `	if( pNos < pStack ){` |
|       - |  944 | `		VM_EXIT_ABORT;` |
|       - |  945 | `	}` |
|       - |  946 | `#endif` |
|       - |  947 | ``	/* `$a > $b` is php's `$b < $a` -- asked from the OTHER SIDE, not read off`` |
|       - |  948 | `	 * this side's sign. The two differ exactly where the comparison is` |
|       - |  949 | `	 * UNORDERED and php answers 1 both ways (a NaN against a number or a` |
|       - |  950 | `	 * string; two same-sized arrays neither of which contains the other): a` |
|       - |  951 | ``	 * greater-than read off `rc > 0` calls both of those TRUE, where php --`` |
|       - |  952 | ``	 * asking `$b < $a` and getting 1 again -- calls them false, as it does`` |
|       - |  953 | `	 * every other relational operator on such a pair. */` |
|  279339 |  954 | `	rc = PH7_MemObjCmp(pTos,pNos,FALSE,0);` |
|  279339 |  955 | `	VM_CMP_REFUSAL_ROUTE()` |
|  279339 |  956 | `	if( pInstr->iOp == PH7_OP_GE ){` |
|  274839 |  957 | `		rc = rc <= 0;` |
|  137648 |  958 | `	}else{` |
|    4505 |  959 | `		rc = rc < 0;` |
|       - |  960 | `	}` |
|  279339 |  961 | `	VmPopOperand(&pTos,1);` |
|  279339 |  962 | `	if( !pInstr->iP2 ){` |
|       - |  963 | `		/* Push comparison result without taking the jump */` |
|  279339 |  964 | `		PH7_MemObjRelease(pTos);` |
|  279339 |  965 | `		pTos->x.iVal = rc;` |
|       - |  966 | `		/* Invalidate any prior representation */` |
|  279339 |  967 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  139898 |  968 | `	}else{` |
|     ! 0 |  969 | `		if( rc ){` |
|       - |  970 | `			/* Jump to the desired location */` |
|     ! 0 |  971 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 |  972 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  973 | `		}` |
|       - |  974 | `	}` |
|  279339 |  975 | `	VM_EXIT_BREAK;` |
|     ! 0 |  976 | `	VM_EXIT_BREAK;` |
|  139898 |  977 | `}` |
|       - |  978 |  |
|       - |  979 | `/*` |
|       - |  980 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|       - |  981 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  982 | ` */` |
| 1018834 |  983 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  984 | `{` |
| 1018839 |  985 | `	ph7_value *pTos = pState->pTos;` |
| 1018839 |  986 | `	ph7_value *pStack = pState->pStack;` |
| 1018839 |  987 | `	VmInstr *aInstr = pState->aInstr;` |
| 1018839 |  988 | `	sxi32 pc = pState->pc;` |
|       - |  989 | `	sxi32 rc;` |
|  510365 |  990 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1018839 |  991 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  992 | `	/* Perform the comparison and act accordingly */` |
|       - |  993 | `#ifdef UNTRUST` |
|       - |  994 | `	if( pNos < pStack ){` |
|       - |  995 | `		VM_EXIT_ABORT;` |
|       - |  996 | `	}` |
|       - |  997 | `#endif` |
|       - |  998 | `	/* An unordered pair answers 1 here too, so both spellings are false for it` |
|       - |  999 | `	 * without a case of their own (see OP_GT/OP_GE above). */` |
| 1018839 | 1000 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 1018839 | 1001 | `	VM_CMP_REFUSAL_ROUTE()` |
| 1018835 | 1002 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|   94452 | 1003 | `		rc = rc < 1;` |
|   47445 | 1004 | `	}else{` |
|  924388 | 1005 | `		rc = rc < 0;` |
|       - | 1006 | `	}` |
| 1018835 | 1007 | `	VmPopOperand(&pTos,1);` |
| 1018835 | 1008 | `	if( !pInstr->iP2 ){` |
|       - | 1009 | `		/* Push comparison result without taking the jump */` |
| 1018835 | 1010 | `		PH7_MemObjRelease(pTos);` |
| 1018835 | 1011 | `		pTos->x.iVal = rc;` |
|       - | 1012 | `		/* Invalidate any prior representation */` |
| 1018835 | 1013 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  510368 | 1014 | `	}else{` |
|     ! 0 | 1015 | `		if( rc ){` |
|       - | 1016 | `			/* Jump to the desired location */` |
|     ! 0 | 1017 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1018 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1019 | `		}` |
|       - | 1020 | `	}` |
| 1018835 | 1021 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1022 | `	VM_EXIT_BREAK;` |
|  510370 | 1023 | `}` |
|       - | 1024 |  |
|       - | 1025 | `/*` |
|       - | 1026 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|       - | 1027 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1028 | ` */` |
| 1108325 | 1029 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1030 | `{` |
| 1108330 | 1031 | `	ph7_value *pTos = pState->pTos;` |
| 1108330 | 1032 | `	ph7_value *pStack = pState->pStack;` |
| 1108330 | 1033 | `	VmInstr *aInstr = pState->aInstr;` |
| 1108330 | 1034 | `	sxi32 pc = pState->pc;` |
|       - | 1035 | `	sxi32 rc;` |
|  554389 | 1036 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1108330 | 1037 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1038 | `	/* Perform the comparison and act accordingly */` |
|       - | 1039 | `#ifdef UNTRUST` |
|       - | 1040 | `	if( pNos < pStack ){` |
|       - | 1041 | `		VM_EXIT_ABORT;` |
|       - | 1042 | `	}` |
|       - | 1043 | `#endif` |
| 1108330 | 1044 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 1108330 | 1045 | `	rc = rc != 0;` |
| 1108330 | 1046 | `	VmPopOperand(&pTos,1);` |
| 1108330 | 1047 | `	if( !pInstr->iP2 ){` |
|       - | 1048 | `		/* Push comparison result without taking the jump */` |
| 1108330 | 1049 | `		PH7_MemObjRelease(pTos);` |
| 1108330 | 1050 | `		pTos->x.iVal = rc;` |
|       - | 1051 | `		/* Invalidate any prior representation */` |
| 1108330 | 1052 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  554394 | 1053 | `	}else{` |
|     ! 0 | 1054 | `		if( rc ){` |
|       - | 1055 | `			/* Jump to the desired location */` |
|     ! 0 | 1056 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1057 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1058 | `		}` |
|       - | 1059 | `	}` |
| 1108330 | 1060 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1061 | `	VM_EXIT_BREAK;` |
|       5 | 1062 | `}` |
|       - | 1063 |  |
|       - | 1064 | `/*` |
|       - | 1065 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|       - | 1066 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1067 | ` */` |
|  798217 | 1068 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1069 | `{` |
|  798222 | 1070 | `	ph7_value *pTos = pState->pTos;` |
|  798222 | 1071 | `	ph7_value *pStack = pState->pStack;` |
|  798222 | 1072 | `	VmInstr *aInstr = pState->aInstr;` |
|  798222 | 1073 | `	sxi32 pc = pState->pc;` |
|       - | 1074 | `	sxi32 rc;` |
|  399989 | 1075 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  798222 | 1076 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1077 | `	/* Perform the comparison and act accordingly */` |
|       - | 1078 | `#ifdef UNTRUST` |
|       - | 1079 | `	if( pNos < pStack ){` |
|       - | 1080 | `		VM_EXIT_ABORT;` |
|       - | 1081 | `	}` |
|       - | 1082 | `#endif` |
|       - | 1083 | ``	/* `NAN === NAN` is false in php, and the comparator says so: an unordered`` |
|       - | 1084 | `	 * pair is 1, never 0. */` |
|  798222 | 1085 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|  798222 | 1086 | `	rc = rc == 0;` |
|  798222 | 1087 | `	VmPopOperand(&pTos,1);` |
|  798222 | 1088 | `	if( !pInstr->iP2 ){` |
|       - | 1089 | `		/* Push comparison result without taking the jump */` |
|  798222 | 1090 | `		PH7_MemObjRelease(pTos);` |
|  798222 | 1091 | `		pTos->x.iVal = rc;` |
|       - | 1092 | `		/* Invalidate any prior representation */` |
|  798222 | 1093 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  399994 | 1094 | `	}else{` |
|     ! 0 | 1095 | `		if( rc ){` |
|       - | 1096 | `			/* Jump to the desired location */` |
|     ! 0 | 1097 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1098 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1099 | `		}` |
|       - | 1100 | `	}` |
|  798222 | 1101 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1102 | `	VM_EXIT_BREAK;` |
|       5 | 1103 | `}` |
|       - | 1104 |  |
|       - | 1105 | `/*` |
|       - | 1106 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|       - | 1107 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1108 | ` */` |
|   15396 | 1109 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1110 | `{` |
|   15401 | 1111 | `	ph7_value *pTos = pState->pTos;` |
|   15401 | 1112 | `	ph7_value *pStack = pState->pStack;` |
|   15401 | 1113 | `	VmInstr *aInstr = pState->aInstr;` |
|   15401 | 1114 | `	sxi32 pc = pState->pc;` |
|       - | 1115 | `	sxi32 rc;` |
|    7698 | 1116 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   15401 | 1117 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1118 | `	/* Perform the comparison and act accordingly */` |
|       - | 1119 | `#ifdef UNTRUST` |
|       - | 1120 | `	if( pNos < pStack ){` |
|       - | 1121 | `		VM_EXIT_ABORT;` |
|       - | 1122 | `	}` |
|       - | 1123 | `#endif` |
|   15401 | 1124 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   15405 | 1125 | `	VM_CMP_REFUSAL_ROUTE()` |
|   15381 | 1126 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|   13071 | 1127 | `		rc = rc == 0;` |
|    6538 | 1128 | `	}else{` |
|    2315 | 1129 | `		rc = rc != 0;` |
|       - | 1130 | `	}` |
|   15381 | 1131 | `	VmPopOperand(&pTos,1);` |
|   15381 | 1132 | `	if( !pInstr->iP2 ){` |
|       - | 1133 | `		/* Push comparison result without taking the jump */` |
|   15381 | 1134 | `		PH7_MemObjRelease(pTos);` |
|   15381 | 1135 | `		pTos->x.iVal = rc;` |
|       - | 1136 | `		/* Invalidate any prior representation */` |
|   15381 | 1137 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    7693 | 1138 | `	}else{` |
|     ! 0 | 1139 | `		if( rc ){` |
|       - | 1140 | `			/* Jump to the desired location */` |
|     ! 0 | 1141 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1142 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1143 | `		}` |
|       - | 1144 | `	}` |
|   15381 | 1145 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1146 | `	VM_EXIT_BREAK;` |
|    7703 | 1147 | `}` |
|       - | 1148 |  |
|       - | 1149 | `/*` |
|       - | 1150 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|       - | 1151 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1152 | ` */` |
|  355650 | 1153 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1154 | `{` |
|  355655 | 1155 | `	ph7_value *pTos = pState->pTos;` |
|  355655 | 1156 | `	ph7_value *pStack = pState->pStack;` |
|  355655 | 1157 | `	VmInstr *aInstr = pState->aInstr;` |
|  355655 | 1158 | `	sxi32 pc = pState->pc;` |
|       - | 1159 | `	sxi32 rc;` |
|  178044 | 1160 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  355655 | 1161 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1162 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|       - | 1163 | `#ifdef UNTRUST` |
|       - | 1164 | `	if( pNos < pStack ){` |
|       - | 1165 | `		VM_EXIT_ABORT;` |
|       - | 1166 | `	}` |
|       - | 1167 | `#endif` |
|       - | 1168 | `	/* Force a boolean cast */` |
|  355655 | 1169 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       8 | 1170 | `		PH7_MemObjToBool(pTos);` |
|       3 | 1171 | `	}` |
|  355655 | 1172 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 | 1173 | `		PH7_MemObjToBool(pNos);` |
|     ! 0 | 1174 | `	}` |
|  355655 | 1175 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|  355655 | 1176 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|  355655 | 1177 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|       - | 1178 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   67820 | 1179 | `		v1 = and_logic[v1*3+v2];` |
|   33916 | 1180 | `	}else{` |
|       - | 1181 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|  287840 | 1182 | `		v1 = or_logic[v1*3+v2];` |
|       - | 1183 | `	}` |
|  355655 | 1184 | `	if( v1 == 2 ){` |
|     ! 0 | 1185 | `		v1 = 1;` |
|     ! 0 | 1186 | `	}` |
|  355655 | 1187 | `	VmPopOperand(&pTos,1);` |
|  355655 | 1188 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|  355655 | 1189 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  355655 | 1190 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1191 | `	VM_EXIT_BREAK;` |
|       5 | 1192 | `}` |
|       - | 1193 |  |
|       - | 1194 | `/*` |
|       - | 1195 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|       - | 1196 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1197 | ` */` |
|     824 | 1198 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 | 1199 | `{` |
|     825 | 1200 | `	ph7_value *pTos = pState->pTos;` |
|     825 | 1201 | `	ph7_value *pStack = pState->pStack;` |
|     825 | 1202 | `	VmInstr *aInstr = pState->aInstr;` |
|     825 | 1203 | `	sxi32 pc = pState->pc;` |
|       - | 1204 | `	sxi32 rc;` |
|     412 | 1205 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     825 | 1206 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1207 | `	ph7_value *pObj;` |
|       - | 1208 | `	sxi64 a,r;` |
|       - | 1209 | `#ifdef UNTRUST` |
|       - | 1210 | `	if( pNos < pStack ){` |
|       - | 1211 | `		VM_EXIT_ABORT;` |
|       - | 1212 | `	}` |
|       - | 1213 | `#endif` |
|       - | 1214 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|       - | 1215 | `	 * this handler, beside the other eleven compound stores.) */` |
|     978 | 1216 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|       - | 1217 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     519 | 1218 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     519 | 1219 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1220 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     519 | 1221 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1222 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     223 | 1223 | `		PH7_MemObjToInteger(pTos);` |
|     111 | 1224 | `	}` |
|     519 | 1225 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     221 | 1226 | `		PH7_MemObjToInteger(pNos);` |
|     110 | 1227 | `	}` |
|       - | 1228 | `	/* Perform the requested operation */` |
|     519 | 1229 | `	a = pTos->x.iVal;` |
|     521 | 1230 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|       - | 1231 | `	/* Push the result */` |
|     471 | 1232 | `	pNos->x.iVal = r;` |
|     471 | 1233 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     471 | 1234 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1235 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     470 | 1236 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     469 | 1237 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     469 | 1238 | `		PH7_MemObjStore(pNos,pObj);` |
|     234 | 1239 | `	}` |
|     471 | 1240 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     471 | 1241 | `	VmPopOperand(&pTos,1);` |
|     471 | 1242 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1243 | `	VM_EXIT_BREAK;` |
|     413 | 1244 | `}` |
|       - | 1245 |  |
|       - | 1246 | `/*` |
|       - | 1247 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|       - | 1248 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1249 | ` */` |
|     930 | 1250 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 | 1251 | `{` |
|     933 | 1252 | `	ph7_value *pTos = pState->pTos;` |
|     933 | 1253 | `	ph7_value *pStack = pState->pStack;` |
|     933 | 1254 | `	VmInstr *aInstr = pState->aInstr;` |
|     933 | 1255 | `	sxi32 pc = pState->pc;` |
|       - | 1256 | `	sxi32 rc;` |
|     465 | 1257 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     933 | 1258 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1259 | `	sxi64 a,r;` |
|       - | 1260 | `#ifdef UNTRUST` |
|       - | 1261 | `	if( pNos < pStack ){` |
|       - | 1262 | `		VM_EXIT_ABORT;` |
|       - | 1263 | `	}` |
|       - | 1264 | `#endif` |
|    1100 | 1265 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|       - | 1266 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     599 | 1267 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     599 | 1268 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     594 | 1269 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     594 | 1270 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     594 | 1271 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1272 | `		PH7_MemObjToInteger(pTos);` |
|     118 | 1273 | `	}` |
|     594 | 1274 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1275 | `		PH7_MemObjToInteger(pNos);` |
|     118 | 1276 | `	}` |
|       - | 1277 | `	/* Perform the requested operation */` |
|     594 | 1278 | `	a = pNos->x.iVal;` |
|     594 | 1279 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|       - | 1280 | `	/* Push the result */` |
|     540 | 1281 | `	pNos->x.iVal = r;` |
|     540 | 1282 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     540 | 1283 | `	VmPopOperand(&pTos,1);` |
|     540 | 1284 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1285 | `	VM_EXIT_BREAK;` |
|     468 | 1286 | `}` |
|       - | 1287 |  |
|       - | 1288 | `/*` |
|       - | 1289 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|       - | 1290 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1291 | ` */` |
|    4888 | 1292 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1293 | `{` |
|       - | 1294 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - | 1295 | `	int rcNa;` |
|    4893 | 1296 | `	const char *zArCls = "TypeError";` |
|    4893 | 1297 | `	ph7_value *pTos = pState->pTos;` |
|    4893 | 1298 | `	ph7_value *pStack = pState->pStack;` |
|    4893 | 1299 | `	VmInstr *aInstr = pState->aInstr;` |
|    4893 | 1300 | `	sxi32 pc = pState->pc;` |
|       - | 1301 | `	sxi32 rc;` |
|    2444 | 1302 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    4893 | 1303 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1304 | `	{` |
|       - | 1305 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - | 1306 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - | 1307 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - | 1308 | `		 * MULTIPLICATION is commutative and the RESULT does not care which operand is` |
|       - | 1309 | `		 * which, but the MESSAGE does: it names them in SOURCE order, and a compound` |
|       - | 1310 | ``		 * assign puts its lvalue on the TOP of the stack (`$x *= [1]` is `int * array`,`` |
|       - | 1311 | ``		 * the way `$x * [1]` is). */`` |
|    4893 | 1312 | `		ph7_value *pMulL = (pInstr->iOp == PH7_OP_MUL_STORE) ? pTos : pNos;` |
|    4893 | 1313 | `		ph7_value *pMulR = (pInstr->iOp == PH7_OP_MUL_STORE) ? pNos : pTos;` |
|       - | 1314 | `		SyBlob sArMsg;` |
|    4893 | 1315 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    4893 | 1316 | `		rcNa = VmArithOperandStep(&(*pVm),pMulL,pMulR,"*",pNos,&zArCls,&sArMsg);` |
|    4893 | 1317 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - | 1318 | `			sxi32 rcAr;` |
|     316 | 1319 | `			VmPopOperand(&pTos,1);` |
|     316 | 1320 | `			PH7_MemObjRelease(pTos);` |
|     316 | 1321 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     316 | 1322 | `			pTos->nIdx = SXU32_HIGH;` |
|     473 | 1323 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     157 | 1324 | `				SyBlobLength(&sArMsg));` |
|     316 | 1325 | `			SyBlobRelease(&sArMsg);` |
|     473 | 1326 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     316 | 1327 | `			rc = rcAr;` |
|     320 | 1328 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1329 | `		}` |
|    4579 | 1330 | `		SyBlobRelease(&sArMsg);` |
|       - | 1331 | `	}` |
|    4579 | 1332 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 | 1333 | `		goto mul_store_write;` |
|       - | 1334 | `	}` |
|       - | 1335 | `	/* Force the operand to be numeric */` |
|       - | 1336 | `#ifdef UNTRUST` |
|       - | 1337 | `	if( pNos < pStack ){` |
|       - | 1338 | `		VM_EXIT_ABORT;` |
|       - | 1339 | `	}` |
|       - | 1340 | `#endif` |
|    4575 | 1341 | `	PH7_MemObjToNumeric(pTos);` |
|    4575 | 1342 | `	PH7_MemObjToNumeric(pNos);` |
|       - | 1343 | `	/* Perform the requested operation */` |
|    6860 | 1344 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - | 1345 | `		/* Floating point arithemic */` |
|       - | 1346 | `		ph7_real a,b,r;` |
|      33 | 1347 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      23 | 1348 | `			PH7_MemObjToReal(pTos);` |
|      11 | 1349 | `		}` |
|      33 | 1350 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       5 | 1351 | `			PH7_MemObjToReal(pNos);` |
|       2 | 1352 | `		}` |
|      33 | 1353 | `		a = pNos->rVal;` |
|      33 | 1354 | `		b = pTos->rVal;` |
|      33 | 1355 | `		r = a * b;` |
|       - | 1356 | `		/* Push the result */` |
|      33 | 1357 | `		pNos->rVal = r;` |
|      33 | 1358 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1359 | `		/* Try to get an integer representation */` |
|      33 | 1360 | `		PH7_MemObjTryInteger(pNos);` |
|      17 | 1361 | `	}else{` |
|       - | 1362 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|       - | 1363 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - | 1364 | `		sxi64 a,b,r;` |
|    4543 | 1365 | `		a = pNos->x.iVal;` |
|    4543 | 1366 | `		b = pTos->x.iVal;` |
|    4543 | 1367 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|       - | 1368 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      57 | 1369 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|      57 | 1370 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1371 | `#else` |
|       - | 1372 | `			pNos->x.iVal = r;` |
|       - | 1373 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1374 | `#endif` |
|      29 | 1375 | `		}else{` |
|    4487 | 1376 | `			pNos->x.iVal = r;` |
|    4487 | 1377 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1378 | `		}` |
|       - | 1379 | `	}` |
|    2287 | 1380 | `mul_store_write:` |
|    4579 | 1381 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|       - | 1382 | `		ph7_value *pObj;` |
|     309 | 1383 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1384 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     308 | 1385 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     307 | 1386 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     307 | 1387 | `			PH7_MemObjStore(pNos,pObj);` |
|     152 | 1388 | `		}` |
|     153 | 1389 | `	}` |
|    4579 | 1390 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    4579 | 1391 | `	VmPopOperand(&pTos,1);` |
|    4579 | 1392 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1393 | `	VM_EXIT_BREAK;` |
|    2449 | 1394 | `}` |
|       - | 1395 |  |
