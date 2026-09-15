# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 665/780 lines (85.26%)

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
|      4 |   29 | `{` |
|     56 |   30 | `	ph7_value *pTos = pState->pTos;` |
|     56 |   31 | `	ph7_value *pStack = pState->pStack;` |
|     56 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|     56 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|     26 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     56 |   36 | `	 SyString sName = { 0 , 0 };` |
|      - |   37 | `	 VmFrame *pFrameLocal;` |
|      - |   38 | `	SyHashEntry *pEntry;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pTos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     56 |   45 | `	if( pInstr->iP2 == 1 ){` |
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
|     46 |   94 | `	if( pInstr->p3 == 0 ){` |
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
|     46 |  111 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|      - |  112 | `	}` |
|     46 |  113 | `	nIdx = pTos->nIdx;` |
|     46 |  114 | `	if(nIdx == SXU32_HIGH ){` |
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
|     46 |  131 | `	}else if( sName.nByte > 0){` |
|     46 |  132 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
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
|     30 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/*` |
|      - |  162 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  163 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  164 | ` */` |
| 240518 |  165 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  166 | `{` |
| 240523 |  167 | `	ph7_value *pTos = pState->pTos;` |
| 240523 |  168 | `	ph7_value *pStack = pState->pStack;` |
| 240523 |  169 | `	VmInstr *aInstr = pState->aInstr;` |
| 240523 |  170 | `	sxi32 pc = pState->pc;` |
|      - |  171 | `	sxi32 rc;` |
| 120259 |  172 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 240523 |  173 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  174 | `	ph7_value *pKey;` |
|      - |  175 | `	sxu32 nIdx;` |
| 240523 |  176 | `	if( pInstr->iP1 ){` |
|      - |  177 | `		/* Key is next on stack */` |
|  69315 |  178 | `		pKey = pTos;` |
|  69315 |  179 | `		pTos--;` |
|  34660 |  180 | `	}else{` |
| 171213 |  181 | `		pKey = 0;` |
|      - |  182 | `	}` |
|      - |  183 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  184 | `		 * normalizes to ""); PHL rejects it. */` |
| 240523 |  185 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69117 |  186 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103676 |  187 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69112 |  188 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|  69117 |  189 | `			if( bNull \|\| bLossyFloat ){` |
|      - |  190 | `				sxi32 rcSc;` |
|      5 |  191 | `				const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 |  192 | `				                         : "Cannot access offset of type float on array";` |
|      5 |  193 | `				PH7_MemObjRelease(pKey);` |
|      5 |  194 | `				VmPopOperand(&pTos,1);` |
|      5 |  195 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      5 |  196 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  197 | `				rc = rcSc;` |
|      5 |  198 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  199 | `			}` |
|  34554 |  200 | `		}` |
| 240519 |  201 | `	nIdx = pTos->nIdx;` |
|      - |  202 | `	{` |
|      - |  203 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  204 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  205 | `		 * the backing variable slot at nIdx. */` |
| 240519 |  206 | `		ph7_class_instance *pInst = 0;` |
| 240519 |  207 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  208 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 240476 |  209 | `		}else if( nIdx != SXU32_HIGH ){` |
| 240433 |  210 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 240433 |  211 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  212 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  213 | `			}` |
| 120214 |  214 | `		}` |
| 240519 |  215 | `		if( pInst ){` |
|     89 |  216 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  217 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  218 | `				ph7_class_method *pMeth;` |
|      - |  219 | `				ph7_value sNullKey;` |
|      - |  220 | `				ph7_value *apArg[2];` |
|     87 |  221 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  222 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  223 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  224 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  225 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  226 | `					VM_EXIT_BREAK;` |
|      - |  227 | `				}` |
|     87 |  228 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  229 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  230 | `				/* Pop container; pTos now points to the value */` |
|     87 |  231 | `				VmPopOperand(&pTos,1);` |
|     87 |  232 | `				if( pKey == 0 ){` |
|     10 |  233 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  234 | `					apArg[0] = &sNullKey;` |
|      6 |  235 | `				}else{` |
|     79 |  236 | `					apArg[0] = pKey;` |
|      - |  237 | `				}` |
|     87 |  238 | `				apArg[1] = pTos;` |
|     87 |  239 | `				if( pMeth ){` |
|     87 |  240 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  241 | `				}` |
|     87 |  242 | `				if( pKey ){` |
|     79 |  243 | `					PH7_MemObjRelease(pKey);` |
|     41 |  244 | `				}else{` |
|     10 |  245 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  246 | `				}` |
|      - |  247 | `				/* Pop the value */` |
|     87 |  248 | `				VmPopOperand(&pTos,1);` |
|     87 |  249 | `				VM_EXIT_BREAK;` |
|      - |  250 | `			}` |
|      - |  251 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  252 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  253 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  254 | `			 * a few lines below). Match PHP. */` |
|      - |  255 | `			{` |
|      - |  256 | `				char zMsg[256];` |
|      3 |  257 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  258 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  259 | `					"Cannot use object of type %.*s as array",` |
|      2 |  260 | `					(int)pName->nByte,pName->zString);` |
|      3 |  261 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  262 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  263 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  264 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  265 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  266 | `			}` |
|      - |  267 | `		}` |
|      - |  268 | `	}` |
| 240433 |  269 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  270 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  271 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  272 | `		 * checking true sharing count, then re-add after separation. */` |
| 240299 |  273 | `		if( nIdx != SXU32_HIGH ){` |
| 240299 |  274 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 360446 |  275 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 240299 |  276 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  277 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  278 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  279 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  280 | `				 * refcounts if the backing array was already separated. */` |
| 240299 |  281 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 240299 |  282 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 240299 |  283 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 240299 |  284 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 240299 |  285 | `					pTos->x.pOther = pMap;` |
| 120152 |  286 | `				}else{` |
|      - |  287 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  288 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  289 | `					pMap = pCur;` |
|      - |  290 | `				}` |
| 120152 |  291 | `			}else{` |
|    ! 0 |  292 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  293 | `			}` |
| 120152 |  294 | `		}else{` |
|    ! 0 |  295 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  296 | `		}` |
| 240299 |  297 | `		if( pMap->iRef < 2 ){` |
|      - |  298 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  299 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  300 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  301 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  302 | `			pMap->iRef = 2;` |
|    ! 0 |  303 | `		}` |
| 120152 |  304 | `	}else{` |
|      - |  305 | `		ph7_value *pObj;` |
|    136 |  306 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    136 |  307 | `		if( pObj == 0 ){` |
|    ! 0 |  308 | `			if( pKey ){` |
|    ! 0 |  309 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  310 | `			}` |
|    ! 0 |  311 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  312 | `			VM_EXIT_BREAK;` |
|      - |  313 | `		}` |
|      - |  314 | `		/* Phase#1: Load the array */` |
|    136 |  315 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  316 | `			VmPopOperand(&pTos,1);` |
|    107 |  317 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  318 | `				/* Force a string cast */` |
|    ! 0 |  319 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  320 | `			}` |
|    107 |  321 | `			if( pKey == 0 ){` |
|      - |  322 | `				/* Append string */` |
|      3 |  323 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      3 |  324 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      1 |  325 | `				}` |
|      2 |  326 | `			}else{` |
|      - |  327 | `				sxi64 iOfft;` |
|      - |  328 | `				sxi64 nLen;` |
|    105 |  329 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  330 | `					/* Force an int cast */` |
|    ! 0 |  331 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  332 | `				}` |
|    105 |  333 | `				iOfft = pKey->x.iVal;` |
|    105 |  334 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  335 | `				if( iOfft < 0 ){` |
|      - |  336 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  337 | `					iOfft += nLen;` |
|      5 |  338 | `					if( iOfft < 0 ){` |
|      4 |  339 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  340 | `							pKey->x.iVal);` |
|      3 |  341 | `						PH7_MemObjRelease(pKey);` |
|      3 |  342 | `						VM_EXIT_BREAK;` |
|      - |  343 | `					}` |
|      1 |  344 | `				}` |
|    103 |  345 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  346 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  347 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  348 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  349 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  350 | `					}` |
|    103 |  351 | `					if( iOfft >= nLen ){` |
|      - |  352 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  353 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  354 | `						 * -- a silently wrong string. */` |
|      - |  355 | `						sxi64 nPad;` |
|      9 |  356 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  357 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  358 | `						}` |
|      3 |  359 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  360 | `					}else{` |
|    101 |  361 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  362 | `						zData[iOfft] = zBlob[0];` |
|      - |  363 | `					}` |
|     51 |  364 | `				}` |
|      - |  365 | `			}` |
|    105 |  366 | `			if( pKey ){` |
|    103 |  367 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  368 | `			}` |
|    105 |  369 | `			VM_EXIT_BREAK;` |
|     30 |  370 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  371 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  372 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  373 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  374 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  375 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     30 |  376 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     30 |  377 | `			if( bScalar ){` |
|      - |  378 | `				sxi32 rcSc;` |
|      8 |  379 | `				if( pKey ){` |
|      5 |  380 | `					PH7_MemObjRelease(pKey);` |
|      2 |  381 | `				}` |
|      8 |  382 | `				VmPopOperand(&pTos,1);` |
|      8 |  383 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  384 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  385 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  386 | `				rc = rcSc;` |
|      8 |  387 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  388 | `			}` |
|      - |  389 | `			/* Force a hashmap cast  */` |
|     24 |  390 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     24 |  391 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  392 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  393 | `				VM_EXIT_ABORT;` |
|      - |  394 | `			}` |
|     11 |  395 | `		}` |
|      - |  396 | `		/* COW separate the backing variable before mutation */` |
|     24 |  397 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  398 | `	}` |
| 240321 |  399 | `	VmPopOperand(&pTos,1);` |
|      - |  400 | `	/* Phase#2: Perform the insertion */` |
| 240321 |  401 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  402 | `		if( pMap == pVm->pGlobal ){` |
|      - |  403 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  404 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  405 | `			if( pKey == 0 ){` |
|    ! 0 |  406 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  407 | `			}else{` |
|      3 |  408 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  409 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  410 | `				}` |
|      3 |  411 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  412 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  413 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  414 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  415 | `					rc = SXRET_OK;` |
|    ! 0 |  416 | `				}else{` |
|      4 |  417 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  418 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  419 | `						0,pTos->nIdx);` |
|      - |  420 | `				}` |
|      - |  421 | `			}` |
|      2 |  422 | `		}else{` |
|      - |  423 | `			/* Insertion by reference */` |
|     17 |  424 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  425 | `		}` |
|     10 |  426 | `	}else{` |
| 240303 |  427 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  428 | `	}` |
| 240321 |  429 | `	if( pKey ){` |
|  69125 |  430 | `		PH7_MemObjRelease(pKey);` |
|  34560 |  431 | `	}` |
|      - |  432 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  433 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  434 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 240321 |  435 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 240317 |  436 | `	VM_EXIT_BREAK;` |
|    ! 0 |  437 | `	VM_EXIT_BREAK;` |
| 120264 |  438 | `}` |
|      - |  439 |  |
|      - |  440 | `/*` |
|      - |  441 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  442 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  443 | ` */` |
|   1140 |  444 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  445 | `{` |
|   1145 |  446 | `	ph7_value *pTos = pState->pTos;` |
|   1145 |  447 | `	ph7_value *pStack = pState->pStack;` |
|   1145 |  448 | `	VmInstr *aInstr = pState->aInstr;` |
|   1145 |  449 | `	sxi32 pc = pState->pc;` |
|      - |  450 | `	sxi32 rc;` |
|    570 |  451 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1145 |  452 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  453 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  454 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  455 | `	 * plain anonymous function with no captured environment. */` |
|   1145 |  456 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  457 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  458 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  459 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  460 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  461 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1145 |  462 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1145 |  463 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  464 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  465 | `		ph7_vm_func *pClosure;` |
|      - |  466 | `		char *zName;` |
|      - |  467 | `		sxu32 mLen;` |
|      - |  468 | `		sxu32 n;` |
|      - |  469 | `		/* Create a new VM function */` |
|   1135 |  470 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  471 | `		/* Generate an unique closure name */` |
|   1135 |  472 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1135 |  473 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  474 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  475 | `			VM_EXIT_ABORT;` |
|      - |  476 | `		}` |
|   1135 |  477 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1135 |  478 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  479 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  480 | `		}` |
|      - |  481 | `		/* Zero the stucture */` |
|   1135 |  482 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  483 | `		/* Perform a structure assignment on read-only items */` |
|   1135 |  484 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1135 |  485 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1135 |  486 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1135 |  487 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  488 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  489 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1135 |  490 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1135 |  491 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1135 |  492 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1135 |  493 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1135 |  494 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1135 |  495 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1135 |  496 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1135 |  497 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1135 |  498 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1135 |  499 | `		if( pClosure->pUserData == 0 ){` |
|      - |  500 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  501 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  502 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1135 |  503 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    565 |  504 | `		}` |
|      - |  505 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  506 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  507 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  508 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1135 |  509 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  510 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  511 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1135 |  512 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1135 |  513 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1135 |  514 | `		pClosure->sFile = pFunc->sFile;` |
|   1135 |  515 | `		pClosure->nLine = pFunc->nLine;` |
|   1135 |  516 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1135 |  517 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  518 | `		/* Register the closure */` |
|   1135 |  519 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  520 | `		/* Set up closure environment */` |
|   1135 |  521 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1135 |  522 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2431 |  523 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  524 | `			ph7_value *pValue;` |
|   1301 |  525 | `			pEnv = &aEnv[n];` |
|   1301 |  526 | `			sEnv.sName  = pEnv->sName;` |
|   1301 |  527 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1301 |  528 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1301 |  529 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1296 |  530 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    701 |  531 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     48 |  532 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  533 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  534 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  535 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  536 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  537 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  538 | `				 * aliases the name to this slot instead of copying a value. */` |
|     70 |  539 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     70 |  540 | `				if( pValue ){` |
|     70 |  541 | `					sEnv.nIdx = pValue->nIdx;` |
|     70 |  542 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     34 |  543 | `				}` |
|     36 |  544 | `			}else{` |
|      - |  545 | `				/* Standard pass by value */` |
|   1233 |  546 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1233 |  547 | `				if( pValue ){` |
|      - |  548 | `					/* Copy imported value */` |
|    157 |  549 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|     76 |  550 | `				}` |
|      - |  551 | `			}` |
|      - |  552 | `			/* Insert the imported variable */` |
|   1301 |  553 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    653 |  554 | `		}` |
|   1135 |  555 | `		pTarget = pClosure;` |
|    565 |  556 | `	}` |
|      - |  557 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  558 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  559 | `	 * path when the closure is dispatched by name. */` |
|   1145 |  560 | `	pTos++;` |
|      - |  561 | `	{` |
|   1145 |  562 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1145 |  563 | `		if( pCloObj ){` |
|   1145 |  564 | `			pCloObj->iRef++;` |
|   1145 |  565 | `			pTos->x.pOther = pCloObj;` |
|   1145 |  566 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    575 |  567 | `		}else{` |
|      - |  568 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  569 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  570 | `		}` |
|      - |  571 | `	}` |
|   1145 |  572 | `	VM_EXIT_BREAK;` |
|    ! 0 |  573 | `	VM_EXIT_BREAK;` |
|    575 |  574 | `}` |
|      - |  575 |  |
|      - |  576 | `/*` |
|      - |  577 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  578 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  579 | ` */` |
| 665519 |  580 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  581 | `{` |
| 665524 |  582 | `	ph7_value *pTos = pState->pTos;` |
| 665524 |  583 | `	ph7_value *pStack = pState->pStack;` |
| 665524 |  584 | `	VmInstr *aInstr = pState->aInstr;` |
| 665524 |  585 | `	sxi32 pc = pState->pc;` |
|      - |  586 | `	sxi32 rc;` |
| 333235 |  587 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 665524 |  588 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 665524 |  589 | `	ph7_hashmap *pMap = 0;` |
|      - |  590 | `	ph7_value *pIdx;` |
| 665524 |  591 | `	pIdx = 0;` |
| 665524 |  592 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  593 | `		if( !pInstr->iP2){` |
|      - |  594 | `			/* No available index,load NULL */` |
|    ! 0 |  595 | `			if( pTos >= pStack ){` |
|    ! 0 |  596 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  597 | `			}else{` |
|      - |  598 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  599 | `				pTos++;` |
|    ! 0 |  600 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  601 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  602 | `			}` |
|      - |  603 | `			/* Emit a notice */` |
|    ! 0 |  604 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  605 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  606 | `			VM_EXIT_BREAK;` |
|      - |  607 | `		}` |
|      2 |  608 | `	}else{` |
| 665522 |  609 | `		pIdx = pTos;` |
| 665522 |  610 | `		pTos--;` |
|      - |  611 | `	}` |
| 665524 |  612 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  613 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  614 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  615 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  616 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|    ! 0 |  617 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|    ! 0 |  618 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|    ! 0 |  619 | `		}` |
|    ! 0 |  620 | `		if( pIdx ){` |
|      - |  621 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  622 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|    ! 0 |  623 | `			PH7_MemObjRelease(pIdx);` |
|    ! 0 |  624 | `		}` |
|    ! 0 |  625 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  626 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  627 | `		VM_EXIT_BREAK;` |
|      - |  628 | `	}` |
| 665524 |  629 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  630 | `		/* String access */` |
| 519412 |  631 | `		if( pIdx ){` |
|      - |  632 | `			sxi64 iOfft;` |
| 519412 |  633 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 519412 |  634 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  635 | `				/* Force an int cast */` |
|    ! 0 |  636 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  637 | `			}` |
| 519412 |  638 | `			iOfft = pIdx->x.iVal;` |
|      - |  639 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  640 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  641 | `			 * number, ran past the end and quietly produced NULL. */` |
| 519412 |  642 | `			if( iOfft < 0 ){` |
|      7 |  643 | `				iOfft += nLen;` |
|      3 |  644 | `			}` |
| 519415 |  645 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  646 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  647 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  648 | `				 * silently produced NULL in both cases). */` |
|      - |  649 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  650 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  651 | `				 * lookups and must stay silent. */` |
|      7 |  652 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6;` |
|      7 |  653 | `				PH7_MemObjRelease(pTos);` |
|      7 |  654 | `				if( bQuiet ){` |
|      5 |  655 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  656 | `				}else{` |
|      3 |  657 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  658 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  659 | `						pIdx->x.iVal);` |
|      - |  660 | `				}` |
|      4 |  661 | `			}else{` |
| 519406 |  662 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 519406 |  663 | `				int c = zData[iOfft];` |
| 519406 |  664 | `				PH7_MemObjRelease(pTos);` |
| 519406 |  665 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 519406 |  666 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  667 | `			}` |
| 260073 |  668 | `		}else{` |
|      - |  669 | `			/* No available index,load NULL */` |
|    ! 0 |  670 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  671 | `		}` |
| 519412 |  672 | `		VM_EXIT_BREAK;` |
|      - |  673 | `	}` |
| 146117 |  674 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  675 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  676 | `		 * iP2 codes:` |
|      - |  677 | `		 *   0 = read       → offsetGet` |
|      - |  678 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  679 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  680 | `		 *   4 = isset()    → offsetExists` |
|      - |  681 | `		 *   5 = unset()    → offsetUnset` |
|      - |  682 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|    179 |  683 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    179 |  684 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    179 |  685 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  686 | `			ph7_class_method *pMeth;` |
|      - |  687 | `			ph7_value sResult;` |
|      - |  688 | `			ph7_value *apArg[1];` |
|    177 |  689 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|      - |  690 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  691 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  692 | `					"Cannot use [] for reading");` |
|    ! 0 |  693 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  694 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  695 | `				VM_EXIT_BREAK;` |
|      - |  696 | `			}` |
|    177 |  697 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    177 |  698 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|      - |  699 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|     71 |  700 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  701 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     71 |  702 | `				apArg[0] = pIdx;` |
|     71 |  703 | `				if( pMeth ){` |
|     71 |  704 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     38 |  705 | `				}` |
|    144 |  706 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  707 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  708 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  709 | `				apArg[0] = pIdx;` |
|     20 |  710 | `				if( pMeth ){` |
|     20 |  711 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  712 | `				}` |
|     12 |  713 | `			}else{` |
|     95 |  714 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  715 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     95 |  716 | `				apArg[0] = pIdx;` |
|     95 |  717 | `				if( pMeth ){` |
|     95 |  718 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     45 |  719 | `				}` |
|      - |  720 | `			}` |
|    177 |  721 | `			if( pInstr->iP2 == 4 ){` |
|      - |  722 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  723 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  724 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  725 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  726 | `				PH7_MemObjRelease(pTos);` |
|     53 |  727 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  728 | `				if( bExists ){` |
|     28 |  729 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  730 | `					pTos->x.iVal = 1;` |
|     16 |  731 | `				}else{` |
|     29 |  732 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  733 | `				}` |
|    153 |  734 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  735 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  736 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  737 | `				PH7_MemObjRelease(pTos);` |
|     20 |  738 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  739 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    121 |  740 | `			}else if( pInstr->iP2 == 6 ){` |
|      - |  741 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  742 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  743 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|     11 |  744 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     11 |  745 | `				PH7_MemObjRelease(&sResult);` |
|     11 |  746 | `				PH7_MemObjRelease(pTos);` |
|     11 |  747 | `				pTos->nIdx = SXU32_HIGH;` |
|     11 |  748 | `				if( !bExists ){` |
|      3 |  749 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      2 |  750 | `				}else{` |
|      9 |  751 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  752 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  753 | `					ph7_value sValue;` |
|      9 |  754 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      9 |  755 | `					apArg[0] = pIdx;` |
|      9 |  756 | `					if( pGet ){` |
|      9 |  757 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      4 |  758 | `					}` |
|      9 |  759 | `					PH7_MemObjStore(&sValue,pTos);` |
|      9 |  760 | `					PH7_MemObjRelease(&sValue);` |
|      - |  761 | `				}` |
|     11 |  762 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     11 |  763 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|    103 |  764 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  765 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  766 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  767 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  768 | `				 *     and push NULL.` |
|      - |  769 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  770 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  771 | `				int bShouldArm = !bExists;` |
|      - |  772 | `				ph7_value sValue;` |
|     10 |  773 | `				PH7_MemObjRelease(&sResult);` |
|      - |  774 | `				/* Reset any prior arming defensively */` |
|     10 |  775 | `				VmCoalesceDisarm(pVm);` |
|     10 |  776 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  777 | `				if( bExists ){` |
|      5 |  778 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  779 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  780 | `					apArg[0] = pIdx;` |
|      5 |  781 | `					if( pGet ){` |
|      5 |  782 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  783 | `					}` |
|      5 |  784 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  785 | `						bShouldArm = 1;` |
|      1 |  786 | `					}` |
|      2 |  787 | `				}` |
|     10 |  788 | `				PH7_MemObjRelease(pTos);` |
|     10 |  789 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  790 | `				if( bShouldArm ){` |
|      - |  791 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  792 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  793 | `					 * intervening expression evaluation. */` |
|      8 |  794 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  795 | `					if( pIdx ){` |
|      8 |  796 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  797 | `					}` |
|      8 |  798 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  799 | `					pInst->iRef++;` |
|      8 |  800 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  801 | `				}else{` |
|      3 |  802 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  803 | `				}` |
|     10 |  804 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  805 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  806 | `				VM_EXIT_BREAK;` |
|    ! 0 |  807 | `			}else{` |
|      - |  808 | `				/* offsetGet: replace pTos with the returned value. */` |
|     95 |  809 | `				PH7_MemObjRelease(pTos);` |
|     95 |  810 | `				PH7_MemObjStore(&sResult,pTos);` |
|     95 |  811 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  812 | `			}` |
|    159 |  813 | `			PH7_MemObjRelease(&sResult);` |
|    159 |  814 | `			if( pIdx ){` |
|    159 |  815 | `				PH7_MemObjRelease(pIdx);` |
|     77 |  816 | `			}` |
|    159 |  817 | `			VM_EXIT_BREAK;` |
|      - |  818 | `		}` |
|      - |  819 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  820 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  821 | `		if( pInst ){` |
|      - |  822 | `			char zMsg[256];` |
|      3 |  823 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  824 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  825 | `				"Cannot use object of type %.*s as array",` |
|      2 |  826 | `				(int)pName->nByte,pName->zString);` |
|      3 |  827 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  828 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  829 | `			PH7_MemObjRelease(pTos);` |
|      3 |  830 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  831 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  832 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  833 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  834 | `			 * execution carried on inside the try block. */` |
|      3 |  835 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  836 | `		}` |
|    ! 0 |  837 | `	}` |
| 145943 |  838 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  839 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  840 | `			ph7_value *pObj;` |
|     23 |  841 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  842 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  843 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  844 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  845 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  846 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  847 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  848 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  849 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  850 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - |  851 | `				 * it is not a bool). */` |
|     23 |  852 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - |  853 | `					SyBlob sErrMsg;` |
|      7 |  854 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 |  855 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - |  856 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 |  857 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 |  858 | `					if( pIdx ){` |
|      7 |  859 | `						PH7_MemObjRelease(pIdx);` |
|      3 |  860 | `					}` |
|      7 |  861 | `					PH7_MemObjRelease(pTos);` |
|      7 |  862 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 |  863 | `					VM_EXIT_BREAK;` |
|      - |  864 | `				}` |
|     17 |  865 | `				PH7_MemObjToHashmap(pObj);` |
|     17 |  866 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 |  867 | `			}` |
|      8 |  868 | `		}` |
|      8 |  869 | `	}` |
| 145937 |  870 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - |  871 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - |  872 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - |  873 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
| 145932 |  874 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 145710 |  875 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  34615 |  876 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  51923 |  877 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  34610 |  878 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  34615 |  879 | `		if( bNull \|\| bLossyFloat ){` |
|      - |  880 | `			SyBlob sErrMsg;` |
|      5 |  881 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 |  882 | `			SyBlobAppend(&sErrMsg,` |
|      2 |  883 | `				bNull ? "Cannot access offset of type null on array"` |
|      - |  884 | `				      : "Cannot access offset of type float on array",` |
|      4 |  885 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - |  886 | `				                      : "Cannot access offset of type float on array"));` |
|      5 |  887 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 |  888 | `			PH7_MemObjRelease(pIdx);` |
|      5 |  889 | `			PH7_MemObjRelease(pTos);` |
|      5 |  890 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 |  891 | `			VM_EXIT_BREAK;` |
|      - |  892 | `		}` |
|  17303 |  893 | `	}` |
| 145933 |  894 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 145707 |  895 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - |  896 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - |  897 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - |  898 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - |  899 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - |  900 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - |  901 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1263 |  902 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    629 |  903 | `		}` |
|      - |  904 | `		/* Point to the hashmap */` |
| 145707 |  905 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 145707 |  906 | `		if( pIdx ){` |
|      - |  907 | `			/* Load the desired entry */` |
| 145705 |  908 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  72850 |  909 | `		}` |
| 145707 |  910 | `		if( pInstr->iP2 == 3 ){` |
|      - |  911 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - |  912 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - |  913 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - |  914 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - |  915 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - |  916 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - |  917 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - |  918 | `			 * correct for the outermost write. */` |
|     21 |  919 | `			int needWrite = (rc != SXRET_OK);` |
|     21 |  920 | `			if( !needWrite && pNode ){` |
|     13 |  921 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 |  922 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 |  923 | `					needWrite = 1;` |
|      3 |  924 | `				}` |
|      6 |  925 | `			}` |
|     21 |  926 | `			if( needWrite ){` |
|     15 |  927 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 |  928 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - |  929 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - |  930 | `					 * into the new map's storage. */` |
|      7 |  931 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 |  932 | `					if( pIdx ){` |
|      7 |  933 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 |  934 | `					}` |
|      3 |  935 | `				}` |
|      7 |  936 | `			}` |
|     10 |  937 | `		}` |
| 145707 |  938 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - |  939 | `			/* Create a new empty entry */` |
|    324 |  940 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 |  941 | `			if( rc == SXRET_OK ){` |
|      - |  942 | `				/* Point to the last inserted entry */` |
|    321 |  943 | `				pNode = pMap->pLast;` |
|    161 |  944 | `			}else{` |
|      - |  945 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - |  946 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - |  947 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - |  948 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 |  949 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - |  950 | `			}` |
|    160 |  951 | `		}` |
|  72850 |  952 | `	}` |
| 145926 |  953 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  39894 |  954 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|    118 |  955 | `	 && !((pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP) ){` |
|      - |  956 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - |  957 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - |  958 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - |  959 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - |  960 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - |  961 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - |  962 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - |  963 | `		 * STRING key quoted. */` |
|      - |  964 | `		SyBlob sMsg;` |
|      5 |  965 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      5 |  966 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - |  967 | `			SyString sKey;` |
|      3 |  968 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  969 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 |  970 | `			}` |
|      3 |  971 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      3 |  972 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      2 |  973 | `		}else{` |
|      3 |  974 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  975 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  976 | `			}` |
|      3 |  977 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - |  978 | `		}` |
|      5 |  979 | `		SyBlobNullAppend(&sMsg);` |
|      5 |  980 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      5 |  981 | `		SyBlobRelease(&sMsg);` |
|      2 |  982 | `	}` |
| 145704 |  983 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  72859 |  984 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2) ){` |
|      - |  985 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - |  986 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|      7 |  987 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      2 |  988 | `			VmArithTypeName(pTos));` |
|      2 |  989 | `	}` |
| 145709 |  990 | `	if( pIdx ){` |
| 145709 |  991 | `		PH7_MemObjRelease(pIdx);` |
|  72852 |  992 | `	}` |
| 145709 |  993 | `	if( rc == SXRET_OK ){` |
|      - |  994 | `		/* Load entry contents */` |
|  66151 |  995 | `		if( pMap->iRef < 2 ){` |
|      - |  996 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - |  997 | `			 * of the entry value,rather than pointing to it.` |
|      - |  998 | `			 */` |
|    101 |  999 | `			pTos->nIdx = SXU32_HIGH;` |
|    101 | 1000 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     52 | 1001 | `		}else{` |
|  66053 | 1002 | `			pTos->nIdx = pNode->nValIdx;` |
|  66053 | 1003 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  66053 | 1004 | `			PH7_HashmapUnref(pMap);` |
|      - | 1005 | `		}` |
|  33078 | 1006 | `	}else{` |
|      - | 1007 | `		/* No such entry,load NULL */` |
|  79563 | 1008 | `		PH7_MemObjRelease(pTos);` |
|  79563 | 1009 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1010 | `	}` |
| 145709 | 1011 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1012 | `	VM_EXIT_BREAK;` |
| 333018 | 1013 | `}` |
|      - | 1014 |  |
|      - | 1015 | `/*` |
|      - | 1016 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1017 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1018 | ` */` |
|  73666 | 1019 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1020 | `{` |
|  73671 | 1021 | `	ph7_value *pTos = pState->pTos;` |
|  73671 | 1022 | `	ph7_value *pStack = pState->pStack;` |
|  73671 | 1023 | `	VmInstr *aInstr = pState->aInstr;` |
|  73671 | 1024 | `	sxi32 pc = pState->pc;` |
|      - | 1025 | `	sxi32 rc;` |
|  36833 | 1026 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1027 | `	ph7_hashmap *pMap;` |
|      - | 1028 | `	/* Allocate a new hashmap instance */` |
|  73671 | 1029 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73671 | 1030 | `	if( pMap == 0 ){` |
|    ! 0 | 1031 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1032 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1033 | `		VM_EXIT_ABORT;` |
|      - | 1034 | `	}` |
|  73671 | 1035 | `	if( pInstr->iP1 > 0 ){` |
|  10923 | 1036 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  10923 | 1037 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1038 | `		/* Perform the insertion */` |
|  41269 | 1039 | `		while( pEntry < pTos ){` |
|  30369 | 1040 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1041 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1042 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1043 | `				 * renumbered. Same routine that backs array_merge. */` |
|    682 | 1044 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1045 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1046 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1047 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1048 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1049 | `						 * map dangling. */` |
|    ! 0 | 1050 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1051 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1052 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1053 | `						break;` |
|      1 | 1054 | `					}` |
|    353 | 1055 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1056 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1057 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1058 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1059 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1060 | `						rcSpread = rcW;` |
|    ! 0 | 1061 | `						break;` |
|      - | 1062 | `					}` |
|      3 | 1063 | `				}else{` |
|      - | 1064 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     20 | 1065 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     20 | 1066 | `					break;` |
|      1 | 1067 | `				}` |
|  30020 | 1068 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1069 | `				/* Insertion by reference */` |
|    181 | 1070 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1071 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1072 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1073 | `					);` |
|     61 | 1074 | `			}else{` |
|      - | 1075 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1076 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1077 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1078 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1079 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1080 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  29569 | 1081 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1082 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11187 | 1083 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16779 | 1084 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11182 | 1085 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11187 | 1086 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1087 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1088 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1089 | `							SyBlob sErrMsg;` |
|      5 | 1090 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1091 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1092 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1093 | `						}` |
|   5591 | 1094 | `					}` |
|      - | 1095 | `				/* Standard insertion */` |
|  44351 | 1096 | `				PH7_HashmapInsert(pMap,` |
|  29564 | 1097 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  14782 | 1098 | `					&pEntry[1]` |
|      - | 1099 | `				);` |
|      - | 1100 | `			}` |
|      - | 1101 | `			/* Next pair on the stack */` |
|  30351 | 1102 | `			pEntry += 2;` |
|      5 | 1103 | `		}` |
|      - | 1104 | `		/* Pop P1 elements */` |
|  10923 | 1105 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  10923 | 1106 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1107 | `			/* Discard the partially-built map and propagate the exception. */` |
|     20 | 1108 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     20 | 1109 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1110 | `				VM_EXIT_ABORT;` |
|      - | 1111 | `			}` |
|      - | 1112 | `			{` |
|      - | 1113 | `				sxi32 iRp;` |
|     20 | 1114 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1115 | `					pc = iRp;` |
|      6 | 1116 | `					VM_EXIT_BREAK;` |
|      - | 1117 | `				}` |
|      - | 1118 | `			}` |
|     15 | 1119 | `			VM_EXIT_EXCEPTION;` |
|      - | 1120 | `		}` |
|   5450 | 1121 | `	}` |
|      - | 1122 | `	/* Push the hashmap */` |
|  73653 | 1123 | `	pTos++;` |
|  73653 | 1124 | `	pTos->nIdx = SXU32_HIGH;` |
|  73653 | 1125 | `	pTos->x.pOther = pMap;` |
|  73653 | 1126 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73653 | 1127 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1128 | `	VM_EXIT_BREAK;` |
|  36838 | 1129 | `}` |
|      - | 1130 |  |
|      - | 1131 | `/*` |
|      - | 1132 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1133 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1134 | ` */` |
|    264 | 1135 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1136 | `{` |
|    269 | 1137 | `	ph7_value *pTos = pState->pTos;` |
|    269 | 1138 | `	ph7_value *pStack = pState->pStack;` |
|    269 | 1139 | `	VmInstr *aInstr = pState->aInstr;` |
|    269 | 1140 | `	sxi32 pc = pState->pc;` |
|      - | 1141 | `	sxi32 rc;` |
|    132 | 1142 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1143 | `	ph7_value *pEntry;` |
|    269 | 1144 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1145 | `		/* Empty list,break immediately */` |
|    ! 0 | 1146 | `		VM_EXIT_BREAK;` |
|      - | 1147 | `	}` |
|    269 | 1148 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1149 | `#ifdef UNTRUST` |
|      - | 1150 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1151 | `		VM_EXIT_ABORT;` |
|      - | 1152 | `	}` |
|      - | 1153 | `#endif` |
|    269 | 1154 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1155 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1156 | `		ph7_hashmap_node *pNode;` |
|      - | 1157 | `		ph7_value sKey,*pObj;` |
|      - | 1158 | `		/* Start Copying */` |
|    265 | 1159 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1160 | `		while( pEntry <= pTos ){` |
|    551 | 1161 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1162 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1163 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1164 | `					if( rc == SXRET_OK ){` |
|      - | 1165 | `						/* Store node value */` |
|    523 | 1166 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1167 | `					}else{` |
|      - | 1168 | `						/* Undefined array key */` |
|      - | 1169 | `						char zMsg[128];` |
|    ! 0 | 1170 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1171 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1172 | `						PH7_MemObjRelease(pObj);` |
|      - | 1173 | `					}` |
|    259 | 1174 | `				}` |
|    259 | 1175 | `			}` |
|    551 | 1176 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1177 | `			pEntry++;` |
|      5 | 1178 | `		}` |
|    135 | 1179 | `	}else{` |
|      - | 1180 | `		/* Source is not an array */` |
|      - | 1181 | `		ph7_value *pObj;` |
|     13 | 1182 | `		while( pEntry <= pTos ){` |
|      9 | 1183 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      9 | 1184 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      9 | 1185 | `					PH7_MemObjRelease(pObj);` |
|      4 | 1186 | `				}` |
|      4 | 1187 | `			}` |
|      9 | 1188 | `			pEntry++;` |
|      1 | 1189 | `		}` |
|      5 | 1190 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1191 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|    ! 0 | 1192 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|    ! 0 | 1193 | `		}` |
|      - | 1194 | `	}` |
|    269 | 1195 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    269 | 1196 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1197 | `	VM_EXIT_BREAK;` |
|    137 | 1198 | `}` |
|      - | 1199 |  |
|      - | 1200 | `/*` |
|      - | 1201 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1202 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1203 | ` */` |
|   6786 | 1204 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1205 | `{` |
|   6791 | 1206 | `	ph7_value *pTos = pState->pTos;` |
|   6791 | 1207 | `	ph7_value *pStack = pState->pStack;` |
|   6791 | 1208 | `	VmInstr *aInstr = pState->aInstr;` |
|   6791 | 1209 | `	sxi32 pc = pState->pc;` |
|      - | 1210 | `	sxi32 rc;` |
|   3393 | 1211 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1212 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6791 | 1213 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6791 | 1214 | `	if( pName && pVm->pFrame ){` |
|      - | 1215 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1216 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1217 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6791 | 1218 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6791 | 1219 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6791 | 1220 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1221 | `			VM_EXIT_ABORT;` |
|      - | 1222 | `		}` |
|      - | 1223 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1224 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1225 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1226 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6789 | 1227 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1228 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1229 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1230 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1231 | `				VM_EXIT_ABORT;` |
|      - | 1232 | `			}` |
|      3 | 1233 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1234 | `		}` |
|   3391 | 1235 | `	}` |
|   6787 | 1236 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1237 | `	VM_EXIT_BREAK;` |
|   3398 | 1238 | `}` |
|      - | 1239 |  |
