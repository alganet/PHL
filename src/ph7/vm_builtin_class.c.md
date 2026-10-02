# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1092/1220 lines (89.51%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|   13491 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|   13496 |   11 | `	if( nArg < 1 ){` |
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
|   13496 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   13496 |   25 | `		if( pClass ){` |
|   13496 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|   13496 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    6725 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|   13496 |   34 | `	return PH7_OK;` |
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
|      82 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      86 |   52 | `	if( nArg < 1 ){` |
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
|      84 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      84 |   66 | `		if( pClass ){` |
|      84 |   67 | `			if( pClass->pBase ){` |
|      78 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      78 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      41 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       7 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      44 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      86 |   80 | `	return PH7_OK;` |
|       4 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * string get_called_class(void)` |
|       - |   84 | ` *   Gets the name of the class the static method is called in.` |
|       - |   85 | ` * Parameters` |
|       - |   86 | ` *  None.` |
|       - |   87 | ` * Return` |
|       - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|       - |   89 | ` */` |
|       6 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   91 | `{` |
|       - |   92 | `	ph7_class *pClass;` |
|       - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|       7 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       7 |   95 | `	if( pClass ){` |
|       - |   96 | `		SyString *pName;` |
|       - |   97 | `		/* Point to the class name */` |
|       7 |   98 | `		pName = &pClass->sName;` |
|       7 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       4 |  100 | `	}else{` |
|     ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  102 | `		SXUNUSED(apArg);` |
|       - |  103 | `		/* Not inside class,return FALSE */` |
|     ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|       - |  105 | `	}` |
|       7 |  106 | `	return PH7_OK;` |
|       1 |  107 | `}` |
|       - |  108 | `/*` |
|       - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|       - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|       - |  111 | ` * string which hold the class name.` |
|       - |  112 | ` */` |
|  221192 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  221197 |  115 | `	ph7_class *pClass = 0;` |
|  221197 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  214122 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  114113 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    7076 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    7076 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    7074 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    3526 |  135 | `		}` |
|    3527 |  136 | `	}` |
|  221197 |  137 | `	return pClass;` |
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
|     122 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  151 | `{` |
|     126 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|     126 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|     122 |  155 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|     102 |  156 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
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
|     124 |  170 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     124 |  171 | `		if( pClass ){` |
|       - |  172 | `			const char *zName;` |
|       - |  173 | `			int nLen;` |
|       - |  174 | `			/* Extract attribute name */` |
|     124 |  175 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     124 |  176 | `			if( nLen > 0 ){` |
|       - |  177 | `				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this` |
|       - |  178 | ``				 * name is not a property (`property_exists('C','someMethod')` is false),`` |
|       - |  179 | `				 * and neither is a class CONSTANT. PHL searched the method table too and` |
|       - |  180 | `				 * answered true for both. */` |
|     124 |  181 | `				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);` |
|     124 |  182 | `				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;` |
|     120 |  183 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND)` |
|      36 |  184 | `				 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - |  185 | `					/* An ON-DEMAND property is on the objects that took it and on no` |
|       - |  186 | `					 * other: php declares none of them, so the class table cannot be` |
|       - |  187 | `					 * what answers here. */` |
|       5 |  188 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       4 |  189 | `					if( pThis == 0` |
|       5 |  190 | `					 \|\| SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) == 0 ){` |
|       3 |  191 | `						pAttr = 0;` |
|       2 |  192 | `					}` |
|     122 |  193 | `				}else if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) ){` |
|     ! 0 |  194 | `					pAttr = 0;   /* asked about the CLASS: php declares no such name */` |
|     ! 0 |  195 | `				}` |
|     124 |  196 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN) != 0 ){` |
|       - |  197 | `					/* A native class's ENGINE SLOT is php's own C struct, not a` |
|       - |  198 | ``					 * declared property: `property_exists($doc, '__res')` is false`` |
|       - |  199 | `					 * under php because no such name is in its properties_info.` |
|       - |  200 | `					 * They are already hidden from Reflection, var_dump and the` |
|       - |  201 | `					 * (array) cast; this was the one door that still showed them. */` |
|       3 |  202 | `					pAttr = 0;` |
|       1 |  203 | `				}` |
|     124 |  204 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - |  205 | `					/* A base's PRIVATE property is invisible to the child it was asked` |
|       - |  206 | ``					 * about, php's `property_info->ce == ce` rule: PHL copies one down`` |
|       - |  207 | `					 * onto every child (its own methods read it through $this), so the` |
|       - |  208 | `					 * table alone said true where php says false. Static or instance,` |
|       - |  209 | `					 * the rule is the same; protected and public are inherited outright.` |
|       - |  210 | `					 * A trait's property belongs to the class that COMPOSED it. */` |
|      56 |  211 | `					res = 1;` |
|      52 |  212 | `					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      33 |  213 | `					 && pAttr->pDeclClass != 0` |
|      18 |  214 | `					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){` |
|       5 |  215 | `						res = 0;` |
|       2 |  216 | `					}` |
|      26 |  217 | `				}` |
|       - |  218 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  219 | `				 * attribute table, not the class's — php reports those too` |
|       - |  220 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|     124 |  221 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|      44 |  222 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      44 |  223 | `					SyHashEntry *pObjE = pThis` |
|      42 |  224 | `						? SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) : 0;` |
|      44 |  225 | `					VmClassAttr *pObjAttr = pObjE ? (VmClassAttr *)pObjE->pUserData : 0;` |
|       - |  226 | `					/* Only a genuinely DYNAMIC one: every instance carries a slot for every` |
|       - |  227 | `					 * DECLARED member too, so an unqualified instance lookup answered true` |
|       - |  228 | `					 * for the base private the class-table rule above had just refused. */` |
|      42 |  229 | `					if( pObjAttr && pObjAttr->pAttr` |
|       6 |  230 | `					 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|       3 |  231 | `						res = 1;` |
|       1 |  232 | `					}` |
|      44 |  233 | `					if( res == 0 && pThis ){` |
|       - |  234 | `						/* php asks the class's has_property handler here too, which` |
|       - |  235 | `						 * is what reports a PDORow's COLUMNS -- a name no table` |
|       - |  236 | `						 * carries and every read answers. Its verdict is the value's` |
|       - |  237 | `						 * (a column holding SQL NULL reports false), which is what` |
|       - |  238 | `						 * php's own handler answers to this question. */` |
|       - |  239 | `						PH7_NativePropCtx sNat;` |
|       - |  240 | `						SyString sNatName;` |
|       - |  241 | `						ph7_value sNatVal;` |
|      42 |  242 | `						SyStringInitFromBuf(&sNatName,zName,(sxu32)nLen);` |
|      42 |  243 | `						PH7_MemObjInit(pCtx->pVm,&sNatVal);` |
|      40 |  244 | `						if( PH7_ClassNativePropAsk(pThis,&sNat,PH7_NATIVE_PROP_EXISTS,` |
|       - |  245 | `								&sNatName,&sNatVal)` |
|      36 |  246 | `						 && sNat.zThrowClass == 0 && ph7_value_to_bool(&sNatVal) ){` |
|      14 |  247 | `							res = 1;` |
|       6 |  248 | `						}` |
|      42 |  249 | `						PH7_MemObjRelease(&sNatVal);` |
|      20 |  250 | `					}` |
|      21 |  251 | `				}` |
|      60 |  252 | `			}` |
|      60 |  253 | `		}` |
|      60 |  254 | `	}` |
|     124 |  255 | `	ph7_result_bool(pCtx,res);` |
|     124 |  256 | `	return PH7_OK;` |
|      65 |  257 | `}` |
|       - |  258 | `/*` |
|       - |  259 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  260 | ` *   Checks if the given method is a class member.` |
|       - |  261 | ` * Parameters` |
|       - |  262 | ` *  class` |
|       - |  263 | ` *   The class name or an object of the class to test for` |
|       - |  264 | ` * property` |
|       - |  265 | ` *  The name of the method` |
|       - |  266 | ` * Return` |
|       - |  267 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  268 | ` */` |
|      96 |  269 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  270 | `{` |
|      99 |  271 | `	int res = 0; /* Assume method does not exists */` |
|      99 |  272 | `	if( nArg > 1 ){` |
|       - |  273 | `		ph7_class *pClass;` |
|      96 |  274 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      57 |  275 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  276 | `			/* An incomplete OBJECT consults its method resolution, which the` |
|       - |  277 | `			 * carrier refuses with php's catchable call Error (probe-verified;` |
|       - |  278 | `			 * the class asked about by NAME answers false the ordinary way). */` |
|       - |  279 | `			SyBlob sIncMsg;` |
|       - |  280 | `			sxi32 rcInc;` |
|       3 |  281 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  282 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  283 | `				"call a method",&sIncMsg);` |
|       4 |  284 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  285 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  286 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  287 | `			return rcInc;` |
|       - |  288 | `		}` |
|      97 |  289 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      97 |  290 | `		if( pClass ){` |
|       - |  291 | `			const char *zName;` |
|       - |  292 | `			int nLen;` |
|       - |  293 | `			/* Extract method name */` |
|      92 |  294 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      92 |  295 | `			if( nLen > 0 ){` |
|       - |  296 | `				/* Perform the lookup in the method table */` |
|      92 |  297 | `				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);` |
|      92 |  298 | `				if( pEntry ){` |
|       - |  299 | ``					/* ...and apply php's one visibility rule here (`func->common.scope`` |
|       - |  300 | ``					 * == ce`): a PRIVATE method is only a method of the class that`` |
|       - |  301 | `					 * declares it. PHL copies a base's private down so an inherited` |
|       - |  302 | `					 * public method can still dispatch it, which made` |
|       - |  303 | ``					 * `method_exists('Child','basePrivate')` answer true where php`` |
|       - |  304 | `					 * answers false — the same shape property_exists() had. Nothing` |
|       - |  305 | `					 * about the CALLING scope enters into it: php answers false for the` |
|       - |  306 | `					 * child from inside the BASE too. A trait's method belongs to the` |
|       - |  307 | `					 * class that COMPOSED it. */` |
|      70 |  308 | `					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      70 |  309 | `					res = 1;` |
|      68 |  310 | `					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      46 |  311 | `					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){` |
|      15 |  312 | `						res = 0;` |
|       7 |  313 | `					}` |
|      34 |  314 | `				}` |
|      45 |  315 | `			}` |
|      45 |  316 | `		}` |
|      47 |  317 | `	}` |
|      97 |  318 | `	ph7_result_bool(pCtx,res);` |
|      97 |  319 | `	return PH7_OK;` |
|      51 |  320 | `}` |
|       - |  321 | `/*` |
|       - |  322 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  323 | ` *   Checks if the class has been defined.` |
|       - |  324 | ` * Parameters` |
|       - |  325 | ` *  class_name` |
|       - |  326 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  327 | ` *   unlinke the standard PHP engine.` |
|       - |  328 | ` *  autoload` |
|       - |  329 | ` *   Whether or not to call __autoload by default.` |
|       - |  330 | ` * Return` |
|       - |  331 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  332 | ` */` |
|     264 |  333 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  334 | `{` |
|     269 |  335 | `	int res = 0; /* Assume class does not exist */` |
|     269 |  336 | `	if( nArg > 0 ){` |
|     269 |  337 | `		SyHashEntry *pEntry = 0;` |
|       - |  338 | `		const char *zName;` |
|       - |  339 | `		int nLen;` |
|     269 |  340 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  341 | `		sxu32 nName;` |
|       - |  342 | `		/* Extract given name */` |
|     269 |  343 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     269 |  344 | `		if( nArg >= 2 ){` |
|     135 |  345 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|      66 |  346 | `		}` |
|       - |  347 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  348 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     269 |  349 | `		nName = (sxu32)nLen;` |
|     269 |  350 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     269 |  351 | `		if( nName > 0 ){` |
|       - |  352 | `			/* Perform a hash lookup first */` |
|     265 |  353 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     130 |  354 | `		}` |
|       - |  355 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|     269 |  356 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  357 | `			/* Try autoload, then re-check */` |
|      29 |  358 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      29 |  359 | `			if( pClass ){` |
|       9 |  360 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  361 | `			}` |
|      12 |  362 | `		}` |
|     269 |  363 | `		if( pEntry ){` |
|       - |  364 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  365 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|     235 |  366 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     247 |  367 | `			while( pClass ){` |
|     235 |  368 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|     223 |  369 | `					res = 1;` |
|     223 |  370 | `					break;` |
|       - |  371 | `				}` |
|      13 |  372 | `				pClass = pClass->pNextName;` |
|       1 |  373 | `			}` |
|     115 |  374 | `		}` |
|     132 |  375 | `	}` |
|     269 |  376 | `	ph7_result_bool(pCtx,res);` |
|     269 |  377 | `	return PH7_OK;` |
|       5 |  378 | `}` |
|       - |  379 | `/*` |
|       - |  380 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  381 | ` *   Checks if the interface has been defined.` |
|       - |  382 | ` * Parameters` |
|       - |  383 | ` *  class_name` |
|       - |  384 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  385 | ` *   unlinke the standard PHP engine.` |
|       - |  386 | ` *  autoload` |
|       - |  387 | ` *   Whether or not to call __autoload by default.` |
|       - |  388 | ` * Return` |
|       - |  389 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  390 | ` */` |
|      62 |  391 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  392 | `{` |
|      67 |  393 | `	int res = 0; /* Assume interface does not exist */` |
|      67 |  394 | `	if( nArg > 0 ){` |
|      67 |  395 | `		SyHashEntry *pEntry = 0;` |
|       - |  396 | `		const char *zName;` |
|       - |  397 | `		int nLen;` |
|      67 |  398 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  399 | `		sxu32 nName;` |
|       - |  400 | `		/* Extract given name */` |
|      67 |  401 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      67 |  402 | `		if( nArg >= 2 ){` |
|      18 |  403 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       8 |  404 | `		}` |
|       - |  405 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  406 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      67 |  407 | `		nName = (sxu32)nLen;` |
|      67 |  408 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  409 | `		/* Perform a hash lookup */` |
|      67 |  410 | `		if( nName > 0 ){` |
|      64 |  411 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      30 |  412 | `		}` |
|       - |  413 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      67 |  414 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  415 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  416 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  417 | `			if( pClass ){` |
|     ! 0 |  418 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  419 | `			}` |
|       1 |  420 | `		}` |
|      67 |  421 | `		if( pEntry ){` |
|      58 |  422 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      60 |  423 | `			while( pClass ){` |
|      58 |  424 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  425 | `					/* interface is available */` |
|      56 |  426 | `					res = 1;` |
|      56 |  427 | `					break;` |
|       - |  428 | `				}` |
|       - |  429 | `				/* Next with the same name */` |
|       3 |  430 | `				pClass = pClass->pNextName;` |
|       1 |  431 | `			}` |
|      27 |  432 | `		}` |
|      31 |  433 | `	}` |
|      67 |  434 | `	ph7_result_bool(pCtx,res);` |
|      67 |  435 | `	return PH7_OK;` |
|       5 |  436 | `}` |
|       - |  437 | `/*` |
|       - |  438 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  439 | ` *   Checks if the trait has been defined.` |
|       - |  440 | ` * Parameters` |
|       - |  441 | ` *  trait` |
|       - |  442 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  443 | ` *  autoload` |
|       - |  444 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  445 | ` * Return` |
|       - |  446 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  447 | ` */` |
|      20 |  448 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  449 | `{` |
|      23 |  450 | `	int res = 0; /* Assume trait does not exist */` |
|      23 |  451 | `	if( nArg > 0 ){` |
|      23 |  452 | `		SyHashEntry *pEntry = 0;` |
|       - |  453 | `		const char *zName;` |
|       - |  454 | `		int nLen;` |
|      23 |  455 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  456 | `		sxu32 nName;` |
|       - |  457 | `		/* Extract given name */` |
|      23 |  458 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      23 |  459 | `		if( nArg >= 2 ){` |
|       5 |  460 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  461 | `		}` |
|       - |  462 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  463 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      23 |  464 | `		nName = (sxu32)nLen;` |
|      23 |  465 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  466 | `		/* Perform a hash lookup */` |
|      23 |  467 | `		if( nName > 0 ){` |
|      20 |  468 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       9 |  469 | `		}` |
|       - |  470 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      23 |  471 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  472 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       5 |  473 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       5 |  474 | `			if( pClass ){` |
|     ! 0 |  475 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  476 | `			}` |
|       2 |  477 | `		}` |
|      23 |  478 | `		if( pEntry ){` |
|      12 |  479 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  480 | `			while( pClass ){` |
|      12 |  481 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  482 | `					/* trait is available */` |
|       6 |  483 | `					res = 1;` |
|       6 |  484 | `					break;` |
|       - |  485 | `				}` |
|       - |  486 | `				/* Next with the same name */` |
|       7 |  487 | `				pClass = pClass->pNextName;` |
|       1 |  488 | `			}` |
|       5 |  489 | `		}` |
|      10 |  490 | `	}` |
|      23 |  491 | `	ph7_result_bool(pCtx,res);` |
|      23 |  492 | `	return PH7_OK;` |
|       3 |  493 | `}` |
|       - |  494 | `/*` |
|       - |  495 | ` * bool class_alias(string $class, string $alias, bool $autoload = true)` |
|       - |  496 | ` *   Creates an alias for a class.` |
|       - |  497 | ` * Parameters` |
|       - |  498 | ` *  class` |
|       - |  499 | ` *    The original class (interface, trait or enum — php aliases all four).` |
|       - |  500 | ` *  alias` |
|       - |  501 | ` *   The alias name for the class.` |
|       - |  502 | ` *  autoload` |
|       - |  503 | ` *   Whether to autoload $class when it is not declared yet. php's default is TRUE,` |
|       - |  504 | ` *   and this is not a detail: the ordinary use of this builtin is a compatibility` |
|       - |  505 | ` *   shim written against a class the AUTOLOADER owns —` |
|       - |  506 | ``  *   `class_alias('\\PHPUnit\\Framework\\TestCase','\\PHPUnit_Framework_TestCase')` `` |
|       - |  507 | ` *   in monolog's test bootstrap is exactly that, and with no autoload it silently` |
|       - |  508 | ` *   answered false, so every test class extending the alias was undefined.` |
|       - |  509 | ` * Return` |
|       - |  510 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  511 | ` */` |
|      22 |  512 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  513 | `{` |
|       - |  514 | `	const char *zOld,*zNew,*zOldRaw,*zNewRaw;` |
|       - |  515 | `	int nOldLen,nNewLen;` |
|       - |  516 | `	sxu32 nOld,nNew;` |
|      25 |  517 | `	int iAutoload = 1;` |
|       - |  518 | `	SyHashEntry *pEntry;` |
|       - |  519 | `	ph7_class *pClass;` |
|       - |  520 | `	char *zDup;` |
|       - |  521 | `	sxi32 rc;` |
|      25 |  522 | `	if( nArg < 2 ){` |
|       - |  523 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  524 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  525 | `		return PH7_OK;` |
|       - |  526 | `	}` |
|       - |  527 | `	/* Extract old class name */` |
|      25 |  528 | `	zOld = zOldRaw = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  529 | `	/* Extract alias name */` |
|      25 |  530 | `	zNew = zNewRaw = ph7_value_to_string(apArg[1],&nNewLen);` |
|      25 |  531 | `	if( nArg >= 3 ){` |
|       3 |  532 | `		iAutoload = ph7_value_to_bool(apArg[2]);` |
|       1 |  533 | `	}` |
|       - |  534 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  535 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  536 | `	 * on the plain name then matches. */` |
|      25 |  537 | `	nOld = (sxu32)nOldLen;` |
|      25 |  538 | `	nNew = (sxu32)nNewLen;` |
|      25 |  539 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|      25 |  540 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       - |  541 | `	/* Perform a hash lookup */` |
|      25 |  542 | `	pEntry = nOld > 0 ? SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld) : 0;` |
|      25 |  543 | `	if( pEntry == 0 && iAutoload && nOld > 0 ){` |
|       - |  544 | `		/* Not declared yet: ask the autoloader, exactly as class_exists() does.` |
|       - |  545 | `		 * iLoadable is FALSE so an interface or a trait comes back too — php` |
|       - |  546 | `		 * aliases those as readily as a class. */` |
|       7 |  547 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zOld,nOld,FALSE) ){` |
|       5 |  548 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|       2 |  549 | `		}` |
|       3 |  550 | `	}` |
|      25 |  551 | `	if( pEntry ==  0 ){` |
|       - |  552 | `		/* php names the class it could not find and answers false. The sentence` |
|       - |  553 | ``		 * carries no `class_alias(): ` prefix, so it is raised on the VM rather`` |
|       - |  554 | `		 * than through the context. */` |
|       5 |  555 | `		VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,"Class \"%.*s\" not found",nOldLen,zOldRaw);` |
|       5 |  556 | `		ph7_result_bool(pCtx,0);` |
|       5 |  557 | `		return PH7_OK;` |
|       - |  558 | `	}` |
|       - |  559 | `	/* Point to the class */` |
|      21 |  560 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|      21 |  561 | `	if( nNew > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zNew,nNew) != 0 ){` |
|       - |  562 | `		/* The alias name is already a declared class/interface/trait/enum. php` |
|       - |  563 | `		 * refuses and — this is php's own quirk, not a slip — names the file and` |
|       - |  564 | `		 * line of the class being ALIASED, not of the name already taken. */` |
|       3 |  565 | `		if( pClass->sFile.nByte > 0 ){` |
|       4 |  566 | `			VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,` |
|       - |  567 | `				"Cannot redeclare class %.*s (previously declared in %.*s:%u)",` |
|       1 |  568 | `				nNewLen,zNewRaw,` |
|       2 |  569 | `				(int)pClass->sFile.nByte,pClass->sFile.zString,pClass->nLine);` |
|       2 |  570 | `		}else{` |
|       - |  571 | `			/* No defining file: php's own wording for a class it did not compile. */` |
|     ! 0 |  572 | `			VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,` |
|     ! 0 |  573 | `				"Cannot redeclare class %.*s",nNewLen,zNewRaw);` |
|       - |  574 | `		}` |
|       3 |  575 | `		ph7_result_bool(pCtx,0);` |
|       3 |  576 | `		return PH7_OK;` |
|       - |  577 | `	}` |
|      19 |  578 | `	if( nNew < 1 ){` |
|       - |  579 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  580 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  581 | `		return PH7_OK;` |
|       - |  582 | `	}` |
|       - |  583 | `	/* Duplicate alias name */` |
|      19 |  584 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|      19 |  585 | `	if( zDup == 0 ){` |
|       - |  586 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  587 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  588 | `		return PH7_OK;` |
|       - |  589 | `	}` |
|       - |  590 | `	/* Create the alias */` |
|      19 |  591 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|      19 |  592 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  593 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  594 | `	}` |
|      19 |  595 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      19 |  596 | `	return PH7_OK;` |
|      14 |  597 | `}` |
|       - |  598 | `/*` |
|       - |  599 | ` * The three KINDS hClass holds. php keeps classes, interfaces, traits and enums in one` |
|       - |  600 | ` * table too and each of its three list builtins filters that table down to its own kind:` |
|       - |  601 | `` * an ENUM is a class (`get_declared_classes()` reports it), an interface and a trait are`` |
|       - |  602 | ` * not.` |
|       - |  603 | ` */` |
|       - |  604 | `#define VM_DECLARED_CLASS      0` |
|       - |  605 | `#define VM_DECLARED_INTERFACE  1` |
|       - |  606 | `#define VM_DECLARED_TRAIT      2` |
|       - |  607 |  |
|    4428 |  608 | `static int VmDeclaredEntryKind(ph7_class *pClass)` |
|       2 |  609 | `{` |
|    4430 |  610 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     416 |  611 | `		return VM_DECLARED_INTERFACE;` |
|       - |  612 | `	}` |
|    4016 |  613 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      40 |  614 | `		return VM_DECLARED_TRAIT;` |
|       - |  615 | `	}` |
|    3978 |  616 | `	return VM_DECLARED_CLASS;` |
|    2216 |  617 | `}` |
|       - |  618 | `struct VmDeclaredList {` |
|       - |  619 | `	int iKind;           /* Which VM_DECLARED_* kind this list wants */` |
|       - |  620 | `	ph7_value *pArray;   /* The array being built */` |
|       - |  621 | `	ph7_value *pName;    /* Scratch name */` |
|       - |  622 | `};` |
|       - |  623 | `/*` |
|       - |  624 | ` * One row of a get_declared_*() answer.` |
|       - |  625 | ` *` |
|       - |  626 | ` * The NAME reported is the table KEY, not the class struct's own name — a distinction` |
|       - |  627 | `` * only `class_alias()` makes visible, since it puts a second key over the same class.`` |
|       - |  628 | ` * php reports the class's declared spelling for the key that IS its name and the ALIAS` |
|       - |  629 | `` * for the other, so `class_alias('C1','C1Alias')` answers both `C1` and `c1alias`,`` |
|       - |  630 | ` * lower-cased because that is the spelling php's own alias key is stored under. PHL` |
|       - |  631 | ` * keeps the declared spelling in every key, so the fold is applied here.` |
|       - |  632 | ` */` |
|    4428 |  633 | `static sxi32 VmDeclaredNameStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 |  634 | `{` |
|    4430 |  635 | `	struct VmDeclaredList *pList = (struct VmDeclaredList *)pUserData;` |
|    4430 |  636 | `	ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    4430 |  637 | `	SyString *pDecl = &pClass->sName;` |
|    4430 |  638 | `	if( VmDeclaredEntryKind(pClass) != pList->iKind ){` |
|    2510 |  639 | `		return SXRET_OK;` |
|       - |  640 | `	}` |
|    1920 |  641 | `	if( pEntry->nKeyLen == pDecl->nByte` |
|    1917 |  642 | `		&& SyStrnmicmp((const char *)pEntry->pKey,pDecl->zString,pEntry->nKeyLen) == 0 ){` |
|       - |  643 | `		/* The key this class was DECLARED under */` |
|    1912 |  644 | `		ph7_value_string(pList->pName,pDecl->zString,(int)pDecl->nByte);` |
|     957 |  645 | `	}else{` |
|       - |  646 | `		/* A class_alias() key: php reports it folded */` |
|      12 |  647 | `		const char *zKey = (const char *)pEntry->pKey;` |
|       - |  648 | `		sxu32 n;` |
|     174 |  649 | `		for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|     164 |  650 | `			char c = (char)SyToLower(zKey[n]);` |
|     164 |  651 | `			ph7_value_string(pList->pName,&c,1);` |
|      83 |  652 | `		}` |
|       - |  653 | `	}` |
|    1922 |  654 | `	ph7_array_add_elem(pList->pArray,0/*Automatic index assign*/,pList->pName); /* Will make it's own copy */` |
|    1922 |  655 | `	ph7_value_reset_string_cursor(pList->pName);` |
|    1922 |  656 | `	return SXRET_OK;` |
|    2216 |  657 | `}` |
|       - |  658 |  |
|       - |  659 | `/*` |
|       - |  660 | ` * Build php's get_declared_classes()/get_declared_interfaces()/get_declared_traits()` |
|       - |  661 | ` * answer for one kind.` |
|       - |  662 | ` */` |
|      14 |  663 | `static int VmDeclaredNameList(ph7_context *pCtx,int iKind)` |
|       2 |  664 | `{` |
|       - |  665 | `	struct VmDeclaredList sList;` |
|       - |  666 | `	/* Create a new array first */` |
|      16 |  667 | `	sList.iKind = iKind;` |
|      16 |  668 | `	sList.pArray = ph7_context_new_array(pCtx);` |
|      16 |  669 | `	sList.pName = ph7_context_new_scalar(pCtx);` |
|      16 |  670 | `	if( sList.pArray == 0 \|\| sList.pName == 0 ){` |
|       - |  671 | `		/* Out of memory,return NULL */` |
|     ! 0 |  672 | `		ph7_result_null(pCtx);` |
|     ! 0 |  673 | `		return PH7_OK;` |
|       - |  674 | `	}` |
|       - |  675 | `	/* hClass is head-pushed, so its forward order is reverse-insertion; php reports` |
|       - |  676 | `	 * these lists in DECLARATION order (its own class table is append-ordered), which` |
|       - |  677 | `	 * is what the backward walk yields. */` |
|      16 |  678 | `	SyHashForEachReverse(&pCtx->pVm->hClass,VmDeclaredNameStep,(void *)&sList);` |
|       - |  679 | `	/* Return the created array */` |
|      16 |  680 | `	ph7_result_value(pCtx,sList.pArray);` |
|      16 |  681 | `	return PH7_OK;` |
|       9 |  682 | `}` |
|       - |  683 | `/*` |
|       - |  684 | ` * array get_declared_classes(void)` |
|       - |  685 | ` *   Returns an array with the name of the defined classes` |
|       - |  686 | ` * Parameters` |
|       - |  687 | ` *  None` |
|       - |  688 | ` * Return` |
|       - |  689 | ` *   Returns an array of the names of the declared classes` |
|       - |  690 | ` *   in the current script.` |
|       - |  691 | ` * Note:` |
|       - |  692 | ` *   NULL is returned on failure.` |
|       - |  693 | ` */` |
|       6 |  694 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  695 | `{` |
|       3 |  696 | `	SXUNUSED(nArg); /* cc warning */` |
|       3 |  697 | `	SXUNUSED(apArg);` |
|       8 |  698 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_CLASS);` |
|       2 |  699 | `}` |
|       - |  700 | `/*` |
|       - |  701 | ` * array get_declared_interfaces(void)` |
|       - |  702 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  703 | ` * Parameters` |
|       - |  704 | ` *  None` |
|       - |  705 | ` * Return` |
|       - |  706 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  707 | ` *   in the current script.` |
|       - |  708 | ` * Note:` |
|       - |  709 | ` *   NULL is returned on failure.` |
|       - |  710 | ` */` |
|       4 |  711 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  712 | `{` |
|       2 |  713 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  714 | `	SXUNUSED(apArg);` |
|       6 |  715 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_INTERFACE);` |
|       2 |  716 | `}` |
|       - |  717 | `/*` |
|       - |  718 | ` * array get_declared_traits(void)` |
|       - |  719 | ` *   Returns an array with the name of the defined traits.` |
|       - |  720 | ` */` |
|       4 |  721 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  722 | `{` |
|       2 |  723 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  724 | `	SXUNUSED(apArg);` |
|       6 |  725 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_TRAIT);` |
|       2 |  726 | `}` |
|       - |  727 | `/*` |
|       - |  728 | ` * Does this method-table entry answer to the method's OWN name (rather than to an` |
|       - |  729 | ` * adaptation alias made from it)? Method names fold case, so the comparison does too.` |
|       - |  730 | ` */` |
|     122 |  731 | `static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  732 | `{` |
|     183 |  733 | `	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte` |
|     122 |  734 | `		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;` |
|       2 |  735 | `}` |
|       - |  736 | `/*` |
|       - |  737 | ` * Which inheritance LEVEL does this method-table entry belong to — the class php would` |
|       - |  738 | ` * have added it under? A method declared in a class body is its own; a trait's is the` |
|       - |  739 | ` * class that COMPOSED it, which is two different questions depending on the entry. An` |
|       - |  740 | ` * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still` |
|       - |  741 | `` * holding this key over this very struct is the one whose `use` block wrote it. A trait`` |
|       - |  742 | ` * method under its own name is the same struct in every class that uses the trait, and` |
|       - |  743 | ` * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait` |
|       - |  744 | ` * composes its own copy, and the parent's inherited entry never replaces it.` |
|       - |  745 | ` */` |
|    2094 |  746 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  747 | `{` |
|    2096 |  748 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|    2096 |  749 | `	ph7_class *pWalk,*pHigh = 0;` |
|    2096 |  750 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2040 |  751 | `		return pDecl;` |
|       - |  752 | `	}` |
|      58 |  753 | `	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){` |
|      62 |  754 | `		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 |  755 | `			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);` |
|      38 |  756 | `			if( pE && pE->pUserData == (void *)pMeth ){` |
|      34 |  757 | `				pHigh = pWalk;` |
|      16 |  758 | `			}` |
|      20 |  759 | `		}` |
|      26 |  760 | `		if( pHigh ){` |
|      26 |  761 | `			return pHigh;` |
|       - |  762 | `		}` |
|     ! 0 |  763 | `	}` |
|      34 |  764 | `	return PH7_VmComposingClass(pClass,pDecl);` |
|    1049 |  765 | `}` |
|       - |  766 | `/*` |
|       - |  767 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  768 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  769 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  770 | ` */` |
|    1142 |  771 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       2 |  772 | `{` |
|    1144 |  773 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    1144 |  774 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    1144 |  775 | `	ph7_value_reset_string_cursor(pName);` |
|    1144 |  776 | `}` |
|       - |  777 | `/*` |
|       - |  778 | ` * The three functions that answer what a class is RELATED to: the ancestors it` |
|       - |  779 | ` * extends, the interfaces it carries and the traits it composed. All three` |
|       - |  780 | ` * resolve the same argument and say the same two things when they cannot, so the` |
|       - |  781 | ` * lookup is written once here.` |
|       - |  782 | ` *` |
|       - |  783 | `` * php's ZPP for `$object_or_class` is Z_PARAM_OBJ_OR_STR, which screens without`` |
|       - |  784 | ` * DECLARING: ReflectionParameter reports no type for that parameter at all, and` |
|       - |  785 | ` * the refusal is still php's standard "must be of type object\|string" sentence.` |
|       - |  786 | ` * That is why the aBuiltinSig[] row leaves it bare and the screen is spelled here.` |
|       - |  787 | ` */` |
|     154 |  788 | `static ph7_class * VmClassRelationTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  789 | `{` |
|       - |  790 | `	const char *zName,*zLook;` |
|       - |  791 | `	int nLen,nShow,bAutoload;` |
|       - |  792 | `	sxu32 nLook;` |
|       - |  793 | `	SyHashEntry *pEntry;` |
|     154 |  794 | `	if( nArg < 1` |
|     154 |  795 | `	 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|     145 |  796 | `	 \|\| (apArg[0]->iFlags & MEMOBJ_NULL) != 0 ){` |
|       - |  797 | `		char zGiven[64];` |
|      49 |  798 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  799 | `			"%s(): Argument #1 ($object_or_class) must be of type object\|string, %s given",` |
|      12 |  800 | `			ph7_function_name(pCtx),` |
|      24 |  801 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|      25 |  802 | `		return 0;` |
|       - |  803 | `	}` |
|     133 |  804 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      20 |  805 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 |  806 | `		return pInst ? pInst->pClass : 0;` |
|       - |  807 | `	}` |
|     115 |  808 | `	bAutoload = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     115 |  809 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     115 |  810 | `	zLook = zName;` |
|     115 |  811 | `	nLook = (sxu32)nLen;` |
|       - |  812 | `	/* A leading '\' is the global-namespace anchor for the LOOKUP and part of the` |
|       - |  813 | `	 * name for the DIAGNOSTIC: php reports back what it was handed. */` |
|     115 |  814 | `	PH7_VmClassNameAnchor(&zLook,&nLook);` |
|     115 |  815 | `	pEntry = nLook > 0 ? SyHashGet(&pCtx->pVm->hClass,(const void *)zLook,nLook) : 0;` |
|     115 |  816 | `	if( pEntry == 0 && nLook > 0 && bAutoload ){` |
|       7 |  817 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zLook,nLook,FALSE) ){` |
|     ! 0 |  818 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zLook,nLook);` |
|     ! 0 |  819 | `		}` |
|       3 |  820 | `	}` |
|     115 |  821 | `	if( pEntry ){` |
|       - |  822 | `		/* Anything a class name can stand for answers, php's zend_lookup_class` |
|       - |  823 | `		 * included: an interface, a trait and an enum all have relations to` |
|       - |  824 | `		 * report, and PHL's class_exists() gate said FALSE for every one of them` |
|       - |  825 | ``		 * -- so `class_implements('Countable')` was false rather than a list. */`` |
|      97 |  826 | `		return (ph7_class *)pEntry->pUserData;` |
|       - |  827 | `	}` |
|       - |  828 | `	/* php has TWO sentences here and the difference is whether it was allowed to` |
|       - |  829 | `	 * look: the autoloading form says the name could not be LOADED, the other only` |
|       - |  830 | `	 * that it does not exist. PHL said neither -- all three answered a bare false,` |
|       - |  831 | `	 * so a typo in a class name was completely silent. The name is printed as php` |
|       - |  832 | `	 * prints it, which stops at the first NUL. */` |
|     103 |  833 | `	for( nShow = 0 ; nShow < nLen && zName[nShow] ; ++nShow ){}` |
|       - |  834 | ``	/* The context prints `class_uses(): ` in front of this itself. */`` |
|      28 |  835 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       9 |  836 | `		bAutoload ? "Class %.*s does not exist and could not be loaded"` |
|       - |  837 | `		          : "Class %.*s does not exist",` |
|       9 |  838 | `		nShow,zName);` |
|      19 |  839 | `	return 0;` |
|      80 |  840 | `}` |
|       - |  841 | ``/* `[$name => $name, ...]` over a list of classes, php's shape for all three. */`` |
|     112 |  842 | `static int VmClassRelationList(ph7_context *pCtx,ph7_class **apClass,sxu32 nClass)` |
|       3 |  843 | `{` |
|       - |  844 | `	ph7_value *pArray,*pName;` |
|       - |  845 | `	sxu32 n;` |
|     115 |  846 | `	pArray = ph7_context_new_array(pCtx);` |
|     115 |  847 | `	pName = ph7_context_new_scalar(pCtx);` |
|     115 |  848 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  849 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  850 | `	}` |
|     269 |  851 | `	for( n = 0 ; n < nClass ; ++n ){` |
|     156 |  852 | `		SyString *pStr = &apClass[n]->sName;` |
|       - |  853 | `		/* The name is its own key. A ph7_value key rather than the strkey door,` |
|       - |  854 | `		 * which wants a NUL-terminated C string a SyString does not promise. */` |
|     156 |  855 | `		ph7_value_string(pName,SyStringData(pStr),(int)SyStringLength(pStr));` |
|     156 |  856 | `		ph7_array_add_elem(pArray,pName,pName);` |
|     156 |  857 | `		ph7_value_reset_string_cursor(pName);` |
|      79 |  858 | `	}` |
|     115 |  859 | `	ph7_result_value(pCtx,pArray);` |
|     115 |  860 | `	return PH7_OK;` |
|      59 |  861 | `}` |
|       - |  862 | `/*` |
|       - |  863 | ` * array\|false class_parents($object_or_class, bool $autoload = true)` |
|       - |  864 | ` *  Every ancestor of the class, nearest first, keyed by its own name.` |
|       - |  865 | ` */` |
|      44 |  866 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  867 | `{` |
|      46 |  868 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  869 | `	SySet aOut;` |
|       - |  870 | `	int rc;` |
|      46 |  871 | `	if( pClass == 0 ){` |
|      15 |  872 | `		ph7_result_bool(pCtx,0);` |
|      15 |  873 | `		return PH7_OK;` |
|       - |  874 | `	}` |
|      32 |  875 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|       - |  876 | `	/* An INTERFACE keeps its first parent on pBase here where php keeps none at` |
|       - |  877 | `	 * all, so an interface answers the empty list the way php's does. */` |
|      32 |  878 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      28 |  879 | `		ph7_class *pUp = pClass->pBase;` |
|      28 |  880 | `		sxu32 nGuard = 0;` |
|      48 |  881 | `		while( pUp && nGuard++ < 1024 ){` |
|      22 |  882 | `			SySetPut(&aOut,(const void *)&pUp);` |
|      22 |  883 | `			pUp = pUp->pBase;` |
|       2 |  884 | `		}` |
|      13 |  885 | `	}` |
|      32 |  886 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      32 |  887 | `	SySetRelease(&aOut);` |
|      32 |  888 | `	return rc;` |
|      24 |  889 | `}` |
|       - |  890 | `/*` |
|       - |  891 | ` * array\|false class_implements($object_or_class, bool $autoload = true)` |
|       - |  892 | ` *  Every interface the class carries, in the order zend linked them (the one` |
|       - |  893 | ` *  Reflection publishes too -- see PH7_ReflectInterfacesOf).` |
|       - |  894 | ` */` |
|      68 |  895 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  896 | `{` |
|      70 |  897 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  898 | `	SySet aOut;` |
|       - |  899 | `	int rc;` |
|      70 |  900 | `	if( pClass == 0 ){` |
|      15 |  901 | `		ph7_result_bool(pCtx,0);` |
|      15 |  902 | `		return PH7_OK;` |
|       - |  903 | `	}` |
|      56 |  904 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|      56 |  905 | `	PH7_ReflectInterfacesOf(pCtx->pVm,pClass,&aOut);` |
|      56 |  906 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      56 |  907 | `	SySetRelease(&aOut);` |
|      56 |  908 | `	return rc;` |
|      36 |  909 | `}` |
|       - |  910 | `/*` |
|       - |  911 | ` * array\|false class_uses($object_or_class, bool $autoload = true)` |
|       - |  912 | `` *  The traits this class composed ITSELF, in the order its `use` clauses named`` |
|       - |  913 | ` *  them. Not the ancestors' -- php answers the empty list for a subclass of a` |
|       - |  914 | ` *  class that uses a trait -- and not the traits those traits flattened in.` |
|       - |  915 | ` */` |
|      42 |  916 | `PH7_PRIVATE int vm_builtin_class_uses(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  917 | `{` |
|      44 |  918 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|      44 |  919 | `	if( pClass == 0 ){` |
|      15 |  920 | `		ph7_result_bool(pCtx,0);` |
|      15 |  921 | `		return PH7_OK;` |
|       - |  922 | `	}` |
|      44 |  923 | `	return VmClassRelationList(pCtx,` |
|      28 |  924 | `		(ph7_class **)SySetBasePtr(&pClass->aTrait),SySetUsed(&pClass->aTrait));` |
|      23 |  925 | `}` |
|       - |  926 | `/*` |
|       - |  927 | ` * array get_class_methods(object\|string $object_or_class)` |
|       - |  928 | ` *   Returns an array with the names of the class methods the CALLING SCOPE can reach,` |
|       - |  929 | ` *   in php's order: each class's own body methods, then its trait composition, then the` |
|       - |  930 | ` *   same again for every ancestor.` |
|       - |  931 | ` * Parameters` |
|       - |  932 | ` *  object_or_class` |
|       - |  933 | ` *   The class name or a class instance. Anything that does not resolve to a class is a` |
|       - |  934 | ` *   TypeError naming the type given — this builtin never answers NULL.` |
|       - |  935 | ` */` |
|      90 |  936 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  937 | `{` |
|       - |  938 | `	ph7_value *pName,*pArray;` |
|       - |  939 | `	SyHashEntry *pEntry;` |
|       - |  940 | `	ph7_class *pClass;` |
|       - |  941 | `	/* Extract the target class first */` |
|      92 |  942 | `	pClass = 0;` |
|      92 |  943 | `	if( nArg > 0 ){` |
|      92 |  944 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      45 |  945 | `	}` |
|      92 |  946 | `	if( pClass == 0 ){` |
|       - |  947 | `		/* php screens the VALUE, not the type: anything that does not resolve to a` |
|       - |  948 | `		 * class — a name nothing declares, an int, an array, null — is ONE TypeError` |
|       - |  949 | `		 * naming the type given. PHL answered NULL for most of them (and the shared` |
|       - |  950 | ``		 * ZPP screen's `must be of type object\|string` for the rest), so a typo in a`` |
|       - |  951 | `		 * class name silently listed nothing. This is why get_class_methods() joins` |
|       - |  952 | `		 * get_class_vars() on the self-checked list in vm_arg_check.c. */` |
|       - |  953 | `		char zGiven[64];` |
|       9 |  954 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  955 | `			"get_class_methods(): Argument #1 ($object_or_class) must be an object "` |
|       - |  956 | `			"or a valid class name, %s given",` |
|       4 |  957 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - |  958 | `	}` |
|       - |  959 | `	/* Create a new array  */` |
|      88 |  960 | `	pArray = ph7_context_new_array(pCtx);` |
|      88 |  961 | `	pName = ph7_context_new_scalar(pCtx);` |
|      88 |  962 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  963 | `		/* Out of memory,return NULL */` |
|     ! 0 |  964 | `		ph7_result_null(pCtx);` |
|     ! 0 |  965 | `		return PH7_OK;` |
|       - |  966 | `	}` |
|       - |  967 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  968 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  969 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  970 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  971 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  972 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  973 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  974 | `	{` |
|       - |  975 | `		SySet aTmp;` |
|       - |  976 | `		SyHashEntry **apEntry;` |
|       - |  977 | `		ph7_class *pLevel;` |
|       - |  978 | `		sxu32 n;` |
|       - |  979 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  980 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  981 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      88 |  982 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      88 |  983 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    1284 |  984 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    1198 |  985 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       2 |  986 | `		}` |
|      88 |  987 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     214 |  988 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  989 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  990 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  991 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  992 | `			SySet aLvl;` |
|       - |  993 | `			SyHashEntry **apLvl;` |
|       - |  994 | `			sxu32 i,j;` |
|     128 |  995 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  996 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  997 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  998 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  999 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    2222 | 1000 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    2096 | 1001 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    2096 | 1002 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - | 1003 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - | 1004 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - | 1005 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - | 1006 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - | 1007 | `				 * methods before its own. */` |
|    2096 | 1008 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - | 1009 | `				/* A FABRICATED method is not in php's function table, and this walk is` |
|       - | 1010 | `` 				 * that table: `get_class_methods($closure)` does not name `__invoke` `` |
|       - | 1011 | ``				 * even though `method_exists($closure,'__invoke')` is true. */`` |
|    2096 | 1012 | `				if( pMethod->iFlags & PH7_CLASS_ATTR_FABRICATED ){` |
|       3 | 1013 | `					continue;` |
|       - | 1014 | `				}` |
|       - | 1015 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - | 1016 | `				 * protected within the hierarchy, private only from the class that` |
|       - | 1017 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - | 1018 | `				 * every private and protected name a class holds. Same decision` |
|       - | 1019 | `				 * get_class_vars() already makes for properties. */` |
|    2094 | 1020 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 1021 | `					SyString sMName;` |
|      98 | 1022 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - | 1023 | `						apEntry[nPick]->nKeyLen);` |
|       - | 1024 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - | 1025 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - | 1026 | `					 * wrote it, and judged against the class that composed the method. */` |
|     146 | 1027 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      48 | 1028 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      48 | 1029 | `							pMethod->iProtection,FALSE) ){` |
|      84 | 1030 | `						continue;` |
|       - | 1031 | `					}` |
|       7 | 1032 | `				}` |
|    2012 | 1033 | `				if( pDecl != pLevel ){` |
|       - | 1034 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - | 1035 | `					 * counts as the class's own level, like php. */` |
|       - | 1036 | `					ph7_class *pWalk;` |
|     869 | 1037 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     283 | 1038 | `						continue;` |
|       - | 1039 | `					}` |
|    1219 | 1040 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|    1219 | 1041 | `						if( pWalk == pDecl ){` |
|     587 | 1042 | `							break;` |
|       - | 1043 | `						}` |
|     317 | 1044 | `					}` |
|     587 | 1045 | `					if( pWalk != 0 ){` |
|     587 | 1046 | `						continue; /* in-chain: its own level emits it */` |
|       - | 1047 | `					}` |
|     ! 0 | 1048 | `				}` |
|    1144 | 1049 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     573 | 1050 | `			}` |
|     128 | 1051 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - | 1052 | `			/* Insertion sort by declaration line (stable) */` |
|    1152 | 1053 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    1026 | 1054 | `				SyHashEntry *pKey = apLvl[i];` |
|    1090 | 1055 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|    1111 | 1056 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 | 1057 | `					apLvl[j] = apLvl[j-1];` |
|      33 | 1058 | `				}` |
|    1026 | 1059 | `				apLvl[j] = pKey;` |
|     514 | 1060 | `			}` |
|       - | 1061 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - | 1062 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - | 1063 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - | 1064 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - | 1065 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - | 1066 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - | 1067 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - | 1068 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - | 1069 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - | 1070 | `			 * claim still goes out at the end. */` |
|    1270 | 1071 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|    1144 | 1072 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|    1144 | 1073 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|    1144 | 1074 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|    1102 | 1075 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|    1102 | 1076 | `					apLvl[i] = 0;` |
|     550 | 1077 | `				}` |
|     573 | 1078 | `			}` |
|       - | 1079 | `			{` |
|     128 | 1080 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|     128 | 1081 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - | 1082 | `				sxu32 k;` |
|     144 | 1083 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 | 1084 | `					ph7_class *pTrait = apTrait[k];` |
|       - | 1085 | `					SySet aTr;` |
|       - | 1086 | `					SyHashEntry **apTr;` |
|       - | 1087 | `					SyHashEntry *pTrE;` |
|       - | 1088 | `					sxu32 t;` |
|      18 | 1089 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 | 1090 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 | 1091 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 | 1092 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 | 1093 | `					}` |
|      18 | 1094 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - | 1095 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - | 1096 | `					 * declaration order. */` |
|      48 | 1097 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 | 1098 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - | 1099 | `						int bWantAlias;` |
|       - | 1100 | `						/* First pass emits the aliases made from this method, second the` |
|       - | 1101 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 | 1102 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 | 1103 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1104 | `								ph7_class_method *pM;` |
|     342 | 1105 | `								if( apLvl[i] == 0 ){` |
|     184 | 1106 | `									continue;` |
|       - | 1107 | `								}` |
|     160 | 1108 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 | 1109 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 | 1110 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 | 1111 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 | 1112 | `										pOrigin->sFunc.sName.zString,` |
|      98 | 1113 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 | 1114 | `									continue;` |
|       - | 1115 | `								}` |
|      68 | 1116 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 | 1117 | `									continue;` |
|       - | 1118 | `								}` |
|      44 | 1119 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 | 1120 | `								apLvl[i] = 0;` |
|      23 | 1121 | `							}` |
|      32 | 1122 | `						}` |
|      17 | 1123 | `					}` |
|      18 | 1124 | `					SySetRelease(&aTr);` |
|      10 | 1125 | `				}` |
|       - | 1126 | `			}` |
|    1270 | 1127 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1128 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - | 1129 | `				 * that another trait then composed, say — keeps the line order. */` |
|    1144 | 1130 | `				if( apLvl[i] != 0 ){` |
|     ! 0 | 1131 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 | 1132 | `				}` |
|     573 | 1133 | `			}` |
|     128 | 1134 | `			SySetRelease(&aLvl);` |
|      65 | 1135 | `		}` |
|      88 | 1136 | `		SySetRelease(&aTmp);` |
|       - | 1137 | `	}` |
|       - | 1138 | `	/* Return the created array */` |
|      88 | 1139 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1140 | `	/*` |
|       - | 1141 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1142 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1143 | `	 */` |
|      88 | 1144 | `	return PH7_OK;` |
|      47 | 1145 | `}` |
|       - | 1146 | `/*` |
|       - | 1147 | ` * The class a TRAIT method's frame is really executing in: walk the receiver's ancestry` |
|       - | 1148 | `` * (or, with no receiver, the current `self`) to the first class that uses this trait.`` |
|       - | 1149 | `` * `class Base { use T; } class Kid extends Base {}` answers Base from a Kid instance, which`` |
|       - | 1150 | ` * is where php composed the method. The trait itself stands when nothing in the chain lists` |
|       - | 1151 | ` * it — a trait used by another trait reaching this through an unusual path; php has no such` |
|       - | 1152 | ` * scope, but a lie would be worse.` |
|       - | 1153 | ` */` |
|      88 | 1154 | `static ph7_class * VmTraitScopeFrom(ph7_vm *pVm,ph7_class *pTrait,VmFrame *pFrame)` |
|       4 | 1155 | `{` |
|      92 | 1156 | `	ph7_class *pWalk = pFrame ? pFrame->pSelfClass : 0;` |
|      92 | 1157 | `	if( pWalk == 0 ){` |
|       - | 1158 | `		/* No activation of its own (a closure body, an initializer): the ambient self. */` |
|     ! 0 | 1159 | `		pWalk = VmCurrentSelf(&(*pVm));` |
|     ! 0 | 1160 | `	}` |
|      92 | 1161 | `	return PH7_VmTraitUsingClass(&(*pVm),pTrait,pWalk ? pWalk : pTrait);` |
|       4 | 1162 | `}` |
|       - | 1163 | `/*` |
|       - | 1164 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - | 1165 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - | 1166 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - | 1167 | ` *` |
|       - | 1168 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - | 1169 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - | 1170 | ` * private/protected refusal raised from inside a class.` |
|       - | 1171 | ` */` |
|    4172 | 1172 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 | 1173 | `{` |
|    4177 | 1174 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1175 | `	ph7_vm_func *pVmFunc;` |
|    4177 | 1176 | `	ph7_class *pScope = 0;` |
|    4295 | 1177 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - | 1178 | `		/* Safely ignore the exception frame */` |
|     122 | 1179 | `		pFrame = pFrame->pParent;` |
|       4 | 1180 | `	}` |
|    4177 | 1181 | `	if( pFrame == 0 ){` |
|     ! 0 | 1182 | `		return 0;` |
|       - | 1183 | `	}` |
|    4177 | 1184 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 1185 | `	/* An INITIALIZER -- a class constant's, or an instance property's default -- is a` |
|       - | 1186 | `	 * mini-program run by VmLocalExec, which pushes no frame of its own. The frame still` |
|       - | 1187 | ``	 * current is therefore the one the `new` executed in, and every branch below would`` |
|       - | 1188 | `	 * answer the CONSTRUCTING class. php runs an initializer in its DECLARING class's` |
|       - | 1189 | ``	 * scope: `private const A;` beside `private string $x = self::A;` is readable from`` |
|       - | 1190 | ``	 * that initializer no matter where the `new` happens to be, and reading it as the`` |
|       - | 1191 | `	 * caller made a library's own tunable default an Error the moment the object was` |
|       - | 1192 | `	 * built from inside another class (phpcs's Tokens::WEIGHTINGS, Collision's` |
|       - | 1193 | `	 * Highlighter::ARROW_SYMBOL_UTF8 -- neither tool could START).` |
|       - | 1194 | `	 *` |
|       - | 1195 | `	 * pConstEvalClass alone is not the test: it stays set while the initializer runs, so` |
|       - | 1196 | `	 * a METHOD the initializer calls would inherit the initializer's scope. The marker is` |
|       - | 1197 | `	 * the frame the eval began in -- the same test PH7_VmPeekDeclaringClass makes for` |
|       - | 1198 | ``	 * `self::` -- and once a method pushes a frame it stops matching. */`` |
|    4172 | 1199 | `	if( pVm->pConstEvalClass` |
|    2092 | 1200 | `	 && pVm->pConstEvalFrame == (void *)VmSkipExceptionFrames(pVm->pFrame) ){` |
|       3 | 1201 | `		pScope = pVm->pConstEvalClass;` |
|       3 | 1202 | `		goto normalize_trait;` |
|       - | 1203 | `	}` |
|       - | 1204 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - | 1205 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|    4175 | 1206 | `	if( pFrame->pBoundScope ){` |
|      36 | 1207 | `		return pFrame->pBoundScope; /* an explicit rebind names a CLASS; never a trait */` |
|       - | 1208 | `	}` |
|    6209 | 1209 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    3219 | 1210 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|    2534 | 1211 | `	}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1212 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - | 1213 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - | 1214 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - | 1215 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - | 1216 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - | 1217 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - | 1218 | `		 * pBoundScope. */` |
|      71 | 1219 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|     892 | 1220 | `	}else if( pVm->pConstEvalClass ){` |
|       - | 1221 | `		/* Constant/property initializer bytecode runs without a method` |
|       - | 1222 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - | 1223 | `		 * constant is reachable from its own class's initializers). */` |
|     ! 0 | 1224 | `		pScope = pVm->pConstEvalClass;` |
|     ! 0 | 1225 | `	}` |
|     426 | 1226 | `normalize_trait:` |
|    4143 | 1227 | `	if( pScope && (pScope->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1228 | `		/* php COMPOSES a trait method into the using class at compile time, so the scope` |
|       - | 1229 | ``		 * its code executes in IS that class — `protected` members of the class are its`` |
|       - | 1230 | `		 * own from in there, and so are the class's protected METHODS. PHL shares a trait` |
|       - | 1231 | `		 * method by pointer and its declaring class stays the trait, so this answered the` |
|       - | 1232 | `		 * trait and every protected access from a trait body was refused. */` |
|      92 | 1233 | `		pScope = VmTraitScopeFrom(&(*pVm),pScope,pFrame);` |
|      44 | 1234 | `	}` |
|    4143 | 1235 | `	return pScope;` |
|    2091 | 1236 | `}` |
|       - | 1237 | `/*` |
|       - | 1238 | ` * The DECLARING-side twin of PH7_VmCallerScope: the class php NAMES as a method's` |
|       - | 1239 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - | 1240 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - | 1241 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - | 1242 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - | 1243 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - | 1244 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - | 1245 | ` *` |
|       - | 1246 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - | 1247 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - | 1248 | ` * refused on a child instance names the BASE.` |
|       - | 1249 | ` */` |
|     824 | 1250 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 | 1251 | `{` |
|     412 | 1252 | `	SXUNUSED(pVm);` |
|    1653 | 1253 | `	return PH7_VmComposingClass(pClass,` |
|     824 | 1254 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 | 1255 | `}` |
|       - | 1256 | `/*` |
|       - | 1257 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - | 1258 | ` * property is composed into the using class exactly as its methods are, and` |
|       - | 1259 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - | 1260 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - | 1261 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - | 1262 | ` */` |
|     870 | 1263 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 | 1264 | `{` |
|       - | 1265 | `	/* One rule, one implementation: a trait's members belong to the class that composed` |
|       - | 1266 | `	 * them, found by walking pClass's ancestry -- through NESTED trait use as well, since` |
|       - | 1267 | ``	 * php flattens `class C { use Outer; } trait Outer { use Inner; }` into C whole. This`` |
|       - | 1268 | `	 * walk used to look at each class's OWN trait list only, so a member reached through` |
|       - | 1269 | `	 * two levels kept the inner trait as its owner and every visibility rule about it` |
|       - | 1270 | `	 * became a question about the trait. */` |
|     875 | 1271 | `	return PH7_VmMemberOwnerClass(pDecl,pClass);` |
|       5 | 1272 | `}` |
|       - | 1273 | `/*` |
|       - | 1274 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1275 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1276 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1277 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1278 | ` * holds no entry for it.` |
|       - | 1279 | ` */` |
|      40 | 1280 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       3 | 1281 | `{` |
|      43 | 1282 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|      43 | 1283 | `	if( pEntry ){` |
|      43 | 1284 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      23 | 1285 | `	}else{` |
|     ! 0 | 1286 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1287 | `	}` |
|      43 | 1288 | `}` |
|       - | 1289 | `/*` |
|       - | 1290 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1291 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1292 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1293 | ` */` |
|  219860 | 1294 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1295 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1296 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1297 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1298 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1299 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1300 | `	)` |
|       5 | 1301 | `{` |
|  219865 | 1302 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    1167 | 1303 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    1167 | 1304 | `		if( pCallerScope == 0 ){` |
|     589 | 1305 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1306 | `		}` |
|     583 | 1307 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1308 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1309 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1310 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1311 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1312 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1313 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1314 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1315 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1316 | `			 * match (dynamic props and other non-declared shapes). */` |
|     333 | 1317 | `			ph7_class *pCaller = pCallerScope;` |
|     497 | 1318 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|     328 | 1319 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|     333 | 1320 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|     333 | 1321 | `			int bGranted = 0;` |
|     333 | 1322 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       8 | 1323 | `				if( pOwn->pDeclClass == 0` |
|       8 | 1324 | `				 \|\| pOwn->pDeclClass == pCaller` |
|       6 | 1325 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|      10 | 1326 | `					bGranted = 1;` |
|       6 | 1327 | `				}` |
|     329 | 1328 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     221 | 1329 | `				bGranted = 1;` |
|     108 | 1330 | `			}` |
|     333 | 1331 | `			if( !bGranted ){` |
|       - | 1332 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1333 | `				ph7_class **apTrait;` |
|       - | 1334 | `				sxu32 nTrait,k;` |
|     108 | 1335 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     108 | 1336 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     108 | 1337 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1338 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1339 | `						bGranted = 1;` |
|     ! 0 | 1340 | `						break;` |
|       - | 1341 | `					}` |
|     ! 0 | 1342 | `				}` |
|      52 | 1343 | `			}` |
|     333 | 1344 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1345 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1346 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1347 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1348 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1349 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1350 | `				 * whose owner is the trait, not the class). */` |
|       - | 1351 | `				ph7_class **apTrait;` |
|       - | 1352 | `				sxu32 nTrait,k;` |
|     ! 0 | 1353 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     ! 0 | 1354 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     ! 0 | 1355 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1356 | `					if( apTrait[k] == pClass ){` |
|     ! 0 | 1357 | `						bGranted = 1;` |
|     ! 0 | 1358 | `						break;` |
|       - | 1359 | `					}` |
|     ! 0 | 1360 | `				}` |
|     ! 0 | 1361 | `			}` |
|     333 | 1362 | `			if( !bGranted ){` |
|     108 | 1363 | `				goto dis; /* Access is forbidden */` |
|       - | 1364 | `			}` |
|     117 | 1365 | `		}else{` |
|       - | 1366 | `			/* Protected */` |
|     255 | 1367 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1368 | `			/* The walk below exists for one shape only -- a SIBLING scope, which reaches` |
|       - | 1369 | `			 * a protected member through the ancestor that INTRODUCES it. When the two` |
|       - | 1370 | `			 * classes are on one inheritance chain the walk cannot change the answer, so` |
|       - | 1371 | `			 * it is skipped: the introducing class is by construction an ancestor-or-self` |
|       - | 1372 | `			 * of pClass, so it lies on that same chain, and one of the two directions of` |
|       - | 1373 | `			 * the test below therefore holds however far the walk climbs. Asked over the` |
|       - | 1374 | `			 * pBase links alone rather than through PH7_VmInstanceOf, so an interface in` |
|       - | 1375 | `			 * the picture cannot make the shortcut claim more than the walk would. It is` |
|       - | 1376 | `			 * the ordinary case -- a class touching its own protected member -- and the` |
|       - | 1377 | `			 * walk it skips costs two hash lookups per ANCESTOR, per access. */` |
|       - | 1378 | `			{` |
|       - | 1379 | `				ph7_class *pChain;` |
|     351 | 1380 | `				for( pChain = pClass ; pChain ; pChain = pChain->pBase ){` |
|     293 | 1381 | `					if( pChain == pBase ){` |
|     197 | 1382 | `						return 1;` |
|       - | 1383 | `					}` |
|      52 | 1384 | `				}` |
|     126 | 1385 | `				for( pChain = pBase ; pChain ; pChain = pChain->pBase ){` |
|     110 | 1386 | `					if( pChain == pClass ){` |
|      46 | 1387 | `						return 1;` |
|       - | 1388 | `					}` |
|      36 | 1389 | `				}` |
|       - | 1390 | `			}` |
|       - | 1391 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1392 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1393 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1394 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1395 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1396 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1397 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1398 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1399 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1400 | `			 * from a sibling, matching php. */` |
|      17 | 1401 | `			ph7_class *pIntro = pClass;` |
|       - | 1402 | `			ph7_class *pAnc;` |
|      39 | 1403 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|      23 | 1404 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|      23 | 1405 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|      23 | 1406 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|      23 | 1407 | `				int bHere = 0;` |
|      23 | 1408 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      15 | 1409 | `					bHere = 1;` |
|       7 | 1410 | `				}` |
|      23 | 1411 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|       7 | 1412 | `					bHere = 1;` |
|       3 | 1413 | `				}` |
|      23 | 1414 | `				if( bHere ){` |
|      21 | 1415 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      10 | 1416 | `				}` |
|      12 | 1417 | `			}` |
|       - | 1418 | `			/* Must be in the same class hierarchy as the introducing class */` |
|      17 | 1419 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      13 | 1420 | `				int bTraitGrant = 0;` |
|      13 | 1421 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1422 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1423 | `					ph7_class **apTrait;` |
|       - | 1424 | `					sxu32 nTrait,k;` |
|     ! 0 | 1425 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     ! 0 | 1426 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     ! 0 | 1427 | `					for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1428 | `						if( apTrait[k] == pClass ){` |
|     ! 0 | 1429 | `							bTraitGrant = 1;` |
|     ! 0 | 1430 | `							break;` |
|       - | 1431 | `						}` |
|     ! 0 | 1432 | `					}` |
|     ! 0 | 1433 | `				}` |
|      13 | 1434 | `				if( !bTraitGrant ){` |
|      13 | 1435 | `					goto dis; /* Access is forbidden */` |
|       - | 1436 | `				}` |
|     ! 0 | 1437 | `			}` |
|       - | 1438 | `		}` |
|     114 | 1439 | `	}` |
|  218931 | 1440 | `	return 1; /* Access is granted */` |
|     350 | 1441 | `dis:` |
|     705 | 1442 | `	if( bLog ){` |
|     ! 0 | 1443 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1444 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1445 | `			&pClass->sDisp,pAttrName);` |
|     ! 0 | 1446 | `	}` |
|     705 | 1447 | `	return 0; /* Access is forbidden */` |
|  109935 | 1448 | `}` |
|       - | 1449 | `/*` |
|       - | 1450 | ` * The same question about one PROPERTY rather than about a NAME.` |
|       - | 1451 | ` *` |
|       - | 1452 | ` * A private member belongs to a SLOT, not to a name: once a base's private` |
|       - | 1453 | ` * instance property is carried into a subclass under php's mangled storage name` |
|       - | 1454 | ` * (PH7_ClassAttrStorageName), one object can hold two of them, and the name-based` |
|       - | 1455 | ` * rule above would hand the executing scope whichever it was shown -- a subclass` |
|       - | 1456 | `` * method reading its own `$q` would be granted its base's `$q` just as readily.`` |
|       - | 1457 | ` * php decides an inherited private by identity alone: the executing scope IS the` |
|       - | 1458 | ` * declaring class, or the slot is not reachable at all.` |
|       - | 1459 | ` *` |
|       - | 1460 | ` * A property the reflected class DECLARED is left to the name-based rule, whose` |
|       - | 1461 | ` * trait grants and legacy fallbacks are what every other caller has always had.` |
|       - | 1462 | ` */` |
|  221452 | 1463 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog)` |
|       5 | 1464 | `{` |
|  221452 | 1465 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  112092 | 1466 | `	 && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2727 | 1467 | `		ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|    2727 | 1468 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    2727 | 1469 | `		int bDeny = 0;` |
|    2727 | 1470 | `		if( pScope && pOwner ){` |
|    2591 | 1471 | `			if( pScope == pOwner ){` |
|    2563 | 1472 | `				return 1;   /* php's rule, stated positively */` |
|       - | 1473 | `			}` |
|      30 | 1474 | `			if( pOwner != pClass ){` |
|       - | 1475 | `				/* An INHERITED private: this slot is the declaring class's and no` |
|       - | 1476 | `				 * one else's, so identity is the whole rule. */` |
|      27 | 1477 | `				bDeny = 1;` |
|      14 | 1478 | `			}else{` |
|       - | 1479 | `				/* The scope declares a private of this NAME, but it is a member of` |
|       - | 1480 | `				 * its own -- granting on the name would hand a base method the` |
|       - | 1481 | `				 * CHILD's property (and the other way round). Properties and` |
|       - | 1482 | `				 * constants are php's two separate namespaces, so ask the table` |
|       - | 1483 | `				 * this member belongs to. */` |
|       4 | 1484 | `				SyHash *pTab = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|       2 | 1485 | `					? &pScope->hConst : &pScope->hAttr;` |
|       4 | 1486 | `				SyHashEntry *pE = SyHashGet(pTab,` |
|       2 | 1487 | `					(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|       3 | 1488 | `				ph7_class_attr *pOwn = pE ? (ph7_class_attr *)pE->pUserData : 0;` |
|       3 | 1489 | `				if( pOwn && pOwn != pAttr && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 1490 | `					bDeny = 1;` |
|     ! 0 | 1491 | `				}` |
|       - | 1492 | `			}` |
|      14 | 1493 | `		}` |
|     168 | 1494 | `		if( bDeny ){` |
|      27 | 1495 | `			if( bLog ){` |
|     ! 0 | 1496 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1497 | `					"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1498 | `					&pClass->sDisp,&pAttr->sName);` |
|     ! 0 | 1499 | `			}` |
|      27 | 1500 | `			return 0;` |
|       - | 1501 | `		}` |
|      69 | 1502 | `	}` |
|  218873 | 1503 | `	return PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,bLog);` |
|  110731 | 1504 | `}` |
|       - | 1505 | `/*` |
|       - | 1506 | ` * Where in the inheritance chain a property was DECLARED, counted from the class` |
|       - | 1507 | ` * being listed: 0 for its own declarations, 1 for its parent's, and so on. A` |
|       - | 1508 | ` * property copied in from a TRAIT reports 0, because php compiles a trait's` |
|       - | 1509 | ` * properties into the using class itself.` |
|       - | 1510 | ` *` |
|       - | 1511 | ` * php's property table is built own-declarations-first and then APPENDED to by` |
|       - | 1512 | ` * each inheritance step, so this depth is the order get_class_vars() answers in.` |
|       - | 1513 | ` * The engine's own table is keyed by name and carries the inherited entries` |
|       - | 1514 | ` * first, which is why listing it has to sort rather than walk.` |
|       - | 1515 | ` */` |
|     140 | 1516 | `static int VmClassAttrDeclDepth(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1517 | `{` |
|     144 | 1518 | `	ph7_class *pCls = pClass;` |
|     144 | 1519 | `	int iDepth = 0;` |
|     196 | 1520 | `	while( pCls ){` |
|     196 | 1521 | `		if( pAttr->pDeclClass == pCls ){` |
|     144 | 1522 | `			return iDepth;` |
|       - | 1523 | `		}` |
|      54 | 1524 | `		pCls = pCls->pBase;` |
|      54 | 1525 | `		++iDepth;` |
|       2 | 1526 | `	}` |
|     ! 0 | 1527 | `	return 0;` |
|      74 | 1528 | `}` |
|       - | 1529 | `/*` |
|       - | 1530 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1531 | ` *   Get the default properties of the class` |
|       - | 1532 | ` * Parameters` |
|       - | 1533 | ` *  class_name` |
|       - | 1534 | ` *   The class name or class instance` |
|       - | 1535 | ` * Return` |
|       - | 1536 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1537 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1538 | ` *  of varname => value.` |
|       - | 1539 | ` * Note:` |
|       - | 1540 | ` *   NULL is returned on failure.` |
|       - | 1541 | ` */` |
|      32 | 1542 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1543 | `{` |
|       - | 1544 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1545 | `	SyHashEntry *pEntry;` |
|       - | 1546 | `	ph7_class *pClass,*pWalk;` |
|       - | 1547 | `	int iPass,iDepth,iMaxDepth;` |
|       - | 1548 | `	/* Extract the target class first */` |
|      36 | 1549 | `	pClass = 0;` |
|      36 | 1550 | `	if( nArg > 0 ){` |
|      36 | 1551 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      16 | 1552 | `	}` |
|      36 | 1553 | `	if( pClass == 0 ){` |
|       - | 1554 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1555 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1556 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1557 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1558 | `		int nLen = 0;` |
|     ! 0 | 1559 | `		const char *zVal = "";` |
|     ! 0 | 1560 | `		if( nArg > 0 ){` |
|     ! 0 | 1561 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1562 | `				zVal = "Array";` |
|     ! 0 | 1563 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1564 | `			}else{` |
|     ! 0 | 1565 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1566 | `			}` |
|     ! 0 | 1567 | `		}` |
|     ! 0 | 1568 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1569 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1570 | `			nLen,zVal);` |
|       - | 1571 | `	}` |
|      36 | 1572 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1573 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1574 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1575 | `		 * here, as it does in php. */` |
|       5 | 1576 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 | 1577 | `		if( rcMat != SXRET_OK ){` |
|       5 | 1578 | `			return rcMat;` |
|       - | 1579 | `		}` |
|     ! 0 | 1580 | `	}` |
|       - | 1581 | `	/* Create a new array  */` |
|      32 | 1582 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 1583 | `	pName = ph7_context_new_scalar(pCtx);` |
|      32 | 1584 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      32 | 1585 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1586 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1587 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1588 | `		return PH7_OK;` |
|       - | 1589 | `	}` |
|       - | 1590 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1591 | `	 *` |
|       - | 1592 | `	 * php walks the property table TWICE — every non-static first, then every` |
|       - | 1593 | `	 * static — so the answer is the instance properties in declaration order` |
|       - | 1594 | `	 * followed by the statics in declaration order, never the two interleaved` |
|       - | 1595 | `	 * the way one pass over a single table produces them.` |
|       - | 1596 | `	 *` |
|       - | 1597 | `	 * And what a static contributes is its DEFAULT, read out of the class's` |
|       - | 1598 | `	 * default table: php never looks at the live static slot here, so a` |
|       - | 1599 | ``	 * `C::$s = 'live'` before the call does not change the answer. The class`` |
|       - | 1600 | `	 * DEFAULT is the compiled initializer, which is what the non-static arm` |
|       - | 1601 | `	 * has always evaluated, so both arms now go through the same one. */` |
|      32 | 1602 | `	iMaxDepth = 0;` |
|      66 | 1603 | `	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 | 1604 | `		++iMaxDepth;` |
|      21 | 1605 | `	}` |
|      88 | 1606 | `	for( iPass = 0 ; iPass < 2 ; ++iPass ){` |
|     128 | 1607 | `	  for( iDepth = 0 ; iDepth < iMaxDepth ; ++iDepth ){` |
|      72 | 1608 | `		SyHashResetLoopCursor(&pClass->hAttr);` |
|     356 | 1609 | `		while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     288 | 1610 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       - | 1611 | `			int bStatic;` |
|     288 | 1612 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1613 | `				/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1614 | `				 * get_class_vars() excludes them (raw surface) */` |
|       5 | 1615 | `				continue;` |
|       - | 1616 | `			}` |
|     284 | 1617 | `			bStatic = (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) != 0;` |
|     284 | 1618 | `			if( bStatic != iPass \|\| VmClassAttrDeclDepth(pClass,pAttr) != iDepth ){` |
|     186 | 1619 | `				continue;` |
|       - | 1620 | `			}` |
|       - | 1621 | `			/* A PRIVATE property is listed only by the class that DECLARED it.` |
|       - | 1622 | `			 * The shared visibility screen answers the question a member ACCESS` |
|       - | 1623 | `			 * asks — may this scope reach the name — and grants a subclass its` |
|       - | 1624 | `			 * inherited copy of a base private, which is the right answer for a` |
|       - | 1625 | `			 * read through the base's own methods and the wrong one here: php` |
|       - | 1626 | `			 * compares the property's declaring class against the scope and` |
|       - | 1627 | `			 * drops it, so a parent's private never appears in a subclass's` |
|       - | 1628 | `			 * listing. */` |
|      98 | 1629 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      61 | 1630 | `			 && pAttr->pDeclClass != 0` |
|      28 | 1631 | `			 && pAttr->pDeclClass != PH7_VmCallerScope(pCtx->pVm) ){` |
|      22 | 1632 | `				continue;` |
|       - | 1633 | `			}` |
|       - | 1634 | `			/* Check if the access is allowed */` |
|      82 | 1635 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      76 | 1636 | `				SyString *pAttrName = &pAttr->sName;` |
|      76 | 1637 | `				ph7_value *pValue = 0;` |
|      76 | 1638 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       - | 1639 | `					/* Constants are materialized lazily and read from their slot */` |
|     ! 0 | 1640 | `					PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     ! 0 | 1641 | `					pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|      76 | 1642 | `				}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      54 | 1643 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1644 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      54 | 1645 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|      54 | 1646 | `					pValue = &sValue;` |
|      25 | 1647 | `				}` |
|       - | 1648 | `				/* Fill in the array */` |
|      76 | 1649 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      76 | 1650 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1651 | `				/* Reset the cursor */` |
|      76 | 1652 | `				ph7_value_reset_string_cursor(pName);` |
|      36 | 1653 | `			}` |
|       4 | 1654 | `		}` |
|      38 | 1655 | `	  }` |
|      32 | 1656 | `	}` |
|      32 | 1657 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1658 | `	/* Return the created array */` |
|      32 | 1659 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1660 | `	/*` |
|       - | 1661 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1662 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1663 | `	 */` |
|      32 | 1664 | `	return PH7_OK;` |
|      20 | 1665 | `}` |
|       - | 1666 | `/*` |
|       - | 1667 | ` * array get_object_vars(object $this)` |
|       - | 1668 | ` *   Gets the properties of the given object` |
|       - | 1669 | ` * Parameters` |
|       - | 1670 | ` *  this` |
|       - | 1671 | ` *   A class instance` |
|       - | 1672 | ` * Return` |
|       - | 1673 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1674 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1675 | ` *  it will be returned with a NULL value.` |
|       - | 1676 | ` * Note:` |
|       - | 1677 | ` *   NULL is returned on failure.` |
|       - | 1678 | ` */` |
|     354 | 1679 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1680 | `{` |
|     359 | 1681 | `	ph7_class_instance *pThis = 0;` |
|       - | 1682 | `	ph7_value *pName,*pArray;` |
|       - | 1683 | `	SyHashEntry *pEntry;` |
|     359 | 1684 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1685 | `		/* Extract the target instance */` |
|     359 | 1686 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     177 | 1687 | `	}` |
|     359 | 1688 | `	if( pThis == 0 ){` |
|       - | 1689 | `		/* No such instance,return NULL */` |
|     ! 0 | 1690 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1691 | `		return PH7_OK;` |
|       - | 1692 | `	}` |
|       - | 1693 | `	/* Create a new array  */` |
|     359 | 1694 | `	pArray = ph7_context_new_array(pCtx);` |
|     359 | 1695 | `	pName = ph7_context_new_scalar(pCtx);` |
|     359 | 1696 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1697 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1698 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1699 | `		return PH7_OK;` |
|       - | 1700 | `	}` |
|       - | 1701 | `	/* A class whose get_properties handler answers this purpose too: php asks` |
|       - | 1702 | `	 * the handler with ZEND_PROP_PURPOSE_GET_OBJECT_VARS, and SimpleXMLElement` |
|       - | 1703 | `	 * is the one native class here that answers it with the same table it shows` |
|       - | 1704 | `	 * var_dump. Every other one answers its real (empty) slots, which is the` |
|       - | 1705 | `	 * walk below. */` |
|     354 | 1706 | `	if( PH7_ClassVarsFromPresent(pThis->pClass)` |
|     183 | 1707 | `	 && PH7_ClassInstancePresent(pThis,pArray,0) ){` |
|       3 | 1708 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 1709 | `		return PH7_OK;` |
|       - | 1710 | `	}` |
|       - | 1711 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1712 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1713 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1714 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1715 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1716 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1717 | `	 * skipped instead of read after free. The name snapshotted is the STORAGE` |
|       - | 1718 | `	 * key -- an inherited private lives under php's mangled one -- while the key` |
|       - | 1719 | `	 * php puts in the answer is the property's plain name. */` |
|       - | 1720 | `	{` |
|       - | 1721 | `		SySet sNames;` |
|       - | 1722 | `		SyString *aName;` |
|       - | 1723 | `		sxu32 iName,nName;` |
|     357 | 1724 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     357 | 1725 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    1565 | 1726 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    1213 | 1727 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    1213 | 1728 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|       - | 1729 | `				/* Only non-static/constant attributes are extracted */` |
|     389 | 1730 | `				continue;` |
|       - | 1731 | `			}` |
|     829 | 1732 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      34 | 1733 | `				continue; /* typed, never written: not there yet (php) */` |
|       - | 1734 | `			}` |
|     792 | 1735 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     401 | 1736 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     ! 0 | 1737 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1738 | `			}` |
|     797 | 1739 | `			if( PH7_ClassInstanceAttrShadowed(pCtx->pVm,pThis,pEntry) ){` |
|       3 | 1740 | `				continue; /* an earlier accessible slot already answers for this name */` |
|       - | 1741 | `			}` |
|       - | 1742 | `			{` |
|       - | 1743 | `				SyString sKey;` |
|     795 | 1744 | `				SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     795 | 1745 | `				SySetPut(&sNames,(const void *)&sKey);` |
|       - | 1746 | `			}` |
|       5 | 1747 | `		}` |
|     357 | 1748 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     357 | 1749 | `		nName = SySetUsed(&sNames);` |
|    1147 | 1750 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     795 | 1751 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1752 | `			VmClassAttr *pVmAttr;` |
|     795 | 1753 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     795 | 1754 | `			if( pEntry == 0 ){` |
|     ! 0 | 1755 | `				continue; /* unset by an earlier hook */` |
|       - | 1756 | `			}` |
|     795 | 1757 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1758 | `			/* Check if the access is allowed */` |
|     795 | 1759 | `			if( PH7_VmClassAttrAccess(pCtx->pVm,pThis->pClass,pVmAttr->pAttr,FALSE) ){` |
|     637 | 1760 | `				ph7_value *pValue = 0;` |
|       - | 1761 | `				ph7_value sHookVal;` |
|       - | 1762 | `				sxi32 rcHk;` |
|       - | 1763 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1764 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     637 | 1765 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     637 | 1766 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     637 | 1767 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1768 | `					pValue = &sHookVal;` |
|     630 | 1769 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1770 | `					/* Extract attribute */` |
|     623 | 1771 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     314 | 1772 | `				}else{` |
|       - | 1773 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1774 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1775 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1776 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1777 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1778 | `					break;` |
|       - | 1779 | `				}` |
|     637 | 1780 | `				if( pValue ){` |
|       - | 1781 | `					/* Insert attribute name in the array -- php unmangles it. */` |
|     953 | 1782 | `					ph7_value_string(pName,SyStringData(&pVmAttr->pAttr->sName),` |
|     632 | 1783 | `						(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     632 | 1784 | `					if( rcHk == SXERR_NOTFOUND` |
|     630 | 1785 | `					 && PH7_ClassAttrIsRef(pThis,pVmAttr) ){` |
|       - | 1786 | `						/* php hands out the property's own REFERENCE for a property that` |
|       - | 1787 | `						 * IS one, so a write through the returned element reaches the` |
|       - | 1788 | `						 * object. A HOOK's answer is a computed value with no slot behind` |
|       - | 1789 | `						 * it and stays a copy. */` |
|      74 | 1790 | `						PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,` |
|      24 | 1791 | `							pName,pVmAttr->nIdx);` |
|      26 | 1792 | `					}else{` |
|     589 | 1793 | `						ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1794 | `					}` |
|     316 | 1795 | `				}` |
|     637 | 1796 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1797 | `				/* Reset the cursor */` |
|     637 | 1798 | `				ph7_value_reset_string_cursor(pName);` |
|     316 | 1799 | `			}` |
|     400 | 1800 | `		}` |
|     357 | 1801 | `		SySetRelease(&sNames);` |
|       - | 1802 | `	}` |
|       - | 1803 | `	/* Return the created array */` |
|     357 | 1804 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1805 | `	/*` |
|       - | 1806 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1807 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1808 | `	 */` |
|     357 | 1809 | `	return PH7_OK;` |
|     182 | 1810 | `}` |
|       - | 1811 | `/*` |
|       - | 1812 | ` * array get_mangled_object_vars(object $object)` |
|       - | 1813 | ` *  The object's own property table, with php's visibility MANGLING left on the keys:` |
|       - | 1814 | `` *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".`` |
|       - | 1815 | ` *` |
|       - | 1816 | ` *  It is get_object_vars()'s opposite in both of that function's decisions — no` |
|       - | 1817 | ` *  visibility screen (every property is reported, from every level of the chain) and` |
|       - | 1818 | `` *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,`` |
|       - | 1819 | ` *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not` |
|       - | 1820 | ` *  the (array) cast either: the cast asks a native class's own handler, so` |
|       - | 1821 | `` *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.`` |
|       - | 1822 | ` */` |
|      46 | 1823 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1824 | `{` |
|      49 | 1825 | `	ph7_class_instance *pThis = 0;` |
|       - | 1826 | `	ph7_value *pArray;` |
|      49 | 1827 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1828 | `		/* Extract the target instance */` |
|      49 | 1829 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      23 | 1830 | `	}` |
|      49 | 1831 | `	if( pThis == 0 ){` |
|       - | 1832 | ``		/* The `object $object` signature row refuses everything else before we run */`` |
|     ! 0 | 1833 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1834 | `		return PH7_OK;` |
|       - | 1835 | `	}` |
|      49 | 1836 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 1837 | `	if( pArray == 0 ){` |
|       - | 1838 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1839 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1840 | `		return PH7_OK;` |
|       - | 1841 | `	}` |
|      49 | 1842 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);` |
|      49 | 1843 | `	ph7_result_value(pCtx,pArray);` |
|      49 | 1844 | `	return PH7_OK;` |
|      26 | 1845 | `}` |
|       - | 1846 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1847 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1848 | ` * detection should reject them up front. */` |
|       - | 1849 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1850 | `/*` |
|       - | 1851 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1852 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1853 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1854 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1855 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1856 | ` */` |
| 2547698 | 1857 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1858 | `{` |
| 2633369 | 1859 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1860 | `		ph7_class **apParent;` |
|       - | 1861 | `		sxu32 n;` |
| 2576239 | 1862 | `		if( pIface == pTarget ){` |
| 2490563 | 1863 | `			return TRUE;` |
|       - | 1864 | `		}` |
|       - | 1865 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1866 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|   85681 | 1867 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   85699 | 1868 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|      29 | 1869 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|      11 | 1870 | `				return TRUE;` |
|       - | 1871 | `			}` |
|      10 | 1872 | `		}` |
|   85671 | 1873 | `		pIface = pIface->pBase;` |
|   85671 | 1874 | `		iDepth++;` |
|       5 | 1875 | `	}` |
|   57135 | 1876 | `	return FALSE;` |
| 1273780 | 1877 | `}` |
|       - | 1878 | `/*` |
|       - | 1879 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1880 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1881 | ` */` |
| 2818815 | 1882 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1883 | `{` |
|       - | 1884 | `	ph7_class **apInterface;` |
|       - | 1885 | `	sxu32 n;` |
| 2818820 | 1886 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1887 | `		/* Empty interface container */` |
|  287328 | 1888 | `		return FALSE;` |
|       - | 1889 | `	}` |
|       - | 1890 | `	/* Point to the set of implemented interfaces */` |
| 2531497 | 1891 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1892 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1893 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2588609 | 1894 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2547675 | 1895 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2490563 | 1896 | `			return TRUE;` |
|       - | 1897 | `		}` |
|   28544 | 1898 | `	}` |
|   40939 | 1899 | `	return FALSE;` |
| 1409153 | 1900 | `}` |
|       - | 1901 | `/*` |
|       - | 1902 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1903 | ` * is an instance of the main class (second argument).` |
|       - | 1904 | ` * Otherwise FALSE is returned.` |
|       - | 1905 | ` */` |
| 4136013 | 1906 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1907 | `{` |
|       - | 1908 | `	ph7_class *pParent;` |
|       - | 1909 | `	sxi32 rc;` |
| 4136018 | 1910 | `	if( pThis == pClass ){` |
|       - | 1911 | `		/* Instance of the same class */` |
| 1472538 | 1912 | `		return TRUE;` |
|       - | 1913 | `	}` |
|       - | 1914 | `	/* Check implemented interfaces */` |
| 2663485 | 1915 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2663485 | 1916 | `	if( rc ){` |
| 2361369 | 1917 | `		return TRUE;` |
|       - | 1918 | `	}` |
|       - | 1919 | `	/* Check parent classes */` |
|  302121 | 1920 | `	pParent = pThis->pBase;` |
|  328238 | 1921 | `	while( pParent ){` |
|  158098 | 1922 | `		if( pParent == pClass ){` |
|       - | 1923 | `			/* Same instance */` |
|    2797 | 1924 | `			return TRUE;` |
|       - | 1925 | `		}` |
|       - | 1926 | `		/* Check the implemented interfaces */` |
|  155306 | 1927 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  155306 | 1928 | `		if( rc ){` |
|  129189 | 1929 | `			return TRUE;` |
|       - | 1930 | `		}` |
|       - | 1931 | `		/* Point to the parent class */` |
|   26122 | 1932 | `		pParent = pParent->pBase;` |
|       5 | 1933 | `	}` |
|       - | 1934 | `	/* Not an instance of the the given class */` |
|  170145 | 1935 | `	return FALSE;` |
| 2067822 | 1936 | `}` |
|       - | 1937 | `/*` |
|       - | 1938 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1939 | ` * is a subclass of the main class (second argument).` |
|       - | 1940 | ` * Otherwise FALSE is returned.` |
|       - | 1941 | ` */` |
|      44 | 1942 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       3 | 1943 | `{` |
|       - | 1944 | `	SyHashEntry *pEntry;` |
|       - | 1945 | `	SyString *pName;` |
|      71 | 1946 | `	while( pClass ){` |
|      67 | 1947 | `		pName = &pClass->sName;` |
|       - | 1948 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      67 | 1949 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      67 | 1950 | `		if( pEntry ){` |
|      33 | 1951 | `			return TRUE;` |
|       - | 1952 | `		}` |
|       - | 1953 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1954 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1955 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1956 | `		 * class's aInterface, missing inherited interfaces. */` |
|      35 | 1957 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1958 | `			return TRUE;` |
|       - | 1959 | `		}` |
|      25 | 1960 | `		pClass = pClass->pBase;` |
|       1 | 1961 | `	}` |
|       - | 1962 | `	/* Not a subclass */` |
|       5 | 1963 | `	return FALSE;` |
|      25 | 1964 | `}` |
|       - | 1965 | `/*` |
|       - | 1966 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1967 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1968 | ` *   as one of its parents.` |
|       - | 1969 | ` * Parameters` |
|       - | 1970 | ` *  object_or_class` |
|       - | 1971 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1972 | ` * class` |
|       - | 1973 | ` *  The class or interface name to test against.` |
|       - | 1974 | ` * allow_string` |
|       - | 1975 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1976 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1977 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1978 | ` * Return` |
|       - | 1979 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1980 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1981 | ` */` |
|      34 | 1982 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1983 | `{` |
|      37 | 1984 | `	int res = 0; /* Assume FALSE by default */` |
|      37 | 1985 | `	if( nArg > 1 ){` |
|      37 | 1986 | `		ph7_class *pThisClass = 0;` |
|      37 | 1987 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1988 | `			/* An object first argument: allow_string is ignored (php). */` |
|      17 | 1989 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      29 | 1990 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1991 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1992 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1993 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1994 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      18 | 1995 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       8 | 1996 | `		}` |
|      37 | 1997 | `		if( pThisClass ){` |
|       - | 1998 | `			/* Extract the given class */` |
|      31 | 1999 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      31 | 2000 | `			if( pClass ){` |
|       - | 2001 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 2002 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      31 | 2003 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      14 | 2004 | `			}` |
|      14 | 2005 | `		}` |
|      17 | 2006 | `	}` |
|       - | 2007 | `	/* Query result */` |
|      37 | 2008 | `	ph7_result_bool(pCtx,res);` |
|      37 | 2009 | `	return PH7_OK;` |
|       3 | 2010 | `}` |
|       - | 2011 | `/*` |
|       - | 2012 | ` * int spl_object_id(object $object)` |
|       - | 2013 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 2014 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 2015 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 2016 | ` */` |
|      26 | 2017 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2018 | `{` |
|       - | 2019 | `	ph7_class_instance *pThis;` |
|      30 | 2020 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2021 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2022 | `		return PH7_OK;` |
|       - | 2023 | `	}` |
|      30 | 2024 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      30 | 2025 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      30 | 2026 | `	return PH7_OK;` |
|      17 | 2027 | `}` |
|       - | 2028 | `/*` |
|       - | 2029 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 2030 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 2031 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 2032 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 2033 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 2034 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 2035 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 2036 | ` *` |
|       - | 2037 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 2038 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 2039 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 2040 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 2041 | ` */` |
|      50 | 2042 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2043 | `{` |
|      51 | 2044 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2045 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2046 | `	char zGiven[64];` |
|       - | 2047 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 2048 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      51 | 2049 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2050 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2051 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 2052 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 2053 | `	}` |
|      51 | 2054 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 2055 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 2056 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 2057 | `	 * resource, and Generator/Fiber. */` |
|      50 | 2058 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|      48 | 2059 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      13 | 2060 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       8 | 2061 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sDisp);` |
|       - | 2062 | `	}` |
|      43 | 2063 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 2064 | `	if( pClone == 0 ){` |
|     ! 0 | 2065 | `		return PH7_VmMemoryError(pVm);` |
|       - | 2066 | `	}` |
|       - | 2067 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 2068 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 2069 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 2070 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 2071 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 2072 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 2073 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 2074 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 2075 | `		sxu32 n;` |
|      53 | 2076 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 2077 | `			ph7_value *pVal,sVal;` |
|       - | 2078 | `			const char *zName;` |
|       - | 2079 | `			sxu32 nName;` |
|       - | 2080 | `			char zKeyBuf[64];` |
|       - | 2081 | `			sxi32 rc;` |
|      31 | 2082 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 2083 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 2084 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 2085 | `				zName = zKeyBuf;` |
|     ! 0 | 2086 | `			}else{` |
|      31 | 2087 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 2088 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 2089 | `			}` |
|      31 | 2090 | `			pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 2091 | `			if( pVal ){` |
|       - | 2092 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 2093 | `				 * property, whose slot reservation used to reallocate pVm->aMemObj` |
|       - | 2094 | `				 * and dangle pVal. Redundant since P1 (fixed segments); left for the` |
|       - | 2095 | `				 * harvest sweep. */` |
|      31 | 2096 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 2097 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 2098 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 2099 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 2100 | `				if( rc != SXRET_OK ){` |
|       7 | 2101 | `					return rc;` |
|       - | 2102 | `				}` |
|      12 | 2103 | `			}` |
|      25 | 2104 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 2105 | `		}` |
|      11 | 2106 | `	}` |
|      37 | 2107 | `	return PH7_OK;` |
|      26 | 2108 | `}` |
|       - | 2109 | `/*` |
|       - | 2110 | ` * string spl_object_hash(object $object)` |
|       - | 2111 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 2112 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 2113 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 2114 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 2115 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 2116 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 2117 | ` */` |
|      18 | 2118 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2119 | `{` |
|       - | 2120 | `	ph7_class_instance *pThis;` |
|      20 | 2121 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2122 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2123 | `		return PH7_OK;` |
|       - | 2124 | `	}` |
|      20 | 2125 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 2126 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 2127 | `	return PH7_OK;` |
|      11 | 2128 | `}` |
|       - | 2129 | `/*` |
|       - | 2130 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 2131 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 2132 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 2133 | ` * Parameters` |
|       - | 2134 | ` *  object_or_class` |
|       - | 2135 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 2136 | ` * class` |
|       - | 2137 | ` *  The class or interface name to test against.` |
|       - | 2138 | ` * allow_string` |
|       - | 2139 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 2140 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 2141 | ` *  object first argument (php).` |
|       - | 2142 | ` * Return` |
|       - | 2143 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 2144 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 2145 | ` */` |
|      56 | 2146 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2147 | `{` |
|      59 | 2148 | `	int res = 0; /* Assume FALSE by default */` |
|      59 | 2149 | `	if( nArg > 1 ){` |
|      59 | 2150 | `		ph7_class *pClass = 0;` |
|      59 | 2151 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 2152 | `			/* An object first argument: allow_string is ignored (php). */` |
|      21 | 2153 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      50 | 2154 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 2155 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 2156 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 2157 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 2158 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      33 | 2159 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      15 | 2160 | `		}` |
|      59 | 2161 | `		if( pClass ){` |
|       - | 2162 | `			/* Extract the target class */` |
|      49 | 2163 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      49 | 2164 | `			if( pMain ){` |
|       - | 2165 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      47 | 2166 | `				res = VmSubclassOf(pClass,pMain);` |
|      22 | 2167 | `			}` |
|      23 | 2168 | `		}` |
|      28 | 2169 | `	}` |
|       - | 2170 | `	/* Query result */` |
|      59 | 2171 | `	ph7_result_bool(pCtx,res);` |
|      59 | 2172 | `	return PH7_OK;` |
|       3 | 2173 | `}` |
|       - | 2174 | `/*` |
|       - | 2175 | `` * php folds `call_user_func('f', ...)` / `call_user_func_array('f', $a)` into a DIRECT`` |
|       - | 2176 | ` * call of f when the callable is a literal string naming a function its compiler can` |
|       - | 2177 | ` * bind; any other callable (a variable, a Closure, an array pair) goes through` |
|       - | 2178 | ` * ZEND_INIT_USER_CALL, which marks the call DYNAMIC. A literal reaches the C body as` |
|       - | 2179 | ` * a constant-marked operand (nIdx == SXU32_HIGH, the mark OP_CALL reads for its own` |
|       - | 2180 | ` * callee), so the same question is answerable here. See ph7_vm::bDynamicForward.` |
|       - | 2181 | ` */` |
|     438 | 2182 | `static int VmForwardIsDynamic(const ph7_value *pCallable)` |
|       5 | 2183 | `{` |
|     443 | 2184 | `	return pCallable->nIdx != SXU32_HIGH \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0;` |
|       5 | 2185 | `}` |
|     286 | 2186 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2187 | `{` |
|       - | 2188 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 2189 | `	sxi32 rc;` |
|     291 | 2190 | `	if( nArg < 1 ){` |
|       - | 2191 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2192 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2193 | `		return PH7_OK;` |
|       - | 2194 | `	}` |
|       - | 2195 | `	{` |
|       - | 2196 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 2197 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     291 | 2198 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     291 | 2199 | `		if( rcCb != PH7_OK ){` |
|      76 | 2200 | `			return rcCb;` |
|       - | 2201 | `		}` |
|       - | 2202 | `	}` |
|     217 | 2203 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     217 | 2204 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2205 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     217 | 2206 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 2207 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 2208 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 2209 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 2210 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 2211 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 2212 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     227 | 2213 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      21 | 2214 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 2215 | `		VmCallArgMap sInner;` |
|       - | 2216 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 2217 | `		 * unset when forwarded, not as stack garbage. */` |
|      21 | 2218 | `		SyZero(&sInner,sizeof(sInner));` |
|      21 | 2219 | `		sInner.bHasNamed = 1;` |
|      21 | 2220 | `		sInner.bIsNamespaced = 0;` |
|       - | 2221 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 2222 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 2223 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 2224 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      21 | 2225 | `		sInner.bStrict = 0;` |
|      21 | 2226 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      21 | 2227 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|       - | 2228 | ``		/* php's compiler rewrites `call_user_func(f, ...)` into a direct call to`` |
|       - | 2229 | `		 * f, so a DROPPED answer here is a dropped answer for the callback: hand` |
|       - | 2230 | `		 * the bit on, one call deep. */` |
|      21 | 2231 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|      21 | 2232 | `		pCtx->pVm->bDynamicForward = VmForwardIsDynamic(apArg[0]);` |
|      21 | 2233 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      11 | 2234 | `	}else{` |
|       - | 2235 | `		/* call_user_func is one of php's two FORWARDS: the callback binds under the` |
|       - | 2236 | `		 * mode of the file that wrote the call_user_func, not weakly like every other` |
|       - | 2237 | `		 * internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 2238 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 2239 | `		 * other internal invocation, and wrong here). */` |
|       - | 2240 | `		VmCallArgMap sFwd;` |
|     197 | 2241 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     197 | 2242 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     197 | 2243 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;   /* see above */` |
|     197 | 2244 | `		pCtx->pVm->bDynamicForward = VmForwardIsDynamic(apArg[0]);` |
|     197 | 2245 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 2246 | `	}` |
|       - | 2247 | `	/* The latches are consumed by the OP_CALL the dispatch builds; clear them for` |
|       - | 2248 | `	 * the paths that never reach one, so they cannot describe some later call. */` |
|     217 | 2249 | `	pCtx->pVm->bDiscardCallback = 0;` |
|     217 | 2250 | `	pCtx->pVm->bDynamicForward = 0;` |
|     217 | 2251 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2252 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 2253 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      43 | 2254 | `		PH7_MemObjRelease(&sResult);` |
|      43 | 2255 | `		return PH7_EXCEPTION;` |
|       - | 2256 | `	}` |
|     177 | 2257 | `	if( rc != SXRET_OK ){` |
|       - | 2258 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2259 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2260 | `	}else{` |
|       - | 2261 | `		/* Callback result */` |
|     177 | 2262 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2263 | `	}` |
|     177 | 2264 | `	PH7_MemObjRelease(&sResult);` |
|     177 | 2265 | `	return PH7_OK;` |
|     148 | 2266 | `}` |
|       - | 2267 | `/*` |
|       - | 2268 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 2269 | ` *  Call a callback with an array of parameters.` |
|       - | 2270 | ` * Parameter` |
|       - | 2271 | ` *  $callback` |
|       - | 2272 | ` *   The callable to be called.` |
|       - | 2273 | ` * $param_arr` |
|       - | 2274 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 2275 | ` * Return` |
|       - | 2276 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 2277 | ` */` |
|     230 | 2278 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2279 | `{` |
|       - | 2280 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 2281 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 2282 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 2283 | `	SySet aArg;               /* Argument value pointers */` |
|     235 | 2284 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     235 | 2285 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     235 | 2286 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 2287 | `	sxi32 rc;` |
|       - | 2288 | `	sxu32 n;` |
|     235 | 2289 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 2290 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2291 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2292 | `		return PH7_OK;` |
|       - | 2293 | `	}` |
|       - | 2294 | `	{` |
|     235 | 2295 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     235 | 2296 | `		if( rcCb != PH7_OK ){` |
|       5 | 2297 | `			return rcCb;` |
|       - | 2298 | `		}` |
|       - | 2299 | `	}` |
|     231 | 2300 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     231 | 2301 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2302 | `	/* Initialize the arguments container */` |
|     231 | 2303 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2304 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 2305 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 2306 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 2307 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 2308 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 2309 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     231 | 2310 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     231 | 2311 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     631 | 2312 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 2313 | `		/* Extract node value */` |
|     404 | 2314 | `		if( (pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     404 | 2315 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      32 | 2316 | `				if( aNames == 0 ){` |
|       - | 2317 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 2318 | `					 * not-yet-seen slot defaults to positional. */` |
|      22 | 2319 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      22 | 2320 | `					if( aNames == 0 ){` |
|     ! 0 | 2321 | `						SySetRelease(&aArg);` |
|     ! 0 | 2322 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2323 | `						if( apNode ){` |
|     ! 0 | 2324 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 2325 | `						}` |
|     ! 0 | 2326 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 2327 | `					}` |
|      22 | 2328 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|      10 | 2329 | `				}` |
|      32 | 2330 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      15 | 2331 | `			}` |
|     404 | 2332 | `			if( apNode == 0 ){` |
|     247 | 2333 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     162 | 2334 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     166 | 2335 | `				if( apNode ){` |
|     166 | 2336 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|      81 | 2337 | `				}` |
|      81 | 2338 | `			}` |
|     404 | 2339 | `			if( apNode ){` |
|     404 | 2340 | `				apNode[nSlot] = pEntry;` |
|     200 | 2341 | `			}` |
|     404 | 2342 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     404 | 2343 | `			nSlot++;` |
|     200 | 2344 | `		}` |
|       - | 2345 | `		/* Point to the next entry */` |
|     404 | 2346 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     204 | 2347 | `	}` |
|       - | 2348 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 2349 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 2350 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 2351 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     231 | 2352 | `	if( apNode ){` |
|     166 | 2353 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     166 | 2354 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     166 | 2355 | `		apNode = 0;` |
|      81 | 2356 | `	}` |
|       - | 2357 | `	/* Try to invoke the callback. Like call_user_func, this is a php FORWARD: a` |
|       - | 2358 | `	 * dropped answer here is a dropped answer for the callback. */` |
|     231 | 2359 | `	pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|     231 | 2360 | `	pCtx->pVm->bDynamicForward = VmForwardIsDynamic(apArg[0]);` |
|     231 | 2361 | `	if( aNames ){` |
|       - | 2362 | `		VmCallArgMap sMap;` |
|      22 | 2363 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      22 | 2364 | `		sMap.bHasNamed = 1;` |
|      22 | 2365 | `		sMap.bIsNamespaced = 0;` |
|       - | 2366 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 2367 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      22 | 2368 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      22 | 2369 | `		sMap.nTotal = nSlot;` |
|      22 | 2370 | `		sMap.aNames = aNames;` |
|      32 | 2371 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      20 | 2372 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      22 | 2373 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      12 | 2374 | `	}else{` |
|       - | 2375 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 2376 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 2377 | `		VmCallArgMap sFwd;` |
|     210 | 2378 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     210 | 2379 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     313 | 2380 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     206 | 2381 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 2382 | `	}` |
|     231 | 2383 | `	pCtx->pVm->bDiscardCallback = 0;   /* see the call_user_func sibling */` |
|     231 | 2384 | `	pCtx->pVm->bDynamicForward = 0;` |
|     231 | 2385 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2386 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     136 | 2387 | `		PH7_MemObjRelease(&sResult);` |
|     136 | 2388 | `		SySetRelease(&aArg);` |
|     136 | 2389 | `		return PH7_EXCEPTION;` |
|       - | 2390 | `	}` |
|      97 | 2391 | `	if( rc != SXRET_OK ){` |
|       - | 2392 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2393 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2394 | `	}else{` |
|       - | 2395 | `		/* Callback result */` |
|      97 | 2396 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2397 | `	}` |
|       - | 2398 | `	/* Cleanup the mess left behind */` |
|      97 | 2399 | `	PH7_MemObjRelease(&sResult);` |
|      97 | 2400 | `	SySetRelease(&aArg);` |
|      97 | 2401 | `	return PH7_OK;` |
|     120 | 2402 | `}` |
|       - | 2403 |  |
