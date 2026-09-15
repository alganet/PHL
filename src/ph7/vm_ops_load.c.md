# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 671/787 lines (85.26%)

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
|     42 |  139 | `			pFrameLocal = pVm->pFrame;` |
|     42 |  140 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  141 | `			/* Query the local frame */` |
|     42 |  142 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|     42 |  143 | `			if( pEntry ){` |
|    ! 0 |  144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|    ! 0 |  145 | `			}else{` |
|     42 |  146 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|     42 |  147 | `				if( pFrameLocal->pParent == 0 ){` |
|      - |  148 | `					/* Insert in the $GLOBALS array */` |
|     38 |  149 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|     18 |  150 | `				}` |
|     42 |  151 | `				if( rc == SXRET_OK ){` |
|     42 |  152 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     20 |  153 | `				}` |
|      - |  154 | `			}` |
|      - |  155 | `		}` |
|     20 |  156 | `	}` |
|     42 |  157 | `	VM_EXIT_BREAK;` |
|    ! 0 |  158 | `	VM_EXIT_BREAK;` |
|     29 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/*` |
|      - |  162 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  163 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  164 | ` */` |
| 240792 |  165 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  166 | `{` |
| 240797 |  167 | `	ph7_value *pTos = pState->pTos;` |
| 240797 |  168 | `	ph7_value *pStack = pState->pStack;` |
| 240797 |  169 | `	VmInstr *aInstr = pState->aInstr;` |
| 240797 |  170 | `	sxi32 pc = pState->pc;` |
|      - |  171 | `	sxi32 rc;` |
| 120396 |  172 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 240797 |  173 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  174 | `	ph7_value *pKey;` |
|      - |  175 | `	sxu32 nIdx;` |
| 240797 |  176 | `	if( pInstr->iP1 ){` |
|      - |  177 | `		/* Key is next on stack */` |
|  69305 |  178 | `		pKey = pTos;` |
|  69305 |  179 | `		pTos--;` |
|  34655 |  180 | `	}else{` |
| 171497 |  181 | `		pKey = 0;` |
|      - |  182 | `	}` |
|      - |  183 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  184 | `		 * normalizes to ""); PHL rejects it. */` |
| 240797 |  185 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69107 |  186 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103661 |  187 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69102 |  188 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|  69107 |  189 | `			if( bNull \|\| bLossyFloat ){` |
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
|  34549 |  200 | `		}` |
| 240793 |  201 | `	nIdx = pTos->nIdx;` |
|      - |  202 | `	{` |
|      - |  203 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  204 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  205 | `		 * the backing variable slot at nIdx. */` |
| 240793 |  206 | `		ph7_class_instance *pInst = 0;` |
| 240793 |  207 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  208 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 240750 |  209 | `		}else if( nIdx != SXU32_HIGH ){` |
| 240707 |  210 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 240707 |  211 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  212 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  213 | `			}` |
| 120351 |  214 | `		}` |
| 240793 |  215 | `		if( pInst ){` |
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
| 240707 |  269 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  270 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  271 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  272 | `		 * checking true sharing count, then re-add after separation. */` |
| 240571 |  273 | `		if( nIdx != SXU32_HIGH ){` |
| 240571 |  274 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 360854 |  275 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 240571 |  276 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  277 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  278 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  279 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  280 | `				 * refcounts if the backing array was already separated. */` |
| 240571 |  281 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 240571 |  282 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 240571 |  283 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 240571 |  284 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 240571 |  285 | `					pTos->x.pOther = pMap;` |
| 120288 |  286 | `				}else{` |
|      - |  287 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  288 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  289 | `					pMap = pCur;` |
|      - |  290 | `				}` |
| 120288 |  291 | `			}else{` |
|    ! 0 |  292 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  293 | `			}` |
| 120288 |  294 | `		}else{` |
|    ! 0 |  295 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  296 | `		}` |
| 240571 |  297 | `		if( pMap->iRef < 2 ){` |
|      - |  298 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  299 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  300 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  301 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  302 | `			pMap->iRef = 2;` |
|    ! 0 |  303 | `		}` |
| 120288 |  304 | `	}else{` |
|      - |  305 | `		ph7_value *pObj;` |
|    139 |  306 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    139 |  307 | `		if( pObj == 0 ){` |
|    ! 0 |  308 | `			if( pKey ){` |
|    ! 0 |  309 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  310 | `			}` |
|    ! 0 |  311 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  312 | `			VM_EXIT_BREAK;` |
|      - |  313 | `		}` |
|      - |  314 | `		/* Phase#1: Load the array */` |
|    139 |  315 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  316 | `			VmPopOperand(&pTos,1);` |
|    107 |  317 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  318 | `				/* Force a string cast */` |
|    ! 0 |  319 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  320 | `			}` |
|    107 |  321 | `			if( pKey == 0 ){` |
|      - |  322 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|      - |  323 | `				 * "[] operator not supported for strings" and leaves the string` |
|      - |  324 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|      - |  325 | `				 * an array from a variable holding a string quietly produced a` |
|      - |  326 | `				 * longer string instead of failing — a wrong answer, not a` |
|      - |  327 | `				 * missing diagnostic. */` |
|      - |  328 | `				SyBlob sErrMsg;` |
|      3 |  329 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  330 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|      - |  331 | `					sizeof("[] operator not supported for strings")-1);` |
|      3 |  332 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  333 | `				VM_EXIT_BREAK;` |
|    ! 0 |  334 | `			}else{` |
|      - |  335 | `				sxi64 iOfft;` |
|      - |  336 | `				sxi64 nLen;` |
|    105 |  337 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  338 | `					/* Force an int cast */` |
|    ! 0 |  339 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  340 | `				}` |
|    105 |  341 | `				iOfft = pKey->x.iVal;` |
|    105 |  342 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  343 | `				if( iOfft < 0 ){` |
|      - |  344 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  345 | `					iOfft += nLen;` |
|      5 |  346 | `					if( iOfft < 0 ){` |
|      4 |  347 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  348 | `							pKey->x.iVal);` |
|      3 |  349 | `						PH7_MemObjRelease(pKey);` |
|      3 |  350 | `						VM_EXIT_BREAK;` |
|      - |  351 | `					}` |
|      1 |  352 | `				}` |
|    103 |  353 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  354 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  355 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  356 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  357 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  358 | `					}` |
|    103 |  359 | `					if( iOfft >= nLen ){` |
|      - |  360 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  361 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  362 | `						 * -- a silently wrong string. */` |
|      - |  363 | `						sxi64 nPad;` |
|      9 |  364 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  365 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  366 | `						}` |
|      3 |  367 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  368 | `					}else{` |
|    101 |  369 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  370 | `						zData[iOfft] = zBlob[0];` |
|      - |  371 | `					}` |
|     51 |  372 | `				}` |
|      - |  373 | `			}` |
|    103 |  374 | `			if( pKey ){` |
|    103 |  375 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  376 | `			}` |
|    103 |  377 | `			VM_EXIT_BREAK;` |
|     33 |  378 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  379 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  380 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  381 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  382 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  383 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     33 |  384 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     33 |  385 | `			if( bScalar ){` |
|      - |  386 | `				sxi32 rcSc;` |
|      8 |  387 | `				if( pKey ){` |
|      5 |  388 | `					PH7_MemObjRelease(pKey);` |
|      2 |  389 | `				}` |
|      8 |  390 | `				VmPopOperand(&pTos,1);` |
|      8 |  391 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  392 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  393 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  394 | `				rc = rcSc;` |
|      8 |  395 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  396 | `			}` |
|      - |  397 | `			/* Force a hashmap cast  */` |
|     27 |  398 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     27 |  399 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  400 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  401 | `				VM_EXIT_ABORT;` |
|      - |  402 | `			}` |
|     12 |  403 | `		}` |
|      - |  404 | `		/* COW separate the backing variable before mutation */` |
|     27 |  405 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  406 | `	}` |
| 240595 |  407 | `	VmPopOperand(&pTos,1);` |
|      - |  408 | `	/* Phase#2: Perform the insertion */` |
| 240595 |  409 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  410 | `		if( pMap == pVm->pGlobal ){` |
|      - |  411 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  412 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  413 | `			if( pKey == 0 ){` |
|    ! 0 |  414 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  415 | `			}else{` |
|      3 |  416 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  417 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  418 | `				}` |
|      3 |  419 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  420 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  421 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  422 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  423 | `					rc = SXRET_OK;` |
|    ! 0 |  424 | `				}else{` |
|      4 |  425 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  426 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  427 | `						0,pTos->nIdx);` |
|      - |  428 | `				}` |
|      - |  429 | `			}` |
|      2 |  430 | `		}else{` |
|      - |  431 | `			/* Insertion by reference */` |
|     17 |  432 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  433 | `		}` |
|     10 |  434 | `	}else{` |
| 240577 |  435 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  436 | `	}` |
| 240595 |  437 | `	if( pKey ){` |
|  69115 |  438 | `		PH7_MemObjRelease(pKey);` |
|  34555 |  439 | `	}` |
|      - |  440 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  441 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  442 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 240595 |  443 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 240591 |  444 | `	VM_EXIT_BREAK;` |
|    ! 0 |  445 | `	VM_EXIT_BREAK;` |
| 120401 |  446 | `}` |
|      - |  447 |  |
|      - |  448 | `/*` |
|      - |  449 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  450 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  451 | ` */` |
|   1146 |  452 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  453 | `{` |
|   1151 |  454 | `	ph7_value *pTos = pState->pTos;` |
|   1151 |  455 | `	ph7_value *pStack = pState->pStack;` |
|   1151 |  456 | `	VmInstr *aInstr = pState->aInstr;` |
|   1151 |  457 | `	sxi32 pc = pState->pc;` |
|      - |  458 | `	sxi32 rc;` |
|    573 |  459 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1151 |  460 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  461 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  462 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  463 | `	 * plain anonymous function with no captured environment. */` |
|   1151 |  464 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  465 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  466 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  467 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  468 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  469 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1151 |  470 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1151 |  471 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  472 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  473 | `		ph7_vm_func *pClosure;` |
|      - |  474 | `		char *zName;` |
|      - |  475 | `		sxu32 mLen;` |
|      - |  476 | `		sxu32 n;` |
|      - |  477 | `		/* Create a new VM function */` |
|   1141 |  478 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  479 | `		/* Generate an unique closure name */` |
|   1141 |  480 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1141 |  481 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  482 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  483 | `			VM_EXIT_ABORT;` |
|      - |  484 | `		}` |
|   1141 |  485 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1141 |  486 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  487 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  488 | `		}` |
|      - |  489 | `		/* Zero the stucture */` |
|   1141 |  490 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  491 | `		/* Perform a structure assignment on read-only items */` |
|   1141 |  492 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1141 |  493 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1141 |  494 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1141 |  495 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  496 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  497 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1141 |  498 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1141 |  499 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1141 |  500 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1141 |  501 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1141 |  502 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1141 |  503 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1141 |  504 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1141 |  505 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1141 |  506 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1141 |  507 | `		if( pClosure->pUserData == 0 ){` |
|      - |  508 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  509 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  510 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1141 |  511 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    568 |  512 | `		}` |
|      - |  513 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  514 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  515 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  516 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1141 |  517 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  518 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  519 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1141 |  520 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1141 |  521 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1141 |  522 | `		pClosure->sFile = pFunc->sFile;` |
|   1141 |  523 | `		pClosure->nLine = pFunc->nLine;` |
|   1141 |  524 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1141 |  525 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  526 | `		/* Register the closure */` |
|   1141 |  527 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  528 | `		/* Set up closure environment */` |
|   1141 |  529 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1141 |  530 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2443 |  531 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  532 | `			ph7_value *pValue;` |
|   1307 |  533 | `			pEnv = &aEnv[n];` |
|   1307 |  534 | `			sEnv.sName  = pEnv->sName;` |
|   1307 |  535 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1307 |  536 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1307 |  537 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1302 |  538 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    704 |  539 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     48 |  540 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  541 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  542 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  543 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  544 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  545 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  546 | `				 * aliases the name to this slot instead of copying a value. */` |
|     70 |  547 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     70 |  548 | `				if( pValue ){` |
|     70 |  549 | `					sEnv.nIdx = pValue->nIdx;` |
|     70 |  550 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     34 |  551 | `				}` |
|     36 |  552 | `			}else{` |
|      - |  553 | `				/* Standard pass by value */` |
|   1239 |  554 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1239 |  555 | `				if( pValue ){` |
|      - |  556 | `					/* Copy imported value */` |
|    156 |  557 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|     76 |  558 | `				}` |
|      - |  559 | `			}` |
|      - |  560 | `			/* Insert the imported variable */` |
|   1307 |  561 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    656 |  562 | `		}` |
|   1141 |  563 | `		pTarget = pClosure;` |
|    568 |  564 | `	}` |
|      - |  565 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  566 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  567 | `	 * path when the closure is dispatched by name. */` |
|   1151 |  568 | `	pTos++;` |
|      - |  569 | `	{` |
|   1151 |  570 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1151 |  571 | `		if( pCloObj ){` |
|   1151 |  572 | `			pCloObj->iRef++;` |
|   1151 |  573 | `			pTos->x.pOther = pCloObj;` |
|   1151 |  574 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    578 |  575 | `		}else{` |
|      - |  576 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  577 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  578 | `		}` |
|      - |  579 | `	}` |
|   1151 |  580 | `	VM_EXIT_BREAK;` |
|    ! 0 |  581 | `	VM_EXIT_BREAK;` |
|    578 |  582 | `}` |
|      - |  583 |  |
|      - |  584 |  |
|      - |  585 | `/*` |
|      - |  586 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  587 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  588 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  589 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  590 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  591 | ` */` |
|     14 |  592 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  593 | `{` |
|     17 |  594 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  595 | `}` |
|      - |  596 | `/*` |
|      - |  597 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  598 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  599 | ` */` |
| 665582 |  600 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  601 | `{` |
| 665587 |  602 | `	ph7_value *pTos = pState->pTos;` |
| 665587 |  603 | `	ph7_value *pStack = pState->pStack;` |
| 665587 |  604 | `	VmInstr *aInstr = pState->aInstr;` |
| 665587 |  605 | `	sxi32 pc = pState->pc;` |
|      - |  606 | `	sxi32 rc;` |
| 333154 |  607 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 665587 |  608 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 665587 |  609 | `	ph7_hashmap *pMap = 0;` |
|      - |  610 | `	ph7_value *pIdx;` |
| 665587 |  611 | `	pIdx = 0;` |
| 665587 |  612 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  613 | `		if( !pInstr->iP2){` |
|      - |  614 | `			/* No available index,load NULL */` |
|    ! 0 |  615 | `			if( pTos >= pStack ){` |
|    ! 0 |  616 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  617 | `			}else{` |
|      - |  618 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  619 | `				pTos++;` |
|    ! 0 |  620 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  621 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  622 | `			}` |
|      - |  623 | `			/* Emit a notice */` |
|    ! 0 |  624 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  625 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  626 | `			VM_EXIT_BREAK;` |
|      - |  627 | `		}` |
|      2 |  628 | `	}else{` |
| 665585 |  629 | `		pIdx = pTos;` |
| 665585 |  630 | `		pTos--;` |
|      - |  631 | `	}` |
| 665587 |  632 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  633 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  634 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  635 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  636 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|    ! 0 |  637 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|    ! 0 |  638 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|    ! 0 |  639 | `		}` |
|    ! 0 |  640 | `		if( pIdx ){` |
|      - |  641 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  642 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|    ! 0 |  643 | `			PH7_MemObjRelease(pIdx);` |
|    ! 0 |  644 | `		}` |
|    ! 0 |  645 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  646 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  647 | `		VM_EXIT_BREAK;` |
|      - |  648 | `	}` |
| 665587 |  649 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  650 | `		/* String access */` |
| 519619 |  651 | `		if( pIdx ){` |
|      - |  652 | `			sxi64 iOfft;` |
| 519619 |  653 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 519619 |  654 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  655 | `				/* Force an int cast */` |
|    ! 0 |  656 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  657 | `			}` |
| 519619 |  658 | `			iOfft = pIdx->x.iVal;` |
|      - |  659 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  660 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  661 | `			 * number, ran past the end and quietly produced NULL. */` |
| 519619 |  662 | `			if( iOfft < 0 ){` |
|      7 |  663 | `				iOfft += nLen;` |
|      3 |  664 | `			}` |
| 519623 |  665 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  666 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  667 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  668 | `				 * silently produced NULL in both cases). */` |
|      - |  669 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  670 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  671 | `				 * lookups and must stay silent. */` |
|     11 |  672 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  673 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  674 | `				PH7_MemObjRelease(pTos);` |
|     10 |  675 | `				if( bQuiet ){` |
|      8 |  676 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  677 | `				}else{` |
|      3 |  678 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  679 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  680 | `						pIdx->x.iVal);` |
|      - |  681 | `				}` |
|      6 |  682 | `			}else{` |
| 519611 |  683 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 519611 |  684 | `				int c = zData[iOfft];` |
| 519611 |  685 | `				PH7_MemObjRelease(pTos);` |
| 519611 |  686 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 519611 |  687 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  688 | `			}` |
| 260178 |  689 | `		}else{` |
|      - |  690 | `			/* No available index,load NULL */` |
|    ! 0 |  691 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  692 | `		}` |
| 519619 |  693 | `		VM_EXIT_BREAK;` |
|      - |  694 | `	}` |
| 145973 |  695 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  696 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  697 | `		 * iP2 codes:` |
|      - |  698 | `		 *   0 = read       → offsetGet` |
|      - |  699 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  700 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  701 | `		 *   4 = isset()    → offsetExists` |
|      - |  702 | `		 *   5 = unset()    → offsetUnset` |
|      - |  703 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  704 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  705 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  706 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  707 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  708 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  709 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  710 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  711 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  712 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  713 | `			ph7_class_method *pMeth;` |
|      - |  714 | `			ph7_value sResult;` |
|      - |  715 | `			ph7_value *apArg[1];` |
|    181 |  716 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  717 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  718 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  719 | `					"Cannot use [] for reading");` |
|    ! 0 |  720 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  721 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  722 | `				VM_EXIT_BREAK;` |
|      - |  723 | `			}` |
|    181 |  724 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  725 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  726 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  727 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  728 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  729 | `				apArg[0] = pIdx;` |
|     81 |  730 | `				if( pMeth ){` |
|     81 |  731 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  732 | `				}` |
|    143 |  733 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  734 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  735 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  736 | `				apArg[0] = pIdx;` |
|     20 |  737 | `				if( pMeth ){` |
|     20 |  738 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  739 | `				}` |
|     12 |  740 | `			}else{` |
|     89 |  741 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  742 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  743 | `				apArg[0] = pIdx;` |
|     89 |  744 | `				if( pMeth ){` |
|     89 |  745 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  746 | `				}` |
|      - |  747 | `			}` |
|    181 |  748 | `			if( pInstr->iP2 == 4 ){` |
|      - |  749 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  750 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  751 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  752 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  753 | `				PH7_MemObjRelease(pTos);` |
|     53 |  754 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  755 | `				if( bExists ){` |
|     28 |  756 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  757 | `					pTos->x.iVal = 1;` |
|     16 |  758 | `				}else{` |
|     29 |  759 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  760 | `				}` |
|    157 |  761 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  762 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  763 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  764 | `				PH7_MemObjRelease(pTos);` |
|     20 |  765 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  766 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  767 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  768 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  769 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  770 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  771 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  772 | `				 * coalesce takes the default, the real value on a hit. */` |
|     23 |  773 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     23 |  774 | `				PH7_MemObjRelease(&sResult);` |
|     23 |  775 | `				PH7_MemObjRelease(pTos);` |
|     23 |  776 | `				pTos->nIdx = SXU32_HIGH;` |
|     23 |  777 | `				if( !bExists ){` |
|      9 |  778 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      6 |  779 | `				}else{` |
|     17 |  780 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  781 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  782 | `					ph7_value sValue;` |
|     17 |  783 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     17 |  784 | `					apArg[0] = pIdx;` |
|     17 |  785 | `					if( pGet ){` |
|     17 |  786 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  787 | `					}` |
|     17 |  788 | `					PH7_MemObjStore(&sValue,pTos);` |
|     17 |  789 | `					PH7_MemObjRelease(&sValue);` |
|      - |  790 | `				}` |
|     23 |  791 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     23 |  792 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  793 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  794 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  795 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  796 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  797 | `				 *     and push NULL.` |
|      - |  798 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  799 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  800 | `				int bShouldArm = !bExists;` |
|      - |  801 | `				ph7_value sValue;` |
|     10 |  802 | `				PH7_MemObjRelease(&sResult);` |
|      - |  803 | `				/* Reset any prior arming defensively */` |
|     10 |  804 | `				VmCoalesceDisarm(pVm);` |
|     10 |  805 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  806 | `				if( bExists ){` |
|      5 |  807 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  808 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  809 | `					apArg[0] = pIdx;` |
|      5 |  810 | `					if( pGet ){` |
|      5 |  811 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  812 | `					}` |
|      5 |  813 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  814 | `						bShouldArm = 1;` |
|      1 |  815 | `					}` |
|      2 |  816 | `				}` |
|     10 |  817 | `				PH7_MemObjRelease(pTos);` |
|     10 |  818 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  819 | `				if( bShouldArm ){` |
|      - |  820 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  821 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  822 | `					 * intervening expression evaluation. */` |
|      8 |  823 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  824 | `					if( pIdx ){` |
|      8 |  825 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  826 | `					}` |
|      8 |  827 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  828 | `					pInst->iRef++;` |
|      8 |  829 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  830 | `				}else{` |
|      3 |  831 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  832 | `				}` |
|     10 |  833 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  834 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  835 | `				VM_EXIT_BREAK;` |
|    ! 0 |  836 | `			}else{` |
|      - |  837 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  838 | `				PH7_MemObjRelease(pTos);` |
|     89 |  839 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  840 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  841 | `			}` |
|    153 |  842 | `			PH7_MemObjRelease(&sResult);` |
|    153 |  843 | `			if( pIdx ){` |
|    153 |  844 | `				PH7_MemObjRelease(pIdx);` |
|     74 |  845 | `			}` |
|    153 |  846 | `			VM_EXIT_BREAK;` |
|      - |  847 | `		}` |
|      - |  848 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  849 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  850 | `		if( pInst ){` |
|      - |  851 | `			char zMsg[256];` |
|      3 |  852 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  853 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  854 | `				"Cannot use object of type %.*s as array",` |
|      2 |  855 | `				(int)pName->nByte,pName->zString);` |
|      3 |  856 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  857 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  858 | `			PH7_MemObjRelease(pTos);` |
|      3 |  859 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  860 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  861 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  862 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  863 | `			 * execution carried on inside the try block. */` |
|      3 |  864 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  865 | `		}` |
|    ! 0 |  866 | `	}` |
| 145795 |  867 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  868 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  869 | `			ph7_value *pObj;` |
|     23 |  870 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  871 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  872 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  873 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  874 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  875 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  876 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  877 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  878 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  879 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - |  880 | `				 * it is not a bool). */` |
|     23 |  881 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - |  882 | `					SyBlob sErrMsg;` |
|      7 |  883 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 |  884 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - |  885 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 |  886 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 |  887 | `					if( pIdx ){` |
|      7 |  888 | `						PH7_MemObjRelease(pIdx);` |
|      3 |  889 | `					}` |
|      7 |  890 | `					PH7_MemObjRelease(pTos);` |
|      7 |  891 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 |  892 | `					VM_EXIT_BREAK;` |
|      - |  893 | `				}` |
|     17 |  894 | `				PH7_MemObjToHashmap(pObj);` |
|     17 |  895 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 |  896 | `			}` |
|      8 |  897 | `		}` |
|      8 |  898 | `	}` |
| 145789 |  899 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - |  900 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - |  901 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - |  902 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
| 145784 |  903 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 145778 |  904 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  33987 |  905 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  50981 |  906 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  33982 |  907 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  33987 |  908 | `		if( bNull \|\| bLossyFloat ){` |
|      - |  909 | `			SyBlob sErrMsg;` |
|      5 |  910 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 |  911 | `			SyBlobAppend(&sErrMsg,` |
|      2 |  912 | `				bNull ? "Cannot access offset of type null on array"` |
|      - |  913 | `				      : "Cannot access offset of type float on array",` |
|      4 |  914 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - |  915 | `				                      : "Cannot access offset of type float on array"));` |
|      5 |  916 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 |  917 | `			PH7_MemObjRelease(pIdx);` |
|      5 |  918 | `			PH7_MemObjRelease(pTos);` |
|      5 |  919 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 |  920 | `			VM_EXIT_BREAK;` |
|      - |  921 | `		}` |
|  16989 |  922 | `	}` |
| 145785 |  923 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 145775 |  924 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - |  925 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - |  926 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - |  927 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - |  928 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - |  929 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - |  930 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1263 |  931 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    629 |  932 | `		}` |
|      - |  933 | `		/* Point to the hashmap */` |
| 145775 |  934 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 145775 |  935 | `		if( pIdx ){` |
|      - |  936 | `			/* Load the desired entry */` |
| 145773 |  937 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  72884 |  938 | `		}` |
| 145775 |  939 | `		if( pInstr->iP2 == 3 ){` |
|      - |  940 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - |  941 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - |  942 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - |  943 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - |  944 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - |  945 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - |  946 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - |  947 | `			 * correct for the outermost write. */` |
|     21 |  948 | `			int needWrite = (rc != SXRET_OK);` |
|     21 |  949 | `			if( !needWrite && pNode ){` |
|     13 |  950 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 |  951 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 |  952 | `					needWrite = 1;` |
|      3 |  953 | `				}` |
|      6 |  954 | `			}` |
|     21 |  955 | `			if( needWrite ){` |
|     15 |  956 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 |  957 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - |  958 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - |  959 | `					 * into the new map's storage. */` |
|      7 |  960 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 |  961 | `					if( pIdx ){` |
|      7 |  962 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 |  963 | `					}` |
|      3 |  964 | `				}` |
|      7 |  965 | `			}` |
|     10 |  966 | `		}` |
| 145775 |  967 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - |  968 | `			/* Create a new empty entry */` |
|    324 |  969 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 |  970 | `			if( rc == SXRET_OK ){` |
|      - |  971 | `				/* Point to the last inserted entry */` |
|    321 |  972 | `				pNode = pMap->pLast;` |
|    161 |  973 | `			}else{` |
|      - |  974 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - |  975 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - |  976 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - |  977 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 |  978 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - |  979 | `			}` |
|    160 |  980 | `		}` |
|  72884 |  981 | `	}` |
| 145778 |  982 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  39892 |  983 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      8 |  984 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - |  985 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - |  986 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - |  987 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - |  988 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - |  989 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - |  990 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - |  991 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - |  992 | `		 * STRING key quoted. */` |
|      - |  993 | `		SyBlob sMsg;` |
|      8 |  994 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 |  995 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - |  996 | `			SyString sKey;` |
|      6 |  997 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  998 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 |  999 | `			}` |
|      6 | 1000 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      6 | 1001 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      4 | 1002 | `		}else{` |
|      3 | 1003 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1004 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1005 | `			}` |
|      3 | 1006 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1007 | `		}` |
|      8 | 1008 | `		SyBlobNullAppend(&sMsg);` |
|      8 | 1009 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 | 1010 | `		SyBlobRelease(&sMsg);` |
|      3 | 1011 | `	}` |
| 145784 | 1012 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  72900 | 1013 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1014 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1015 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1016 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1017 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1018 | `			VmArithTypeName(pTos));` |
|      3 | 1019 | `	}` |
| 145789 | 1020 | `	if( pIdx ){` |
| 145789 | 1021 | `		PH7_MemObjRelease(pIdx);` |
|  72892 | 1022 | `	}` |
| 145789 | 1023 | `	if( rc == SXRET_OK ){` |
|      - | 1024 | `		/* Load entry contents */` |
|  66017 | 1025 | `		if( pMap->iRef < 2 ){` |
|      - | 1026 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1027 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1028 | `			 */` |
|    101 | 1029 | `			pTos->nIdx = SXU32_HIGH;` |
|    101 | 1030 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     52 | 1031 | `		}else{` |
|  65919 | 1032 | `			pTos->nIdx = pNode->nValIdx;` |
|  65919 | 1033 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  65919 | 1034 | `			PH7_HashmapUnref(pMap);` |
|      - | 1035 | `		}` |
|  33011 | 1036 | `	}else{` |
|      - | 1037 | `		/* No such entry,load NULL */` |
|  79777 | 1038 | `		PH7_MemObjRelease(pTos);` |
|  79777 | 1039 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1040 | `	}` |
| 145789 | 1041 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1042 | `	VM_EXIT_BREAK;` |
| 333165 | 1043 | `}` |
|      - | 1044 |  |
|      - | 1045 | `/*` |
|      - | 1046 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1047 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1048 | ` */` |
|  73688 | 1049 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1050 | `{` |
|  73693 | 1051 | `	ph7_value *pTos = pState->pTos;` |
|  73693 | 1052 | `	ph7_value *pStack = pState->pStack;` |
|  73693 | 1053 | `	VmInstr *aInstr = pState->aInstr;` |
|  73693 | 1054 | `	sxi32 pc = pState->pc;` |
|      - | 1055 | `	sxi32 rc;` |
|  36844 | 1056 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1057 | `	ph7_hashmap *pMap;` |
|      - | 1058 | `	/* Allocate a new hashmap instance */` |
|  73693 | 1059 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73693 | 1060 | `	if( pMap == 0 ){` |
|    ! 0 | 1061 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1062 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1063 | `		VM_EXIT_ABORT;` |
|      - | 1064 | `	}` |
|  73693 | 1065 | `	if( pInstr->iP1 > 0 ){` |
|  10927 | 1066 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  10927 | 1067 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1068 | `		/* Perform the insertion */` |
|  41289 | 1069 | `		while( pEntry < pTos ){` |
|  30385 | 1070 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1071 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1072 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1073 | `				 * renumbered. Same routine that backs array_merge. */` |
|    683 | 1074 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1075 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1076 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1077 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1078 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1079 | `						 * map dangling. */` |
|    ! 0 | 1080 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1081 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1082 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1083 | `						break;` |
|      1 | 1084 | `					}` |
|    354 | 1085 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1086 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1087 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1088 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1089 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1090 | `						rcSpread = rcW;` |
|    ! 0 | 1091 | `						break;` |
|      - | 1092 | `					}` |
|      3 | 1093 | `				}else{` |
|      - | 1094 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     21 | 1095 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     21 | 1096 | `					break;` |
|      1 | 1097 | `				}` |
|  30036 | 1098 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1099 | `				/* Insertion by reference */` |
|    181 | 1100 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1101 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1102 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1103 | `					);` |
|     61 | 1104 | `			}else{` |
|      - | 1105 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1106 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1107 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1108 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1109 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1110 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  29585 | 1111 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1112 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11183 | 1113 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16773 | 1114 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11178 | 1115 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11183 | 1116 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1117 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1118 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1119 | `							SyBlob sErrMsg;` |
|      5 | 1120 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1121 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1122 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1123 | `						}` |
|   5589 | 1124 | `					}` |
|      - | 1125 | `				/* Standard insertion */` |
|  44375 | 1126 | `				PH7_HashmapInsert(pMap,` |
|  29580 | 1127 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  14790 | 1128 | `					&pEntry[1]` |
|      - | 1129 | `				);` |
|      - | 1130 | `			}` |
|      - | 1131 | `			/* Next pair on the stack */` |
|  30367 | 1132 | `			pEntry += 2;` |
|      5 | 1133 | `		}` |
|      - | 1134 | `		/* Pop P1 elements */` |
|  10927 | 1135 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  10927 | 1136 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1137 | `			/* Discard the partially-built map and propagate the exception. */` |
|     21 | 1138 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     21 | 1139 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1140 | `				VM_EXIT_ABORT;` |
|      - | 1141 | `			}` |
|      - | 1142 | `			{` |
|      - | 1143 | `				sxi32 iRp;` |
|     21 | 1144 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1145 | `					pc = iRp;` |
|      6 | 1146 | `					VM_EXIT_BREAK;` |
|      - | 1147 | `				}` |
|      - | 1148 | `			}` |
|     15 | 1149 | `			VM_EXIT_EXCEPTION;` |
|      - | 1150 | `		}` |
|   5452 | 1151 | `	}` |
|      - | 1152 | `	/* Push the hashmap */` |
|  73675 | 1153 | `	pTos++;` |
|  73675 | 1154 | `	pTos->nIdx = SXU32_HIGH;` |
|  73675 | 1155 | `	pTos->x.pOther = pMap;` |
|  73675 | 1156 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73675 | 1157 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1158 | `	VM_EXIT_BREAK;` |
|  36849 | 1159 | `}` |
|      - | 1160 |  |
|      - | 1161 | `/*` |
|      - | 1162 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1163 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1164 | ` */` |
|    264 | 1165 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1166 | `{` |
|    269 | 1167 | `	ph7_value *pTos = pState->pTos;` |
|    269 | 1168 | `	ph7_value *pStack = pState->pStack;` |
|    269 | 1169 | `	VmInstr *aInstr = pState->aInstr;` |
|    269 | 1170 | `	sxi32 pc = pState->pc;` |
|      - | 1171 | `	sxi32 rc;` |
|    132 | 1172 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1173 | `	ph7_value *pEntry;` |
|    269 | 1174 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1175 | `		/* Empty list,break immediately */` |
|    ! 0 | 1176 | `		VM_EXIT_BREAK;` |
|      - | 1177 | `	}` |
|    269 | 1178 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1179 | `#ifdef UNTRUST` |
|      - | 1180 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1181 | `		VM_EXIT_ABORT;` |
|      - | 1182 | `	}` |
|      - | 1183 | `#endif` |
|    269 | 1184 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1185 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1186 | `		ph7_hashmap_node *pNode;` |
|      - | 1187 | `		ph7_value sKey,*pObj;` |
|      - | 1188 | `		/* Start Copying */` |
|    265 | 1189 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1190 | `		while( pEntry <= pTos ){` |
|    551 | 1191 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1192 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1193 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1194 | `					if( rc == SXRET_OK ){` |
|      - | 1195 | `						/* Store node value */` |
|    523 | 1196 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1197 | `					}else{` |
|      - | 1198 | `						/* Undefined array key */` |
|      - | 1199 | `						char zMsg[128];` |
|    ! 0 | 1200 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1201 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1202 | `						PH7_MemObjRelease(pObj);` |
|      - | 1203 | `					}` |
|    259 | 1204 | `				}` |
|    259 | 1205 | `			}` |
|    551 | 1206 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1207 | `			pEntry++;` |
|      5 | 1208 | `		}` |
|    135 | 1209 | `	}else{` |
|      - | 1210 | `		/* Source is not an array */` |
|      - | 1211 | `		ph7_value *pObj;` |
|     13 | 1212 | `		while( pEntry <= pTos ){` |
|      9 | 1213 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      9 | 1214 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      9 | 1215 | `					PH7_MemObjRelease(pObj);` |
|      4 | 1216 | `				}` |
|      4 | 1217 | `			}` |
|      9 | 1218 | `			pEntry++;` |
|      1 | 1219 | `		}` |
|      5 | 1220 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1221 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|    ! 0 | 1222 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|    ! 0 | 1223 | `		}` |
|      - | 1224 | `	}` |
|    269 | 1225 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    269 | 1226 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1227 | `	VM_EXIT_BREAK;` |
|    137 | 1228 | `}` |
|      - | 1229 |  |
|      - | 1230 | `/*` |
|      - | 1231 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1232 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1233 | ` */` |
|   6732 | 1234 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1235 | `{` |
|   6737 | 1236 | `	ph7_value *pTos = pState->pTos;` |
|   6737 | 1237 | `	ph7_value *pStack = pState->pStack;` |
|   6737 | 1238 | `	VmInstr *aInstr = pState->aInstr;` |
|   6737 | 1239 | `	sxi32 pc = pState->pc;` |
|      - | 1240 | `	sxi32 rc;` |
|   3366 | 1241 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1242 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6737 | 1243 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6737 | 1244 | `	if( pName && pVm->pFrame ){` |
|      - | 1245 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1246 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1247 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6737 | 1248 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6737 | 1249 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6737 | 1250 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1251 | `			VM_EXIT_ABORT;` |
|      - | 1252 | `		}` |
|      - | 1253 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1254 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1255 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1256 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6735 | 1257 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1258 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1259 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1260 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1261 | `				VM_EXIT_ABORT;` |
|      - | 1262 | `			}` |
|      3 | 1263 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1264 | `		}` |
|   3364 | 1265 | `	}` |
|   6733 | 1266 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1267 | `	VM_EXIT_BREAK;` |
|   3371 | 1268 | `}` |
|      - | 1269 |  |
