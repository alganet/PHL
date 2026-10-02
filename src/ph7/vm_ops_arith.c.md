# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 732/808 lines (90.59%)

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
|       - |   32 | ` *` |
|       - |   33 | `` * The STRICT pair (`===`, `!==`) needs it too, though no compare handler is ever`` |
|       - |   34 | ` * asked there: the recursion refusal a cyclic array or object records comes from` |
|       - |   35 | `` * the walk itself, and php throws out of `$a === $b` exactly as it does out of`` |
|       - |   36 | `` * `$a == $b`. Without the route the record simply stayed pending and fired at the`` |
|       - |   37 | ` * next host call, attributing the throw to an unrelated builtin.` |
|       - |   38 | ` */` |
|       - |   39 | `#define VM_CMP_REFUSAL_ROUTE() \` |
|       - |   40 | `	if( PH7_CmpRefusalPending(pVm) ){ \` |
|       - |   41 | `		sxi32 _rcCmp; \` |
|       - |   42 | `		VmPopOperand(&pTos,1); \` |
|       - |   43 | `		PH7_MemObjRelease(pTos); \` |
|       - |   44 | `		MemObjSetType(pTos,MEMOBJ_NULL); \` |
|       - |   45 | `		pTos->nIdx = SXU32_HIGH; \` |
|       - |   46 | `		_rcCmp = PH7_CmpRefusalRaise(pVm); \` |
|       - |   47 | `		if( _rcCmp == SXERR_ABORT ){ VM_EXIT_ABORT; } \` |
|       - |   48 | `		PH7_THROW_ROUTE_MIDEXPR(_rcCmp) \` |
|       - |   49 | `	}` |
|       - |   50 |  |
|       - |   51 | `/*` |
|       - |   52 | ` * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of` |
|       - |   53 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   54 | ` */` |
|     118 |   55 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |   56 | `{` |
|     121 |   57 | `	ph7_value *pTos = pState->pTos;` |
|     121 |   58 | `	ph7_value *pStack = pState->pStack;` |
|     121 |   59 | `	VmInstr *aInstr = pState->aInstr;` |
|     121 |   60 | `	sxi32 pc = pState->pc;` |
|       - |   61 | `	sxi32 rc;` |
|      59 |   62 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     121 |   63 | `	ph7_value *pNos = &pTos[-1];` |
|       - |   64 | `	ph7_value *pObj;` |
|       - |   65 | `	sxu32 nIdx;` |
|       - |   66 | `#ifdef UNTRUST` |
|       - |   67 | `	if( pNos < pStack ){` |
|       - |   68 | `		VM_EXIT_ABORT;` |
|       - |   69 | `	}` |
|       - |   70 | `#endif` |
|     121 |   71 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|       - |   72 | ``		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a`` |
|       - |   73 | `		 * COAL entry targeting exactly THIS store (owner + pc identity — a` |
|       - |   74 | `		 * stale entry from an abandoned statement can never match, and nested` |
|       - |   75 | `		 * arms in the RHS were consumed/dropped above this one). Dispatch the` |
|       - |   76 | `		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=` |
|       - |   77 | `		 * residual: pre-fix the assign bypassed __set through the normal slot` |
|       - |   78 | `		 * path). The RHS stays as the expression result. */` |
|      15 |   79 | `		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      14 |   80 | `		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr` |
|      14 |   81 | `		 && pTop->nPc == (sxu32)pc` |
|      15 |   82 | `		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK \|\| pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){` |
|      15 |   83 | `			VmHookRmw sPend = *pTop;` |
|      15 |   84 | `			(void)SySetPop(&pVm->aHookRmw);` |
|      15 |   85 | `			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){` |
|      11 |   86 | `				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);` |
|      11 |   87 | `				if( rcHs == PH7_ABORT ){` |
|     ! 0 |   88 | `					SyBlobRelease(&sPend.sName);` |
|     ! 0 |   89 | `					PH7_ClassInstanceUnref(sPend.pThis);` |
|     ! 0 |   90 | `					VM_EXIT_ABORT;` |
|       - |   91 | `				}` |
|       6 |   92 | `			}else{` |
|       - |   93 | `				SyString sSetName;` |
|       5 |   94 | `				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));` |
|       5 |   95 | `				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);` |
|       - |   96 | `			}` |
|      15 |   97 | `			SyBlobRelease(&sPend.sName);` |
|      15 |   98 | `			PH7_ClassInstanceUnref(sPend.pThis);` |
|      15 |   99 | `			PH7_MemObjStore(pTos,pNos);` |
|      15 |  100 | `			pNos->nIdx = SXU32_HIGH;` |
|      15 |  101 | `			VmPopOperand(&pTos,1);` |
|      15 |  102 | `			VM_EXIT_BREAK;` |
|       - |  103 | `		}` |
|     ! 0 |  104 | `	}` |
|       - |  105 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|       - |  106 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|       - |  107 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|     107 |  108 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|      10 |  109 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|      10 |  110 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  111 | `			"offsetSet",sizeof("offsetSet")-1);` |
|       - |  112 | `		ph7_value *apArg[2];` |
|      10 |  113 | `		apArg[0] = &pVm->sCoalesceKey;` |
|      10 |  114 | `		apArg[1] = pTos;` |
|      10 |  115 | `		if( pSet == 0 ){` |
|       - |  116 | `			/* A container that answered the READ and has no ArrayAccess: its own` |
|       - |  117 | `			 * dimension handler is offered the store first -- php's` |
|       - |  118 | `			 * SimpleXMLElement takes it. One that stores nothing (DOMNodeList,` |
|       - |  119 | `			 * PDORow) leaves it, and the store is then the same one` |
|       - |  120 | ``			 * `$list[9] = 'x'` performs, so it takes the same Error. */`` |
|       - |  121 | `			char zMsg[256];` |
|       - |  122 | `			sxu32 nMsg;` |
|       - |  123 | `			PH7_NativeDimCtx sDim;` |
|       3 |  124 | `			if( PH7_ClassNativeDimStore(pInst,PH7_NATIVE_DIM_WRITE,` |
|       3 |  125 | `				&pVm->sCoalesceKey,pTos,&sDim) && sDim.zThrowClass == 0 ){` |
|     ! 0 |  126 | `				PH7_MemObjStore(pTos,pNos);` |
|     ! 0 |  127 | `				VmPopOperand(&pTos,1);` |
|     ! 0 |  128 | `				VmCoalesceDisarm(pVm);` |
|     ! 0 |  129 | `				VM_EXIT_BREAK;` |
|       - |  130 | `			}` |
|       3 |  131 | `			if( sDim.zThrowClass ){` |
|     ! 0 |  132 | `				rc = VmThrowFromVm(&(*pVm),sDim.zThrowClass,sDim.zThrowMsg,` |
|     ! 0 |  133 | `					(sxu32)SyStrlen(sDim.zThrowMsg));` |
|     ! 0 |  134 | `				VmPopOperand(&pTos,1);` |
|     ! 0 |  135 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  136 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 |  137 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 |  138 | `				VmCoalesceDisarm(pVm);` |
|     ! 0 |  139 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 |  140 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  141 | `			}` |
|       4 |  142 | `			nMsg = PH7_ClassNativeDimRefusal(pInst,PH7_NATIVE_DIM_WRITE,` |
|       1 |  143 | `				zMsg,sizeof(zMsg));` |
|       3 |  144 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|       3 |  145 | `			VmPopOperand(&pTos,1);` |
|       3 |  146 | `			PH7_MemObjRelease(pTos);` |
|       3 |  147 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  148 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  149 | `			VmCoalesceDisarm(pVm);` |
|       3 |  150 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  151 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  152 | `		}` |
|       8 |  153 | `		PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|       - |  154 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|       8 |  155 | `		PH7_MemObjStore(pTos,pNos);` |
|       8 |  156 | `		VmPopOperand(&pTos,1);` |
|       - |  157 | `		/* Disarm and release the cached instance ref + key. */` |
|       8 |  158 | `		VmCoalesceDisarm(pVm);` |
|       8 |  159 | `		VM_EXIT_BREAK;` |
|       - |  160 | `	}` |
|      98 |  161 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|       - |  162 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|       - |  163 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|       - |  164 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|       - |  165 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|       - |  166 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|       - |  167 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|       - |  168 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|       - |  169 | `		 * offset the quiet peek let through raises at the store. */` |
|      43 |  170 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|      64 |  171 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|      42 |  172 | `			? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|      43 |  173 | `		sxi64 iOfft = 0;` |
|       - |  174 | `		SyBlob sTypeMsg;` |
|       - |  175 | `		int eOfft;` |
|      43 |  176 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|      43 |  177 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|       - |  178 | `			&iOfft,&sTypeMsg);` |
|      43 |  179 | `		if( eOfft == VM_STROFF_REJECT ){` |
|       - |  180 | `			sxi32 rcSo;` |
|       3 |  181 | `			VmPopOperand(&pTos,1);` |
|       3 |  182 | `			PH7_MemObjRelease(pTos);` |
|       3 |  183 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  184 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  185 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|       3 |  186 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  187 | `			rc = rcSo;` |
|       3 |  188 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  189 | `		}` |
|      41 |  190 | `		SyBlobRelease(&sTypeMsg);` |
|       - |  191 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|       - |  192 | `		{` |
|      41 |  193 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|      41 |  194 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       - |  195 | `		}` |
|      41 |  196 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|      41 |  197 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|       - |  198 | `				sxi32 rcEm;` |
|       5 |  199 | `				VmPopOperand(&pTos,1);` |
|       5 |  200 | `				PH7_MemObjRelease(pTos);` |
|       5 |  201 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 |  202 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 |  203 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  204 | `					"Cannot assign an empty string to a string offset",` |
|       - |  205 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|       5 |  206 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  207 | `				rc = rcEm;` |
|       5 |  208 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  209 | `			}` |
|      18 |  210 | `		}` |
|       - |  211 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|       - |  212 | `		 * not free what the carrier owns, so release it here — every other exit` |
|       - |  213 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|      37 |  214 | `		VmFreeCoalStrOff(pCoalOff);` |
|      37 |  215 | `		pNos->x.pOther = 0;` |
|      37 |  216 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|       - |  217 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|      37 |  218 | `		PH7_MemObjStore(pTos,pNos);` |
|      37 |  219 | `		pNos->nIdx = SXU32_HIGH;` |
|      37 |  220 | `		VmPopOperand(&pTos,1);` |
|      37 |  221 | `		VM_EXIT_BREAK;` |
|       - |  222 | `	}` |
|      56 |  223 | `	nIdx = pNos->nIdx;` |
|      56 |  224 | `	if( nIdx == SXU32_HIGH ){` |
|       - |  225 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] .= "x"`, `mk()->p += 1`):`` |
|       - |  226 | `		 * php computes it, drops it with the temporary and stays silent. Every case` |
|       - |  227 | `		 * that IS a refusal — a class constant, a hooked or handler-backed property —` |
|       - |  228 | `		 * is decided before the VM sees it. */` |
|      56 |  229 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      56 |  230 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      54 |  231 | `		PH7_MemObjStore(pTos,pObj);` |
|      26 |  232 | `	}` |
|      54 |  233 | `	PH7_MemObjStore(pTos,pNos);` |
|      54 |  234 | `	VmPopOperand(&pTos,1);` |
|      54 |  235 | `	VM_EXIT_BREAK;` |
|     ! 0 |  236 | `	VM_EXIT_BREAK;` |
|      62 |  237 | `}` |
|       - |  238 |  |
|       - |  239 | `/*` |
|       - |  240 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|       - |  241 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  242 | ` */` |
|     960 |  243 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |  244 | `{` |
|       - |  245 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  246 | `	int rcNa;` |
|     963 |  247 | `	const char *zArCls = "TypeError";` |
|     963 |  248 | `	ph7_value *pTos = pState->pTos;` |
|     963 |  249 | `	ph7_value *pStack = pState->pStack;` |
|     963 |  250 | `	VmInstr *aInstr = pState->aInstr;` |
|     963 |  251 | `	sxi32 pc = pState->pc;` |
|       - |  252 | `	sxi32 rc;` |
|     480 |  253 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     963 |  254 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  255 | `	{` |
|       - |  256 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  257 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  258 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  259 | `		SyBlob sArMsg;` |
|     963 |  260 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     963 |  261 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"/",pNos,&zArCls,&sArMsg);` |
|     963 |  262 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  263 | `			sxi32 rcAr;` |
|     161 |  264 | `			VmPopOperand(&pTos,1);` |
|     161 |  265 | `			PH7_MemObjRelease(pTos);` |
|     161 |  266 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     161 |  267 | `			pTos->nIdx = SXU32_HIGH;` |
|     241 |  268 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  269 | `				SyBlobLength(&sArMsg));` |
|     161 |  270 | `			SyBlobRelease(&sArMsg);` |
|     241 |  271 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     161 |  272 | `			rc = rcAr;` |
|     161 |  273 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  274 | `		}` |
|     803 |  275 | `		SyBlobRelease(&sArMsg);` |
|       - |  276 | `	}` |
|     803 |  277 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  278 | `		VmPopOperand(&pTos,1);` |
|       3 |  279 | `		VM_EXIT_BREAK;` |
|       - |  280 | `	}` |
|       - |  281 | `	ph7_real a,b,r;` |
|       - |  282 | `#ifdef UNTRUST` |
|       - |  283 | `	if( pNos < pStack ){` |
|       - |  284 | `		VM_EXIT_ABORT;` |
|       - |  285 | `	}` |
|       - |  286 | `#endif` |
|       - |  287 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|       - |  288 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|       - |  289 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|       - |  290 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|       - |  291 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|     801 |  292 | `	PH7_MemObjToNumeric(pTos);` |
|     801 |  293 | `	PH7_MemObjToNumeric(pNos);` |
|     801 |  294 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     731 |  295 | `		sxi64 ia = pNos->x.iVal;` |
|     731 |  296 | `		sxi64 ib = pTos->x.iVal;` |
|     731 |  297 | `		sxi64 iQuot = 0;` |
|     731 |  298 | `		int bExact = 0;` |
|     731 |  299 | `		if( ib == 0 ){` |
|      74 |  300 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      76 |  301 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     658 |  302 | `		}else if( ib == -1 ){` |
|       - |  303 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|       - |  304 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|       - |  305 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|       - |  306 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|       - |  307 | `			 * alongside the remainder, so testing the remainder first trapped` |
|       - |  308 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|       - |  309 | `			 * OP_MOD screens the same hazard the same way. */` |
|       - |  310 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  311 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|       - |  312 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|       - |  313 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|       - |  314 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|       - |  315 | `			bExact = 1;` |
|       - |  316 | `#else` |
|      19 |  317 | `			if( ia != SMALLEST_INT64 ){` |
|      15 |  318 | `				iQuot = -ia;` |
|      15 |  319 | `				bExact = 1;` |
|       8 |  320 | `			}` |
|       - |  321 | `#endif` |
|     649 |  322 | `		}else if( ia % ib == 0 ){` |
|     130 |  323 | `			iQuot = ia / ib;` |
|     130 |  324 | `			bExact = 1;` |
|      64 |  325 | `		}` |
|     658 |  326 | `		if( bExact ){` |
|     144 |  327 | `			pNos->x.iVal = iQuot;` |
|     144 |  328 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     144 |  329 | `			VmPopOperand(&pTos,1);` |
|     144 |  330 | `			VM_EXIT_BREAK;` |
|       - |  331 | `		}` |
|     257 |  332 | `	}` |
|       - |  333 | `	/* Force the operands to be real */` |
|     586 |  334 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     567 |  335 | `		PH7_MemObjToReal(pTos);` |
|     283 |  336 | `	}` |
|     586 |  337 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     515 |  338 | `		PH7_MemObjToReal(pNos);` |
|     257 |  339 | `	}` |
|       - |  340 | `	/* Perform the requested operation */` |
|     586 |  341 | `	a = pNos->rVal;` |
|     586 |  342 | `	b = pTos->rVal;` |
|     586 |  343 | `	if( b == 0 ){` |
|       - |  344 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  345 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|       3 |  346 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       3 |  347 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  348 | `	}else{` |
|     584 |  349 | `		r = a/b;` |
|       - |  350 | `		/* Push the result */` |
|     584 |  351 | `		pNos->rVal = r;` |
|     584 |  352 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  353 | `	}` |
|     584 |  354 | `	VmPopOperand(&pTos,1);` |
|     584 |  355 | `	VM_EXIT_BREAK;` |
|     ! 0 |  356 | `	VM_EXIT_BREAK;` |
|     483 |  357 | `}` |
|       - |  358 |  |
|       - |  359 | `/*` |
|       - |  360 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|       - |  361 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  362 | ` */` |
|     406 |  363 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 |  364 | `{` |
|       - |  365 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  366 | `	int rcNa;` |
|     407 |  367 | `	const char *zArCls = "TypeError";` |
|     407 |  368 | `	ph7_value *pTos = pState->pTos;` |
|     407 |  369 | `	ph7_value *pStack = pState->pStack;` |
|     407 |  370 | `	VmInstr *aInstr = pState->aInstr;` |
|     407 |  371 | `	sxi32 pc = pState->pc;` |
|       - |  372 | `	sxi32 rc;` |
|     203 |  373 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     407 |  374 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  375 | `	ph7_value *pObj;` |
|       - |  376 | `	sxi64 a,b,r;` |
|       - |  377 | `#ifdef UNTRUST` |
|       - |  378 | `	if( pNos < pStack ){` |
|       - |  379 | `		VM_EXIT_ABORT;` |
|       - |  380 | `	}` |
|       - |  381 | `#endif` |
|       - |  382 | `	{` |
|       - |  383 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  384 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  385 | `		SyBlob sArMsg;` |
|     407 |  386 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     407 |  387 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"%",pNos,&zArCls,&sArMsg);` |
|     407 |  388 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  389 | `			sxi32 rcAr;` |
|     151 |  390 | `			VmPopOperand(&pTos,1);` |
|     151 |  391 | `			PH7_MemObjRelease(pTos);` |
|     151 |  392 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     151 |  393 | `			pTos->nIdx = SXU32_HIGH;` |
|     226 |  394 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      75 |  395 | `				SyBlobLength(&sArMsg));` |
|     151 |  396 | `			SyBlobRelease(&sArMsg);` |
|     226 |  397 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     151 |  398 | `			rc = rcAr;` |
|     151 |  399 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  400 | `		}` |
|     257 |  401 | `		SyBlobRelease(&sArMsg);` |
|       - |  402 | `	}` |
|     257 |  403 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  404 | `		goto mod_store_write;` |
|       - |  405 | `	}` |
|       - |  406 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     257 |  407 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     257 |  408 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  409 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     257 |  410 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  411 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     113 |  412 | `		PH7_MemObjToInteger(pTos);` |
|      56 |  413 | `	}` |
|     257 |  414 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     111 |  415 | `		PH7_MemObjToInteger(pNos);` |
|      55 |  416 | `	}` |
|       - |  417 | `	/* Perform the requested operation */` |
|     257 |  418 | `	a = pTos->x.iVal;` |
|     257 |  419 | `	b = pNos->x.iVal;` |
|     257 |  420 | `	if( b == 0 ){` |
|       - |  421 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  422 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      71 |  423 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      71 |  424 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  425 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|     187 |  426 | `	}else if( b == -1 ){` |
|       - |  427 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|       - |  428 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|       3 |  429 | `		r = 0;` |
|       2 |  430 | `	}else{` |
|     185 |  431 | `		r = a%b;` |
|       - |  432 | `	}` |
|       - |  433 | `	/* Push the result */` |
|     187 |  434 | `	pNos->x.iVal = r;` |
|     187 |  435 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      93 |  436 | `mod_store_write:` |
|     187 |  437 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  438 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     186 |  439 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     185 |  440 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     185 |  441 | `		PH7_MemObjStore(pNos,pObj);` |
|      92 |  442 | `	}` |
|     187 |  443 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     187 |  444 | `	VmPopOperand(&pTos,1);` |
|     187 |  445 | `	VM_EXIT_BREAK;` |
|     ! 0 |  446 | `	VM_EXIT_BREAK;` |
|     204 |  447 | `}` |
|       - |  448 |  |
|       - |  449 | `/*` |
|       - |  450 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|       - |  451 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  452 | ` */` |
|    2688 |  453 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  454 | `{` |
|       - |  455 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  456 | `	int rcNa;` |
|    2693 |  457 | `	const char *zArCls = "TypeError";` |
|    2693 |  458 | `	ph7_value *pTos = pState->pTos;` |
|    2693 |  459 | `	ph7_value *pStack = pState->pStack;` |
|    2693 |  460 | `	VmInstr *aInstr = pState->aInstr;` |
|    2693 |  461 | `	sxi32 pc = pState->pc;` |
|       - |  462 | `	sxi32 rc;` |
|    1344 |  463 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    2693 |  464 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  465 | `	{` |
|       - |  466 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  467 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  468 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  469 | `		SyBlob sArMsg;` |
|    2693 |  470 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    2693 |  471 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"%",pNos,&zArCls,&sArMsg);` |
|    2693 |  472 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  473 | `			sxi32 rcAr;` |
|     163 |  474 | `			VmPopOperand(&pTos,1);` |
|     163 |  475 | `			PH7_MemObjRelease(pTos);` |
|     163 |  476 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     163 |  477 | `			pTos->nIdx = SXU32_HIGH;` |
|     244 |  478 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      81 |  479 | `				SyBlobLength(&sArMsg));` |
|     163 |  480 | `			SyBlobRelease(&sArMsg);` |
|     244 |  481 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     163 |  482 | `			rc = rcAr;` |
|     163 |  483 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  484 | `		}` |
|    2531 |  485 | `		SyBlobRelease(&sArMsg);` |
|       - |  486 | `	}` |
|    2531 |  487 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  488 | `		VmPopOperand(&pTos,1);` |
|       3 |  489 | `		VM_EXIT_BREAK;` |
|       - |  490 | `	}` |
|       - |  491 | `	sxi64 a,b,r;` |
|       - |  492 | `#ifdef UNTRUST` |
|       - |  493 | `	if( pNos < pStack ){` |
|       - |  494 | `		VM_EXIT_ABORT;` |
|       - |  495 | `	}` |
|       - |  496 | `#endif` |
|       - |  497 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    2529 |  498 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    2529 |  499 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    2515 |  500 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    2515 |  501 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    2513 |  502 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     117 |  503 | `		PH7_MemObjToInteger(pTos);` |
|      58 |  504 | `	}` |
|    2513 |  505 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     122 |  506 | `		PH7_MemObjToInteger(pNos);` |
|      60 |  507 | `	}` |
|       - |  508 | `	/* Perform the requested operation */` |
|    2513 |  509 | `	a = pNos->x.iVal;` |
|    2513 |  510 | `	b = pTos->x.iVal;` |
|    2513 |  511 | `	if( b == 0 ){` |
|       - |  512 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  513 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      76 |  514 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      80 |  515 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  516 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|    2439 |  517 | `	}else if( b == -1 ){` |
|       - |  518 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|       - |  519 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|       - |  520 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|       - |  521 | `		 * remainder. php's result here is 0. */` |
|       5 |  522 | `		r = 0;` |
|       3 |  523 | `	}else{` |
|    2435 |  524 | `		r = a%b;` |
|       - |  525 | `	}` |
|       - |  526 | `	/* Push the result */` |
|    2439 |  527 | `	pNos->x.iVal = r;` |
|    2439 |  528 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    2439 |  529 | `	VmPopOperand(&pTos,1);` |
|    2439 |  530 | `	VM_EXIT_BREAK;` |
|     ! 0 |  531 | `	VM_EXIT_BREAK;` |
|    1349 |  532 | `}` |
|       - |  533 |  |
|       - |  534 | `/*` |
|       - |  535 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|       - |  536 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  537 | ` */` |
|     408 |  538 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  539 | `{` |
|       - |  540 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  541 | `	int rcNa;` |
|     410 |  542 | `	const char *zArCls = "TypeError";` |
|     410 |  543 | `	ph7_value *pTos = pState->pTos;` |
|     410 |  544 | `	ph7_value *pStack = pState->pStack;` |
|     410 |  545 | `	VmInstr *aInstr = pState->aInstr;` |
|     410 |  546 | `	sxi32 pc = pState->pc;` |
|       - |  547 | `	sxi32 rc;` |
|     204 |  548 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     410 |  549 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  550 | `	ph7_value *pObj;` |
|       - |  551 | `#ifdef UNTRUST` |
|       - |  552 | `	if( pNos < pStack ){` |
|       - |  553 | `		VM_EXIT_ABORT;` |
|       - |  554 | `	}` |
|       - |  555 | `#endif` |
|       - |  556 | `	{` |
|       - |  557 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  558 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  559 | `		SyBlob sArMsg;` |
|     410 |  560 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     410 |  561 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"-",pNos,&zArCls,&sArMsg);` |
|     410 |  562 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  563 | `			sxi32 rcAr;` |
|     153 |  564 | `			VmPopOperand(&pTos,1);` |
|     153 |  565 | `			PH7_MemObjRelease(pTos);` |
|     153 |  566 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     153 |  567 | `			pTos->nIdx = SXU32_HIGH;` |
|     229 |  568 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      76 |  569 | `				SyBlobLength(&sArMsg));` |
|     153 |  570 | `			SyBlobRelease(&sArMsg);` |
|     229 |  571 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     153 |  572 | `			rc = rcAr;` |
|     153 |  573 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  574 | `		}` |
|     258 |  575 | `		SyBlobRelease(&sArMsg);` |
|       - |  576 | `	}` |
|     258 |  577 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  578 | `		goto sub_store_write;` |
|       - |  579 | `	}` |
|       - |  580 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     258 |  581 | `	PH7_MemObjToNumeric(pTos);` |
|     258 |  582 | `	PH7_MemObjToNumeric(pNos);` |
|     386 |  583 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  584 | `		/* Floating point arithemic */` |
|       - |  585 | `		ph7_real a,b,r;` |
|     ! 0 |  586 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  587 | `			PH7_MemObjToReal(pTos);` |
|     ! 0 |  588 | `		}` |
|     ! 0 |  589 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  590 | `			PH7_MemObjToReal(pNos);` |
|     ! 0 |  591 | `		}` |
|     ! 0 |  592 | `		a = pTos->rVal;` |
|     ! 0 |  593 | `		b = pNos->rVal;` |
|     ! 0 |  594 | `		r = a - b;` |
|       - |  595 | `		/* Push the result */` |
|     ! 0 |  596 | `		pNos->rVal = r;` |
|     ! 0 |  597 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  598 | `		/* Try to get an integer representation */` |
|     ! 0 |  599 | `		PH7_MemObjTryInteger(pNos);` |
|     ! 0 |  600 | `	}else{` |
|       - |  601 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  602 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  603 | `		sxi64 a,b,r;` |
|     258 |  604 | `		a = pTos->x.iVal;` |
|     258 |  605 | `		b = pNos->x.iVal;` |
|     258 |  606 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  607 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       7 |  608 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|       7 |  609 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  610 | `#else` |
|       - |  611 | `			pNos->x.iVal = r;` |
|       - |  612 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  613 | `#endif` |
|       4 |  614 | `		}else{` |
|     252 |  615 | `			pNos->x.iVal = r;` |
|     252 |  616 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  617 | `		}` |
|       - |  618 | `	}` |
|     128 |  619 | `sub_store_write:` |
|     258 |  620 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  621 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     258 |  622 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     258 |  623 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     258 |  624 | `		PH7_MemObjStore(pNos,pObj);` |
|     128 |  625 | `	}` |
|     258 |  626 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     258 |  627 | `	VmPopOperand(&pTos,1);` |
|     258 |  628 | `	VM_EXIT_BREAK;` |
|     ! 0 |  629 | `	VM_EXIT_BREAK;` |
|     206 |  630 | `}` |
|       - |  631 |  |
|       - |  632 | `/*` |
|       - |  633 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|       - |  634 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  635 | ` */` |
|   73680 |  636 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  637 | `{` |
|       - |  638 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  639 | `	int rcNa;` |
|   73685 |  640 | `	const char *zArCls = "TypeError";` |
|   73685 |  641 | `	ph7_value *pTos = pState->pTos;` |
|   73685 |  642 | `	ph7_value *pStack = pState->pStack;` |
|   73685 |  643 | `	VmInstr *aInstr = pState->aInstr;` |
|   73685 |  644 | `	sxi32 pc = pState->pc;` |
|       - |  645 | `	sxi32 rc;` |
|   37458 |  646 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   73685 |  647 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  648 | `#ifdef UNTRUST` |
|       - |  649 | `	if( pNos < pStack ){` |
|       - |  650 | `		VM_EXIT_ABORT;` |
|       - |  651 | `	}` |
|       - |  652 | `#endif` |
|       - |  653 | `	{` |
|       - |  654 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  655 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  656 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  657 | `		SyBlob sArMsg;` |
|   73685 |  658 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   73685 |  659 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"-",pNos,&zArCls,&sArMsg);` |
|   73685 |  660 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  661 | `			sxi32 rcAr;` |
|     162 |  662 | `			VmPopOperand(&pTos,1);` |
|     162 |  663 | `			PH7_MemObjRelease(pTos);` |
|     162 |  664 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     162 |  665 | `			pTos->nIdx = SXU32_HIGH;` |
|     242 |  666 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  667 | `				SyBlobLength(&sArMsg));` |
|     162 |  668 | `			SyBlobRelease(&sArMsg);` |
|     242 |  669 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     162 |  670 | `			rc = rcAr;` |
|     162 |  671 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  672 | `		}` |
|   73525 |  673 | `		SyBlobRelease(&sArMsg);` |
|       - |  674 | `	}` |
|   73525 |  675 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 |  676 | `		VmPopOperand(&pTos,1);` |
|       5 |  677 | `		VM_EXIT_BREAK;` |
|       - |  678 | `	}` |
|       - |  679 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|       - |  680 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|       - |  681 | `	 * quietly evaluated to 0. */` |
|   73521 |  682 | `	PH7_MemObjToNumeric(pTos);` |
|   73521 |  683 | `	PH7_MemObjToNumeric(pNos);` |
|   73521 |  684 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  685 | `		/* Floating point arithemic */` |
|       - |  686 | `		ph7_real a,b,r;` |
|     157 |  687 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      21 |  688 | `			PH7_MemObjToReal(pTos);` |
|      10 |  689 | `		}` |
|     157 |  690 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       9 |  691 | `			PH7_MemObjToReal(pNos);` |
|       4 |  692 | `		}` |
|     157 |  693 | `		a = pNos->rVal;` |
|     157 |  694 | `		b = pTos->rVal;` |
|     157 |  695 | `		r = a - b;` |
|       - |  696 | `		/* Push the result */` |
|     157 |  697 | `		pNos->rVal = r;` |
|     157 |  698 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  699 | `		/* Try to get an integer representation */` |
|     157 |  700 | `		PH7_MemObjTryInteger(pNos);` |
|      80 |  701 | `	}else{` |
|       - |  702 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  703 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  704 | `		sxi64 a,b,r;` |
|   73367 |  705 | `		a = pNos->x.iVal;` |
|   73367 |  706 | `		b = pTos->x.iVal;` |
|   73367 |  707 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  708 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 |  709 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      11 |  710 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  711 | `#else` |
|       - |  712 | `			pNos->x.iVal = r;` |
|       - |  713 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  714 | `#endif` |
|       6 |  715 | `		}else{` |
|   73357 |  716 | `			pNos->x.iVal = r;` |
|   73357 |  717 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  718 | `		}` |
|       - |  719 | `	}` |
|   73521 |  720 | `	VmPopOperand(&pTos,1);` |
|   73521 |  721 | `	VM_EXIT_BREAK;` |
|     ! 0 |  722 | `	VM_EXIT_BREAK;` |
|   37463 |  723 | `}` |
|       - |  724 |  |
|       - |  725 | `/*` |
|       - |  726 | `` * php's `**`, as a value operation.`` |
|       - |  727 | ` *` |
|       - |  728 | ` * In php pow() IS the exponentiation operator -- both compile to the same` |
|       - |  729 | ` * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and` |
|       - |  730 | ` * every edge value have to come from ONE place here too. This is that place:` |
|       - |  731 | ` * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call` |
|       - |  732 | ` * it, after the caller has run VmArithOperandCheck() over the two operands (the` |
|       - |  733 | ` * two sites word their throw differently -- one settles an operand stack first,` |
|       - |  734 | ` * the other is inside a C builtin -- so the CHECK stays with the caller and only` |
|       - |  735 | ` * the arithmetic is shared).` |
|       - |  736 | ` *` |
|       - |  737 | ` * pBase and pExp are converted in place, which is what the opcode arm already` |
|       - |  738 | ` * did to its stack slots; pOut may alias either of them and is written last.` |
|       - |  739 | ` */` |
|     790 |  740 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)` |
|       3 |  741 | `{` |
|       - |  742 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - |  743 | `	int bBothInt;` |
|     793 |  744 | `	int usedInt = 0;` |
|       - |  745 | `	ph7_real a, b, r;` |
|       - |  746 | `#endif` |
|     793 |  747 | `	sxi64 base_i = 0, exp_i = 0;` |
|     793 |  748 | `	PH7_MemObjToNumeric(pBase);` |
|     793 |  749 | `	PH7_MemObjToNumeric(pExp);` |
|       - |  750 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    1547 |  751 | `	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&` |
|     754 |  752 | `	           ((pExp->iFlags & MEMOBJ_REAL) == 0);` |
|     793 |  753 | `	if( bBothInt ){` |
|     749 |  754 | `		base_i = pBase->x.iVal;` |
|     749 |  755 | `		exp_i  = pExp->x.iVal;` |
|     373 |  756 | `	}` |
|     793 |  757 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|     757 |  758 | `		PH7_MemObjToReal(pBase);` |
|     377 |  759 | `	}` |
|     793 |  760 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|     785 |  761 | `		PH7_MemObjToReal(pExp);` |
|     391 |  762 | `	}` |
|     793 |  763 | `	a = pBase->rVal;` |
|     793 |  764 | `	b = pExp->rVal;` |
|     793 |  765 | `	r = pow(a, b);` |
|       - |  766 | `	/* int ** non-negative int is php's OWN loop, not a call to pow(), and the` |
|       - |  767 | `	 * difference is visible in the answer twice over. php multiplies in` |
|       - |  768 | ``	 * `pow_function_base`'s doubling loop and, the moment a step OVERFLOWS, finishes`` |
|       - |  769 | ``	 * in DOUBLE space FROM THERE — `dval * pow(l2, i)` with whatever exponent is`` |
|       - |  770 | `	 * left — rather than re-computing pow(base, exp) from the original operands.` |
|       - |  771 | ``	 * That carries the accumulated SIGN, so `(-3) ** PHP_INT_MAX` is -INF where`` |
|       - |  772 | ``	 * pow() answers +INF, and it rounds differently, so `3 ** 100` is`` |
|       - |  773 | ``	 * 5.1537752073201141e+47 where pow() gives ...132e+47 and `10 ** 64` is`` |
|       - |  774 | `	 * 1.0000000000000002e+64 where pow() gives exactly 1e+64. 33 rows of a 380-row` |
|       - |  775 | `	 * base×exponent sweep were wrong, sign included.` |
|       - |  776 | `	 * The overflowing product is the DOUBLE product of the two operands, which is` |
|       - |  777 | `	 * what php's ZEND_SIGNED_MULTIPLY_LONG hands back wherever it detects the` |
|       - |  778 | `	 * overflow with a builtin (gcc/clang) or with _mul128 (MSVC) — the two` |
|       - |  779 | `	 * platforms PHL builds on. */` |
|     793 |  780 | `	if( bBothInt && exp_i >= 0 ){` |
|     689 |  781 | `		sxi64 l1 = 1, l2 = base_i, i = exp_i, iProd;` |
|     689 |  782 | `		if( i == 0 ){` |
|       - |  783 | `			/* Anything to the 0 is int 1 — php answers before it looks at the base. */` |
|     149 |  784 | `			pOut->x.iVal = 1;` |
|     149 |  785 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|     149 |  786 | `			usedInt = 1;` |
|     615 |  787 | `		}else if( l2 == 0 ){` |
|      93 |  788 | `			pOut->x.iVal = 0;` |
|      93 |  789 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|      93 |  790 | `			usedInt = 1;` |
|      47 |  791 | `		}else{` |
|    3475 |  792 | `			while( i >= 1 ){` |
|    3475 |  793 | `				if( i % 2 ){` |
|    1775 |  794 | `					--i;` |
|    1775 |  795 | `					if( PH7_MUL_OVERFLOW64(l1, l2, &iProd) ){` |
|      53 |  796 | `						r = ((ph7_real)l1 * (ph7_real)l2) * pow((ph7_real)l2,(ph7_real)i);` |
|      53 |  797 | `						break;` |
|       - |  798 | `					}` |
|    1723 |  799 | `					l1 = iProd;` |
|     863 |  800 | `				}else{` |
|    1703 |  801 | `					i /= 2;` |
|    1703 |  802 | `					if( PH7_MUL_OVERFLOW64(l2, l2, &iProd) ){` |
|      45 |  803 | `						r = (ph7_real)l1 * pow((ph7_real)l2 * (ph7_real)l2,(ph7_real)i);` |
|      45 |  804 | `						break;` |
|       - |  805 | `					}` |
|    1659 |  806 | `					l2 = iProd;` |
|       - |  807 | `				}` |
|    3379 |  808 | `				if( i == 0 ){` |
|     353 |  809 | `					pOut->x.iVal = l1;` |
|     353 |  810 | `					MemObjSetType(pOut, MEMOBJ_INT);` |
|     353 |  811 | `					usedInt = 1;` |
|     353 |  812 | `					break;` |
|       - |  813 | `				}` |
|       3 |  814 | `			}` |
|       - |  815 | `		}` |
|     343 |  816 | `	}` |
|     793 |  817 | `	if( !usedInt ){` |
|     201 |  818 | `		pOut->rVal = r;` |
|     201 |  819 | `		MemObjSetType(pOut, MEMOBJ_REAL);` |
|     100 |  820 | `	}` |
|       - |  821 | `#else` |
|       - |  822 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|       - |  823 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|       - |  824 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|       - |  825 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|       - |  826 | `	 * represented. */` |
|       - |  827 | `	base_i = pBase->x.iVal;` |
|       - |  828 | `	exp_i  = pExp->x.iVal;` |
|       - |  829 | `	{` |
|       - |  830 | `		sxi64 result_i = 1;` |
|       - |  831 | `		sxi64 cur_base = base_i;` |
|       - |  832 | `		sxi64 cur_exp  = exp_i;` |
|       - |  833 | `		if( cur_exp < 0 ){` |
|       - |  834 | `			result_i = 0;` |
|       - |  835 | `		}else{` |
|       - |  836 | `			while( cur_exp > 0 ){` |
|       - |  837 | `				if( cur_exp & 1 ){` |
|       - |  838 | `					result_i *= cur_base;` |
|       - |  839 | `				}` |
|       - |  840 | `				cur_exp >>= 1;` |
|       - |  841 | `				if( cur_exp > 0 ){` |
|       - |  842 | `					cur_base *= cur_base;` |
|       - |  843 | `				}` |
|       - |  844 | `			}` |
|       - |  845 | `		}` |
|       - |  846 | `		pOut->x.iVal = result_i;` |
|       - |  847 | `		MemObjSetType(pOut, MEMOBJ_INT);` |
|       - |  848 | `	}` |
|       - |  849 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     793 |  850 | `}` |
|       - |  851 | `/*` |
|       - |  852 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|       - |  853 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  854 | ` */` |
|    1068 |  855 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  856 | `{` |
|       - |  857 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  858 | `	int rcNa;` |
|    1070 |  859 | `	const char *zArCls = "TypeError";` |
|    1070 |  860 | `	ph7_value *pTos = pState->pTos;` |
|    1070 |  861 | `	ph7_value *pStack = pState->pStack;` |
|    1070 |  862 | `	VmInstr *aInstr = pState->aInstr;` |
|    1070 |  863 | `	sxi32 pc = pState->pc;` |
|       - |  864 | `	sxi32 rc;` |
|     534 |  865 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    1070 |  866 | `	ph7_value *pNos = &pTos[-1];` |
|    1070 |  867 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|       - |  868 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|       - |  869 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|       - |  870 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|       - |  871 | `	 */` |
|    1070 |  872 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    1070 |  873 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|       - |  874 | `	{` |
|       - |  875 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  876 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  877 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - |  878 | `		 * The message names the operands in SOURCE order, so it takes the same` |
|       - |  879 | ``		 * base/exponent convention as the operation: `$x **= "abc"` is`` |
|       - |  880 | ``		 * `int ** string`, the way `$x ** "abc"` is. */`` |
|       - |  881 | `		SyBlob sArMsg;` |
|    1070 |  882 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    1070 |  883 | `		rcNa = VmArithOperandStep(&(*pVm),pBase,pExp,"**",pNos,&zArCls,&sArMsg);` |
|    1070 |  884 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  885 | `			sxi32 rcAr;` |
|     322 |  886 | `			VmPopOperand(&pTos,1);` |
|     322 |  887 | `			PH7_MemObjRelease(pTos);` |
|     322 |  888 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     322 |  889 | `			pTos->nIdx = SXU32_HIGH;` |
|     482 |  890 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     160 |  891 | `				SyBlobLength(&sArMsg));` |
|     322 |  892 | `			SyBlobRelease(&sArMsg);` |
|     482 |  893 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     322 |  894 | `			rc = rcAr;` |
|     326 |  895 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  896 | `		}` |
|     749 |  897 | `		SyBlobRelease(&sArMsg);` |
|       - |  898 | `	}` |
|       - |  899 | `#ifdef UNTRUST` |
|       - |  900 | `	if( pNos < pStack ){` |
|       - |  901 | `		VM_EXIT_ABORT;` |
|       - |  902 | `	}` |
|       - |  903 | `#endif` |
|     749 |  904 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|     745 |  905 | `		PH7_MemObjPow(pBase,pExp,pNos);` |
|     372 |  906 | `	}` |
|     749 |  907 | `	if( bStore ){` |
|       - |  908 | `		ph7_value *pObj;` |
|     275 |  909 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  910 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     274 |  911 | `		}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     273 |  912 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     273 |  913 | `			PH7_MemObjStore(pNos,pObj);` |
|     136 |  914 | `		}` |
|     137 |  915 | `	}` |
|     749 |  916 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     749 |  917 | `	VmPopOperand(&pTos,1);` |
|     749 |  918 | `	VM_EXIT_BREAK;` |
|     ! 0 |  919 | `	VM_EXIT_BREAK;` |
|     536 |  920 | `}` |
|       - |  921 |  |
|       - |  922 | `/*` |
|       - |  923 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|       - |  924 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  925 | ` */` |
|    1850 |  926 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  927 | `{` |
|    1855 |  928 | `	ph7_value *pTos = pState->pTos;` |
|    1855 |  929 | `	ph7_value *pStack = pState->pStack;` |
|    1855 |  930 | `	VmInstr *aInstr = pState->aInstr;` |
|    1855 |  931 | `	sxi32 pc = pState->pc;` |
|       - |  932 | `	sxi32 rc;` |
|     925 |  933 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    1855 |  934 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  935 | `#ifdef UNTRUST` |
|       - |  936 | `	if( pNos < pStack ){` |
|       - |  937 | `		VM_EXIT_ABORT;` |
|       - |  938 | `	}` |
|       - |  939 | `#endif` |
|       - |  940 | `	/* php answers the UNORDERED comparison -- a NaN, or two arrays neither of` |
|       - |  941 | `	 * which contains the other -- with 1 whichever way round it is asked, and` |
|       - |  942 | `	 * PH7_MemObjCmp does the same, so the spaceship needs no case of its own. */` |
|    1855 |  943 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    1859 |  944 | `	VM_CMP_REFUSAL_ROUTE()` |
|    1845 |  945 | `	rc = (rc > 0) - (rc < 0);   /* normalize to exactly -1, 0 or 1 */` |
|    1845 |  946 | `	VmPopOperand(&pTos,1);` |
|    1845 |  947 | `	PH7_MemObjRelease(pTos);` |
|    1845 |  948 | `	pTos->x.iVal = rc;` |
|    1845 |  949 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|    1845 |  950 | `	VM_EXIT_BREAK;` |
|     ! 0 |  951 | `	VM_EXIT_BREAK;` |
|     930 |  952 | `}` |
|       - |  953 |  |
|       - |  954 | `/*` |
|       - |  955 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|       - |  956 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  957 | ` */` |
|  347039 |  958 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  959 | `{` |
|  347044 |  960 | `	ph7_value *pTos = pState->pTos;` |
|  347044 |  961 | `	ph7_value *pStack = pState->pStack;` |
|  347044 |  962 | `	VmInstr *aInstr = pState->aInstr;` |
|  347044 |  963 | `	sxi32 pc = pState->pc;` |
|       - |  964 | `	sxi32 rc;` |
|  174160 |  965 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  347044 |  966 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  967 | `	/* Perform the comparison and act accordingly */` |
|       - |  968 | `#ifdef UNTRUST` |
|       - |  969 | `	if( pNos < pStack ){` |
|       - |  970 | `		VM_EXIT_ABORT;` |
|       - |  971 | `	}` |
|       - |  972 | `#endif` |
|       - |  973 | ``	/* `$a > $b` is php's `$b < $a` -- asked from the OTHER SIDE, not read off`` |
|       - |  974 | `	 * this side's sign. The two differ exactly where the comparison is` |
|       - |  975 | `	 * UNORDERED and php answers 1 both ways (a NaN against a number or a` |
|       - |  976 | `	 * string; two same-sized arrays neither of which contains the other): a` |
|       - |  977 | ``	 * greater-than read off `rc > 0` calls both of those TRUE, where php --`` |
|       - |  978 | ``	 * asking `$b < $a` and getting 1 again -- calls them false, as it does`` |
|       - |  979 | `	 * every other relational operator on such a pair. */` |
|  347044 |  980 | `	rc = PH7_MemObjCmp(pTos,pNos,FALSE,0);` |
|  347046 |  981 | `	VM_CMP_REFUSAL_ROUTE()` |
|  347042 |  982 | `	if( pInstr->iOp == PH7_OP_GE ){` |
|  341623 |  983 | `		rc = rc <= 0;` |
|  171457 |  984 | `	}else{` |
|    5424 |  985 | `		rc = rc < 0;` |
|       - |  986 | `	}` |
|  347042 |  987 | `	VmPopOperand(&pTos,1);` |
|  347042 |  988 | `	if( !pInstr->iP2 ){` |
|       - |  989 | `		/* Push comparison result without taking the jump */` |
|  347042 |  990 | `		PH7_MemObjRelease(pTos);` |
|  347042 |  991 | `		pTos->x.iVal = rc;` |
|       - |  992 | `		/* Invalidate any prior representation */` |
|  347042 |  993 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  174164 |  994 | `	}else{` |
|     ! 0 |  995 | `		if( rc ){` |
|       - |  996 | `			/* Jump to the desired location */` |
|     ! 0 |  997 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 |  998 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  999 | `		}` |
|       - | 1000 | `	}` |
|  347042 | 1001 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1002 | `	VM_EXIT_BREAK;` |
|  174165 | 1003 | `}` |
|       - | 1004 |  |
|       - | 1005 | `/*` |
|       - | 1006 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|       - | 1007 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1008 | ` */` |
| 1239349 | 1009 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1010 | `{` |
| 1239354 | 1011 | `	ph7_value *pTos = pState->pTos;` |
| 1239354 | 1012 | `	ph7_value *pStack = pState->pStack;` |
| 1239354 | 1013 | `	VmInstr *aInstr = pState->aInstr;` |
| 1239354 | 1014 | `	sxi32 pc = pState->pc;` |
|       - | 1015 | `	sxi32 rc;` |
|  621515 | 1016 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1239354 | 1017 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1018 | `	/* Perform the comparison and act accordingly */` |
|       - | 1019 | `#ifdef UNTRUST` |
|       - | 1020 | `	if( pNos < pStack ){` |
|       - | 1021 | `		VM_EXIT_ABORT;` |
|       - | 1022 | `	}` |
|       - | 1023 | `#endif` |
|       - | 1024 | `	/* An unordered pair answers 1 here too, so both spellings are false for it` |
|       - | 1025 | `	 * without a case of their own (see OP_GT/OP_GE above). */` |
| 1239354 | 1026 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 1239356 | 1027 | `	VM_CMP_REFUSAL_ROUTE()` |
| 1239348 | 1028 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|  104516 | 1029 | `		rc = rc < 1;` |
|   52898 | 1030 | `	}else{` |
| 1134837 | 1031 | `		rc = rc < 0;` |
|       - | 1032 | `	}` |
| 1239348 | 1033 | `	VmPopOperand(&pTos,1);` |
| 1239348 | 1034 | `	if( !pInstr->iP2 ){` |
|       - | 1035 | `		/* Push comparison result without taking the jump */` |
| 1239348 | 1036 | `		PH7_MemObjRelease(pTos);` |
| 1239348 | 1037 | `		pTos->x.iVal = rc;` |
|       - | 1038 | `		/* Invalidate any prior representation */` |
| 1239348 | 1039 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  621517 | 1040 | `	}else{` |
|     ! 0 | 1041 | `		if( rc ){` |
|       - | 1042 | `			/* Jump to the desired location */` |
|     ! 0 | 1043 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1044 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1045 | `		}` |
|       - | 1046 | `	}` |
| 1239348 | 1047 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1048 | `	VM_EXIT_BREAK;` |
|  621520 | 1049 | `}` |
|       - | 1050 |  |
|       - | 1051 | `/*` |
|       - | 1052 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|       - | 1053 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1054 | ` */` |
| 1279624 | 1055 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1056 | `{` |
| 1279629 | 1057 | `	ph7_value *pTos = pState->pTos;` |
| 1279629 | 1058 | `	ph7_value *pStack = pState->pStack;` |
| 1279629 | 1059 | `	VmInstr *aInstr = pState->aInstr;` |
| 1279629 | 1060 | `	sxi32 pc = pState->pc;` |
|       - | 1061 | `	sxi32 rc;` |
|  640399 | 1062 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1279629 | 1063 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1064 | `	/* Perform the comparison and act accordingly */` |
|       - | 1065 | `#ifdef UNTRUST` |
|       - | 1066 | `	if( pNos < pStack ){` |
|       - | 1067 | `		VM_EXIT_ABORT;` |
|       - | 1068 | `	}` |
|       - | 1069 | `#endif` |
| 1279629 | 1070 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 1279631 | 1071 | `	VM_CMP_REFUSAL_ROUTE()` |
| 1279627 | 1072 | `	rc = rc != 0;` |
| 1279627 | 1073 | `	VmPopOperand(&pTos,1);` |
| 1279627 | 1074 | `	if( !pInstr->iP2 ){` |
|       - | 1075 | `		/* Push comparison result without taking the jump */` |
| 1279627 | 1076 | `		PH7_MemObjRelease(pTos);` |
| 1279627 | 1077 | `		pTos->x.iVal = rc;` |
|       - | 1078 | `		/* Invalidate any prior representation */` |
| 1279627 | 1079 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  640403 | 1080 | `	}else{` |
|     ! 0 | 1081 | `		if( rc ){` |
|       - | 1082 | `			/* Jump to the desired location */` |
|     ! 0 | 1083 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1084 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1085 | `		}` |
|       - | 1086 | `	}` |
| 1279627 | 1087 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1088 | `	VM_EXIT_BREAK;` |
|  640404 | 1089 | `}` |
|       - | 1090 |  |
|       - | 1091 | `/*` |
|       - | 1092 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|       - | 1093 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1094 | ` */` |
| 1014558 | 1095 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1096 | `{` |
| 1014563 | 1097 | `	ph7_value *pTos = pState->pTos;` |
| 1014563 | 1098 | `	ph7_value *pStack = pState->pStack;` |
| 1014563 | 1099 | `	VmInstr *aInstr = pState->aInstr;` |
| 1014563 | 1100 | `	sxi32 pc = pState->pc;` |
|       - | 1101 | `	sxi32 rc;` |
|  508283 | 1102 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1014563 | 1103 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1104 | `	/* Perform the comparison and act accordingly */` |
|       - | 1105 | `#ifdef UNTRUST` |
|       - | 1106 | `	if( pNos < pStack ){` |
|       - | 1107 | `		VM_EXIT_ABORT;` |
|       - | 1108 | `	}` |
|       - | 1109 | `#endif` |
|       - | 1110 | ``	/* `NAN === NAN` is false in php, and the comparator says so: an unordered`` |
|       - | 1111 | `	 * pair is 1, never 0. */` |
| 1014563 | 1112 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 1014565 | 1113 | `	VM_CMP_REFUSAL_ROUTE()` |
| 1014561 | 1114 | `	rc = rc == 0;` |
| 1014561 | 1115 | `	VmPopOperand(&pTos,1);` |
| 1014561 | 1116 | `	if( !pInstr->iP2 ){` |
|       - | 1117 | `		/* Push comparison result without taking the jump */` |
| 1014561 | 1118 | `		PH7_MemObjRelease(pTos);` |
| 1014561 | 1119 | `		pTos->x.iVal = rc;` |
|       - | 1120 | `		/* Invalidate any prior representation */` |
| 1014561 | 1121 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  508287 | 1122 | `	}else{` |
|     ! 0 | 1123 | `		if( rc ){` |
|       - | 1124 | `			/* Jump to the desired location */` |
|     ! 0 | 1125 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1126 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1127 | `		}` |
|       - | 1128 | `	}` |
| 1014561 | 1129 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1130 | `	VM_EXIT_BREAK;` |
|  508288 | 1131 | `}` |
|       - | 1132 |  |
|       - | 1133 | `/*` |
|       - | 1134 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|       - | 1135 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1136 | ` */` |
|   17431 | 1137 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1138 | `{` |
|   17436 | 1139 | `	ph7_value *pTos = pState->pTos;` |
|   17436 | 1140 | `	ph7_value *pStack = pState->pStack;` |
|   17436 | 1141 | `	VmInstr *aInstr = pState->aInstr;` |
|   17436 | 1142 | `	sxi32 pc = pState->pc;` |
|       - | 1143 | `	sxi32 rc;` |
|    8708 | 1144 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   17436 | 1145 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1146 | `	/* Perform the comparison and act accordingly */` |
|       - | 1147 | `#ifdef UNTRUST` |
|       - | 1148 | `	if( pNos < pStack ){` |
|       - | 1149 | `		VM_EXIT_ABORT;` |
|       - | 1150 | `	}` |
|       - | 1151 | `#endif` |
|   17436 | 1152 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   17444 | 1153 | `	VM_CMP_REFUSAL_ROUTE()` |
|   17406 | 1154 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|   14694 | 1155 | `		rc = rc == 0;` |
|    7345 | 1156 | `	}else{` |
|    2717 | 1157 | `		rc = rc != 0;` |
|       - | 1158 | `	}` |
|   17406 | 1159 | `	VmPopOperand(&pTos,1);` |
|   17406 | 1160 | `	if( !pInstr->iP2 ){` |
|       - | 1161 | `		/* Push comparison result without taking the jump */` |
|   17406 | 1162 | `		PH7_MemObjRelease(pTos);` |
|   17406 | 1163 | `		pTos->x.iVal = rc;` |
|       - | 1164 | `		/* Invalidate any prior representation */` |
|   17406 | 1165 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    8698 | 1166 | `	}else{` |
|     ! 0 | 1167 | `		if( rc ){` |
|       - | 1168 | `			/* Jump to the desired location */` |
|     ! 0 | 1169 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1170 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1171 | `		}` |
|       - | 1172 | `	}` |
|   17406 | 1173 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1174 | `	VM_EXIT_BREAK;` |
|    8713 | 1175 | `}` |
|       - | 1176 |  |
|       - | 1177 | `/*` |
|       - | 1178 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|       - | 1179 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1180 | ` */` |
|  433951 | 1181 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1182 | `{` |
|  433956 | 1183 | `	ph7_value *pTos = pState->pTos;` |
|  433956 | 1184 | `	ph7_value *pStack = pState->pStack;` |
|  433956 | 1185 | `	VmInstr *aInstr = pState->aInstr;` |
|  433956 | 1186 | `	sxi32 pc = pState->pc;` |
|       - | 1187 | `	sxi32 rc;` |
|  217595 | 1188 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  433956 | 1189 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1190 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|       - | 1191 | `#ifdef UNTRUST` |
|       - | 1192 | `	if( pNos < pStack ){` |
|       - | 1193 | `		VM_EXIT_ABORT;` |
|       - | 1194 | `	}` |
|       - | 1195 | `#endif` |
|       - | 1196 | `	/* Force a boolean cast */` |
|  433956 | 1197 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      16 | 1198 | `		PH7_MemObjToBool(pTos);` |
|       7 | 1199 | `	}` |
|  433956 | 1200 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 | 1201 | `		PH7_MemObjToBool(pNos);` |
|     ! 0 | 1202 | `	}` |
|  433956 | 1203 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|  433956 | 1204 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|  433956 | 1205 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|       - | 1206 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   82229 | 1207 | `		v1 = and_logic[v1*3+v2];` |
|   41107 | 1208 | `	}else{` |
|       - | 1209 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|  351732 | 1210 | `		v1 = or_logic[v1*3+v2];` |
|       - | 1211 | `	}` |
|  433956 | 1212 | `	if( v1 == 2 ){` |
|     ! 0 | 1213 | `		v1 = 1;` |
|     ! 0 | 1214 | `	}` |
|  433956 | 1215 | `	VmPopOperand(&pTos,1);` |
|  433956 | 1216 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|  433956 | 1217 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  433956 | 1218 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1219 | `	VM_EXIT_BREAK;` |
|       5 | 1220 | `}` |
|       - | 1221 |  |
|       - | 1222 | `/*` |
|       - | 1223 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|       - | 1224 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1225 | ` */` |
|     824 | 1226 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 | 1227 | `{` |
|     825 | 1228 | `	ph7_value *pTos = pState->pTos;` |
|     825 | 1229 | `	ph7_value *pStack = pState->pStack;` |
|     825 | 1230 | `	VmInstr *aInstr = pState->aInstr;` |
|     825 | 1231 | `	sxi32 pc = pState->pc;` |
|       - | 1232 | `	sxi32 rc;` |
|     412 | 1233 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     825 | 1234 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1235 | `	ph7_value *pObj;` |
|       - | 1236 | `	sxi64 a,r;` |
|       - | 1237 | `#ifdef UNTRUST` |
|       - | 1238 | `	if( pNos < pStack ){` |
|       - | 1239 | `		VM_EXIT_ABORT;` |
|       - | 1240 | `	}` |
|       - | 1241 | `#endif` |
|       - | 1242 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|       - | 1243 | `	 * this handler, beside the other eleven compound stores.) */` |
|     978 | 1244 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|       - | 1245 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     519 | 1246 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     519 | 1247 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1248 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     519 | 1249 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1250 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     223 | 1251 | `		PH7_MemObjToInteger(pTos);` |
|     111 | 1252 | `	}` |
|     519 | 1253 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     221 | 1254 | `		PH7_MemObjToInteger(pNos);` |
|     110 | 1255 | `	}` |
|       - | 1256 | `	/* Perform the requested operation */` |
|     519 | 1257 | `	a = pTos->x.iVal;` |
|     519 | 1258 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|       - | 1259 | `	/* Push the result */` |
|     471 | 1260 | `	pNos->x.iVal = r;` |
|     471 | 1261 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     471 | 1262 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1263 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     470 | 1264 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     469 | 1265 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     469 | 1266 | `		PH7_MemObjStore(pNos,pObj);` |
|     234 | 1267 | `	}` |
|     471 | 1268 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     471 | 1269 | `	VmPopOperand(&pTos,1);` |
|     471 | 1270 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1271 | `	VM_EXIT_BREAK;` |
|     413 | 1272 | `}` |
|       - | 1273 |  |
|       - | 1274 | `/*` |
|       - | 1275 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|       - | 1276 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1277 | ` */` |
|    3359 | 1278 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 | 1279 | `{` |
|    3362 | 1280 | `	ph7_value *pTos = pState->pTos;` |
|    3362 | 1281 | `	ph7_value *pStack = pState->pStack;` |
|    3362 | 1282 | `	VmInstr *aInstr = pState->aInstr;` |
|    3362 | 1283 | `	sxi32 pc = pState->pc;` |
|       - | 1284 | `	sxi32 rc;` |
|    1679 | 1285 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3362 | 1286 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1287 | `	sxi64 a,r;` |
|       - | 1288 | `#ifdef UNTRUST` |
|       - | 1289 | `	if( pNos < pStack ){` |
|       - | 1290 | `		VM_EXIT_ABORT;` |
|       - | 1291 | `	}` |
|       - | 1292 | `#endif` |
|    3529 | 1293 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|       - | 1294 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    3028 | 1295 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    3028 | 1296 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    3024 | 1297 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    3024 | 1298 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    3024 | 1299 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1300 | `		PH7_MemObjToInteger(pTos);` |
|     118 | 1301 | `	}` |
|    3024 | 1302 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1303 | `		PH7_MemObjToInteger(pNos);` |
|     118 | 1304 | `	}` |
|       - | 1305 | `	/* Perform the requested operation */` |
|    3024 | 1306 | `	a = pNos->x.iVal;` |
|    3024 | 1307 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|       - | 1308 | `	/* Push the result */` |
|    2970 | 1309 | `	pNos->x.iVal = r;` |
|    2970 | 1310 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    2970 | 1311 | `	VmPopOperand(&pTos,1);` |
|    2970 | 1312 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1313 | `	VM_EXIT_BREAK;` |
|    1682 | 1314 | `}` |
|       - | 1315 |  |
|       - | 1316 | `/*` |
|       - | 1317 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|       - | 1318 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1319 | ` */` |
|    7590 | 1320 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1321 | `{` |
|       - | 1322 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - | 1323 | `	int rcNa;` |
|    7595 | 1324 | `	const char *zArCls = "TypeError";` |
|    7595 | 1325 | `	ph7_value *pTos = pState->pTos;` |
|    7595 | 1326 | `	ph7_value *pStack = pState->pStack;` |
|    7595 | 1327 | `	VmInstr *aInstr = pState->aInstr;` |
|    7595 | 1328 | `	sxi32 pc = pState->pc;` |
|       - | 1329 | `	sxi32 rc;` |
|    3788 | 1330 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    7595 | 1331 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1332 | `	{` |
|       - | 1333 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - | 1334 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - | 1335 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - | 1336 | `		 * MULTIPLICATION is commutative and the RESULT does not care which operand is` |
|       - | 1337 | `		 * which, but the MESSAGE does: it names them in SOURCE order, and a compound` |
|       - | 1338 | ``		 * assign puts its lvalue on the TOP of the stack (`$x *= [1]` is `int * array`,`` |
|       - | 1339 | ``		 * the way `$x * [1]` is). */`` |
|    7595 | 1340 | `		ph7_value *pMulL = (pInstr->iOp == PH7_OP_MUL_STORE) ? pTos : pNos;` |
|    7595 | 1341 | `		ph7_value *pMulR = (pInstr->iOp == PH7_OP_MUL_STORE) ? pNos : pTos;` |
|       - | 1342 | `		SyBlob sArMsg;` |
|    7595 | 1343 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    7595 | 1344 | `		rcNa = VmArithOperandStep(&(*pVm),pMulL,pMulR,"*",pNos,&zArCls,&sArMsg);` |
|    7595 | 1345 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - | 1346 | `			sxi32 rcAr;` |
|     316 | 1347 | `			VmPopOperand(&pTos,1);` |
|     316 | 1348 | `			PH7_MemObjRelease(pTos);` |
|     316 | 1349 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     316 | 1350 | `			pTos->nIdx = SXU32_HIGH;` |
|     473 | 1351 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     157 | 1352 | `				SyBlobLength(&sArMsg));` |
|     316 | 1353 | `			SyBlobRelease(&sArMsg);` |
|     473 | 1354 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     316 | 1355 | `			rc = rcAr;` |
|     318 | 1356 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1357 | `		}` |
|    7281 | 1358 | `		SyBlobRelease(&sArMsg);` |
|       - | 1359 | `	}` |
|    7281 | 1360 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 | 1361 | `		goto mul_store_write;` |
|       - | 1362 | `	}` |
|       - | 1363 | `	/* Force the operand to be numeric */` |
|       - | 1364 | `#ifdef UNTRUST` |
|       - | 1365 | `	if( pNos < pStack ){` |
|       - | 1366 | `		VM_EXIT_ABORT;` |
|       - | 1367 | `	}` |
|       - | 1368 | `#endif` |
|    7277 | 1369 | `	PH7_MemObjToNumeric(pTos);` |
|    7277 | 1370 | `	PH7_MemObjToNumeric(pNos);` |
|       - | 1371 | `	/* Perform the requested operation */` |
|   10906 | 1372 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - | 1373 | `		/* Floating point arithemic */` |
|       - | 1374 | `		ph7_real a,b,r;` |
|      36 | 1375 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      26 | 1376 | `			PH7_MemObjToReal(pTos);` |
|      12 | 1377 | `		}` |
|      36 | 1378 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       5 | 1379 | `			PH7_MemObjToReal(pNos);` |
|       2 | 1380 | `		}` |
|      36 | 1381 | `		a = pNos->rVal;` |
|      36 | 1382 | `		b = pTos->rVal;` |
|      36 | 1383 | `		r = a * b;` |
|       - | 1384 | `		/* Push the result */` |
|      36 | 1385 | `		pNos->rVal = r;` |
|      36 | 1386 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1387 | `		/* Try to get an integer representation */` |
|      36 | 1388 | `		PH7_MemObjTryInteger(pNos);` |
|      19 | 1389 | `	}else{` |
|       - | 1390 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|       - | 1391 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - | 1392 | `		sxi64 a,b,r;` |
|    7243 | 1393 | `		a = pNos->x.iVal;` |
|    7243 | 1394 | `		b = pTos->x.iVal;` |
|    7243 | 1395 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|       - | 1396 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      57 | 1397 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|      57 | 1398 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1399 | `#else` |
|       - | 1400 | `			pNos->x.iVal = r;` |
|       - | 1401 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1402 | `#endif` |
|      29 | 1403 | `		}else{` |
|    7187 | 1404 | `			pNos->x.iVal = r;` |
|    7187 | 1405 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1406 | `		}` |
|       - | 1407 | `	}` |
|    3645 | 1408 | `mul_store_write:` |
|    7281 | 1409 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|       - | 1410 | `		ph7_value *pObj;` |
|     335 | 1411 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1412 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     334 | 1413 | `		}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     333 | 1414 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     333 | 1415 | `			PH7_MemObjStore(pNos,pObj);` |
|     165 | 1416 | `		}` |
|     166 | 1417 | `	}` |
|    7281 | 1418 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    7281 | 1419 | `	VmPopOperand(&pTos,1);` |
|    7281 | 1420 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1421 | `	VM_EXIT_BREAK;` |
|    3793 | 1422 | `}` |
|       - | 1423 |  |
