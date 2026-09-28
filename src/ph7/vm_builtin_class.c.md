# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 923/1034 lines (89.26%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|   11330 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|   11335 |   11 | `	if( nArg < 1 ){` |
|       - |   12 | `		/* Check if we are inside a class */` |
|     ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     ! 0 |   14 | `		if( pClass ){` |
|       - |   15 | `			/* Point to the class name */` |
|     ! 0 |   16 | `			pName = &pClass->sName;` |
|     ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     ! 0 |   18 | `		}else{` |
|       - |   19 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|       - |   21 | `		}` |
|     ! 0 |   22 | `	}else{` |
|       - |   23 | `		/* Extract the target class */` |
|   11335 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   11335 |   25 | `		if( pClass ){` |
|   11335 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|   11335 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    5670 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|   11335 |   34 | `	return PH7_OK;` |
|       5 |   35 | `}` |
|       - |   36 | `/*` |
|       - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|       - |   38 | ` *   Returns the name of the parent class of an object` |
|       - |   39 | ` * Parameters` |
|       - |   40 | ` *  object` |
|       - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|       - |   42 | ` * Return` |
|       - |   43 | ` *  The name of the parent class of which object is an instance.` |
|       - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|       - |   45 | ` *  not have a parent.` |
|       - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|       - |   47 | ` */` |
|      80 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      85 |   52 | `	if( nArg < 1 ){` |
|       - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|       3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       3 |   55 | `		if( pClass && pClass->pBase ){` |
|       - |   56 | `			/* Point to the class name */` |
|       3 |   57 | `			pName = &pClass->pBase->sName;` |
|       3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       2 |   59 | `		}else{` |
|       - |   60 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|       - |   62 | `		}` |
|       2 |   63 | `	}else{` |
|       - |   64 | `		/* Extract the target class */` |
|      83 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      83 |   66 | `		if( pClass ){` |
|      83 |   67 | `			if( pClass->pBase ){` |
|      77 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      77 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      41 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       7 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      44 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      85 |   80 | `	return PH7_OK;` |
|       5 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * string get_called_class(void)` |
|       - |   84 | ` *   Gets the name of the class the static method is called in.` |
|       - |   85 | ` * Parameters` |
|       - |   86 | ` *  None.` |
|       - |   87 | ` * Return` |
|       - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|       - |   89 | ` */` |
|       4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   91 | `{` |
|       - |   92 | `	ph7_class *pClass;` |
|       - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|       5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       5 |   95 | `	if( pClass ){` |
|       - |   96 | `		SyString *pName;` |
|       - |   97 | `		/* Point to the class name */` |
|       5 |   98 | `		pName = &pClass->sName;` |
|       5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       3 |  100 | `	}else{` |
|     ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  102 | `		SXUNUSED(apArg);` |
|       - |  103 | `		/* Not inside class,return FALSE */` |
|     ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|       - |  105 | `	}` |
|       5 |  106 | `	return PH7_OK;` |
|       1 |  107 | `}` |
|       - |  108 | `/*` |
|       - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|       - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|       - |  111 | ` * string which hold the class name.` |
|       - |  112 | ` */` |
|  216552 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  216557 |  115 | `	ph7_class *pClass = 0;` |
|  216557 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  211695 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  110712 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    4863 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    4863 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    4861 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    2428 |  135 | `		}` |
|    2429 |  136 | `	}` |
|  216557 |  137 | `	return pClass;` |
|       5 |  138 | `}` |
|       - |  139 | `/*` |
|       - |  140 | ` * bool property_exists(mixed $class,string $property)` |
|       - |  141 | ` *   Checks if the object or class has a property.` |
|       - |  142 | ` * Parameters` |
|       - |  143 | ` *  class` |
|       - |  144 | ` *   The class name or an object of the class to test for` |
|       - |  145 | ` * property` |
|       - |  146 | ` *  The name of the property` |
|       - |  147 | ` * Return` |
|       - |  148 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|       - |  149 | ` */` |
|      90 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  151 | `{` |
|      93 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|      93 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|      90 |  155 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      75 |  156 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  157 | `			/* An incomplete OBJECT: php's has_property probe is the access warning` |
|       - |  158 | `			 * (qualified with this builtin's own name) answering false. The class` |
|       - |  159 | `			 * asked about by NAME stays an ordinary lookup. */` |
|       - |  160 | `			SyBlob sIncMsg;` |
|       3 |  161 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  162 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  163 | `				"access a property",&sIncMsg);` |
|       4 |  164 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%.*s",` |
|       2 |  165 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  166 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  167 | `			ph7_result_bool(pCtx,0);` |
|       3 |  168 | `			return PH7_OK;` |
|       - |  169 | `		}` |
|      91 |  170 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      91 |  171 | `		if( pClass ){` |
|       - |  172 | `			const char *zName;` |
|       - |  173 | `			int nLen;` |
|       - |  174 | `			/* Extract attribute name */` |
|      91 |  175 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      91 |  176 | `			if( nLen > 0 ){` |
|       - |  177 | `				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this` |
|       - |  178 | ``				 * name is not a property (`property_exists('C','someMethod')` is false),`` |
|       - |  179 | `				 * and neither is a class CONSTANT. PHL searched the method table too and` |
|       - |  180 | `				 * answered true for both. */` |
|      91 |  181 | `				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);` |
|      91 |  182 | `				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;` |
|      88 |  183 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND)` |
|      31 |  184 | `				 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - |  185 | `					/* An ON-DEMAND property is on the objects that took it and on no` |
|       - |  186 | `					 * other: php declares none of them, so the class table cannot be` |
|       - |  187 | `					 * what answers here. */` |
|       5 |  188 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       4 |  189 | `					if( pThis == 0` |
|       5 |  190 | `					 \|\| SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) == 0 ){` |
|       3 |  191 | `						pAttr = 0;` |
|       2 |  192 | `					}` |
|      89 |  193 | `				}else if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) ){` |
|     ! 0 |  194 | `					pAttr = 0;   /* asked about the CLASS: php declares no such name */` |
|     ! 0 |  195 | `				}` |
|      91 |  196 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - |  197 | `					/* A base's PRIVATE property is invisible to the child it was asked` |
|       - |  198 | ``					 * about, php's `property_info->ce == ce` rule: PHL copies one down`` |
|       - |  199 | `					 * onto every child (its own methods read it through $this), so the` |
|       - |  200 | `					 * table alone said true where php says false. Static or instance,` |
|       - |  201 | `					 * the rule is the same; protected and public are inherited outright.` |
|       - |  202 | `					 * A trait's property belongs to the class that COMPOSED it. */` |
|      49 |  203 | `					res = 1;` |
|      46 |  204 | `					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      31 |  205 | `					 && pAttr->pDeclClass != 0` |
|      19 |  206 | `					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){` |
|       9 |  207 | `						res = 0;` |
|       4 |  208 | `					}` |
|      23 |  209 | `				}` |
|       - |  210 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  211 | `				 * attribute table, not the class's — php reports those too` |
|       - |  212 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|      91 |  213 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|      27 |  214 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      27 |  215 | `					SyHashEntry *pObjE = pThis` |
|      26 |  216 | `						? SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) : 0;` |
|      27 |  217 | `					VmClassAttr *pObjAttr = pObjE ? (VmClassAttr *)pObjE->pUserData : 0;` |
|       - |  218 | `					/* Only a genuinely DYNAMIC one: every instance carries a slot for every` |
|       - |  219 | `					 * DECLARED member too, so an unqualified instance lookup answered true` |
|       - |  220 | `					 * for the base private the class-table rule above had just refused. */` |
|      26 |  221 | `					if( pObjAttr && pObjAttr->pAttr` |
|       3 |  222 | `					 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|       3 |  223 | `						res = 1;` |
|       1 |  224 | `					}` |
|      27 |  225 | `					if( res == 0 && pThis ){` |
|       - |  226 | `						/* php asks the class's has_property handler here too, which` |
|       - |  227 | `						 * is what reports a PDORow's COLUMNS -- a name no table` |
|       - |  228 | `						 * carries and every read answers. Its verdict is the value's` |
|       - |  229 | `						 * (a column holding SQL NULL reports false), which is what` |
|       - |  230 | `						 * php's own handler answers to this question. */` |
|       - |  231 | `						PH7_NativePropCtx sNat;` |
|       - |  232 | `						SyString sNatName;` |
|       - |  233 | `						ph7_value sNatVal;` |
|      25 |  234 | `						SyStringInitFromBuf(&sNatName,zName,(sxu32)nLen);` |
|      25 |  235 | `						PH7_MemObjInit(pCtx->pVm,&sNatVal);` |
|      24 |  236 | `						if( PH7_ClassNativePropAsk(pThis,&sNat,PH7_NATIVE_PROP_EXISTS,` |
|       - |  237 | `								&sNatName,&sNatVal)` |
|      21 |  238 | `						 && sNat.zThrowClass == 0 && ph7_value_to_bool(&sNatVal) ){` |
|       5 |  239 | `							res = 1;` |
|       2 |  240 | `						}` |
|      25 |  241 | `						PH7_MemObjRelease(&sNatVal);` |
|      12 |  242 | `					}` |
|      13 |  243 | `				}` |
|      44 |  244 | `			}` |
|      44 |  245 | `		}` |
|      44 |  246 | `	}` |
|      91 |  247 | `	ph7_result_bool(pCtx,res);` |
|      91 |  248 | `	return PH7_OK;` |
|      48 |  249 | `}` |
|       - |  250 | `/*` |
|       - |  251 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  252 | ` *   Checks if the given method is a class member.` |
|       - |  253 | ` * Parameters` |
|       - |  254 | ` *  class` |
|       - |  255 | ` *   The class name or an object of the class to test for` |
|       - |  256 | ` * property` |
|       - |  257 | ` *  The name of the method` |
|       - |  258 | ` * Return` |
|       - |  259 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  260 | ` */` |
|      82 |  261 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  262 | `{` |
|      85 |  263 | `	int res = 0; /* Assume method does not exists */` |
|      85 |  264 | `	if( nArg > 1 ){` |
|       - |  265 | `		ph7_class *pClass;` |
|      82 |  266 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      48 |  267 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  268 | `			/* An incomplete OBJECT consults its method resolution, which the` |
|       - |  269 | `			 * carrier refuses with php's catchable call Error (probe-verified;` |
|       - |  270 | `			 * the class asked about by NAME answers false the ordinary way). */` |
|       - |  271 | `			SyBlob sIncMsg;` |
|       - |  272 | `			sxi32 rcInc;` |
|       3 |  273 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  274 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  275 | `				"call a method",&sIncMsg);` |
|       4 |  276 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  277 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  278 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  279 | `			return rcInc;` |
|       - |  280 | `		}` |
|      83 |  281 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      83 |  282 | `		if( pClass ){` |
|       - |  283 | `			const char *zName;` |
|       - |  284 | `			int nLen;` |
|       - |  285 | `			/* Extract method name */` |
|      78 |  286 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      78 |  287 | `			if( nLen > 0 ){` |
|       - |  288 | `				/* Perform the lookup in the method table */` |
|      78 |  289 | `				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);` |
|      78 |  290 | `				if( pEntry ){` |
|       - |  291 | ``					/* ...and apply php's one visibility rule here (`func->common.scope`` |
|       - |  292 | ``					 * == ce`): a PRIVATE method is only a method of the class that`` |
|       - |  293 | `					 * declares it. PHL copies a base's private down so an inherited` |
|       - |  294 | `					 * public method can still dispatch it, which made` |
|       - |  295 | ``					 * `method_exists('Child','basePrivate')` answer true where php`` |
|       - |  296 | `					 * answers false — the same shape property_exists() had. Nothing` |
|       - |  297 | `					 * about the CALLING scope enters into it: php answers false for the` |
|       - |  298 | `					 * child from inside the BASE too. A trait's method belongs to the` |
|       - |  299 | `					 * class that COMPOSED it. */` |
|      58 |  300 | `					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      58 |  301 | `					res = 1;` |
|      56 |  302 | `					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      40 |  303 | `					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){` |
|      15 |  304 | `						res = 0;` |
|       7 |  305 | `					}` |
|      28 |  306 | `				}` |
|      38 |  307 | `			}` |
|      38 |  308 | `		}` |
|      40 |  309 | `	}` |
|      83 |  310 | `	ph7_result_bool(pCtx,res);` |
|      83 |  311 | `	return PH7_OK;` |
|      44 |  312 | `}` |
|       - |  313 | `/*` |
|       - |  314 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  315 | ` *   Checks if the class has been defined.` |
|       - |  316 | ` * Parameters` |
|       - |  317 | ` *  class_name` |
|       - |  318 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  319 | ` *   unlinke the standard PHP engine.` |
|       - |  320 | ` *  autoload` |
|       - |  321 | ` *   Whether or not to call __autoload by default.` |
|       - |  322 | ` * Return` |
|       - |  323 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  324 | ` */` |
|     132 |  325 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  326 | `{` |
|     137 |  327 | `	int res = 0; /* Assume class does not exist */` |
|     137 |  328 | `	if( nArg > 0 ){` |
|     137 |  329 | `		SyHashEntry *pEntry = 0;` |
|       - |  330 | `		const char *zName;` |
|       - |  331 | `		int nLen;` |
|     137 |  332 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  333 | `		sxu32 nName;` |
|       - |  334 | `		/* Extract given name */` |
|     137 |  335 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     137 |  336 | `		if( nArg >= 2 ){` |
|       6 |  337 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  338 | `		}` |
|       - |  339 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  340 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     137 |  341 | `		nName = (sxu32)nLen;` |
|     137 |  342 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     137 |  343 | `		if( nName > 0 ){` |
|       - |  344 | `			/* Perform a hash lookup first */` |
|     133 |  345 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      64 |  346 | `		}` |
|       - |  347 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|     137 |  348 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  349 | `			/* Try autoload, then re-check */` |
|      27 |  350 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      27 |  351 | `			if( pClass ){` |
|       9 |  352 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  353 | `			}` |
|      11 |  354 | `		}` |
|     137 |  355 | `		if( pEntry ){` |
|       - |  356 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  357 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|     115 |  358 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     119 |  359 | `			while( pClass ){` |
|     115 |  360 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|     111 |  361 | `					res = 1;` |
|     111 |  362 | `					break;` |
|       - |  363 | `				}` |
|       5 |  364 | `				pClass = pClass->pNextName;` |
|       1 |  365 | `			}` |
|      55 |  366 | `		}` |
|      66 |  367 | `	}` |
|     137 |  368 | `	ph7_result_bool(pCtx,res);` |
|     137 |  369 | `	return PH7_OK;` |
|       5 |  370 | `}` |
|       - |  371 | `/*` |
|       - |  372 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  373 | ` *   Checks if the interface has been defined.` |
|       - |  374 | ` * Parameters` |
|       - |  375 | ` *  class_name` |
|       - |  376 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  377 | ` *   unlinke the standard PHP engine.` |
|       - |  378 | ` *  autoload` |
|       - |  379 | ` *   Whether or not to call __autoload by default.` |
|       - |  380 | ` * Return` |
|       - |  381 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  382 | ` */` |
|      46 |  383 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  384 | `{` |
|      49 |  385 | `	int res = 0; /* Assume interface does not exist */` |
|      49 |  386 | `	if( nArg > 0 ){` |
|      49 |  387 | `		SyHashEntry *pEntry = 0;` |
|       - |  388 | `		const char *zName;` |
|       - |  389 | `		int nLen;` |
|      49 |  390 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  391 | `		sxu32 nName;` |
|       - |  392 | `		/* Extract given name */` |
|      49 |  393 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      49 |  394 | `		if( nArg >= 2 ){` |
|     ! 0 |  395 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     ! 0 |  396 | `		}` |
|       - |  397 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  398 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      49 |  399 | `		nName = (sxu32)nLen;` |
|      49 |  400 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  401 | `		/* Perform a hash lookup */` |
|      49 |  402 | `		if( nName > 0 ){` |
|      46 |  403 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      22 |  404 | `		}` |
|       - |  405 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      49 |  406 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  407 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  408 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  409 | `			if( pClass ){` |
|     ! 0 |  410 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  411 | `			}` |
|       1 |  412 | `		}` |
|      49 |  413 | `		if( pEntry ){` |
|      44 |  414 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      46 |  415 | `			while( pClass ){` |
|      44 |  416 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  417 | `					/* interface is available */` |
|      42 |  418 | `					res = 1;` |
|      42 |  419 | `					break;` |
|       - |  420 | `				}` |
|       - |  421 | `				/* Next with the same name */` |
|       3 |  422 | `				pClass = pClass->pNextName;` |
|       1 |  423 | `			}` |
|      21 |  424 | `		}` |
|      23 |  425 | `	}` |
|      49 |  426 | `	ph7_result_bool(pCtx,res);` |
|      49 |  427 | `	return PH7_OK;` |
|       3 |  428 | `}` |
|       - |  429 | `/*` |
|       - |  430 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  431 | ` *   Checks if the trait has been defined.` |
|       - |  432 | ` * Parameters` |
|       - |  433 | ` *  trait` |
|       - |  434 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  435 | ` *  autoload` |
|       - |  436 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  437 | ` * Return` |
|       - |  438 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  439 | ` */` |
|      18 |  440 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  441 | `{` |
|      21 |  442 | `	int res = 0; /* Assume trait does not exist */` |
|      21 |  443 | `	if( nArg > 0 ){` |
|      21 |  444 | `		SyHashEntry *pEntry = 0;` |
|       - |  445 | `		const char *zName;` |
|       - |  446 | `		int nLen;` |
|      21 |  447 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  448 | `		sxu32 nName;` |
|       - |  449 | `		/* Extract given name */` |
|      21 |  450 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      21 |  451 | `		if( nArg >= 2 ){` |
|       3 |  452 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       1 |  453 | `		}` |
|       - |  454 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  455 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      21 |  456 | `		nName = (sxu32)nLen;` |
|      21 |  457 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  458 | `		/* Perform a hash lookup */` |
|      21 |  459 | `		if( nName > 0 ){` |
|      18 |  460 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       8 |  461 | `		}` |
|       - |  462 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      21 |  463 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  464 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       5 |  465 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       5 |  466 | `			if( pClass ){` |
|     ! 0 |  467 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  468 | `			}` |
|       2 |  469 | `		}` |
|      21 |  470 | `		if( pEntry ){` |
|      12 |  471 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  472 | `			while( pClass ){` |
|      12 |  473 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  474 | `					/* trait is available */` |
|       6 |  475 | `					res = 1;` |
|       6 |  476 | `					break;` |
|       - |  477 | `				}` |
|       - |  478 | `				/* Next with the same name */` |
|       7 |  479 | `				pClass = pClass->pNextName;` |
|       1 |  480 | `			}` |
|       5 |  481 | `		}` |
|       9 |  482 | `	}` |
|      21 |  483 | `	ph7_result_bool(pCtx,res);` |
|      21 |  484 | `	return PH7_OK;` |
|       3 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * bool class_alias([string $original[,string $alias ]])` |
|       - |  488 | ` *   Creates an alias for a class.` |
|       - |  489 | ` * Parameters` |
|       - |  490 | ` *  original` |
|       - |  491 | ` *    The original class.` |
|       - |  492 | ` *  alias` |
|       - |  493 | ` *   The alias name for the class.` |
|       - |  494 | ` * Return` |
|       - |  495 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  496 | ` */` |
|      10 |  497 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  498 | `{` |
|       - |  499 | `	const char *zOld,*zNew;` |
|       - |  500 | `	int nOldLen,nNewLen;` |
|       - |  501 | `	sxu32 nOld,nNew;` |
|       - |  502 | `	SyHashEntry *pEntry;` |
|       - |  503 | `	ph7_class *pClass;` |
|       - |  504 | `	char *zDup;` |
|       - |  505 | `	sxi32 rc;` |
|      12 |  506 | `	if( nArg < 2 ){` |
|       - |  507 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  508 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  509 | `		return PH7_OK;` |
|       - |  510 | `	}` |
|       - |  511 | `	/* Extract old class name */` |
|      12 |  512 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  513 | `	/* Extract alias name */` |
|      12 |  514 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|       - |  515 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  516 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  517 | `	 * on the plain name then matches. */` |
|      12 |  518 | `	nOld = (sxu32)nOldLen;` |
|      12 |  519 | `	nNew = (sxu32)nNewLen;` |
|      12 |  520 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|      12 |  521 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|      12 |  522 | `	if( nNew < 1 ){` |
|       - |  523 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  524 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  525 | `		return PH7_OK;` |
|       - |  526 | `	}` |
|       - |  527 | `	/* Perform a hash lookup */` |
|      12 |  528 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|      12 |  529 | `	if( pEntry ==  0 ){` |
|       - |  530 | `		/* No such class,return FALSE */` |
|     ! 0 |  531 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  532 | `		return PH7_OK;` |
|       - |  533 | `	}` |
|       - |  534 | `	/* Point to the class */` |
|      12 |  535 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  536 | `	/* Duplicate alias name */` |
|      12 |  537 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|      12 |  538 | `	if( zDup == 0 ){` |
|       - |  539 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  540 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  541 | `		return PH7_OK;` |
|       - |  542 | `	}` |
|       - |  543 | `	/* Create the alias */` |
|      12 |  544 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|      12 |  545 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  546 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  547 | `	}` |
|      12 |  548 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      12 |  549 | `	return PH7_OK;` |
|       7 |  550 | `}` |
|       - |  551 | `/*` |
|       - |  552 | ` * The three KINDS hClass holds. php keeps classes, interfaces, traits and enums in one` |
|       - |  553 | ` * table too and each of its three list builtins filters that table down to its own kind:` |
|       - |  554 | `` * an ENUM is a class (`get_declared_classes()` reports it), an interface and a trait are`` |
|       - |  555 | ` * not.` |
|       - |  556 | ` */` |
|       - |  557 | `#define VM_DECLARED_CLASS      0` |
|       - |  558 | `#define VM_DECLARED_INTERFACE  1` |
|       - |  559 | `#define VM_DECLARED_TRAIT      2` |
|       - |  560 |  |
|    3920 |  561 | `static int VmDeclaredEntryKind(ph7_class *pClass)` |
|       2 |  562 | `{` |
|    3922 |  563 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     400 |  564 | `		return VM_DECLARED_INTERFACE;` |
|       - |  565 | `	}` |
|    3524 |  566 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      24 |  567 | `		return VM_DECLARED_TRAIT;` |
|       - |  568 | `	}` |
|    3502 |  569 | `	return VM_DECLARED_CLASS;` |
|    1962 |  570 | `}` |
|       - |  571 | `struct VmDeclaredList {` |
|       - |  572 | `	int iKind;           /* Which VM_DECLARED_* kind this list wants */` |
|       - |  573 | `	ph7_value *pArray;   /* The array being built */` |
|       - |  574 | `	ph7_value *pName;    /* Scratch name */` |
|       - |  575 | `};` |
|       - |  576 | `/*` |
|       - |  577 | ` * One row of a get_declared_*() answer.` |
|       - |  578 | ` *` |
|       - |  579 | ` * The NAME reported is the table KEY, not the class struct's own name — a distinction` |
|       - |  580 | `` * only `class_alias()` makes visible, since it puts a second key over the same class.`` |
|       - |  581 | ` * php reports the class's declared spelling for the key that IS its name and the ALIAS` |
|       - |  582 | `` * for the other, so `class_alias('C1','C1Alias')` answers both `C1` and `c1alias`,`` |
|       - |  583 | ` * lower-cased because that is the spelling php's own alias key is stored under. PHL` |
|       - |  584 | ` * keeps the declared spelling in every key, so the fold is applied here.` |
|       - |  585 | ` */` |
|    3920 |  586 | `static sxi32 VmDeclaredNameStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 |  587 | `{` |
|    3922 |  588 | `	struct VmDeclaredList *pList = (struct VmDeclaredList *)pUserData;` |
|    3922 |  589 | `	ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    3922 |  590 | `	SyString *pDecl = &pClass->sName;` |
|    3922 |  591 | `	if( VmDeclaredEntryKind(pClass) != pList->iKind ){` |
|    2226 |  592 | `		return SXRET_OK;` |
|       - |  593 | `	}` |
|    1696 |  594 | `	if( pEntry->nKeyLen == pDecl->nByte` |
|    1693 |  595 | `		&& SyStrnmicmp((const char *)pEntry->pKey,pDecl->zString,pEntry->nKeyLen) == 0 ){` |
|       - |  596 | `		/* The key this class was DECLARED under */` |
|    1688 |  597 | `		ph7_value_string(pList->pName,pDecl->zString,(int)pDecl->nByte);` |
|     845 |  598 | `	}else{` |
|       - |  599 | `		/* A class_alias() key: php reports it folded */` |
|      12 |  600 | `		const char *zKey = (const char *)pEntry->pKey;` |
|       - |  601 | `		sxu32 n;` |
|     174 |  602 | `		for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|     164 |  603 | `			char c = (char)SyToLower(zKey[n]);` |
|     164 |  604 | `			ph7_value_string(pList->pName,&c,1);` |
|      83 |  605 | `		}` |
|       - |  606 | `	}` |
|    1698 |  607 | `	ph7_array_add_elem(pList->pArray,0/*Automatic index assign*/,pList->pName); /* Will make it's own copy */` |
|    1698 |  608 | `	ph7_value_reset_string_cursor(pList->pName);` |
|    1698 |  609 | `	return SXRET_OK;` |
|    1962 |  610 | `}` |
|       - |  611 |  |
|       - |  612 | `/*` |
|       - |  613 | ` * Build php's get_declared_classes()/get_declared_interfaces()/get_declared_traits()` |
|       - |  614 | ` * answer for one kind.` |
|       - |  615 | ` */` |
|      14 |  616 | `static int VmDeclaredNameList(ph7_context *pCtx,int iKind)` |
|       2 |  617 | `{` |
|       - |  618 | `	struct VmDeclaredList sList;` |
|       - |  619 | `	/* Create a new array first */` |
|      16 |  620 | `	sList.iKind = iKind;` |
|      16 |  621 | `	sList.pArray = ph7_context_new_array(pCtx);` |
|      16 |  622 | `	sList.pName = ph7_context_new_scalar(pCtx);` |
|      16 |  623 | `	if( sList.pArray == 0 \|\| sList.pName == 0 ){` |
|       - |  624 | `		/* Out of memory,return NULL */` |
|     ! 0 |  625 | `		ph7_result_null(pCtx);` |
|     ! 0 |  626 | `		return PH7_OK;` |
|       - |  627 | `	}` |
|       - |  628 | `	/* hClass is head-pushed, so its forward order is reverse-insertion; php reports` |
|       - |  629 | `	 * these lists in DECLARATION order (its own class table is append-ordered), which` |
|       - |  630 | `	 * is what the backward walk yields. */` |
|      16 |  631 | `	SyHashForEachReverse(&pCtx->pVm->hClass,VmDeclaredNameStep,(void *)&sList);` |
|       - |  632 | `	/* Return the created array */` |
|      16 |  633 | `	ph7_result_value(pCtx,sList.pArray);` |
|      16 |  634 | `	return PH7_OK;` |
|       9 |  635 | `}` |
|       - |  636 | `/*` |
|       - |  637 | ` * array get_declared_classes(void)` |
|       - |  638 | ` *   Returns an array with the name of the defined classes` |
|       - |  639 | ` * Parameters` |
|       - |  640 | ` *  None` |
|       - |  641 | ` * Return` |
|       - |  642 | ` *   Returns an array of the names of the declared classes` |
|       - |  643 | ` *   in the current script.` |
|       - |  644 | ` * Note:` |
|       - |  645 | ` *   NULL is returned on failure.` |
|       - |  646 | ` */` |
|       6 |  647 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  648 | `{` |
|       3 |  649 | `	SXUNUSED(nArg); /* cc warning */` |
|       3 |  650 | `	SXUNUSED(apArg);` |
|       8 |  651 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_CLASS);` |
|       2 |  652 | `}` |
|       - |  653 | `/*` |
|       - |  654 | ` * array get_declared_interfaces(void)` |
|       - |  655 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  656 | ` * Parameters` |
|       - |  657 | ` *  None` |
|       - |  658 | ` * Return` |
|       - |  659 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  660 | ` *   in the current script.` |
|       - |  661 | ` * Note:` |
|       - |  662 | ` *   NULL is returned on failure.` |
|       - |  663 | ` */` |
|       4 |  664 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  665 | `{` |
|       2 |  666 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  667 | `	SXUNUSED(apArg);` |
|       6 |  668 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_INTERFACE);` |
|       2 |  669 | `}` |
|       - |  670 | `/*` |
|       - |  671 | ` * array get_declared_traits(void)` |
|       - |  672 | ` *   Returns an array with the name of the defined traits.` |
|       - |  673 | ` */` |
|       4 |  674 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  675 | `{` |
|       2 |  676 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  677 | `	SXUNUSED(apArg);` |
|       6 |  678 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_TRAIT);` |
|       2 |  679 | `}` |
|       - |  680 | `/*` |
|       - |  681 | ` * Does this method-table entry answer to the method's OWN name (rather than to an` |
|       - |  682 | ` * adaptation alias made from it)? Method names fold case, so the comparison does too.` |
|       - |  683 | ` */` |
|     122 |  684 | `static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  685 | `{` |
|     183 |  686 | `	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte` |
|     122 |  687 | `		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;` |
|       2 |  688 | `}` |
|       - |  689 | `/*` |
|       - |  690 | ` * Which inheritance LEVEL does this method-table entry belong to — the class php would` |
|       - |  691 | ` * have added it under? A method declared in a class body is its own; a trait's is the` |
|       - |  692 | ` * class that COMPOSED it, which is two different questions depending on the entry. An` |
|       - |  693 | ` * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still` |
|       - |  694 | `` * holding this key over this very struct is the one whose `use` block wrote it. A trait`` |
|       - |  695 | ` * method under its own name is the same struct in every class that uses the trait, and` |
|       - |  696 | ` * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait` |
|       - |  697 | ` * composes its own copy, and the parent's inherited entry never replaces it.` |
|       - |  698 | ` */` |
|     392 |  699 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       3 |  700 | `{` |
|     395 |  701 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|     395 |  702 | `	ph7_class *pWalk,*pHigh = 0;` |
|     395 |  703 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     338 |  704 | `		return pDecl;` |
|       - |  705 | `	}` |
|      58 |  706 | `	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){` |
|      62 |  707 | `		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 |  708 | `			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);` |
|      38 |  709 | `			if( pE && pE->pUserData == (void *)pMeth ){` |
|      34 |  710 | `				pHigh = pWalk;` |
|      16 |  711 | `			}` |
|      20 |  712 | `		}` |
|      26 |  713 | `		if( pHigh ){` |
|      26 |  714 | `			return pHigh;` |
|       - |  715 | `		}` |
|     ! 0 |  716 | `	}` |
|      34 |  717 | `	return PH7_VmComposingClass(pClass,pDecl);` |
|     199 |  718 | `}` |
|       - |  719 | `/*` |
|       - |  720 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  721 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  722 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  723 | ` */` |
|     226 |  724 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       3 |  725 | `{` |
|     229 |  726 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|     229 |  727 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     229 |  728 | `	ph7_value_reset_string_cursor(pName);` |
|     229 |  729 | `}` |
|       - |  730 | `/*` |
|       - |  731 | ` * array get_class_methods(object\|string $object_or_class)` |
|       - |  732 | ` *   Returns an array with the names of the class methods the CALLING SCOPE can reach,` |
|       - |  733 | ` *   in php's order: each class's own body methods, then its trait composition, then the` |
|       - |  734 | ` *   same again for every ancestor.` |
|       - |  735 | ` * Parameters` |
|       - |  736 | ` *  object_or_class` |
|       - |  737 | ` *   The class name or a class instance. Anything that does not resolve to a class is a` |
|       - |  738 | ` *   TypeError naming the type given — this builtin never answers NULL.` |
|       - |  739 | ` */` |
|      48 |  740 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  741 | `{` |
|       - |  742 | `	ph7_value *pName,*pArray;` |
|       - |  743 | `	SyHashEntry *pEntry;` |
|       - |  744 | `	ph7_class *pClass;` |
|       - |  745 | `	/* Extract the target class first */` |
|      51 |  746 | `	pClass = 0;` |
|      51 |  747 | `	if( nArg > 0 ){` |
|      51 |  748 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      24 |  749 | `	}` |
|      51 |  750 | `	if( pClass == 0 ){` |
|       - |  751 | `		/* php screens the VALUE, not the type: anything that does not resolve to a` |
|       - |  752 | `		 * class — a name nothing declares, an int, an array, null — is ONE TypeError` |
|       - |  753 | `		 * naming the type given. PHL answered NULL for most of them (and the shared` |
|       - |  754 | ``		 * ZPP screen's `must be of type object\|string` for the rest), so a typo in a`` |
|       - |  755 | `		 * class name silently listed nothing. This is why get_class_methods() joins` |
|       - |  756 | `		 * get_class_vars() on the self-checked list in vm_arg_check.c. */` |
|       - |  757 | `		char zGiven[64];` |
|       9 |  758 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  759 | `			"get_class_methods(): Argument #1 ($object_or_class) must be an object "` |
|       - |  760 | `			"or a valid class name, %s given",` |
|       4 |  761 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - |  762 | `	}` |
|       - |  763 | `	/* Create a new array  */` |
|      47 |  764 | `	pArray = ph7_context_new_array(pCtx);` |
|      47 |  765 | `	pName = ph7_context_new_scalar(pCtx);` |
|      47 |  766 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  767 | `		/* Out of memory,return NULL */` |
|     ! 0 |  768 | `		ph7_result_null(pCtx);` |
|     ! 0 |  769 | `		return PH7_OK;` |
|       - |  770 | `	}` |
|       - |  771 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  772 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  773 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  774 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  775 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  776 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  777 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  778 | `	{` |
|       - |  779 | `		SySet aTmp;` |
|       - |  780 | `		SyHashEntry **apEntry;` |
|       - |  781 | `		ph7_class *pLevel;` |
|       - |  782 | `		sxu32 n;` |
|       - |  783 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  784 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  785 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      47 |  786 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      47 |  787 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|     321 |  788 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|     277 |  789 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       3 |  790 | `		}` |
|      47 |  791 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     107 |  792 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  793 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  794 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  795 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  796 | `			SySet aLvl;` |
|       - |  797 | `			SyHashEntry **apLvl;` |
|       - |  798 | `			sxu32 i,j;` |
|      63 |  799 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  800 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  801 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  802 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  803 | `			 * hash) come out in declaration order walked oldest-first. */` |
|     455 |  804 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|     395 |  805 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|     395 |  806 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - |  807 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - |  808 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - |  809 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - |  810 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - |  811 | `				 * methods before its own. */` |
|     395 |  812 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - |  813 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - |  814 | `				 * protected within the hierarchy, private only from the class that` |
|       - |  815 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - |  816 | `				 * every private and protected name a class holds. Same decision` |
|       - |  817 | `				 * get_class_vars() already makes for properties. */` |
|     395 |  818 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  819 | `					SyString sMName;` |
|      93 |  820 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - |  821 | `						apEntry[nPick]->nKeyLen);` |
|       - |  822 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - |  823 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - |  824 | `					 * wrote it, and judged against the class that composed the method. */` |
|     139 |  825 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      46 |  826 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      46 |  827 | `							pMethod->iProtection,FALSE) ){` |
|      79 |  828 | `						continue;` |
|       - |  829 | `					}` |
|       7 |  830 | `				}` |
|     317 |  831 | `				if( pDecl != pLevel ){` |
|       - |  832 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - |  833 | `					 * counts as the class's own level, like php. */` |
|       - |  834 | `					ph7_class *pWalk;` |
|      89 |  835 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|      35 |  836 | `						continue;` |
|       - |  837 | `					}` |
|     109 |  838 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     109 |  839 | `						if( pWalk == pDecl ){` |
|      55 |  840 | `							break;` |
|       - |  841 | `						}` |
|      28 |  842 | `					}` |
|      55 |  843 | `					if( pWalk != 0 ){` |
|      55 |  844 | `						continue; /* in-chain: its own level emits it */` |
|       - |  845 | `					}` |
|     ! 0 |  846 | `				}` |
|     229 |  847 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     116 |  848 | `			}` |
|      63 |  849 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - |  850 | `			/* Insertion sort by declaration line (stable) */` |
|     231 |  851 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|     171 |  852 | `				SyHashEntry *pKey = apLvl[i];` |
|     235 |  853 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|     255 |  854 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 |  855 | `					apLvl[j] = apLvl[j-1];` |
|      33 |  856 | `				}` |
|     171 |  857 | `				apLvl[j] = pKey;` |
|      87 |  858 | `			}` |
|       - |  859 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - |  860 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - |  861 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - |  862 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - |  863 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - |  864 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - |  865 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - |  866 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - |  867 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - |  868 | `			 * claim still goes out at the end. */` |
|     289 |  869 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     229 |  870 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     229 |  871 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|     229 |  872 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|     186 |  873 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     186 |  874 | `					apLvl[i] = 0;` |
|      92 |  875 | `				}` |
|     116 |  876 | `			}` |
|       - |  877 | `			{` |
|      63 |  878 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|      63 |  879 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - |  880 | `				sxu32 k;` |
|      79 |  881 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 |  882 | `					ph7_class *pTrait = apTrait[k];` |
|       - |  883 | `					SySet aTr;` |
|       - |  884 | `					SyHashEntry **apTr;` |
|       - |  885 | `					SyHashEntry *pTrE;` |
|       - |  886 | `					sxu32 t;` |
|      18 |  887 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 |  888 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 |  889 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 |  890 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 |  891 | `					}` |
|      18 |  892 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - |  893 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - |  894 | `					 * declaration order. */` |
|      48 |  895 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 |  896 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - |  897 | `						int bWantAlias;` |
|       - |  898 | `						/* First pass emits the aliases made from this method, second the` |
|       - |  899 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 |  900 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 |  901 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  902 | `								ph7_class_method *pM;` |
|     342 |  903 | `								if( apLvl[i] == 0 ){` |
|     184 |  904 | `									continue;` |
|       - |  905 | `								}` |
|     160 |  906 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 |  907 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 |  908 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 |  909 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 |  910 | `										pOrigin->sFunc.sName.zString,` |
|      98 |  911 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 |  912 | `									continue;` |
|       - |  913 | `								}` |
|      68 |  914 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 |  915 | `									continue;` |
|       - |  916 | `								}` |
|      44 |  917 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 |  918 | `								apLvl[i] = 0;` |
|      23 |  919 | `							}` |
|      32 |  920 | `						}` |
|      17 |  921 | `					}` |
|      18 |  922 | `					SySetRelease(&aTr);` |
|      10 |  923 | `				}` |
|       - |  924 | `			}` |
|     289 |  925 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  926 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - |  927 | `				 * that another trait then composed, say — keeps the line order. */` |
|     229 |  928 | `				if( apLvl[i] != 0 ){` |
|     ! 0 |  929 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 |  930 | `				}` |
|     116 |  931 | `			}` |
|      63 |  932 | `			SySetRelease(&aLvl);` |
|      33 |  933 | `		}` |
|      47 |  934 | `		SySetRelease(&aTmp);` |
|       - |  935 | `	}` |
|       - |  936 | `	/* Return the created array */` |
|      47 |  937 | `	ph7_result_value(pCtx,pArray);` |
|       - |  938 | `	/*` |
|       - |  939 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  940 | `	 * automatically as soon we return from this foreign function.` |
|       - |  941 | `	 */` |
|      47 |  942 | `	return PH7_OK;` |
|      27 |  943 | `}` |
|       - |  944 | `/*` |
|       - |  945 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - |  946 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - |  947 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - |  948 | ` *` |
|       - |  949 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - |  950 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - |  951 | ` * private/protected refusal raised from inside a class.` |
|       - |  952 | ` */` |
|    3044 |  953 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 |  954 | `{` |
|    3049 |  955 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - |  956 | `	ph7_vm_func *pVmFunc;` |
|    3145 |  957 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - |  958 | `		/* Safely ignore the exception frame */` |
|     101 |  959 | `		pFrame = pFrame->pParent;` |
|       5 |  960 | `	}` |
|    3049 |  961 | `	if( pFrame == 0 ){` |
|     ! 0 |  962 | `		return 0;` |
|       - |  963 | `	}` |
|    3049 |  964 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  965 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - |  966 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|    3049 |  967 | `	if( pFrame->pBoundScope ){` |
|      32 |  968 | `		return pFrame->pBoundScope;` |
|       - |  969 | `	}` |
|    3019 |  970 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    2479 |  971 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  972 | `	}` |
|     545 |  973 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  974 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - |  975 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - |  976 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - |  977 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - |  978 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - |  979 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - |  980 | `		 * pBoundScope. */` |
|      67 |  981 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  982 | `	}` |
|     479 |  983 | `	if( pVm->pConstEvalClass ){` |
|       - |  984 | `		/* Constant/property initializer bytecode runs without a method` |
|       - |  985 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - |  986 | `		 * constant is reachable from its own class's initializers). */` |
|       3 |  987 | `		return pVm->pConstEvalClass;` |
|       - |  988 | `	}` |
|     477 |  989 | `	return 0;` |
|    1527 |  990 | `}` |
|       - |  991 | `/*` |
|       - |  992 | ` * The scope php NAMES in a visibility Error. PH7_VmCallerScope with one adjustment: php` |
|       - |  993 | ` * flattens a TRAIT into the class that uses it, so code running in a trait method reports` |
|       - |  994 | ` * the USING class ("from scope Base"), never the trait — and not the RECEIVER's class` |
|       - |  995 | `` * either, so `class Kid extends Base` (Base being the one that composed the trait) still`` |
|       - |  996 | ` * reports Base. Walk the receiver's ancestry to the first class that uses this trait; the` |
|       - |  997 | ` * trait itself stands when nothing does (nothing php would print, but better than a lie).` |
|       - |  998 | ` *` |
|       - |  999 | ` * Kept apart from PH7_VmCallerScope because the ACCESS decision genuinely wants the trait:` |
|       - | 1000 | ` * its private/protected branches grant on "the caller is a trait used by the target class"` |
|       - | 1001 | ` * and on the reverse, and both compare against the trait itself.` |
|       - | 1002 | ` */` |
|      92 | 1003 | `PH7_PRIVATE ph7_class * PH7_VmCallerScopeName(ph7_vm *pVm)` |
|       4 | 1004 | `{` |
|      96 | 1005 | `	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       - | 1006 | `	VmFrame *pFrame;` |
|       - | 1007 | `	ph7_class *pWalk;` |
|      96 | 1008 | `	if( pScope == 0 \|\| (pScope->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      92 | 1009 | `		return pScope;` |
|       - | 1010 | `	}` |
|       5 | 1011 | `	pFrame = pVm->pFrame;` |
|       5 | 1012 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|     ! 0 | 1013 | `		pFrame = pFrame->pParent;` |
|     ! 0 | 1014 | `	}` |
|       5 | 1015 | `	pWalk = (pFrame && pFrame->pThis) ? pFrame->pThis->pClass : VmCurrentSelf(&(*pVm));` |
|       7 | 1016 | `	for( ; pWalk ; pWalk = pWalk->pBase ){` |
|       7 | 1017 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|       7 | 1018 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - | 1019 | `		sxu32 k;` |
|       7 | 1020 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|       5 | 1021 | `			if( apTrait[k] == pScope ){` |
|       5 | 1022 | `				return pWalk;` |
|       - | 1023 | `			}` |
|     ! 0 | 1024 | `		}` |
|       2 | 1025 | `	}` |
|     ! 0 | 1026 | `	return pScope;` |
|      50 | 1027 | `}` |
|       - | 1028 | `/*` |
|       - | 1029 | ` * The DECLARING-side twin of PH7_VmCallerScopeName: the class php NAMES as a method's` |
|       - | 1030 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - | 1031 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - | 1032 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - | 1033 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - | 1034 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - | 1035 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - | 1036 | ` *` |
|       - | 1037 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - | 1038 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - | 1039 | ` * refused on a child instance names the BASE.` |
|       - | 1040 | ` */` |
|     786 | 1041 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 | 1042 | `{` |
|     393 | 1043 | `	SXUNUSED(pVm);` |
|    1577 | 1044 | `	return PH7_VmComposingClass(pClass,` |
|     786 | 1045 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 | 1046 | `}` |
|       - | 1047 | `/*` |
|       - | 1048 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - | 1049 | ` * property is composed into the using class exactly as its methods are, and` |
|       - | 1050 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - | 1051 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - | 1052 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - | 1053 | ` */` |
|     834 | 1054 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 | 1055 | `{` |
|       - | 1056 | `	ph7_class *pWalk;` |
|     839 | 1057 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     741 | 1058 | `		return pDecl;` |
|       - | 1059 | `	}` |
|     139 | 1060 | `	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|     137 | 1061 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|     137 | 1062 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - | 1063 | `		sxu32 k;` |
|     159 | 1064 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|     121 | 1065 | `			if( apTrait[k] == pDecl ){` |
|      99 | 1066 | `				return pWalk;` |
|       - | 1067 | `			}` |
|      13 | 1068 | `		}` |
|      21 | 1069 | `	}` |
|       3 | 1070 | `	return pDecl;` |
|     422 | 1071 | `}` |
|       - | 1072 | `/*` |
|       - | 1073 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1074 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1075 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1076 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1077 | ` * holds no entry for it.` |
|       - | 1078 | ` */` |
|      38 | 1079 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       3 | 1080 | `{` |
|      41 | 1081 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|      41 | 1082 | `	if( pEntry ){` |
|      41 | 1083 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      22 | 1084 | `	}else{` |
|     ! 0 | 1085 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1086 | `	}` |
|      41 | 1087 | `}` |
|       - | 1088 | `/*` |
|       - | 1089 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1090 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1091 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1092 | ` */` |
|  213992 | 1093 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1094 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1095 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1096 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1097 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1098 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1099 | `	)` |
|       5 | 1100 | `{` |
|  213997 | 1101 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    2957 | 1102 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    2957 | 1103 | `		if( pCallerScope == 0 ){` |
|     403 | 1104 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1105 | `		}` |
|    2559 | 1106 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1107 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1108 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1109 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1110 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1111 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1112 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1113 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1114 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1115 | `			 * match (dynamic props and other non-declared shapes). */` |
|    2369 | 1116 | `			ph7_class *pCaller = pCallerScope;` |
|    3551 | 1117 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|    2364 | 1118 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|    2369 | 1119 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|    2369 | 1120 | `			int bGranted = 0;` |
|    2369 | 1121 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    2044 | 1122 | `				if( pOwn->pDeclClass == 0` |
|    2044 | 1123 | `				 \|\| pOwn->pDeclClass == pCaller` |
|    1043 | 1124 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|    2019 | 1125 | `					bGranted = 1;` |
|    1012 | 1126 | `				}` |
|    1347 | 1127 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     221 | 1128 | `				bGranted = 1;` |
|     108 | 1129 | `			}` |
|    2369 | 1130 | `			if( !bGranted ){` |
|       - | 1131 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1132 | `				ph7_class **apTrait;` |
|       - | 1133 | `				sxu32 nTrait,k;` |
|     138 | 1134 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     138 | 1135 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     138 | 1136 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1137 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1138 | `						bGranted = 1;` |
|     ! 0 | 1139 | `						break;` |
|       - | 1140 | `					}` |
|     ! 0 | 1141 | `				}` |
|      67 | 1142 | `			}` |
|    2369 | 1143 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1144 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1145 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1146 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1147 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1148 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1149 | `				 * whose owner is the trait, not the class). */` |
|       - | 1150 | `				ph7_class **apTrait;` |
|       - | 1151 | `				sxu32 nTrait,k;` |
|     ! 0 | 1152 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     ! 0 | 1153 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     ! 0 | 1154 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1155 | `					if( apTrait[k] == pClass ){` |
|     ! 0 | 1156 | `						bGranted = 1;` |
|     ! 0 | 1157 | `						break;` |
|       - | 1158 | `					}` |
|     ! 0 | 1159 | `				}` |
|     ! 0 | 1160 | `			}` |
|    2369 | 1161 | `			if( !bGranted ){` |
|     138 | 1162 | `				goto dis; /* Access is forbidden */` |
|       - | 1163 | `			}` |
|    1120 | 1164 | `		}else{` |
|       - | 1165 | `			/* Protected */` |
|     194 | 1166 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1167 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1168 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1169 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1170 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1171 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1172 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1173 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1174 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1175 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1176 | `			 * from a sibling, matching php. */` |
|     194 | 1177 | `			ph7_class *pIntro = pClass;` |
|       - | 1178 | `			ph7_class *pAnc;` |
|     478 | 1179 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|     288 | 1180 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|     288 | 1181 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     288 | 1182 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|     288 | 1183 | `				int bHere = 0;` |
|     288 | 1184 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      55 | 1185 | `					bHere = 1;` |
|      27 | 1186 | `				}` |
|     288 | 1187 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|     105 | 1188 | `					bHere = 1;` |
|      51 | 1189 | `				}` |
|     288 | 1190 | `				if( bHere ){` |
|     159 | 1191 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      78 | 1192 | `				}` |
|     146 | 1193 | `			}` |
|       - | 1194 | `			/* Must be in the same class hierarchy as the introducing class */` |
|     194 | 1195 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      14 | 1196 | `				int bTraitGrant = 0;` |
|      14 | 1197 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1198 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1199 | `					ph7_class **apTrait;` |
|       - | 1200 | `					sxu32 nTrait,k;` |
|       3 | 1201 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|       3 | 1202 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|       3 | 1203 | `					for(k = 0; k < nTrait; k++){` |
|       3 | 1204 | `						if( apTrait[k] == pClass ){` |
|       3 | 1205 | `							bTraitGrant = 1;` |
|       3 | 1206 | `							break;` |
|       - | 1207 | `						}` |
|     ! 0 | 1208 | `					}` |
|       1 | 1209 | `				}` |
|      14 | 1210 | `				if( !bTraitGrant ){` |
|      11 | 1211 | `					goto dis; /* Access is forbidden */` |
|       - | 1212 | `				}` |
|       1 | 1213 | `			}` |
|       - | 1214 | `		}` |
|    1205 | 1215 | `	}` |
|  213455 | 1216 | `	return 1; /* Access is granted */` |
|     271 | 1217 | `dis:` |
|     547 | 1218 | `	if( bLog ){` |
|     ! 0 | 1219 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1220 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1221 | `			&pClass->sName,pAttrName);` |
|     ! 0 | 1222 | `	}` |
|     547 | 1223 | `	return 0; /* Access is forbidden */` |
|  107001 | 1224 | `}` |
|       - | 1225 | `/*` |
|       - | 1226 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1227 | ` *   Get the default properties of the class` |
|       - | 1228 | ` * Parameters` |
|       - | 1229 | ` *  class_name` |
|       - | 1230 | ` *   The class name or class instance` |
|       - | 1231 | ` * Return` |
|       - | 1232 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1233 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1234 | ` *  of varname => value.` |
|       - | 1235 | ` * Note:` |
|       - | 1236 | ` *   NULL is returned on failure.` |
|       - | 1237 | ` */` |
|      20 | 1238 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1239 | `{` |
|       - | 1240 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1241 | `	SyHashEntry *pEntry;` |
|       - | 1242 | `	ph7_class *pClass;` |
|       - | 1243 | `	/* Extract the target class first */` |
|      23 | 1244 | `	pClass = 0;` |
|      23 | 1245 | `	if( nArg > 0 ){` |
|      23 | 1246 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      10 | 1247 | `	}` |
|      23 | 1248 | `	if( pClass == 0 ){` |
|       - | 1249 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1250 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1251 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1252 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1253 | `		int nLen = 0;` |
|     ! 0 | 1254 | `		const char *zVal = "";` |
|     ! 0 | 1255 | `		if( nArg > 0 ){` |
|     ! 0 | 1256 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1257 | `				zVal = "Array";` |
|     ! 0 | 1258 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1259 | `			}else{` |
|     ! 0 | 1260 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1261 | `			}` |
|     ! 0 | 1262 | `		}` |
|     ! 0 | 1263 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1264 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1265 | `			nLen,zVal);` |
|       - | 1266 | `	}` |
|      23 | 1267 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1268 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1269 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1270 | `		 * here, as it does in php. */` |
|       5 | 1271 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 | 1272 | `		if( rcMat != SXRET_OK ){` |
|       5 | 1273 | `			return rcMat;` |
|       - | 1274 | `		}` |
|     ! 0 | 1275 | `	}` |
|       - | 1276 | `	/* Create a new array  */` |
|      19 | 1277 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 1278 | `	pName = ph7_context_new_scalar(pCtx);` |
|      19 | 1279 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      19 | 1280 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1281 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1282 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1283 | `		return PH7_OK;` |
|       - | 1284 | `	}` |
|       - | 1285 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|      19 | 1286 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      55 | 1287 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      39 | 1288 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      39 | 1289 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1290 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1291 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 | 1292 | `			continue;` |
|       - | 1293 | `		}` |
|       - | 1294 | `		/* Check if the access is allowed */` |
|      37 | 1295 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      31 | 1296 | `			SyString *pAttrName = &pAttr->sName;` |
|      31 | 1297 | `			ph7_value *pValue = 0;` |
|      31 | 1298 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1299 | `				/* Static slots are computed at mount; constants lazily */` |
|       8 | 1300 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       8 | 1301 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       5 | 1302 | `			}else{` |
|      24 | 1303 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 | 1304 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1305 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 | 1306 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 | 1307 | `					pValue = &sValue;` |
|       2 | 1308 | `				}` |
|       - | 1309 | `			}` |
|       - | 1310 | `			/* Fill in the array */` |
|      31 | 1311 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      31 | 1312 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1313 | `			/* Reset the cursor */` |
|      31 | 1314 | `			ph7_value_reset_string_cursor(pName);` |
|      14 | 1315 | `		}` |
|       3 | 1316 | `	}` |
|      19 | 1317 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1318 | `	/* Return the created array */` |
|      19 | 1319 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1320 | `	/*` |
|       - | 1321 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1322 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1323 | `	 */` |
|      19 | 1324 | `	return PH7_OK;` |
|      13 | 1325 | `}` |
|       - | 1326 | `/*` |
|       - | 1327 | ` * array get_object_vars(object $this)` |
|       - | 1328 | ` *   Gets the properties of the given object` |
|       - | 1329 | ` * Parameters` |
|       - | 1330 | ` *  this` |
|       - | 1331 | ` *   A class instance` |
|       - | 1332 | ` * Return` |
|       - | 1333 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1334 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1335 | ` *  it will be returned with a NULL value.` |
|       - | 1336 | ` * Note:` |
|       - | 1337 | ` *   NULL is returned on failure.` |
|       - | 1338 | ` */` |
|     256 | 1339 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1340 | `{` |
|     261 | 1341 | `	ph7_class_instance *pThis = 0;` |
|       - | 1342 | `	ph7_value *pName,*pArray;` |
|       - | 1343 | `	SyHashEntry *pEntry;` |
|     261 | 1344 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1345 | `		/* Extract the target instance */` |
|     261 | 1346 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     128 | 1347 | `	}` |
|     261 | 1348 | `	if( pThis == 0 ){` |
|       - | 1349 | `		/* No such instance,return NULL */` |
|     ! 0 | 1350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1351 | `		return PH7_OK;` |
|       - | 1352 | `	}` |
|       - | 1353 | `	/* Create a new array  */` |
|     261 | 1354 | `	pArray = ph7_context_new_array(pCtx);` |
|     261 | 1355 | `	pName = ph7_context_new_scalar(pCtx);` |
|     261 | 1356 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1357 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1358 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1359 | `		return PH7_OK;` |
|       - | 1360 | `	}` |
|       - | 1361 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1362 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1363 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1364 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1365 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1366 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1367 | `	 * skipped instead of read after free. */` |
|       - | 1368 | `	{` |
|       - | 1369 | `		SySet sNames;` |
|       - | 1370 | `		SyString *aName;` |
|       - | 1371 | `		sxu32 iName,nName;` |
|     261 | 1372 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     261 | 1373 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    1231 | 1374 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     975 | 1375 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     975 | 1376 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|       - | 1377 | `				/* Only non-static/constant attributes are extracted */` |
|     365 | 1378 | `				continue;` |
|       - | 1379 | `			}` |
|     615 | 1380 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      28 | 1381 | `				continue; /* typed, never written: not there yet (php) */` |
|       - | 1382 | `			}` |
|     584 | 1383 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     297 | 1384 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     ! 0 | 1385 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1386 | `			}` |
|     589 | 1387 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       5 | 1388 | `		}` |
|     261 | 1389 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     261 | 1390 | `		nName = SySetUsed(&sNames);` |
|     845 | 1391 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     589 | 1392 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1393 | `			VmClassAttr *pVmAttr;` |
|     589 | 1394 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     589 | 1395 | `			if( pEntry == 0 ){` |
|     ! 0 | 1396 | `				continue; /* unset by an earlier hook */` |
|       - | 1397 | `			}` |
|     589 | 1398 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1399 | `			/* Check if the access is allowed */` |
|     589 | 1400 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     545 | 1401 | `				ph7_value *pValue = 0;` |
|       - | 1402 | `				ph7_value sHookVal;` |
|       - | 1403 | `				sxi32 rcHk;` |
|       - | 1404 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1405 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     545 | 1406 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     545 | 1407 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     545 | 1408 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1409 | `					pValue = &sHookVal;` |
|     538 | 1410 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1411 | `					/* Extract attribute */` |
|     531 | 1412 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     268 | 1413 | `				}else{` |
|       - | 1414 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1415 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1416 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1417 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1418 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1419 | `					break;` |
|       - | 1420 | `				}` |
|     545 | 1421 | `				if( pValue ){` |
|       - | 1422 | `					/* Insert attribute name in the array */` |
|     545 | 1423 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     545 | 1424 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     270 | 1425 | `				}` |
|     545 | 1426 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1427 | `				/* Reset the cursor */` |
|     545 | 1428 | `				ph7_value_reset_string_cursor(pName);` |
|     270 | 1429 | `			}` |
|     297 | 1430 | `		}` |
|     261 | 1431 | `		SySetRelease(&sNames);` |
|       - | 1432 | `	}` |
|       - | 1433 | `	/* Return the created array */` |
|     261 | 1434 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1435 | `	/*` |
|       - | 1436 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1437 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1438 | `	 */` |
|     261 | 1439 | `	return PH7_OK;` |
|     133 | 1440 | `}` |
|       - | 1441 | `/*` |
|       - | 1442 | ` * array get_mangled_object_vars(object $object)` |
|       - | 1443 | ` *  The object's own property table, with php's visibility MANGLING left on the keys:` |
|       - | 1444 | `` *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".`` |
|       - | 1445 | ` *` |
|       - | 1446 | ` *  It is get_object_vars()'s opposite in both of that function's decisions — no` |
|       - | 1447 | ` *  visibility screen (every property is reported, from every level of the chain) and` |
|       - | 1448 | `` *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,`` |
|       - | 1449 | ` *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not` |
|       - | 1450 | ` *  the (array) cast either: the cast asks a native class's own handler, so` |
|       - | 1451 | `` *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.`` |
|       - | 1452 | ` */` |
|      14 | 1453 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1454 | `{` |
|      16 | 1455 | `	ph7_class_instance *pThis = 0;` |
|       - | 1456 | `	ph7_value *pArray;` |
|      16 | 1457 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1458 | `		/* Extract the target instance */` |
|      16 | 1459 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       7 | 1460 | `	}` |
|      16 | 1461 | `	if( pThis == 0 ){` |
|       - | 1462 | ``		/* The `object $object` signature row refuses everything else before we run */`` |
|     ! 0 | 1463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1464 | `		return PH7_OK;` |
|       - | 1465 | `	}` |
|      16 | 1466 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 1467 | `	if( pArray == 0 ){` |
|       - | 1468 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1469 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1470 | `		return PH7_OK;` |
|       - | 1471 | `	}` |
|      16 | 1472 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);` |
|      16 | 1473 | `	ph7_result_value(pCtx,pArray);` |
|      16 | 1474 | `	return PH7_OK;` |
|       9 | 1475 | `}` |
|       - | 1476 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1477 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1478 | ` * detection should reject them up front. */` |
|       - | 1479 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1480 | `/*` |
|       - | 1481 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1482 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1483 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1484 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1485 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1486 | ` */` |
| 2526155 | 1487 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1488 | `{` |
| 2588732 | 1489 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1490 | `		ph7_class **apParent;` |
|       - | 1491 | `		sxu32 n;` |
| 2546914 | 1492 | `		if( pIface == pTarget ){` |
| 2484340 | 1493 | `			return TRUE;` |
|       - | 1494 | `		}` |
|       - | 1495 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1496 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|   62579 | 1497 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   62583 | 1498 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       7 | 1499 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|       3 | 1500 | `				return TRUE;` |
|       - | 1501 | `			}` |
|       3 | 1502 | `		}` |
|   62577 | 1503 | `		pIface = pIface->pBase;` |
|   62577 | 1504 | `		iDepth++;` |
|       5 | 1505 | `	}` |
|   41823 | 1506 | `	return FALSE;` |
| 1263096 | 1507 | `}` |
|       - | 1508 | `/*` |
|       - | 1509 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1510 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1511 | ` */` |
| 2780942 | 1512 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1513 | `{` |
|       - | 1514 | `	ph7_class **apInterface;` |
|       - | 1515 | `	sxu32 n;` |
| 2780947 | 1516 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1517 | `		/* Empty interface container */` |
|  265672 | 1518 | `		return FALSE;` |
|       - | 1519 | `	}` |
|       - | 1520 | `	/* Point to the set of implemented interfaces */` |
| 2515280 | 1521 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1522 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1523 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2557094 | 1524 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2526154 | 1525 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2484340 | 1526 | `			return TRUE;` |
|       - | 1527 | `		}` |
|   20926 | 1528 | `	}` |
|   30945 | 1529 | `	return FALSE;` |
| 1390489 | 1530 | `}` |
|       - | 1531 | `/*` |
|       - | 1532 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1533 | ` * is an instance of the main class (second argument).` |
|       - | 1534 | ` * Otherwise FALSE is returned.` |
|       - | 1535 | ` */` |
| 4073228 | 1536 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1537 | `{` |
|       - | 1538 | `	ph7_class *pParent;` |
|       - | 1539 | `	sxi32 rc;` |
| 4073233 | 1540 | `	if( pThis == pClass ){` |
|       - | 1541 | `		/* Instance of the same class */` |
| 1438064 | 1542 | `		return TRUE;` |
|       - | 1543 | `	}` |
|       - | 1544 | `	/* Check implemented interfaces */` |
| 2635174 | 1545 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2635174 | 1546 | `	if( rc ){` |
| 2358591 | 1547 | `		return TRUE;` |
|       - | 1548 | `	}` |
|       - | 1549 | `	/* Check parent classes */` |
|  276588 | 1550 | `	pParent = pThis->pBase;` |
|  296588 | 1551 | `	while( pParent ){` |
|  148424 | 1552 | `		if( pParent == pClass ){` |
|       - | 1553 | `			/* Same instance */` |
|    2685 | 1554 | `			return TRUE;` |
|       - | 1555 | `		}` |
|       - | 1556 | `		/* Check the implemented interfaces */` |
|  145744 | 1557 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  145744 | 1558 | `		if( rc ){` |
|  125744 | 1559 | `			return TRUE;` |
|       - | 1560 | `		}` |
|       - | 1561 | `		/* Point to the parent class */` |
|   20005 | 1562 | `		pParent = pParent->pBase;` |
|       5 | 1563 | `	}` |
|       - | 1564 | `	/* Not an instance of the the given class */` |
|  148169 | 1565 | `	return FALSE;` |
| 2036630 | 1566 | `}` |
|       - | 1567 | `/*` |
|       - | 1568 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1569 | ` * is a subclass of the main class (second argument).` |
|       - | 1570 | ` * Otherwise FALSE is returned.` |
|       - | 1571 | ` */` |
|      44 | 1572 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       2 | 1573 | `{` |
|       - | 1574 | `	SyHashEntry *pEntry;` |
|       - | 1575 | `	SyString *pName;` |
|      70 | 1576 | `	while( pClass ){` |
|      66 | 1577 | `		pName = &pClass->sName;` |
|       - | 1578 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      66 | 1579 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      66 | 1580 | `		if( pEntry ){` |
|      32 | 1581 | `			return TRUE;` |
|       - | 1582 | `		}` |
|       - | 1583 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1584 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1585 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1586 | `		 * class's aInterface, missing inherited interfaces. */` |
|      35 | 1587 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1588 | `			return TRUE;` |
|       - | 1589 | `		}` |
|      25 | 1590 | `		pClass = pClass->pBase;` |
|       1 | 1591 | `	}` |
|       - | 1592 | `	/* Not a subclass */` |
|       5 | 1593 | `	return FALSE;` |
|      24 | 1594 | `}` |
|       - | 1595 | `/*` |
|       - | 1596 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1597 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1598 | ` *   as one of its parents.` |
|       - | 1599 | ` * Parameters` |
|       - | 1600 | ` *  object_or_class` |
|       - | 1601 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1602 | ` * class` |
|       - | 1603 | ` *  The class or interface name to test against.` |
|       - | 1604 | ` * allow_string` |
|       - | 1605 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1606 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1607 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1608 | ` * Return` |
|       - | 1609 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1610 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1611 | ` */` |
|      34 | 1612 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1613 | `{` |
|      37 | 1614 | `	int res = 0; /* Assume FALSE by default */` |
|      37 | 1615 | `	if( nArg > 1 ){` |
|      37 | 1616 | `		ph7_class *pThisClass = 0;` |
|      37 | 1617 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1618 | `			/* An object first argument: allow_string is ignored (php). */` |
|      17 | 1619 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      29 | 1620 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1621 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1622 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1623 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1624 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      18 | 1625 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       8 | 1626 | `		}` |
|      37 | 1627 | `		if( pThisClass ){` |
|       - | 1628 | `			/* Extract the given class */` |
|      31 | 1629 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      31 | 1630 | `			if( pClass ){` |
|       - | 1631 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1632 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      31 | 1633 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      14 | 1634 | `			}` |
|      14 | 1635 | `		}` |
|      17 | 1636 | `	}` |
|       - | 1637 | `	/* Query result */` |
|      37 | 1638 | `	ph7_result_bool(pCtx,res);` |
|      37 | 1639 | `	return PH7_OK;` |
|       3 | 1640 | `}` |
|       - | 1641 | `/*` |
|       - | 1642 | ` * int spl_object_id(object $object)` |
|       - | 1643 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1644 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1645 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1646 | ` */` |
|      22 | 1647 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1648 | `{` |
|       - | 1649 | `	ph7_class_instance *pThis;` |
|      26 | 1650 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1651 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1652 | `		return PH7_OK;` |
|       - | 1653 | `	}` |
|      26 | 1654 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      26 | 1655 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      26 | 1656 | `	return PH7_OK;` |
|      15 | 1657 | `}` |
|       - | 1658 | `/*` |
|       - | 1659 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 1660 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 1661 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 1662 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 1663 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 1664 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 1665 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 1666 | ` *` |
|       - | 1667 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 1668 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 1669 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 1670 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 1671 | ` */` |
|      50 | 1672 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1673 | `{` |
|      51 | 1674 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1675 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1676 | `	char zGiven[64];` |
|       - | 1677 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 1678 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      51 | 1679 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1680 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1681 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 1682 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 1683 | `	}` |
|      51 | 1684 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1685 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 1686 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 1687 | `	 * resource, and Generator/Fiber. */` |
|      50 | 1688 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|      48 | 1689 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      13 | 1690 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       8 | 1691 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sName);` |
|       - | 1692 | `	}` |
|      43 | 1693 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 1694 | `	if( pClone == 0 ){` |
|     ! 0 | 1695 | `		return PH7_VmMemoryError(pVm);` |
|       - | 1696 | `	}` |
|       - | 1697 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 1698 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 1699 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 1700 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 1701 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 1702 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 1703 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 1704 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 1705 | `		sxu32 n;` |
|      53 | 1706 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 1707 | `			ph7_value *pVal,sVal;` |
|       - | 1708 | `			const char *zName;` |
|       - | 1709 | `			sxu32 nName;` |
|       - | 1710 | `			char zKeyBuf[64];` |
|       - | 1711 | `			sxi32 rc;` |
|      31 | 1712 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 1713 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 1714 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 1715 | `				zName = zKeyBuf;` |
|     ! 0 | 1716 | `			}else{` |
|      31 | 1717 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 1718 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 1719 | `			}` |
|      31 | 1720 | `			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 1721 | `			if( pVal ){` |
|       - | 1722 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 1723 | `				 * property, whose slot reservation can reallocate pVm->aMemObj and` |
|       - | 1724 | `				 * dangle pVal (a pointer into it). */` |
|      31 | 1725 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 1726 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 1727 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 1728 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 1729 | `				if( rc != SXRET_OK ){` |
|       7 | 1730 | `					return rc;` |
|       - | 1731 | `				}` |
|      12 | 1732 | `			}` |
|      25 | 1733 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 1734 | `		}` |
|      11 | 1735 | `	}` |
|      37 | 1736 | `	return PH7_OK;` |
|      26 | 1737 | `}` |
|       - | 1738 | `/*` |
|       - | 1739 | ` * string spl_object_hash(object $object)` |
|       - | 1740 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 1741 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 1742 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 1743 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 1744 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 1745 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 1746 | ` */` |
|      18 | 1747 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1748 | `{` |
|       - | 1749 | `	ph7_class_instance *pThis;` |
|      20 | 1750 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1751 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1752 | `		return PH7_OK;` |
|       - | 1753 | `	}` |
|      20 | 1754 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 1755 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 1756 | `	return PH7_OK;` |
|      11 | 1757 | `}` |
|       - | 1758 | `/*` |
|       - | 1759 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 1760 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 1761 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 1762 | ` * Parameters` |
|       - | 1763 | ` *  object_or_class` |
|       - | 1764 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 1765 | ` * class` |
|       - | 1766 | ` *  The class or interface name to test against.` |
|       - | 1767 | ` * allow_string` |
|       - | 1768 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 1769 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 1770 | ` *  object first argument (php).` |
|       - | 1771 | ` * Return` |
|       - | 1772 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 1773 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 1774 | ` */` |
|      56 | 1775 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1776 | `{` |
|      58 | 1777 | `	int res = 0; /* Assume FALSE by default */` |
|      58 | 1778 | `	if( nArg > 1 ){` |
|      58 | 1779 | `		ph7_class *pClass = 0;` |
|      58 | 1780 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1781 | `			/* An object first argument: allow_string is ignored (php). */` |
|      20 | 1782 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      49 | 1783 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 1784 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 1785 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 1786 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 1787 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      32 | 1788 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      15 | 1789 | `		}` |
|      58 | 1790 | `		if( pClass ){` |
|       - | 1791 | `			/* Extract the target class */` |
|      48 | 1792 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      48 | 1793 | `			if( pMain ){` |
|       - | 1794 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      46 | 1795 | `				res = VmSubclassOf(pClass,pMain);` |
|      22 | 1796 | `			}` |
|      23 | 1797 | `		}` |
|      28 | 1798 | `	}` |
|       - | 1799 | `	/* Query result */` |
|      58 | 1800 | `	ph7_result_bool(pCtx,res);` |
|      58 | 1801 | `	return PH7_OK;` |
|       2 | 1802 | `}` |
|     236 | 1803 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1804 | `{` |
|       - | 1805 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 1806 | `	sxi32 rc;` |
|     240 | 1807 | `	if( nArg < 1 ){` |
|       - | 1808 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1809 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1810 | `		return PH7_OK;` |
|       - | 1811 | `	}` |
|       - | 1812 | `	{` |
|       - | 1813 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 1814 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     240 | 1815 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     240 | 1816 | `		if( rcCb != PH7_OK ){` |
|      72 | 1817 | `			return rcCb;` |
|       - | 1818 | `		}` |
|       - | 1819 | `	}` |
|     170 | 1820 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     170 | 1821 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1822 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     170 | 1823 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 1824 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 1825 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 1826 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 1827 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 1828 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 1829 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     180 | 1830 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      21 | 1831 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 1832 | `		VmCallArgMap sInner;` |
|       - | 1833 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 1834 | `		 * unset when forwarded, not as stack garbage. */` |
|      21 | 1835 | `		SyZero(&sInner,sizeof(sInner));` |
|      21 | 1836 | `		sInner.bHasNamed = 1;` |
|      21 | 1837 | `		sInner.bIsNamespaced = 0;` |
|       - | 1838 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 1839 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 1840 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 1841 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      21 | 1842 | `		sInner.bStrict = 0;` |
|      21 | 1843 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      21 | 1844 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|       - | 1845 | ``		/* php's compiler rewrites `call_user_func(f, ...)` into a direct call to`` |
|       - | 1846 | `		 * f, so a DROPPED answer here is a dropped answer for the callback: hand` |
|       - | 1847 | `		 * the bit on, one call deep. */` |
|      21 | 1848 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|      21 | 1849 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      11 | 1850 | `	}else{` |
|       - | 1851 | `		/* call_user_func is one of php's two FORWARDS: the callback binds under the` |
|       - | 1852 | `		 * mode of the file that wrote the call_user_func, not weakly like every other` |
|       - | 1853 | `		 * internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 1854 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 1855 | `		 * other internal invocation, and wrong here). */` |
|       - | 1856 | `		VmCallArgMap sFwd;` |
|     150 | 1857 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     150 | 1858 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     150 | 1859 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;   /* see above */` |
|     150 | 1860 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 1861 | `	}` |
|       - | 1862 | `	/* The latch is consumed by the OP_CALL the dispatch builds; clear it for the` |
|       - | 1863 | `	 * paths that never reach one, so it cannot describe some later call. */` |
|     170 | 1864 | `	pCtx->pVm->bDiscardCallback = 0;` |
|     170 | 1865 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1866 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 1867 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      18 | 1868 | `		PH7_MemObjRelease(&sResult);` |
|      18 | 1869 | `		return PH7_EXCEPTION;` |
|       - | 1870 | `	}` |
|     153 | 1871 | `	if( rc != SXRET_OK ){` |
|       - | 1872 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1873 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1874 | `	}else{` |
|       - | 1875 | `		/* Callback result */` |
|     153 | 1876 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1877 | `	}` |
|     153 | 1878 | `	PH7_MemObjRelease(&sResult);` |
|     153 | 1879 | `	return PH7_OK;` |
|     122 | 1880 | `}` |
|       - | 1881 | `/*` |
|       - | 1882 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 1883 | ` *  Call a callback with an array of parameters.` |
|       - | 1884 | ` * Parameter` |
|       - | 1885 | ` *  $callback` |
|       - | 1886 | ` *   The callable to be called.` |
|       - | 1887 | ` * $param_arr` |
|       - | 1888 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 1889 | ` * Return` |
|       - | 1890 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 1891 | ` */` |
|     202 | 1892 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1893 | `{` |
|       - | 1894 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 1895 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 1896 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 1897 | `	SySet aArg;               /* Argument value pointers */` |
|     205 | 1898 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     205 | 1899 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     205 | 1900 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 1901 | `	sxi32 rc;` |
|       - | 1902 | `	sxu32 n;` |
|     205 | 1903 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1904 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1905 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1906 | `		return PH7_OK;` |
|       - | 1907 | `	}` |
|       - | 1908 | `	{` |
|     205 | 1909 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     205 | 1910 | `		if( rcCb != PH7_OK ){` |
|       5 | 1911 | `			return rcCb;` |
|       - | 1912 | `		}` |
|       - | 1913 | `	}` |
|     201 | 1914 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     201 | 1915 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1916 | `	/* Initialize the arguments container */` |
|     201 | 1917 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1918 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 1919 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 1920 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 1921 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 1922 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 1923 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     201 | 1924 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     201 | 1925 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     589 | 1926 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1927 | `		/* Extract node value */` |
|     390 | 1928 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     390 | 1929 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      29 | 1930 | `				if( aNames == 0 ){` |
|       - | 1931 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 1932 | `					 * not-yet-seen slot defaults to positional. */` |
|      19 | 1933 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      19 | 1934 | `					if( aNames == 0 ){` |
|     ! 0 | 1935 | `						SySetRelease(&aArg);` |
|     ! 0 | 1936 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1937 | `						if( apNode ){` |
|     ! 0 | 1938 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 1939 | `						}` |
|     ! 0 | 1940 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 1941 | `					}` |
|      19 | 1942 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|       9 | 1943 | `				}` |
|      29 | 1944 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      14 | 1945 | `			}` |
|     390 | 1946 | `			if( apNode == 0 ){` |
|     230 | 1947 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     152 | 1948 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     154 | 1949 | `				if( apNode ){` |
|     154 | 1950 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|      76 | 1951 | `				}` |
|      76 | 1952 | `			}` |
|     390 | 1953 | `			if( apNode ){` |
|     390 | 1954 | `				apNode[nSlot] = pEntry;` |
|     194 | 1955 | `			}` |
|     390 | 1956 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     390 | 1957 | `			nSlot++;` |
|     194 | 1958 | `		}` |
|       - | 1959 | `		/* Point to the next entry */` |
|     390 | 1960 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     196 | 1961 | `	}` |
|       - | 1962 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 1963 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 1964 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 1965 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     201 | 1966 | `	if( apNode ){` |
|     154 | 1967 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     154 | 1968 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     154 | 1969 | `		apNode = 0;` |
|      76 | 1970 | `	}` |
|       - | 1971 | `	/* Try to invoke the callback. Like call_user_func, this is a php FORWARD: a` |
|       - | 1972 | `	 * dropped answer here is a dropped answer for the callback. */` |
|     201 | 1973 | `	pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|     201 | 1974 | `	if( aNames ){` |
|       - | 1975 | `		VmCallArgMap sMap;` |
|      19 | 1976 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      19 | 1977 | `		sMap.bHasNamed = 1;` |
|      19 | 1978 | `		sMap.bIsNamespaced = 0;` |
|       - | 1979 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 1980 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      19 | 1981 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      19 | 1982 | `		sMap.nTotal = nSlot;` |
|      19 | 1983 | `		sMap.aNames = aNames;` |
|      28 | 1984 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      18 | 1985 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      19 | 1986 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      10 | 1987 | `	}else{` |
|       - | 1988 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 1989 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 1990 | `		VmCallArgMap sFwd;` |
|     183 | 1991 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     183 | 1992 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     273 | 1993 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     180 | 1994 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 1995 | `	}` |
|     201 | 1996 | `	pCtx->pVm->bDiscardCallback = 0;   /* see the call_user_func sibling */` |
|     201 | 1997 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1998 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     113 | 1999 | `		PH7_MemObjRelease(&sResult);` |
|     113 | 2000 | `		SySetRelease(&aArg);` |
|     113 | 2001 | `		return PH7_EXCEPTION;` |
|       - | 2002 | `	}` |
|      89 | 2003 | `	if( rc != SXRET_OK ){` |
|       - | 2004 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2005 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2006 | `	}else{` |
|       - | 2007 | `		/* Callback result */` |
|      89 | 2008 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2009 | `	}` |
|       - | 2010 | `	/* Cleanup the mess left behind */` |
|      89 | 2011 | `	PH7_MemObjRelease(&sResult);` |
|      89 | 2012 | `	SySetRelease(&aArg);` |
|      89 | 2013 | `	return PH7_OK;` |
|     104 | 2014 | `}` |
|       - | 2015 |  |
