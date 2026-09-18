# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 754/860 lines (87.67%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   21 | `#include "vm_dispatch.h"` |
|      - |   22 | `#define sState (*pState)` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of` |
|      - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   27 | ` */` |
|     52 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   29 | `{` |
|     55 |   30 | `	ph7_value *pTos = pState->pTos;` |
|     55 |   31 | `	ph7_value *pStack = pState->pStack;` |
|     55 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|     55 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|     26 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     55 |   36 | `	 SyString sName = { 0 , 0 };` |
|      - |   37 | `	 VmFrame *pFrameLocal;` |
|      - |   38 | `	SyHashEntry *pEntry;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pTos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     55 |   45 | `	if( pInstr->iP2 == 1 ){` |
|      - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|      - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|      - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|      - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|      - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|      - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|     11 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|     11 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|     11 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|     11 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|     11 |   56 | `		if( nSrcIdx == SXU32_HIGH ){` |
|      - |   57 | ``			/* php: the RHS of `=&` must be a variable, not a constant expression.`` |
|      - |   58 | `			 * (The compiler already rejects the obvious literal forms.) */` |
|    ! 0 |   59 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |   60 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     11 |   61 | `		}else if( pVmAttr ){` |
|      9 |   62 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      9 |   63 | `			if( nOldIdx != nSrcIdx ){` |
|      9 |   64 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      - |   65 | `					/* Release this property's own (unshared) slot before repointing.` |
|      - |   66 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|      - |   67 | `					 * drop any typed-slot enforcement entry too. */` |
|      9 |   68 | `					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      5 |   69 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|      2 |   70 | `					}` |
|      9 |   71 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      4 |   72 | `				}` |
|      9 |   73 | `				pVmAttr->nIdx = nSrcIdx;` |
|      9 |   74 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      9 |   75 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      9 |   76 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      5 |   77 | `			}` |
|      7 |   78 | `		}else if( pStAttr ){` |
|      3 |   79 | `			if( pStAttr->nIdx != nSrcIdx ){` |
|      3 |   80 | `				pStAttr->nIdx = nSrcIdx;` |
|      3 |   81 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      1 |   82 | `			}` |
|      1 |   83 | `		}` |
|     11 |   84 | `		if( pVm->pRefTargetThis ){` |
|      9 |   85 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      4 |   86 | `		}` |
|     11 |   87 | `		pVm->pRefTargetAttr = 0;` |
|     11 |   88 | `		pVm->pRefTargetStaticAttr = 0;` |
|     11 |   89 | `		pVm->pRefTargetThis = 0;` |
|      - |   90 | `		/* Pop the member-result; leave the source as the expression value. */` |
|     11 |   91 | `		VmPopOperand(&pTos,1);` |
|     11 |   92 | `		VM_EXIT_BREAK;` |
|      - |   93 | `	}` |
|     45 |   94 | `	if( pInstr->p3 == 0 ){` |
|      - |   95 | `		char *zName;` |
|      - |   96 | `		/* Take the variable name from the Next on the stack */` |
|    ! 0 |   97 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |   98 | `			/* Force a string cast */` |
|    ! 0 |   99 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  100 | `		}` |
|    ! 0 |  101 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  102 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|    ! 0 |  103 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  104 | `			if( zName ){` |
|    ! 0 |  105 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  106 | `			}` |
|    ! 0 |  107 | `		}` |
|    ! 0 |  108 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  109 | `		pTos--;` |
|    ! 0 |  110 | `	}else{` |
|     45 |  111 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|      - |  112 | `	}` |
|     45 |  113 | `	nIdx = pTos->nIdx;` |
|     45 |  114 | `	if(nIdx == SXU32_HIGH ){` |
|    ! 0 |  115 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|    ! 0 |  116 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  117 | `				"Reference operator require a variable not a constant as it's right operand");` |
|    ! 0 |  118 | `		}else{` |
|      - |  119 | `			ph7_value *pObj;` |
|      - |  120 | `			/* Extract the desired variable and if not available dynamically create it */` |
|    ! 0 |  121 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|    ! 0 |  122 | `			if( pObj == 0 ){` |
|    ! 0 |  123 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - |  124 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|    ! 0 |  125 | `				VM_EXIT_ABORT;` |
|      - |  126 | `			}` |
|      - |  127 | `			/* Perform the store operation */` |
|    ! 0 |  128 | `			PH7_MemObjStore(pTos,pObj);` |
|    ! 0 |  129 | `			pTos->nIdx = pObj->nIdx;` |
|    ! 0 |  130 | `		}` |
|     45 |  131 | `	}else if( sName.nByte > 0){` |
|     45 |  132 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      - |  133 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|      3 |  134 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|      3 |  135 | `			pVm->iExitStatus = 255;` |
|      3 |  136 | `			pVm->bHaltRequested = 1;` |
|      3 |  137 | `			VM_EXIT_ABORT;` |
|    ! 0 |  138 | `		}else{` |
|     43 |  139 | `			pFrameLocal = pVm->pFrame;` |
|     43 |  140 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  141 | `			/* Query the local frame */` |
|     43 |  142 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|     43 |  143 | `			if( pEntry ){` |
|    ! 0 |  144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|    ! 0 |  145 | `			}else{` |
|     43 |  146 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|     43 |  147 | `				if( pFrameLocal->pParent == 0 ){` |
|      - |  148 | `					/* Insert in the $GLOBALS array */` |
|     39 |  149 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|     18 |  150 | `				}` |
|     43 |  151 | `				if( rc == SXRET_OK ){` |
|     43 |  152 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     20 |  153 | `				}` |
|      - |  154 | `			}` |
|      - |  155 | `		}` |
|     20 |  156 | `	}` |
|     43 |  157 | `	VM_EXIT_BREAK;` |
|    ! 0 |  158 | `	VM_EXIT_BREAK;` |
|     29 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|      - |  162 | `#define VM_IDX_CTX_ISSET 4` |
|      - |  163 | `#define VM_IDX_CTX_UNSET 5` |
|      - |  164 | `#define VM_IDX_CTX_EMPTY 6` |
|      - |  165 | `/*` |
|      - |  166 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|      - |  167 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|      - |  168 | ` * an array — and wording the failure by context:` |
|      - |  169 | ` *` |
|      - |  170 | ` *   read/write   Cannot access offset of type Foo on array` |
|      - |  171 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|      - |  172 | ` *   unset        Cannot unset offset of type Foo on array` |
|      - |  173 | ` *` |
|      - |  174 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|      - |  175 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|      - |  176 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|      - |  177 | ` *` |
|      - |  178 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|      - |  179 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|      - |  180 | ` * an access).` |
|      - |  181 | ` */` |
| 226924 |  182 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|      5 |  183 | `{` |
|      - |  184 | `	const char *zType;` |
| 226929 |  185 | `	SyString *pClass = 0;` |
| 226929 |  186 | `	if( pKey == 0 ){` |
|    ! 0 |  187 | `		return FALSE;` |
|      - |  188 | `	}` |
| 226929 |  189 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|     15 |  190 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|     15 |  191 | `		if( pInst && pInst->pClass ){` |
|     15 |  192 | `			pClass = &pInst->pClass->sName;` |
|      7 |  193 | `		}` |
|     15 |  194 | `		zType = "object";` |
| 226922 |  195 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  196 | `		zType = "array";` |
|      3 |  197 | `	}else{` |
| 226911 |  198 | `		return FALSE;` |
|      - |  199 | `	}` |
|     19 |  200 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|     19 |  201 | `	if( iCtx == VM_IDX_CTX_UNSET ){` |
|      3 |  202 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|      2 |  203 | `	}else{` |
|     17 |  204 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|      - |  205 | `	}` |
|     19 |  206 | `	if( pClass ){` |
|     15 |  207 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      8 |  208 | `	}else{` |
|      5 |  209 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|      - |  210 | `	}` |
|     19 |  211 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|      7 |  212 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|      4 |  213 | `	}else{` |
|     13 |  214 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|      - |  215 | `	}` |
|     19 |  216 | `	return TRUE;` |
| 113467 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|      - |  220 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|      - |  221 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|      - |  222 | ` * takes over.` |
|      - |  223 | ` */` |
| 226906 |  224 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  225 | `{` |
|      - |  226 | `	sxu32 nId;` |
| 226911 |  227 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
| 226907 |  228 | `		return;` |
|      - |  229 | `	}` |
|      5 |  230 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      7 |  231 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      2 |  232 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      5 |  233 | `	PH7_MemObjRelease(pKey);` |
|      5 |  234 | `	pKey->x.iVal = (sxi64)nId;` |
|      5 |  235 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
| 113458 |  236 | `}` |
|      - |  237 | `/*` |
|      - |  238 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  239 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  240 | ` */` |
| 243104 |  241 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  242 | `{` |
| 243109 |  243 | `	ph7_value *pTos = pState->pTos;` |
| 243109 |  244 | `	ph7_value *pStack = pState->pStack;` |
| 243109 |  245 | `	VmInstr *aInstr = pState->aInstr;` |
| 243109 |  246 | `	sxi32 pc = pState->pc;` |
|      - |  247 | `	sxi32 rc;` |
| 121552 |  248 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 243109 |  249 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  250 | `	ph7_value *pKey;` |
|      - |  251 | `	sxu32 nIdx;` |
| 243109 |  252 | `	if( pInstr->iP1 ){` |
|      - |  253 | `		/* Key is next on stack */` |
|  69489 |  254 | `		pKey = pTos;` |
|  69489 |  255 | `		pTos--;` |
|  34747 |  256 | `	}else{` |
| 173625 |  257 | `		pKey = 0;` |
|      - |  258 | `	}` |
|      - |  259 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  260 | `		 * normalizes to ""); PHL rejects it. */` |
| 243109 |  261 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69291 |  262 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103937 |  263 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69286 |  264 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|      - |  265 | `			SyBlob sTypeMsg;` |
|      - |  266 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|      - |  267 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  69291 |  268 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|      - |  269 | `				sxi32 rcSc;` |
|      5 |  270 | `				PH7_MemObjRelease(pKey);` |
|      5 |  271 | `				VmPopOperand(&pTos,1);` |
|      5 |  272 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      9 |  273 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  274 | `				rc = rcSc;` |
|      5 |  275 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  276 | `			}` |
|  69287 |  277 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|  69287 |  278 | `			if( bNull \|\| bLossyFloat ){` |
|      - |  279 | `				sxi32 rcSc;` |
|      5 |  280 | `				const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 |  281 | `				                         : "Cannot access offset of type float on array";` |
|      5 |  282 | `				PH7_MemObjRelease(pKey);` |
|      5 |  283 | `				VmPopOperand(&pTos,1);` |
|      5 |  284 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      5 |  285 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  286 | `				rc = rcSc;` |
|      5 |  287 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  288 | `			}` |
|  34639 |  289 | `		}` |
| 243101 |  290 | `	nIdx = pTos->nIdx;` |
|      - |  291 | `	{` |
|      - |  292 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  293 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  294 | `		 * the backing variable slot at nIdx. */` |
| 243101 |  295 | `		ph7_class_instance *pInst = 0;` |
| 243101 |  296 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  297 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 243058 |  298 | `		}else if( nIdx != SXU32_HIGH ){` |
| 243015 |  299 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 243015 |  300 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  301 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  302 | `			}` |
| 121505 |  303 | `		}` |
| 243101 |  304 | `		if( pInst ){` |
|     89 |  305 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  306 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  307 | `				ph7_class_method *pMeth;` |
|      - |  308 | `				ph7_value sNullKey;` |
|      - |  309 | `				ph7_value *apArg[2];` |
|     87 |  310 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  311 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  312 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  313 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  314 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  315 | `					VM_EXIT_BREAK;` |
|      - |  316 | `				}` |
|     87 |  317 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  318 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  319 | `				/* Pop container; pTos now points to the value */` |
|     87 |  320 | `				VmPopOperand(&pTos,1);` |
|     87 |  321 | `				if( pKey == 0 ){` |
|     10 |  322 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  323 | `					apArg[0] = &sNullKey;` |
|      6 |  324 | `				}else{` |
|     79 |  325 | `					apArg[0] = pKey;` |
|      - |  326 | `				}` |
|     87 |  327 | `				apArg[1] = pTos;` |
|     87 |  328 | `				if( pMeth ){` |
|     87 |  329 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  330 | `				}` |
|     87 |  331 | `				if( pKey ){` |
|     79 |  332 | `					PH7_MemObjRelease(pKey);` |
|     41 |  333 | `				}else{` |
|     10 |  334 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  335 | `				}` |
|      - |  336 | `				/* Pop the value */` |
|     87 |  337 | `				VmPopOperand(&pTos,1);` |
|     87 |  338 | `				VM_EXIT_BREAK;` |
|      - |  339 | `			}` |
|      - |  340 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  341 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  342 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  343 | `			 * a few lines below). Match PHP. */` |
|      - |  344 | `			{` |
|      - |  345 | `				char zMsg[256];` |
|      3 |  346 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  347 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  348 | `					"Cannot use object of type %.*s as array",` |
|      2 |  349 | `					(int)pName->nByte,pName->zString);` |
|      3 |  350 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  351 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  352 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  353 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  354 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  355 | `			}` |
|      - |  356 | `		}` |
|      - |  357 | `	}` |
| 243015 |  358 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  359 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  360 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  361 | `		 * checking true sharing count, then re-add after separation. */` |
| 242879 |  362 | `		if( nIdx != SXU32_HIGH ){` |
| 242879 |  363 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 364316 |  364 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 242879 |  365 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  366 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  367 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  368 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  369 | `				 * refcounts if the backing array was already separated. */` |
| 242879 |  370 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 242879 |  371 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 242879 |  372 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 242879 |  373 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 242879 |  374 | `					pTos->x.pOther = pMap;` |
| 121442 |  375 | `				}else{` |
|      - |  376 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  377 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  378 | `					pMap = pCur;` |
|      - |  379 | `				}` |
| 121442 |  380 | `			}else{` |
|    ! 0 |  381 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  382 | `			}` |
| 121442 |  383 | `		}else{` |
|    ! 0 |  384 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  385 | `		}` |
| 242879 |  386 | `		if( pMap->iRef < 2 ){` |
|      - |  387 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  388 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  389 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  390 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  391 | `			pMap->iRef = 2;` |
|    ! 0 |  392 | `		}` |
| 121442 |  393 | `	}else{` |
|      - |  394 | `		ph7_value *pObj;` |
|    139 |  395 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    139 |  396 | `		if( pObj == 0 ){` |
|    ! 0 |  397 | `			if( pKey ){` |
|    ! 0 |  398 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  399 | `			}` |
|    ! 0 |  400 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  401 | `			VM_EXIT_BREAK;` |
|      - |  402 | `		}` |
|      - |  403 | `		/* Phase#1: Load the array */` |
|    139 |  404 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  405 | `			VmPopOperand(&pTos,1);` |
|    107 |  406 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  407 | `				/* Force a string cast */` |
|    ! 0 |  408 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  409 | `			}` |
|    107 |  410 | `			if( pKey == 0 ){` |
|      - |  411 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|      - |  412 | `				 * "[] operator not supported for strings" and leaves the string` |
|      - |  413 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|      - |  414 | `				 * an array from a variable holding a string quietly produced a` |
|      - |  415 | `				 * longer string instead of failing — a wrong answer, not a` |
|      - |  416 | `				 * missing diagnostic. */` |
|      - |  417 | `				SyBlob sErrMsg;` |
|      3 |  418 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  419 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|      - |  420 | `					sizeof("[] operator not supported for strings")-1);` |
|      3 |  421 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  422 | `				VM_EXIT_BREAK;` |
|    ! 0 |  423 | `			}else{` |
|      - |  424 | `				sxi64 iOfft;` |
|      - |  425 | `				sxi64 nLen;` |
|    105 |  426 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  427 | `					/* Force an int cast */` |
|    ! 0 |  428 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  429 | `				}` |
|    105 |  430 | `				iOfft = pKey->x.iVal;` |
|    105 |  431 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  432 | `				if( iOfft < 0 ){` |
|      - |  433 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  434 | `					iOfft += nLen;` |
|      5 |  435 | `					if( iOfft < 0 ){` |
|      4 |  436 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  437 | `							pKey->x.iVal);` |
|      3 |  438 | `						PH7_MemObjRelease(pKey);` |
|      3 |  439 | `						VM_EXIT_BREAK;` |
|      - |  440 | `					}` |
|      1 |  441 | `				}` |
|    103 |  442 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  443 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  444 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  445 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  446 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  447 | `					}` |
|    103 |  448 | `					if( iOfft >= nLen ){` |
|      - |  449 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  450 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  451 | `						 * -- a silently wrong string. */` |
|      - |  452 | `						sxi64 nPad;` |
|      9 |  453 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  454 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  455 | `						}` |
|      3 |  456 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  457 | `					}else{` |
|    101 |  458 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  459 | `						zData[iOfft] = zBlob[0];` |
|      - |  460 | `					}` |
|     51 |  461 | `				}` |
|      - |  462 | `			}` |
|    103 |  463 | `			if( pKey ){` |
|    103 |  464 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  465 | `			}` |
|    103 |  466 | `			VM_EXIT_BREAK;` |
|     33 |  467 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  468 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  469 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  470 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  471 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  472 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     33 |  473 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     33 |  474 | `			if( bScalar ){` |
|      - |  475 | `				sxi32 rcSc;` |
|      8 |  476 | `				if( pKey ){` |
|      5 |  477 | `					PH7_MemObjRelease(pKey);` |
|      2 |  478 | `				}` |
|      8 |  479 | `				VmPopOperand(&pTos,1);` |
|      8 |  480 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  481 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  482 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  483 | `				rc = rcSc;` |
|      8 |  484 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  485 | `			}` |
|      - |  486 | `			/* Force a hashmap cast  */` |
|     27 |  487 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     27 |  488 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  489 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  490 | `				VM_EXIT_ABORT;` |
|      - |  491 | `			}` |
|     12 |  492 | `		}` |
|      - |  493 | `		/* COW separate the backing variable before mutation */` |
|     27 |  494 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  495 | `	}` |
| 242903 |  496 | `	VmPopOperand(&pTos,1);` |
|      - |  497 | `	/* Phase#2: Perform the insertion */` |
| 242903 |  498 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  499 | `		if( pMap == pVm->pGlobal ){` |
|      - |  500 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  501 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  502 | `			if( pKey == 0 ){` |
|    ! 0 |  503 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  504 | `			}else{` |
|      3 |  505 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  506 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  507 | `				}` |
|      3 |  508 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  509 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  510 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  511 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  512 | `					rc = SXRET_OK;` |
|    ! 0 |  513 | `				}else{` |
|      4 |  514 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  515 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  516 | `						0,pTos->nIdx);` |
|      - |  517 | `				}` |
|      - |  518 | `			}` |
|      2 |  519 | `		}else{` |
|      - |  520 | `			/* Insertion by reference */` |
|     17 |  521 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  522 | `		}` |
|     10 |  523 | `	}else{` |
| 242885 |  524 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  525 | `	}` |
| 242903 |  526 | `	if( pKey ){` |
|  69295 |  527 | `		PH7_MemObjRelease(pKey);` |
|  34645 |  528 | `	}` |
|      - |  529 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  530 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  531 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 242903 |  532 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 242899 |  533 | `	VM_EXIT_BREAK;` |
|    ! 0 |  534 | `	VM_EXIT_BREAK;` |
| 121557 |  535 | `}` |
|      - |  536 |  |
|      - |  537 | `/*` |
|      - |  538 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  539 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  540 | ` */` |
|   1198 |  541 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  542 | `{` |
|   1203 |  543 | `	ph7_value *pTos = pState->pTos;` |
|   1203 |  544 | `	ph7_value *pStack = pState->pStack;` |
|   1203 |  545 | `	VmInstr *aInstr = pState->aInstr;` |
|   1203 |  546 | `	sxi32 pc = pState->pc;` |
|      - |  547 | `	sxi32 rc;` |
|    599 |  548 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1203 |  549 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  550 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  551 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  552 | `	 * plain anonymous function with no captured environment. */` |
|   1203 |  553 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  554 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  555 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  556 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  557 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  558 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1203 |  559 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1203 |  560 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  561 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  562 | `		ph7_vm_func *pClosure;` |
|      - |  563 | `		char *zName;` |
|      - |  564 | `		sxu32 mLen;` |
|      - |  565 | `		sxu32 n;` |
|      - |  566 | `		/* Create a new VM function */` |
|   1193 |  567 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  568 | `		/* Generate an unique closure name */` |
|   1193 |  569 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1193 |  570 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  571 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  572 | `			VM_EXIT_ABORT;` |
|      - |  573 | `		}` |
|   1193 |  574 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1193 |  575 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  576 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  577 | `		}` |
|      - |  578 | `		/* Zero the stucture */` |
|   1193 |  579 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  580 | `		/* Perform a structure assignment on read-only items */` |
|   1193 |  581 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1193 |  582 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1193 |  583 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1193 |  584 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  585 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  586 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1193 |  587 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1193 |  588 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1193 |  589 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1193 |  590 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1193 |  591 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1193 |  592 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1193 |  593 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1193 |  594 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1193 |  595 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1193 |  596 | `		if( pClosure->pUserData == 0 ){` |
|      - |  597 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  598 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  599 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1193 |  600 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    594 |  601 | `		}` |
|      - |  602 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  603 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  604 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  605 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1193 |  606 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  607 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  608 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1193 |  609 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1193 |  610 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1193 |  611 | `		pClosure->sFile = pFunc->sFile;` |
|   1193 |  612 | `		pClosure->nLine = pFunc->nLine;` |
|   1193 |  613 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1193 |  614 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  615 | `		/* Register the closure */` |
|   1193 |  616 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  617 | `		/* Set up closure environment */` |
|   1193 |  618 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1193 |  619 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2605 |  620 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  621 | `			ph7_value *pValue;` |
|   1417 |  622 | `			pEnv = &aEnv[n];` |
|   1417 |  623 | `			sEnv.sName  = pEnv->sName;` |
|   1417 |  624 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1417 |  625 | `			sEnv.nLine = pEnv->nLine;` |
|   1417 |  626 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1417 |  627 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1412 |  628 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    775 |  629 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     64 |  630 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  631 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  632 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  633 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  634 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  635 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  636 | `				 * aliases the name to this slot instead of copying a value. */` |
|     98 |  637 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     98 |  638 | `				if( pValue ){` |
|     98 |  639 | `					sEnv.nIdx = pValue->nIdx;` |
|     98 |  640 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     47 |  641 | `				}` |
|     51 |  642 | `			}else{` |
|      - |  643 | `				/* Standard pass by value */` |
|   1323 |  644 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1323 |  645 | `				if( pValue ){` |
|      - |  646 | `					/* Copy imported value */` |
|    183 |  647 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   1234 |  648 | `				}else if( (pFunc->iFlags & VM_FUNC_ARROW) == 0` |
|    877 |  649 | `					&& (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    317 |  650 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      5 |  651 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  652 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|      - |  653 | `					 * warns when the variable is undefined there (the by-ref form` |
|      - |  654 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|      - |  655 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns, and an` |
|      - |  656 | `					 * arrow function's implicit captures (VM_FUNC_ARROW) warn only` |
|      - |  657 | `					 * when the body reads them, not here. The capture still proceeds` |
|      - |  658 | `					 * as NULL, as php does. php attributes the warning to the` |
|      - |  659 | `					 * capture's own line (which can differ from the OP_LOAD_CLOSURE` |
|      - |  660 | `					 * instruction line when the use-clause wraps), so borrow the` |
|      - |  661 | `					 * recorded line for the emission and restore it. */` |
|     11 |  662 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|     11 |  663 | `					if( sEnv.nLine ){` |
|     11 |  664 | `						pVm->nCurLine = sEnv.nLine;` |
|      5 |  665 | `					}` |
|     11 |  666 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|     11 |  667 | `					pVm->nCurLine = nSavedLine;` |
|      5 |  668 | `				}` |
|      - |  669 | `			}` |
|      - |  670 | `			/* Insert the imported variable */` |
|   1417 |  671 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    711 |  672 | `		}` |
|   1193 |  673 | `		pTarget = pClosure;` |
|    594 |  674 | `	}` |
|      - |  675 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  676 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  677 | `	 * path when the closure is dispatched by name. */` |
|   1203 |  678 | `	pTos++;` |
|      - |  679 | `	{` |
|   1203 |  680 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1203 |  681 | `		if( pCloObj ){` |
|   1203 |  682 | `			pCloObj->iRef++;` |
|   1203 |  683 | `			pTos->x.pOther = pCloObj;` |
|   1203 |  684 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    604 |  685 | `		}else{` |
|      - |  686 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  687 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  688 | `		}` |
|      - |  689 | `	}` |
|   1203 |  690 | `	VM_EXIT_BREAK;` |
|    ! 0 |  691 | `	VM_EXIT_BREAK;` |
|    604 |  692 | `}` |
|      - |  693 |  |
|      - |  694 |  |
|      - |  695 | `/*` |
|      - |  696 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  697 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  698 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  699 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  700 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  701 | ` */` |
|     14 |  702 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  703 | `{` |
|     17 |  704 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  705 | `}` |
|      - |  706 | `/*` |
|      - |  707 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  708 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  709 | ` */` |
| 679439 |  710 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  711 | `{` |
| 679444 |  712 | `	ph7_value *pTos = pState->pTos;` |
| 679444 |  713 | `	ph7_value *pStack = pState->pStack;` |
| 679444 |  714 | `	VmInstr *aInstr = pState->aInstr;` |
| 679444 |  715 | `	sxi32 pc = pState->pc;` |
|      - |  716 | `	sxi32 rc;` |
| 340087 |  717 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 679444 |  718 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 679444 |  719 | `	ph7_hashmap *pMap = 0;` |
|      - |  720 | `	ph7_value *pIdx;` |
| 679444 |  721 | `	pIdx = 0;` |
| 679444 |  722 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  723 | `		if( !pInstr->iP2){` |
|      - |  724 | `			/* No available index,load NULL */` |
|    ! 0 |  725 | `			if( pTos >= pStack ){` |
|    ! 0 |  726 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  727 | `			}else{` |
|      - |  728 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  729 | `				pTos++;` |
|    ! 0 |  730 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  731 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  732 | `			}` |
|      - |  733 | `			/* Emit a notice */` |
|    ! 0 |  734 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  735 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  736 | `			VM_EXIT_BREAK;` |
|      - |  737 | `		}` |
|      2 |  738 | `	}else{` |
| 679442 |  739 | `		pIdx = pTos;` |
| 679442 |  740 | `		pTos--;` |
|      - |  741 | `	}` |
| 679444 |  742 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  743 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  744 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  745 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  746 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|      7 |  747 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  748 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|      2 |  749 | `		}` |
|      7 |  750 | `		if( pIdx ){` |
|      - |  751 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  752 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|      7 |  753 | `			PH7_MemObjRelease(pIdx);` |
|      3 |  754 | `		}` |
|      7 |  755 | `		PH7_MemObjRelease(pTos);` |
|      7 |  756 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      7 |  757 | `		VM_EXIT_BREAK;` |
|      - |  758 | `	}` |
| 679438 |  759 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  760 | `		/* String access */` |
| 532816 |  761 | `		if( pIdx ){` |
|      - |  762 | `			sxi64 iOfft;` |
| 532816 |  763 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 532816 |  764 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  765 | `				/* Force an int cast */` |
|    ! 0 |  766 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  767 | `			}` |
| 532816 |  768 | `			iOfft = pIdx->x.iVal;` |
|      - |  769 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  770 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  771 | `			 * number, ran past the end and quietly produced NULL. */` |
| 532816 |  772 | `			if( iOfft < 0 ){` |
|      7 |  773 | `				iOfft += nLen;` |
|      3 |  774 | `			}` |
| 532820 |  775 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  776 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  777 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  778 | `				 * silently produced NULL in both cases). */` |
|      - |  779 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  780 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  781 | `				 * lookups and must stay silent. */` |
|     11 |  782 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  783 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  784 | `				PH7_MemObjRelease(pTos);` |
|     10 |  785 | `				if( bQuiet ){` |
|      8 |  786 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  787 | `				}else{` |
|      3 |  788 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  789 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  790 | `						pIdx->x.iVal);` |
|      - |  791 | `				}` |
|      6 |  792 | `			}else{` |
| 532808 |  793 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 532808 |  794 | `				int c = zData[iOfft];` |
| 532808 |  795 | `				PH7_MemObjRelease(pTos);` |
| 532808 |  796 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 532808 |  797 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  798 | `			}` |
| 266781 |  799 | `		}else{` |
|      - |  800 | `			/* No available index,load NULL */` |
|    ! 0 |  801 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  802 | `		}` |
| 532816 |  803 | `		VM_EXIT_BREAK;` |
|      - |  804 | `	}` |
| 146627 |  805 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  806 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  807 | `		 * iP2 codes:` |
|      - |  808 | `		 *   0 = read       → offsetGet` |
|      - |  809 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  810 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  811 | `		 *   4 = isset()    → offsetExists` |
|      - |  812 | `		 *   5 = unset()    → offsetUnset` |
|      - |  813 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  814 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  815 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  816 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  817 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  818 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  819 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  820 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  821 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  822 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  823 | `			ph7_class_method *pMeth;` |
|      - |  824 | `			ph7_value sResult;` |
|      - |  825 | `			ph7_value *apArg[1];` |
|    181 |  826 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  827 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  828 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  829 | `					"Cannot use [] for reading");` |
|    ! 0 |  830 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  831 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  832 | `				VM_EXIT_BREAK;` |
|      - |  833 | `			}` |
|    181 |  834 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  835 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  836 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  837 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  838 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  839 | `				apArg[0] = pIdx;` |
|     81 |  840 | `				if( pMeth ){` |
|     81 |  841 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  842 | `				}` |
|    143 |  843 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  844 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  845 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  846 | `				apArg[0] = pIdx;` |
|     20 |  847 | `				if( pMeth ){` |
|     20 |  848 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  849 | `				}` |
|     12 |  850 | `			}else{` |
|     89 |  851 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  852 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  853 | `				apArg[0] = pIdx;` |
|     89 |  854 | `				if( pMeth ){` |
|     89 |  855 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  856 | `				}` |
|      - |  857 | `			}` |
|    181 |  858 | `			if( pInstr->iP2 == 4 ){` |
|      - |  859 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  860 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  861 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  862 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  863 | `				PH7_MemObjRelease(pTos);` |
|     53 |  864 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  865 | `				if( bExists ){` |
|     28 |  866 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  867 | `					pTos->x.iVal = 1;` |
|     16 |  868 | `				}else{` |
|     29 |  869 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  870 | `				}` |
|    157 |  871 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  872 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  873 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  874 | `				PH7_MemObjRelease(pTos);` |
|     20 |  875 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  876 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  877 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  878 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  879 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  880 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  881 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  882 | `				 * coalesce takes the default, the real value on a hit. */` |
|     23 |  883 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     23 |  884 | `				PH7_MemObjRelease(&sResult);` |
|     23 |  885 | `				PH7_MemObjRelease(pTos);` |
|     23 |  886 | `				pTos->nIdx = SXU32_HIGH;` |
|     23 |  887 | `				if( !bExists ){` |
|      9 |  888 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      6 |  889 | `				}else{` |
|     17 |  890 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  891 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  892 | `					ph7_value sValue;` |
|     17 |  893 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     17 |  894 | `					apArg[0] = pIdx;` |
|     17 |  895 | `					if( pGet ){` |
|     17 |  896 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  897 | `					}` |
|     17 |  898 | `					PH7_MemObjStore(&sValue,pTos);` |
|     17 |  899 | `					PH7_MemObjRelease(&sValue);` |
|      - |  900 | `				}` |
|     23 |  901 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     23 |  902 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  903 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  904 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  905 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  906 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  907 | `				 *     and push NULL.` |
|      - |  908 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  909 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  910 | `				int bShouldArm = !bExists;` |
|      - |  911 | `				ph7_value sValue;` |
|     10 |  912 | `				PH7_MemObjRelease(&sResult);` |
|      - |  913 | `				/* Reset any prior arming defensively */` |
|     10 |  914 | `				VmCoalesceDisarm(pVm);` |
|     10 |  915 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  916 | `				if( bExists ){` |
|      5 |  917 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  918 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  919 | `					apArg[0] = pIdx;` |
|      5 |  920 | `					if( pGet ){` |
|      5 |  921 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  922 | `					}` |
|      5 |  923 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  924 | `						bShouldArm = 1;` |
|      1 |  925 | `					}` |
|      2 |  926 | `				}` |
|     10 |  927 | `				PH7_MemObjRelease(pTos);` |
|     10 |  928 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  929 | `				if( bShouldArm ){` |
|      - |  930 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  931 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  932 | `					 * intervening expression evaluation. */` |
|      8 |  933 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  934 | `					if( pIdx ){` |
|      8 |  935 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  936 | `					}` |
|      8 |  937 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  938 | `					pInst->iRef++;` |
|      8 |  939 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  940 | `				}else{` |
|      3 |  941 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  942 | `				}` |
|     10 |  943 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  944 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  945 | `				VM_EXIT_BREAK;` |
|    ! 0 |  946 | `			}else{` |
|      - |  947 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  948 | `				PH7_MemObjRelease(pTos);` |
|     89 |  949 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  950 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  951 | `			}` |
|    153 |  952 | `			PH7_MemObjRelease(&sResult);` |
|    153 |  953 | `			if( pIdx ){` |
|    153 |  954 | `				PH7_MemObjRelease(pIdx);` |
|     74 |  955 | `			}` |
|    153 |  956 | `			VM_EXIT_BREAK;` |
|      - |  957 | `		}` |
|      - |  958 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  959 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  960 | `		if( pInst ){` |
|      - |  961 | `			char zMsg[256];` |
|      3 |  962 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  963 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  964 | `				"Cannot use object of type %.*s as array",` |
|      2 |  965 | `				(int)pName->nByte,pName->zString);` |
|      3 |  966 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  967 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  968 | `			PH7_MemObjRelease(pTos);` |
|      3 |  969 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  970 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  971 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  972 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  973 | `			 * execution carried on inside the try block. */` |
|      3 |  974 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  975 | `		}` |
|    ! 0 |  976 | `	}` |
| 146449 |  977 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  978 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  979 | `			ph7_value *pObj;` |
|     23 |  980 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  981 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  982 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  983 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  984 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  985 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  986 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  987 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  988 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  989 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - |  990 | `				 * it is not a bool). */` |
|     23 |  991 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - |  992 | `					SyBlob sErrMsg;` |
|      7 |  993 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 |  994 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - |  995 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 |  996 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 |  997 | `					if( pIdx ){` |
|      7 |  998 | `						PH7_MemObjRelease(pIdx);` |
|      3 |  999 | `					}` |
|      7 | 1000 | `					PH7_MemObjRelease(pTos);` |
|      7 | 1001 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 | 1002 | `					VM_EXIT_BREAK;` |
|      - | 1003 | `				}` |
|     17 | 1004 | `				PH7_MemObjToHashmap(pObj);` |
|     17 | 1005 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 | 1006 | `			}` |
|      8 | 1007 | `		}` |
|      8 | 1008 | `	}` |
| 146443 | 1009 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - | 1010 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - | 1011 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - | 1012 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|      - | 1013 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|      - | 1014 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|      - | 1015 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|      - | 1016 | `	 * accepted with a warning and becomes its integer id. */` |
| 146443 | 1017 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1018 | `		SyBlob sTypeMsg;` |
| 146431 | 1019 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,pInstr->iP2,&sTypeMsg) ){` |
|     13 | 1020 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|     13 | 1021 | `			PH7_MemObjRelease(pIdx);` |
|     13 | 1022 | `			PH7_MemObjRelease(pTos);` |
|     13 | 1023 | `			pTos->nIdx = SXU32_HIGH;` |
|     13 | 1024 | `			VM_EXIT_BREAK;` |
|      - | 1025 | `		}` |
| 146419 | 1026 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  73207 | 1027 | `	}` |
| 146426 | 1028 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 146420 | 1029 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  33915 | 1030 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  50873 | 1031 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  33910 | 1032 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  33915 | 1033 | `		if( bNull \|\| bLossyFloat ){` |
|      - | 1034 | `			SyBlob sErrMsg;` |
|      5 | 1035 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1036 | `			SyBlobAppend(&sErrMsg,` |
|      2 | 1037 | `				bNull ? "Cannot access offset of type null on array"` |
|      - | 1038 | `				      : "Cannot access offset of type float on array",` |
|      4 | 1039 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - | 1040 | `				                      : "Cannot access offset of type float on array"));` |
|      5 | 1041 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 | 1042 | `			PH7_MemObjRelease(pIdx);` |
|      5 | 1043 | `			PH7_MemObjRelease(pTos);` |
|      5 | 1044 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 | 1045 | `			VM_EXIT_BREAK;` |
|      - | 1046 | `		}` |
|  16953 | 1047 | `	}` |
| 146427 | 1048 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 146417 | 1049 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - | 1050 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - | 1051 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - | 1052 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - | 1053 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - | 1054 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - | 1055 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1265 | 1056 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    630 | 1057 | `		}` |
|      - | 1058 | `		/* Point to the hashmap */` |
| 146417 | 1059 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 146417 | 1060 | `		if( pIdx ){` |
|      - | 1061 | `			/* Load the desired entry */` |
| 146415 | 1062 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  73205 | 1063 | `		}` |
| 146417 | 1064 | `		if( pInstr->iP2 == 3 ){` |
|      - | 1065 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - | 1066 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - | 1067 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - | 1068 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - | 1069 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - | 1070 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - | 1071 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - | 1072 | `			 * correct for the outermost write. */` |
|     21 | 1073 | `			int needWrite = (rc != SXRET_OK);` |
|     21 | 1074 | `			if( !needWrite && pNode ){` |
|     13 | 1075 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 | 1076 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 | 1077 | `					needWrite = 1;` |
|      3 | 1078 | `				}` |
|      6 | 1079 | `			}` |
|     21 | 1080 | `			if( needWrite ){` |
|     15 | 1081 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 | 1082 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - | 1083 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - | 1084 | `					 * into the new map's storage. */` |
|      7 | 1085 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 | 1086 | `					if( pIdx ){` |
|      7 | 1087 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 | 1088 | `					}` |
|      3 | 1089 | `				}` |
|      7 | 1090 | `			}` |
|     10 | 1091 | `		}` |
| 146417 | 1092 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - | 1093 | `			/* Create a new empty entry */` |
|    324 | 1094 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 | 1095 | `			if( rc == SXRET_OK ){` |
|      - | 1096 | `				/* Point to the last inserted entry */` |
|    321 | 1097 | `				pNode = pMap->pLast;` |
|    161 | 1098 | `			}else{` |
|      - | 1099 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - | 1100 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - | 1101 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - | 1102 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 | 1103 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - | 1104 | `			}` |
|    160 | 1105 | `		}` |
|  73205 | 1106 | `	}` |
| 146420 | 1107 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  40394 | 1108 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      8 | 1109 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1110 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - | 1111 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - | 1112 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - | 1113 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - | 1114 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - | 1115 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - | 1116 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - | 1117 | `		 * STRING key quoted. */` |
|      - | 1118 | `		SyBlob sMsg;` |
|      8 | 1119 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 | 1120 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - | 1121 | `			SyString sKey;` |
|      6 | 1122 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1123 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 | 1124 | `			}` |
|      6 | 1125 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      6 | 1126 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      4 | 1127 | `		}else{` |
|      3 | 1128 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1129 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1130 | `			}` |
|      3 | 1131 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1132 | `		}` |
|      8 | 1133 | `		SyBlobNullAppend(&sMsg);` |
|      8 | 1134 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 | 1135 | `		SyBlobRelease(&sMsg);` |
|      3 | 1136 | `	}` |
| 146426 | 1137 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  73221 | 1138 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1139 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1140 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1141 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1142 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1143 | `			VmArithTypeName(pTos));` |
|      3 | 1144 | `	}` |
| 146431 | 1145 | `	if( pIdx ){` |
| 146431 | 1146 | `		PH7_MemObjRelease(pIdx);` |
|  73213 | 1147 | `	}` |
| 146431 | 1148 | `	if( rc == SXRET_OK ){` |
|      - | 1149 | `		/* Load entry contents */` |
|  65655 | 1150 | `		if( pMap->iRef < 2 ){` |
|      - | 1151 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1152 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1153 | `			 */` |
|    117 | 1154 | `			pTos->nIdx = SXU32_HIGH;` |
|    117 | 1155 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     60 | 1156 | `		}else{` |
|  65541 | 1157 | `			pTos->nIdx = pNode->nValIdx;` |
|  65541 | 1158 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  65541 | 1159 | `			PH7_HashmapUnref(pMap);` |
|      - | 1160 | `		}` |
|  32830 | 1161 | `	}else{` |
|      - | 1162 | `		/* No such entry,load NULL */` |
|  80781 | 1163 | `		PH7_MemObjRelease(pTos);` |
|  80781 | 1164 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1165 | `	}` |
| 146431 | 1166 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1167 | `	VM_EXIT_BREAK;` |
| 340098 | 1168 | `}` |
|      - | 1169 |  |
|      - | 1170 | `/*` |
|      - | 1171 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1172 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1173 | ` */` |
|  73970 | 1174 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1175 | `{` |
|  73975 | 1176 | `	ph7_value *pTos = pState->pTos;` |
|  73975 | 1177 | `	ph7_value *pStack = pState->pStack;` |
|  73975 | 1178 | `	VmInstr *aInstr = pState->aInstr;` |
|  73975 | 1179 | `	sxi32 pc = pState->pc;` |
|      - | 1180 | `	sxi32 rc;` |
|  36985 | 1181 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1182 | `	ph7_hashmap *pMap;` |
|      - | 1183 | `	/* Allocate a new hashmap instance */` |
|  73975 | 1184 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73975 | 1185 | `	if( pMap == 0 ){` |
|    ! 0 | 1186 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1187 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1188 | `		VM_EXIT_ABORT;` |
|      - | 1189 | `	}` |
|  73975 | 1190 | `	if( pInstr->iP1 > 0 ){` |
|  11027 | 1191 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  11027 | 1192 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1193 | `		/* Perform the insertion */` |
|  41837 | 1194 | `		while( pEntry < pTos ){` |
|  30833 | 1195 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1196 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1197 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1198 | `				 * renumbered. Same routine that backs array_merge. */` |
|    682 | 1199 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1200 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1201 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1202 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1203 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1204 | `						 * map dangling. */` |
|    ! 0 | 1205 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1206 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1207 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1208 | `						break;` |
|      1 | 1209 | `					}` |
|    353 | 1210 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1211 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1212 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1213 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1214 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1215 | `						rcSpread = rcW;` |
|    ! 0 | 1216 | `						break;` |
|      - | 1217 | `					}` |
|      3 | 1218 | `				}else{` |
|      - | 1219 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     20 | 1220 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     20 | 1221 | `					break;` |
|      1 | 1222 | `				}` |
|  30484 | 1223 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1224 | `				/* Insertion by reference */` |
|    181 | 1225 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1226 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1227 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1228 | `					);` |
|     61 | 1229 | `			}else{` |
|      - | 1230 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1231 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1232 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1233 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1234 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1235 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  30033 | 1236 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1237 | `						/* An object/array literal key is php's TypeError, a resource one` |
|      - | 1238 | `						 * warns and becomes its id — same rules as a subscript. */` |
|      - | 1239 | `						SyBlob sTypeMsg;` |
|  11217 | 1240 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|      3 | 1241 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|      2 | 1242 | `						}else{` |
|  11215 | 1243 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|      - | 1244 | `						}` |
|      - | 1245 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11217 | 1246 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16824 | 1247 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11212 | 1248 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11217 | 1249 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1250 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1251 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1252 | `							SyBlob sErrMsg;` |
|      5 | 1253 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1254 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1255 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1256 | `						}` |
|   5606 | 1257 | `					}` |
|      - | 1258 | `				/* Standard insertion */` |
|  45047 | 1259 | `				PH7_HashmapInsert(pMap,` |
|  30028 | 1260 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  15014 | 1261 | `					&pEntry[1]` |
|      - | 1262 | `				);` |
|      - | 1263 | `			}` |
|      - | 1264 | `			/* Next pair on the stack */` |
|  30815 | 1265 | `			pEntry += 2;` |
|      5 | 1266 | `		}` |
|      - | 1267 | `		/* Pop P1 elements */` |
|  11027 | 1268 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  11027 | 1269 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1270 | `			/* Discard the partially-built map and propagate the exception. */` |
|     20 | 1271 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     20 | 1272 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1273 | `				VM_EXIT_ABORT;` |
|      - | 1274 | `			}` |
|      - | 1275 | `			{` |
|      - | 1276 | `				sxi32 iRp;` |
|     20 | 1277 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1278 | `					pc = iRp;` |
|      6 | 1279 | `					VM_EXIT_BREAK;` |
|      - | 1280 | `				}` |
|      - | 1281 | `			}` |
|     15 | 1282 | `			VM_EXIT_EXCEPTION;` |
|      - | 1283 | `		}` |
|   5502 | 1284 | `	}` |
|      - | 1285 | `	/* Push the hashmap */` |
|  73957 | 1286 | `	pTos++;` |
|  73957 | 1287 | `	pTos->nIdx = SXU32_HIGH;` |
|  73957 | 1288 | `	pTos->x.pOther = pMap;` |
|  73957 | 1289 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73957 | 1290 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1291 | `	VM_EXIT_BREAK;` |
|  36990 | 1292 | `}` |
|      - | 1293 |  |
|      - | 1294 | `/*` |
|      - | 1295 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1296 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1297 | ` */` |
|    266 | 1298 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1299 | `{` |
|    271 | 1300 | `	ph7_value *pTos = pState->pTos;` |
|    271 | 1301 | `	ph7_value *pStack = pState->pStack;` |
|    271 | 1302 | `	VmInstr *aInstr = pState->aInstr;` |
|    271 | 1303 | `	sxi32 pc = pState->pc;` |
|      - | 1304 | `	sxi32 rc;` |
|    133 | 1305 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1306 | `	ph7_value *pEntry;` |
|    271 | 1307 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1308 | `		/* Empty list,break immediately */` |
|    ! 0 | 1309 | `		VM_EXIT_BREAK;` |
|      - | 1310 | `	}` |
|    271 | 1311 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1312 | `#ifdef UNTRUST` |
|      - | 1313 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1314 | `		VM_EXIT_ABORT;` |
|      - | 1315 | `	}` |
|      - | 1316 | `#endif` |
|    271 | 1317 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1318 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1319 | `		ph7_hashmap_node *pNode;` |
|      - | 1320 | `		ph7_value sKey,*pObj;` |
|      - | 1321 | `		/* Start Copying */` |
|    265 | 1322 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1323 | `		while( pEntry <= pTos ){` |
|    551 | 1324 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1325 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1326 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1327 | `					if( rc == SXRET_OK ){` |
|      - | 1328 | `						/* Store node value */` |
|    523 | 1329 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1330 | `					}else{` |
|      - | 1331 | `						/* Undefined array key */` |
|      - | 1332 | `						char zMsg[128];` |
|    ! 0 | 1333 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1334 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1335 | `						PH7_MemObjRelease(pObj);` |
|      - | 1336 | `					}` |
|    259 | 1337 | `				}` |
|    259 | 1338 | `			}` |
|    551 | 1339 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1340 | `			pEntry++;` |
|      5 | 1341 | `		}` |
|    135 | 1342 | `	}else{` |
|      - | 1343 | `		/* Source is not an array */` |
|      - | 1344 | `		ph7_value *pObj;` |
|     18 | 1345 | `		while( pEntry <= pTos ){` |
|     12 | 1346 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|     12 | 1347 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|     12 | 1348 | `					PH7_MemObjRelease(pObj);` |
|      5 | 1349 | `				}` |
|      5 | 1350 | `			}` |
|     12 | 1351 | `			pEntry++;` |
|      2 | 1352 | `		}` |
|      8 | 1353 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1354 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|      3 | 1355 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      1 | 1356 | `		}` |
|      - | 1357 | `	}` |
|    271 | 1358 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    271 | 1359 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1360 | `	VM_EXIT_BREAK;` |
|    138 | 1361 | `}` |
|      - | 1362 |  |
|      - | 1363 | `/*` |
|      - | 1364 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1365 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1366 | ` */` |
|   6754 | 1367 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1368 | `{` |
|   6759 | 1369 | `	ph7_value *pTos = pState->pTos;` |
|   6759 | 1370 | `	ph7_value *pStack = pState->pStack;` |
|   6759 | 1371 | `	VmInstr *aInstr = pState->aInstr;` |
|   6759 | 1372 | `	sxi32 pc = pState->pc;` |
|      - | 1373 | `	sxi32 rc;` |
|   3377 | 1374 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1375 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6759 | 1376 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6759 | 1377 | `	if( pName && pVm->pFrame ){` |
|      - | 1378 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1379 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1380 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6759 | 1381 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6759 | 1382 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6759 | 1383 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1384 | `			VM_EXIT_ABORT;` |
|      - | 1385 | `		}` |
|      - | 1386 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1387 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1388 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1389 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6757 | 1390 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1391 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1392 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1393 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1394 | `				VM_EXIT_ABORT;` |
|      - | 1395 | `			}` |
|      3 | 1396 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1397 | `		}` |
|   3375 | 1398 | `	}` |
|   6755 | 1399 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1400 | `	VM_EXIT_BREAK;` |
|   3382 | 1401 | `}` |
|      - | 1402 |  |
