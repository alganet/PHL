# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 865/975 lines (88.72%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|    3478 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|    3483 |   11 | `	if( nArg < 1 ){` |
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
|    3483 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    3483 |   25 | `		if( pClass ){` |
|    3483 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|    3483 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    1744 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|    3483 |   34 | `	return PH7_OK;` |
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
|      66 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      71 |   52 | `	if( nArg < 1 ){` |
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
|      69 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      69 |   66 | `		if( pClass ){` |
|      69 |   67 | `			if( pClass->pBase ){` |
|      67 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      67 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      36 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       3 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      37 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      71 |   80 | `	return PH7_OK;` |
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
|  206782 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  206787 |  115 | `	ph7_class *pClass = 0;` |
|  206787 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  203749 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  104915 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    3039 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    3039 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    3037 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    1516 |  135 | `		}` |
|    1517 |  136 | `	}` |
|  206787 |  137 | `	return pClass;` |
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
|      56 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  151 | `{` |
|      58 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|      58 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|      56 |  155 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      41 |  156 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
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
|      56 |  170 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      56 |  171 | `		if( pClass ){` |
|       - |  172 | `			const char *zName;` |
|       - |  173 | `			int nLen;` |
|       - |  174 | `			/* Extract attribute name */` |
|      56 |  175 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      56 |  176 | `			if( nLen > 0 ){` |
|       - |  177 | `				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this` |
|       - |  178 | ``				 * name is not a property (`property_exists('C','someMethod')` is false),`` |
|       - |  179 | `				 * and neither is a class CONSTANT. PHL searched the method table too and` |
|       - |  180 | `				 * answered true for both. */` |
|      56 |  181 | `				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);` |
|      56 |  182 | `				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;` |
|      56 |  183 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - |  184 | `					/* A base's PRIVATE property is invisible to the child it was asked` |
|       - |  185 | ``					 * about, php's `property_info->ce == ce` rule: PHL copies one down`` |
|       - |  186 | `					 * onto every child (its own methods read it through $this), so the` |
|       - |  187 | `					 * table alone said true where php says false. Static or instance,` |
|       - |  188 | `					 * the rule is the same; protected and public are inherited outright.` |
|       - |  189 | `					 * A trait's property belongs to the class that COMPOSED it. */` |
|      34 |  190 | `					res = 1;` |
|      32 |  191 | `					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      24 |  192 | `					 && pAttr->pDeclClass != 0` |
|      18 |  193 | `					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){` |
|       9 |  194 | `						res = 0;` |
|       4 |  195 | `					}` |
|      16 |  196 | `				}` |
|       - |  197 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  198 | `				 * attribute table, not the class's — php reports those too` |
|       - |  199 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|      56 |  200 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       9 |  201 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       9 |  202 | `					SyHashEntry *pObjE = pThis` |
|       8 |  203 | `						? SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) : 0;` |
|       9 |  204 | `					VmClassAttr *pObjAttr = pObjE ? (VmClassAttr *)pObjE->pUserData : 0;` |
|       - |  205 | `					/* Only a genuinely DYNAMIC one: every instance carries a slot for every` |
|       - |  206 | `					 * DECLARED member too, so an unqualified instance lookup answered true` |
|       - |  207 | `					 * for the base private the class-table rule above had just refused. */` |
|       8 |  208 | `					if( pObjAttr && pObjAttr->pAttr` |
|       3 |  209 | `					 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|       3 |  210 | `						res = 1;` |
|       1 |  211 | `					}` |
|       4 |  212 | `				}` |
|      27 |  213 | `			}` |
|      27 |  214 | `		}` |
|      27 |  215 | `	}` |
|      56 |  216 | `	ph7_result_bool(pCtx,res);` |
|      56 |  217 | `	return PH7_OK;` |
|      30 |  218 | `}` |
|       - |  219 | `/*` |
|       - |  220 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  221 | ` *   Checks if the given method is a class member.` |
|       - |  222 | ` * Parameters` |
|       - |  223 | ` *  class` |
|       - |  224 | ` *   The class name or an object of the class to test for` |
|       - |  225 | ` * property` |
|       - |  226 | ` *  The name of the method` |
|       - |  227 | ` * Return` |
|       - |  228 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  229 | ` */` |
|      50 |  230 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  231 | `{` |
|      52 |  232 | `	int res = 0; /* Assume method does not exists */` |
|      52 |  233 | `	if( nArg > 1 ){` |
|       - |  234 | `		ph7_class *pClass;` |
|      50 |  235 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      28 |  236 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  237 | `			/* An incomplete OBJECT consults its method resolution, which the` |
|       - |  238 | `			 * carrier refuses with php's catchable call Error (probe-verified;` |
|       - |  239 | `			 * the class asked about by NAME answers false the ordinary way). */` |
|       - |  240 | `			SyBlob sIncMsg;` |
|       - |  241 | `			sxi32 rcInc;` |
|       3 |  242 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  243 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  244 | `				"call a method",&sIncMsg);` |
|       4 |  245 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  246 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  247 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  248 | `			return rcInc;` |
|       - |  249 | `		}` |
|      50 |  250 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      50 |  251 | `		if( pClass ){` |
|       - |  252 | `			const char *zName;` |
|       - |  253 | `			int nLen;` |
|       - |  254 | `			/* Extract method name */` |
|      46 |  255 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      46 |  256 | `			if( nLen > 0 ){` |
|       - |  257 | `				/* Perform the lookup in the method table */` |
|      46 |  258 | `				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);` |
|      46 |  259 | `				if( pEntry ){` |
|       - |  260 | ``					/* ...and apply php's one visibility rule here (`func->common.scope`` |
|       - |  261 | ``					 * == ce`): a PRIVATE method is only a method of the class that`` |
|       - |  262 | `					 * declares it. PHL copies a base's private down so an inherited` |
|       - |  263 | `					 * public method can still dispatch it, which made` |
|       - |  264 | ``					 * `method_exists('Child','basePrivate')` answer true where php`` |
|       - |  265 | `					 * answers false — the same shape property_exists() had. Nothing` |
|       - |  266 | `					 * about the CALLING scope enters into it: php answers false for the` |
|       - |  267 | `					 * child from inside the BASE too. A trait's method belongs to the` |
|       - |  268 | `					 * class that COMPOSED it. */` |
|      42 |  269 | `					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      42 |  270 | `					res = 1;` |
|      40 |  271 | `					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      32 |  272 | `					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){` |
|      15 |  273 | `						res = 0;` |
|       7 |  274 | `					}` |
|      20 |  275 | `				}` |
|      22 |  276 | `			}` |
|      22 |  277 | `		}` |
|      24 |  278 | `	}` |
|      50 |  279 | `	ph7_result_bool(pCtx,res);` |
|      50 |  280 | `	return PH7_OK;` |
|      27 |  281 | `}` |
|       - |  282 | `/*` |
|       - |  283 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  284 | ` *   Checks if the class has been defined.` |
|       - |  285 | ` * Parameters` |
|       - |  286 | ` *  class_name` |
|       - |  287 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  288 | ` *   unlinke the standard PHP engine.` |
|       - |  289 | ` *  autoload` |
|       - |  290 | ` *   Whether or not to call __autoload by default.` |
|       - |  291 | ` * Return` |
|       - |  292 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  293 | ` */` |
|     106 |  294 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  295 | `{` |
|     110 |  296 | `	int res = 0; /* Assume class does not exist */` |
|     110 |  297 | `	if( nArg > 0 ){` |
|     110 |  298 | `		SyHashEntry *pEntry = 0;` |
|       - |  299 | `		const char *zName;` |
|       - |  300 | `		int nLen;` |
|     110 |  301 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  302 | `		sxu32 nName;` |
|       - |  303 | `		/* Extract given name */` |
|     110 |  304 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     110 |  305 | `		if( nArg >= 2 ){` |
|       6 |  306 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  307 | `		}` |
|       - |  308 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  309 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     110 |  310 | `		nName = (sxu32)nLen;` |
|     110 |  311 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     110 |  312 | `		if( nName > 0 ){` |
|       - |  313 | `			/* Perform a hash lookup first */` |
|     106 |  314 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      51 |  315 | `		}` |
|       - |  316 | `		/* Gate autoload on the ORIGINAL length (nLen), not the stripped nName:` |
|       - |  317 | `		 * php autoloads a lone "\" (with the empty stripped name) but not "". */` |
|     110 |  318 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  319 | `			/* Try autoload, then re-check */` |
|      28 |  320 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      28 |  321 | `			if( pClass ){` |
|       9 |  322 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  323 | `			}` |
|      12 |  324 | `		}` |
|     110 |  325 | `		if( pEntry ){` |
|       - |  326 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  327 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|      88 |  328 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      92 |  329 | `			while( pClass ){` |
|      88 |  330 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|      84 |  331 | `					res = 1;` |
|      84 |  332 | `					break;` |
|       - |  333 | `				}` |
|       5 |  334 | `				pClass = pClass->pNextName;` |
|       1 |  335 | `			}` |
|      42 |  336 | `		}` |
|      53 |  337 | `	}` |
|     110 |  338 | `	ph7_result_bool(pCtx,res);` |
|     110 |  339 | `	return PH7_OK;` |
|       4 |  340 | `}` |
|       - |  341 | `/*` |
|       - |  342 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  343 | ` *   Checks if the interface has been defined.` |
|       - |  344 | ` * Parameters` |
|       - |  345 | ` *  class_name` |
|       - |  346 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  347 | ` *   unlinke the standard PHP engine.` |
|       - |  348 | ` *  autoload` |
|       - |  349 | ` *   Whether or not to call __autoload by default.` |
|       - |  350 | ` * Return` |
|       - |  351 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  352 | ` */` |
|      36 |  353 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  354 | `{` |
|      39 |  355 | `	int res = 0; /* Assume interface does not exist */` |
|      39 |  356 | `	if( nArg > 0 ){` |
|      39 |  357 | `		SyHashEntry *pEntry = 0;` |
|       - |  358 | `		const char *zName;` |
|       - |  359 | `		int nLen;` |
|      39 |  360 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  361 | `		sxu32 nName;` |
|       - |  362 | `		/* Extract given name */` |
|      39 |  363 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      39 |  364 | `		if( nArg >= 2 ){` |
|     ! 0 |  365 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     ! 0 |  366 | `		}` |
|       - |  367 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  368 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      39 |  369 | `		nName = (sxu32)nLen;` |
|      39 |  370 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  371 | `		/* Perform a hash lookup */` |
|      39 |  372 | `		if( nName > 0 ){` |
|      39 |  373 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      18 |  374 | `		}` |
|       - |  375 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  376 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      39 |  377 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  378 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  379 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  380 | `			if( pClass ){` |
|     ! 0 |  381 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  382 | `			}` |
|       1 |  383 | `		}` |
|      39 |  384 | `		if( pEntry ){` |
|      37 |  385 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      39 |  386 | `			while( pClass ){` |
|      37 |  387 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  388 | `					/* interface is available */` |
|      35 |  389 | `					res = 1;` |
|      35 |  390 | `					break;` |
|       - |  391 | `				}` |
|       - |  392 | `				/* Next with the same name */` |
|       3 |  393 | `				pClass = pClass->pNextName;` |
|       1 |  394 | `			}` |
|      17 |  395 | `		}` |
|      18 |  396 | `	}` |
|      39 |  397 | `	ph7_result_bool(pCtx,res);` |
|      39 |  398 | `	return PH7_OK;` |
|       3 |  399 | `}` |
|       - |  400 | `/*` |
|       - |  401 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  402 | ` *   Checks if the trait has been defined.` |
|       - |  403 | ` * Parameters` |
|       - |  404 | ` *  trait` |
|       - |  405 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  406 | ` *  autoload` |
|       - |  407 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  408 | ` * Return` |
|       - |  409 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  410 | ` */` |
|      16 |  411 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  412 | `{` |
|      18 |  413 | `	int res = 0; /* Assume trait does not exist */` |
|      18 |  414 | `	if( nArg > 0 ){` |
|      18 |  415 | `		SyHashEntry *pEntry = 0;` |
|       - |  416 | `		const char *zName;` |
|       - |  417 | `		int nLen;` |
|      18 |  418 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  419 | `		sxu32 nName;` |
|       - |  420 | `		/* Extract given name */` |
|      18 |  421 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      18 |  422 | `		if( nArg >= 2 ){` |
|       3 |  423 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       1 |  424 | `		}` |
|       - |  425 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  426 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      18 |  427 | `		nName = (sxu32)nLen;` |
|      18 |  428 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  429 | `		/* Perform a hash lookup */` |
|      18 |  430 | `		if( nName > 0 ){` |
|      18 |  431 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       8 |  432 | `		}` |
|       - |  433 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  434 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      18 |  435 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  436 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       5 |  437 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       5 |  438 | `			if( pClass ){` |
|     ! 0 |  439 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  440 | `			}` |
|       2 |  441 | `		}` |
|      18 |  442 | `		if( pEntry ){` |
|      12 |  443 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  444 | `			while( pClass ){` |
|      12 |  445 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  446 | `					/* trait is available */` |
|       6 |  447 | `					res = 1;` |
|       6 |  448 | `					break;` |
|       - |  449 | `				}` |
|       - |  450 | `				/* Next with the same name */` |
|       7 |  451 | `				pClass = pClass->pNextName;` |
|       1 |  452 | `			}` |
|       5 |  453 | `		}` |
|       8 |  454 | `	}` |
|      18 |  455 | `	ph7_result_bool(pCtx,res);` |
|      18 |  456 | `	return PH7_OK;` |
|       2 |  457 | `}` |
|       - |  458 | `/*` |
|       - |  459 | ` * bool class_alias([string $original[,string $alias ]])` |
|       - |  460 | ` *   Creates an alias for a class.` |
|       - |  461 | ` * Parameters` |
|       - |  462 | ` *  original` |
|       - |  463 | ` *    The original class.` |
|       - |  464 | ` *  alias` |
|       - |  465 | ` *   The alias name for the class.` |
|       - |  466 | ` * Return` |
|       - |  467 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  468 | ` */` |
|       4 |  469 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  470 | `{` |
|       - |  471 | `	const char *zOld,*zNew;` |
|       - |  472 | `	int nOldLen,nNewLen;` |
|       - |  473 | `	sxu32 nOld,nNew;` |
|       - |  474 | `	SyHashEntry *pEntry;` |
|       - |  475 | `	ph7_class *pClass;` |
|       - |  476 | `	char *zDup;` |
|       - |  477 | `	sxi32 rc;` |
|       6 |  478 | `	if( nArg < 2 ){` |
|       - |  479 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  480 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  481 | `		return PH7_OK;` |
|       - |  482 | `	}` |
|       - |  483 | `	/* Extract old class name */` |
|       6 |  484 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  485 | `	/* Extract alias name */` |
|       6 |  486 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|       - |  487 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  488 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  489 | `	 * on the plain name then matches. */` |
|       6 |  490 | `	nOld = (sxu32)nOldLen;` |
|       6 |  491 | `	nNew = (sxu32)nNewLen;` |
|       6 |  492 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|       6 |  493 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       6 |  494 | `	if( nNew < 1 ){` |
|       - |  495 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  496 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  497 | `		return PH7_OK;` |
|       - |  498 | `	}` |
|       - |  499 | `	/* Perform a hash lookup */` |
|       6 |  500 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|       6 |  501 | `	if( pEntry ==  0 ){` |
|       - |  502 | `		/* No such class,return FALSE */` |
|     ! 0 |  503 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  504 | `		return PH7_OK;` |
|       - |  505 | `	}` |
|       - |  506 | `	/* Point to the class */` |
|       6 |  507 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  508 | `	/* Duplicate alias name */` |
|       6 |  509 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|       6 |  510 | `	if( zDup == 0 ){` |
|       - |  511 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  512 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  513 | `		return PH7_OK;` |
|       - |  514 | `	}` |
|       - |  515 | `	/* Create the alias */` |
|       6 |  516 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|       6 |  517 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  518 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  519 | `	}` |
|       6 |  520 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       6 |  521 | `	return PH7_OK;` |
|       4 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * array get_declared_classes(void)` |
|       - |  525 | ` *   Returns an array with the name of the defined classes` |
|       - |  526 | ` * Parameters` |
|       - |  527 | ` *  None` |
|       - |  528 | ` * Return` |
|       - |  529 | ` *   Returns an array of the names of the declared classes` |
|       - |  530 | ` *   in the current script.` |
|       - |  531 | ` * Note:` |
|       - |  532 | ` *   NULL is returned on failure.` |
|       - |  533 | ` */` |
|       2 |  534 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  535 | `{` |
|       - |  536 | `	ph7_value *pName,*pArray;` |
|       - |  537 | `	SyHashEntry *pEntry;` |
|       - |  538 | `	/* Create a new array first */` |
|       3 |  539 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  540 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  541 | `	if( pArray == 0 \|\| pName == 0){` |
|     ! 0 |  542 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  543 | `		SXUNUSED(apArg);` |
|       - |  544 | `		/* Out of memory,return NULL */` |
|     ! 0 |  545 | `		ph7_result_null(pCtx);` |
|     ! 0 |  546 | `		return PH7_OK;` |
|       - |  547 | `	}` |
|       - |  548 | `	/* Fill the array with the defined classes */` |
|       3 |  549 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     424 |  550 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     421 |  551 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  552 | `		/* Do not register classes defined as interfaces */` |
|     421 |  553 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     381 |  554 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  555 | `			/* insert class name */` |
|     381 |  556 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  557 | `			/* Reset the cursor */` |
|     381 |  558 | `			ph7_value_reset_string_cursor(pName);` |
|     190 |  559 | `		}` |
|       1 |  560 | `	}` |
|       - |  561 | `	/* Return the created array */` |
|       3 |  562 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  563 | `	return PH7_OK;` |
|       2 |  564 | `}` |
|       - |  565 | `/*` |
|       - |  566 | ` * array get_declared_interfaces(void)` |
|       - |  567 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  568 | ` * Parameters` |
|       - |  569 | ` *  None` |
|       - |  570 | ` * Return` |
|       - |  571 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  572 | ` *   in the current script.` |
|       - |  573 | ` * Note:` |
|       - |  574 | ` *   NULL is returned on failure.` |
|       - |  575 | ` */` |
|       2 |  576 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  577 | `{` |
|       - |  578 | `	ph7_value *pName,*pArray;` |
|       - |  579 | `	SyHashEntry *pEntry;` |
|       - |  580 | `	/* Create a new array first */` |
|       3 |  581 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  582 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  583 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  584 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  585 | `		SXUNUSED(apArg);` |
|       - |  586 | `		/* Out of memory,return NULL */` |
|     ! 0 |  587 | `		ph7_result_null(pCtx);` |
|     ! 0 |  588 | `		return PH7_OK;` |
|       - |  589 | `	}` |
|       - |  590 | `	/* Fill the array with the defined classes */` |
|       3 |  591 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     426 |  592 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     423 |  593 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  594 | `		/* Register classes defined as interfaces only */` |
|     423 |  595 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      43 |  596 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  597 | `			/* insert interface name */` |
|      43 |  598 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  599 | `			/* Reset the cursor */` |
|      43 |  600 | `			ph7_value_reset_string_cursor(pName);` |
|      21 |  601 | `		}` |
|       1 |  602 | `	}` |
|       - |  603 | `	/* Return the created array */` |
|       3 |  604 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  605 | `	return PH7_OK;` |
|       2 |  606 | `}` |
|       - |  607 | `/*` |
|       - |  608 | ` * Does this method-table entry answer to the method's OWN name (rather than to an` |
|       - |  609 | ` * adaptation alias made from it)? Method names fold case, so the comparison does too.` |
|       - |  610 | ` */` |
|     122 |  611 | `static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  612 | `{` |
|     183 |  613 | `	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte` |
|     122 |  614 | `		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;` |
|       2 |  615 | `}` |
|       - |  616 | `/*` |
|       - |  617 | ` * Which inheritance LEVEL does this method-table entry belong to — the class php would` |
|       - |  618 | ` * have added it under? A method declared in a class body is its own; a trait's is the` |
|       - |  619 | ` * class that COMPOSED it, which is two different questions depending on the entry. An` |
|       - |  620 | ` * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still` |
|       - |  621 | `` * holding this key over this very struct is the one whose `use` block wrote it. A trait`` |
|       - |  622 | ` * method under its own name is the same struct in every class that uses the trait, and` |
|       - |  623 | ` * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait` |
|       - |  624 | ` * composes its own copy, and the parent's inherited entry never replaces it.` |
|       - |  625 | ` */` |
|     386 |  626 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       3 |  627 | `{` |
|     389 |  628 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|     389 |  629 | `	ph7_class *pWalk,*pHigh = 0;` |
|     389 |  630 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     332 |  631 | `		return pDecl;` |
|       - |  632 | `	}` |
|      58 |  633 | `	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){` |
|      62 |  634 | `		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 |  635 | `			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);` |
|      38 |  636 | `			if( pE && pE->pUserData == (void *)pMeth ){` |
|      34 |  637 | `				pHigh = pWalk;` |
|      16 |  638 | `			}` |
|      20 |  639 | `		}` |
|      26 |  640 | `		if( pHigh ){` |
|      26 |  641 | `			return pHigh;` |
|       - |  642 | `		}` |
|     ! 0 |  643 | `	}` |
|      34 |  644 | `	return PH7_VmComposingClass(pClass,pDecl);` |
|     196 |  645 | `}` |
|       - |  646 | `/*` |
|       - |  647 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  648 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  649 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  650 | ` */` |
|     220 |  651 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       3 |  652 | `{` |
|     223 |  653 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|     223 |  654 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     223 |  655 | `	ph7_value_reset_string_cursor(pName);` |
|     223 |  656 | `}` |
|       - |  657 | `/*` |
|       - |  658 | ` * array get_class_methods(object\|string $object_or_class)` |
|       - |  659 | ` *   Returns an array with the names of the class methods the CALLING SCOPE can reach,` |
|       - |  660 | ` *   in php's order: each class's own body methods, then its trait composition, then the` |
|       - |  661 | ` *   same again for every ancestor.` |
|       - |  662 | ` * Parameters` |
|       - |  663 | ` *  object_or_class` |
|       - |  664 | ` *   The class name or a class instance. Anything that does not resolve to a class is a` |
|       - |  665 | ` *   TypeError naming the type given — this builtin never answers NULL.` |
|       - |  666 | ` */` |
|      46 |  667 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  668 | `{` |
|       - |  669 | `	ph7_value *pName,*pArray;` |
|       - |  670 | `	SyHashEntry *pEntry;` |
|       - |  671 | `	ph7_class *pClass;` |
|       - |  672 | `	/* Extract the target class first */` |
|      49 |  673 | `	pClass = 0;` |
|      49 |  674 | `	if( nArg > 0 ){` |
|      49 |  675 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      23 |  676 | `	}` |
|      49 |  677 | `	if( pClass == 0 ){` |
|       - |  678 | `		/* php screens the VALUE, not the type: anything that does not resolve to a` |
|       - |  679 | `		 * class — a name nothing declares, an int, an array, null — is ONE TypeError` |
|       - |  680 | `		 * naming the type given. PHL answered NULL for most of them (and the shared` |
|       - |  681 | ``		 * ZPP screen's `must be of type object\|string` for the rest), so a typo in a`` |
|       - |  682 | `		 * class name silently listed nothing. This is why get_class_methods() joins` |
|       - |  683 | `		 * get_class_vars() on the self-checked list in vm_arg_check.c. */` |
|       - |  684 | `		char zGiven[64];` |
|       9 |  685 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  686 | `			"get_class_methods(): Argument #1 ($object_or_class) must be an object "` |
|       - |  687 | `			"or a valid class name, %s given",` |
|       4 |  688 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - |  689 | `	}` |
|       - |  690 | `	/* Create a new array  */` |
|      45 |  691 | `	pArray = ph7_context_new_array(pCtx);` |
|      45 |  692 | `	pName = ph7_context_new_scalar(pCtx);` |
|      45 |  693 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  694 | `		/* Out of memory,return NULL */` |
|     ! 0 |  695 | `		ph7_result_null(pCtx);` |
|     ! 0 |  696 | `		return PH7_OK;` |
|       - |  697 | `	}` |
|       - |  698 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  699 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  700 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  701 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  702 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  703 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  704 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  705 | `	{` |
|       - |  706 | `		SySet aTmp;` |
|       - |  707 | `		SyHashEntry **apEntry;` |
|       - |  708 | `		ph7_class *pLevel;` |
|       - |  709 | `		sxu32 n;` |
|       - |  710 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  711 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  712 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      45 |  713 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      45 |  714 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|     313 |  715 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|     271 |  716 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       3 |  717 | `		}` |
|      45 |  718 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     103 |  719 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  720 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  721 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  722 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  723 | `			SySet aLvl;` |
|       - |  724 | `			SyHashEntry **apLvl;` |
|       - |  725 | `			sxu32 i,j;` |
|      61 |  726 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  727 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  728 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  729 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  730 | `			 * hash) come out in declaration order walked oldest-first. */` |
|     447 |  731 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|     389 |  732 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|     389 |  733 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - |  734 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - |  735 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - |  736 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - |  737 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - |  738 | `				 * methods before its own. */` |
|     389 |  739 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - |  740 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - |  741 | `				 * protected within the hierarchy, private only from the class that` |
|       - |  742 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - |  743 | `				 * every private and protected name a class holds. Same decision` |
|       - |  744 | `				 * get_class_vars() already makes for properties. */` |
|     389 |  745 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  746 | `					SyString sMName;` |
|      93 |  747 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - |  748 | `						apEntry[nPick]->nKeyLen);` |
|       - |  749 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - |  750 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - |  751 | `					 * wrote it, and judged against the class that composed the method. */` |
|     139 |  752 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      46 |  753 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      46 |  754 | `							pMethod->iProtection,FALSE) ){` |
|      79 |  755 | `						continue;` |
|       - |  756 | `					}` |
|       7 |  757 | `				}` |
|     311 |  758 | `				if( pDecl != pLevel ){` |
|       - |  759 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - |  760 | `					 * counts as the class's own level, like php. */` |
|       - |  761 | `					ph7_class *pWalk;` |
|      89 |  762 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|      35 |  763 | `						continue;` |
|       - |  764 | `					}` |
|     109 |  765 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     109 |  766 | `						if( pWalk == pDecl ){` |
|      55 |  767 | `							break;` |
|       - |  768 | `						}` |
|      28 |  769 | `					}` |
|      55 |  770 | `					if( pWalk != 0 ){` |
|      55 |  771 | `						continue; /* in-chain: its own level emits it */` |
|       - |  772 | `					}` |
|     ! 0 |  773 | `				}` |
|     223 |  774 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     113 |  775 | `			}` |
|      61 |  776 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - |  777 | `			/* Insertion sort by declaration line (stable) */` |
|     225 |  778 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|     167 |  779 | `				SyHashEntry *pKey = apLvl[i];` |
|     231 |  780 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|     251 |  781 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 |  782 | `					apLvl[j] = apLvl[j-1];` |
|      33 |  783 | `				}` |
|     167 |  784 | `				apLvl[j] = pKey;` |
|      85 |  785 | `			}` |
|       - |  786 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - |  787 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - |  788 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - |  789 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - |  790 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - |  791 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - |  792 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - |  793 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - |  794 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - |  795 | `			 * claim still goes out at the end. */` |
|     281 |  796 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     223 |  797 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     223 |  798 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|     223 |  799 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|     180 |  800 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     180 |  801 | `					apLvl[i] = 0;` |
|      89 |  802 | `				}` |
|     113 |  803 | `			}` |
|       - |  804 | `			{` |
|      61 |  805 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|      61 |  806 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - |  807 | `				sxu32 k;` |
|      77 |  808 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 |  809 | `					ph7_class *pTrait = apTrait[k];` |
|       - |  810 | `					SySet aTr;` |
|       - |  811 | `					SyHashEntry **apTr;` |
|       - |  812 | `					SyHashEntry *pTrE;` |
|       - |  813 | `					sxu32 t;` |
|      18 |  814 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 |  815 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 |  816 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 |  817 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 |  818 | `					}` |
|      18 |  819 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - |  820 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - |  821 | `					 * declaration order. */` |
|      48 |  822 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 |  823 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - |  824 | `						int bWantAlias;` |
|       - |  825 | `						/* First pass emits the aliases made from this method, second the` |
|       - |  826 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 |  827 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 |  828 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  829 | `								ph7_class_method *pM;` |
|     342 |  830 | `								if( apLvl[i] == 0 ){` |
|     184 |  831 | `									continue;` |
|       - |  832 | `								}` |
|     160 |  833 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 |  834 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 |  835 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 |  836 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 |  837 | `										pOrigin->sFunc.sName.zString,` |
|      98 |  838 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 |  839 | `									continue;` |
|       - |  840 | `								}` |
|      68 |  841 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 |  842 | `									continue;` |
|       - |  843 | `								}` |
|      44 |  844 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 |  845 | `								apLvl[i] = 0;` |
|      23 |  846 | `							}` |
|      32 |  847 | `						}` |
|      17 |  848 | `					}` |
|      18 |  849 | `					SySetRelease(&aTr);` |
|      10 |  850 | `				}` |
|       - |  851 | `			}` |
|     281 |  852 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  853 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - |  854 | `				 * that another trait then composed, say — keeps the line order. */` |
|     223 |  855 | `				if( apLvl[i] != 0 ){` |
|     ! 0 |  856 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 |  857 | `				}` |
|     113 |  858 | `			}` |
|      61 |  859 | `			SySetRelease(&aLvl);` |
|      32 |  860 | `		}` |
|      45 |  861 | `		SySetRelease(&aTmp);` |
|       - |  862 | `	}` |
|       - |  863 | `	/* Return the created array */` |
|      45 |  864 | `	ph7_result_value(pCtx,pArray);` |
|       - |  865 | `	/*` |
|       - |  866 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  867 | `	 * automatically as soon we return from this foreign function.` |
|       - |  868 | `	 */` |
|      45 |  869 | `	return PH7_OK;` |
|      26 |  870 | `}` |
|       - |  871 | `/*` |
|       - |  872 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - |  873 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - |  874 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - |  875 | ` *` |
|       - |  876 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - |  877 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - |  878 | ` * private/protected refusal raised from inside a class.` |
|       - |  879 | ` */` |
|    1922 |  880 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 |  881 | `{` |
|    1927 |  882 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - |  883 | `	ph7_vm_func *pVmFunc;` |
|    2015 |  884 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - |  885 | `		/* Safely ignore the exception frame */` |
|      92 |  886 | `		pFrame = pFrame->pParent;` |
|       4 |  887 | `	}` |
|    1927 |  888 | `	if( pFrame == 0 ){` |
|     ! 0 |  889 | `		return 0;` |
|       - |  890 | `	}` |
|    1927 |  891 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  892 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - |  893 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|    1927 |  894 | `	if( pFrame->pBoundScope ){` |
|      32 |  895 | `		return pFrame->pBoundScope;` |
|       - |  896 | `	}` |
|    1897 |  897 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    1405 |  898 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  899 | `	}` |
|     497 |  900 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  901 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - |  902 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - |  903 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - |  904 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - |  905 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - |  906 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - |  907 | `		 * pBoundScope. */` |
|      67 |  908 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  909 | `	}` |
|     431 |  910 | `	if( pVm->pConstEvalClass ){` |
|       - |  911 | `		/* Constant/property initializer bytecode runs without a method` |
|       - |  912 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - |  913 | `		 * constant is reachable from its own class's initializers). */` |
|       3 |  914 | `		return pVm->pConstEvalClass;` |
|       - |  915 | `	}` |
|     429 |  916 | `	return 0;` |
|     966 |  917 | `}` |
|       - |  918 | `/*` |
|       - |  919 | ` * The scope php NAMES in a visibility Error. PH7_VmCallerScope with one adjustment: php` |
|       - |  920 | ` * flattens a TRAIT into the class that uses it, so code running in a trait method reports` |
|       - |  921 | ` * the USING class ("from scope Base"), never the trait — and not the RECEIVER's class` |
|       - |  922 | `` * either, so `class Kid extends Base` (Base being the one that composed the trait) still`` |
|       - |  923 | ` * reports Base. Walk the receiver's ancestry to the first class that uses this trait; the` |
|       - |  924 | ` * trait itself stands when nothing does (nothing php would print, but better than a lie).` |
|       - |  925 | ` *` |
|       - |  926 | ` * Kept apart from PH7_VmCallerScope because the ACCESS decision genuinely wants the trait:` |
|       - |  927 | ` * its private/protected branches grant on "the caller is a trait used by the target class"` |
|       - |  928 | ` * and on the reverse, and both compare against the trait itself.` |
|       - |  929 | ` */` |
|      90 |  930 | `PH7_PRIVATE ph7_class * PH7_VmCallerScopeName(ph7_vm *pVm)` |
|       3 |  931 | `{` |
|      93 |  932 | `	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       - |  933 | `	VmFrame *pFrame;` |
|       - |  934 | `	ph7_class *pWalk;` |
|      93 |  935 | `	if( pScope == 0 \|\| (pScope->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      89 |  936 | `		return pScope;` |
|       - |  937 | `	}` |
|       5 |  938 | `	pFrame = pVm->pFrame;` |
|       5 |  939 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|     ! 0 |  940 | `		pFrame = pFrame->pParent;` |
|     ! 0 |  941 | `	}` |
|       5 |  942 | `	pWalk = (pFrame && pFrame->pThis) ? pFrame->pThis->pClass : VmCurrentSelf(&(*pVm));` |
|       7 |  943 | `	for( ; pWalk ; pWalk = pWalk->pBase ){` |
|       7 |  944 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|       7 |  945 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - |  946 | `		sxu32 k;` |
|       7 |  947 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|       5 |  948 | `			if( apTrait[k] == pScope ){` |
|       5 |  949 | `				return pWalk;` |
|       - |  950 | `			}` |
|     ! 0 |  951 | `		}` |
|       2 |  952 | `	}` |
|     ! 0 |  953 | `	return pScope;` |
|      48 |  954 | `}` |
|       - |  955 | `/*` |
|       - |  956 | ` * The DECLARING-side twin of PH7_VmCallerScopeName: the class php NAMES as a method's` |
|       - |  957 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - |  958 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - |  959 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - |  960 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - |  961 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - |  962 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - |  963 | ` *` |
|       - |  964 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - |  965 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - |  966 | ` * refused on a child instance names the BASE.` |
|       - |  967 | ` */` |
|     732 |  968 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  969 | `{` |
|     366 |  970 | `	SXUNUSED(pVm);` |
|    1469 |  971 | `	return PH7_VmComposingClass(pClass,` |
|     732 |  972 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 |  973 | `}` |
|       - |  974 | `/*` |
|       - |  975 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - |  976 | ` * property is composed into the using class exactly as its methods are, and` |
|       - |  977 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - |  978 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - |  979 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - |  980 | ` */` |
|     780 |  981 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 |  982 | `{` |
|       - |  983 | `	ph7_class *pWalk;` |
|     785 |  984 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     687 |  985 | `		return pDecl;` |
|       - |  986 | `	}` |
|     139 |  987 | `	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|     137 |  988 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|     137 |  989 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - |  990 | `		sxu32 k;` |
|     159 |  991 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|     121 |  992 | `			if( apTrait[k] == pDecl ){` |
|      99 |  993 | `				return pWalk;` |
|       - |  994 | `			}` |
|      13 |  995 | `		}` |
|      21 |  996 | `	}` |
|       3 |  997 | `	return pDecl;` |
|     395 |  998 | `}` |
|       - |  999 | `/*` |
|       - | 1000 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1001 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1002 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1003 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1004 | ` * holds no entry for it.` |
|       - | 1005 | ` */` |
|      30 | 1006 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       1 | 1007 | `{` |
|      31 | 1008 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|      31 | 1009 | `	if( pEntry ){` |
|      31 | 1010 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      16 | 1011 | `	}else{` |
|     ! 0 | 1012 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1013 | `	}` |
|      31 | 1014 | `}` |
|       - | 1015 | `/*` |
|       - | 1016 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1017 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1018 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1019 | ` */` |
|  207712 | 1020 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1021 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1022 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1023 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1024 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1025 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1026 | `	)` |
|       5 | 1027 | `{` |
|  207717 | 1028 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    1837 | 1029 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    1837 | 1030 | `		if( pCallerScope == 0 ){` |
|     357 | 1031 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1032 | `		}` |
|    1485 | 1033 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1034 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1035 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1036 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1037 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1038 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1039 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1040 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1041 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1042 | `			 * match (dynamic props and other non-declared shapes). */` |
|    1303 | 1043 | `			ph7_class *pCaller = pCallerScope;` |
|    1952 | 1044 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|    1298 | 1045 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|    1303 | 1046 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|    1303 | 1047 | `			int bGranted = 0;` |
|    1303 | 1048 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     988 | 1049 | `				if( pOwn->pDeclClass == 0` |
|     988 | 1050 | `				 \|\| pOwn->pDeclClass == pCaller` |
|     515 | 1051 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     963 | 1052 | `					bGranted = 1;` |
|     484 | 1053 | `				}` |
|     809 | 1054 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     219 | 1055 | `				bGranted = 1;` |
|     107 | 1056 | `			}` |
|    1303 | 1057 | `			if( !bGranted ){` |
|       - | 1058 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1059 | `				ph7_class **apTrait;` |
|       - | 1060 | `				sxu32 nTrait,k;` |
|     130 | 1061 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     130 | 1062 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     130 | 1063 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1064 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1065 | `						bGranted = 1;` |
|     ! 0 | 1066 | `						break;` |
|       - | 1067 | `					}` |
|     ! 0 | 1068 | `				}` |
|      63 | 1069 | `			}` |
|    1303 | 1070 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1071 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1072 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1073 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1074 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1075 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1076 | `				 * whose owner is the trait, not the class). */` |
|       - | 1077 | `				ph7_class **apTrait;` |
|       - | 1078 | `				sxu32 nTrait,k;` |
|     ! 0 | 1079 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     ! 0 | 1080 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     ! 0 | 1081 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1082 | `					if( apTrait[k] == pClass ){` |
|     ! 0 | 1083 | `						bGranted = 1;` |
|     ! 0 | 1084 | `						break;` |
|       - | 1085 | `					}` |
|     ! 0 | 1086 | `				}` |
|     ! 0 | 1087 | `			}` |
|    1303 | 1088 | `			if( !bGranted ){` |
|     130 | 1089 | `				goto dis; /* Access is forbidden */` |
|       - | 1090 | `			}` |
|     591 | 1091 | `		}else{` |
|       - | 1092 | `			/* Protected */` |
|     187 | 1093 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1094 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1095 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1096 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1097 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1098 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1099 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1100 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1101 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1102 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1103 | `			 * from a sibling, matching php. */` |
|     187 | 1104 | `			ph7_class *pIntro = pClass;` |
|       - | 1105 | `			ph7_class *pAnc;` |
|     459 | 1106 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|     277 | 1107 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|     277 | 1108 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     277 | 1109 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|     277 | 1110 | `				int bHere = 0;` |
|     277 | 1111 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      55 | 1112 | `					bHere = 1;` |
|      27 | 1113 | `				}` |
|     277 | 1114 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|      97 | 1115 | `					bHere = 1;` |
|      47 | 1116 | `				}` |
|     277 | 1117 | `				if( bHere ){` |
|     151 | 1118 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      74 | 1119 | `				}` |
|     141 | 1120 | `			}` |
|       - | 1121 | `			/* Must be in the same class hierarchy as the introducing class */` |
|     187 | 1122 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      14 | 1123 | `				int bTraitGrant = 0;` |
|      14 | 1124 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1125 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1126 | `					ph7_class **apTrait;` |
|       - | 1127 | `					sxu32 nTrait,k;` |
|       3 | 1128 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|       3 | 1129 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|       3 | 1130 | `					for(k = 0; k < nTrait; k++){` |
|       3 | 1131 | `						if( apTrait[k] == pClass ){` |
|       3 | 1132 | `							bTraitGrant = 1;` |
|       3 | 1133 | `							break;` |
|       - | 1134 | `						}` |
|     ! 0 | 1135 | `					}` |
|       1 | 1136 | `				}` |
|      14 | 1137 | `				if( !bTraitGrant ){` |
|      11 | 1138 | `					goto dis; /* Access is forbidden */` |
|       - | 1139 | `				}` |
|       1 | 1140 | `			}` |
|       - | 1141 | `		}` |
|     672 | 1142 | `	}` |
|  207229 | 1143 | `	return 1; /* Access is granted */` |
|     244 | 1144 | `dis:` |
|     493 | 1145 | `	if( bLog ){` |
|     ! 0 | 1146 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1147 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1148 | `			&pClass->sName,pAttrName);` |
|     ! 0 | 1149 | `	}` |
|     493 | 1150 | `	return 0; /* Access is forbidden */` |
|  103861 | 1151 | `}` |
|       - | 1152 | `/*` |
|       - | 1153 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1154 | ` *   Get the default properties of the class` |
|       - | 1155 | ` * Parameters` |
|       - | 1156 | ` *  class_name` |
|       - | 1157 | ` *   The class name or class instance` |
|       - | 1158 | ` * Return` |
|       - | 1159 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1160 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1161 | ` *  of varname => value.` |
|       - | 1162 | ` * Note:` |
|       - | 1163 | ` *   NULL is returned on failure.` |
|       - | 1164 | ` */` |
|      12 | 1165 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1166 | `{` |
|       - | 1167 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1168 | `	SyHashEntry *pEntry;` |
|       - | 1169 | `	ph7_class *pClass;` |
|       - | 1170 | `	/* Extract the target class first */` |
|      15 | 1171 | `	pClass = 0;` |
|      15 | 1172 | `	if( nArg > 0 ){` |
|      15 | 1173 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       6 | 1174 | `	}` |
|      15 | 1175 | `	if( pClass == 0 ){` |
|       - | 1176 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1177 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1178 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1179 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1180 | `		int nLen = 0;` |
|     ! 0 | 1181 | `		const char *zVal = "";` |
|     ! 0 | 1182 | `		if( nArg > 0 ){` |
|     ! 0 | 1183 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1184 | `				zVal = "Array";` |
|     ! 0 | 1185 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1186 | `			}else{` |
|     ! 0 | 1187 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1188 | `			}` |
|     ! 0 | 1189 | `		}` |
|     ! 0 | 1190 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1191 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1192 | `			nLen,zVal);` |
|       - | 1193 | `	}` |
|      15 | 1194 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1195 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1196 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1197 | `		 * here, as it does in php. */` |
|       5 | 1198 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 | 1199 | `		if( rcMat != SXRET_OK ){` |
|       5 | 1200 | `			return rcMat;` |
|       - | 1201 | `		}` |
|     ! 0 | 1202 | `	}` |
|       - | 1203 | `	/* Create a new array  */` |
|      11 | 1204 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 1205 | `	pName = ph7_context_new_scalar(pCtx);` |
|      11 | 1206 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      11 | 1207 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1208 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1209 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1210 | `		return PH7_OK;` |
|       - | 1211 | `	}` |
|       - | 1212 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|      11 | 1213 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      23 | 1214 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      15 | 1215 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      15 | 1216 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1217 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1218 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 | 1219 | `			continue;` |
|       - | 1220 | `		}` |
|       - | 1221 | `		/* Check if the access is allowed */` |
|      13 | 1222 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      13 | 1223 | `			SyString *pAttrName = &pAttr->sName;` |
|      13 | 1224 | `			ph7_value *pValue = 0;` |
|      13 | 1225 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1226 | `				/* Static slots are computed at mount; constants lazily */` |
|       8 | 1227 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       8 | 1228 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       5 | 1229 | `			}else{` |
|       6 | 1230 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 | 1231 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1232 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 | 1233 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 | 1234 | `					pValue = &sValue;` |
|       2 | 1235 | `				}` |
|       - | 1236 | `			}` |
|       - | 1237 | `			/* Fill in the array */` |
|      13 | 1238 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      13 | 1239 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1240 | `			/* Reset the cursor */` |
|      13 | 1241 | `			ph7_value_reset_string_cursor(pName);` |
|       5 | 1242 | `		}` |
|       3 | 1243 | `	}` |
|      11 | 1244 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1245 | `	/* Return the created array */` |
|      11 | 1246 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1247 | `	/*` |
|       - | 1248 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1249 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1250 | `	 */` |
|      11 | 1251 | `	return PH7_OK;` |
|       9 | 1252 | `}` |
|       - | 1253 | `/*` |
|       - | 1254 | ` * array get_object_vars(object $this)` |
|       - | 1255 | ` *   Gets the properties of the given object` |
|       - | 1256 | ` * Parameters` |
|       - | 1257 | ` *  this` |
|       - | 1258 | ` *   A class instance` |
|       - | 1259 | ` * Return` |
|       - | 1260 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1261 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1262 | ` *  it will be returned with a NULL value.` |
|       - | 1263 | ` * Note:` |
|       - | 1264 | ` *   NULL is returned on failure.` |
|       - | 1265 | ` */` |
|     106 | 1266 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1267 | `{` |
|     111 | 1268 | `	ph7_class_instance *pThis = 0;` |
|       - | 1269 | `	ph7_value *pName,*pArray;` |
|       - | 1270 | `	SyHashEntry *pEntry;` |
|     111 | 1271 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1272 | `		/* Extract the target instance */` |
|     111 | 1273 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      53 | 1274 | `	}` |
|     111 | 1275 | `	if( pThis == 0 ){` |
|       - | 1276 | `		/* No such instance,return NULL */` |
|     ! 0 | 1277 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1278 | `		return PH7_OK;` |
|       - | 1279 | `	}` |
|       - | 1280 | `	/* Create a new array  */` |
|     111 | 1281 | `	pArray = ph7_context_new_array(pCtx);` |
|     111 | 1282 | `	pName = ph7_context_new_scalar(pCtx);` |
|     111 | 1283 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1284 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1285 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1286 | `		return PH7_OK;` |
|       - | 1287 | `	}` |
|       - | 1288 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1289 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1290 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1291 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1292 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1293 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1294 | `	 * skipped instead of read after free. */` |
|       - | 1295 | `	{` |
|       - | 1296 | `		SySet sNames;` |
|       - | 1297 | `		SyString *aName;` |
|       - | 1298 | `		sxu32 iName,nName;` |
|     111 | 1299 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     111 | 1300 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|     499 | 1301 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     393 | 1302 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     393 | 1303 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|       - | 1304 | `				/* Only non-static/constant attributes are extracted */` |
|     203 | 1305 | `				continue;` |
|       - | 1306 | `			}` |
|     188 | 1307 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      98 | 1308 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 | 1309 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1310 | `			}` |
|     190 | 1311 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       4 | 1312 | `		}` |
|     111 | 1313 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     111 | 1314 | `		nName = SySetUsed(&sNames);` |
|     297 | 1315 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     190 | 1316 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1317 | `			VmClassAttr *pVmAttr;` |
|     190 | 1318 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     190 | 1319 | `			if( pEntry == 0 ){` |
|     ! 0 | 1320 | `				continue; /* unset by an earlier hook */` |
|       - | 1321 | `			}` |
|     190 | 1322 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1323 | `			/* Check if the access is allowed */` |
|     190 | 1324 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     148 | 1325 | `				ph7_value *pValue = 0;` |
|       - | 1326 | `				ph7_value sHookVal;` |
|       - | 1327 | `				sxi32 rcHk;` |
|       - | 1328 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1329 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     148 | 1330 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     148 | 1331 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     148 | 1332 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1333 | `					pValue = &sHookVal;` |
|     141 | 1334 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1335 | `					/* Extract attribute */` |
|     134 | 1336 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      69 | 1337 | `				}else{` |
|       - | 1338 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1339 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1340 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1341 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1342 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1343 | `					break;` |
|       - | 1344 | `				}` |
|     148 | 1345 | `				if( pValue ){` |
|       - | 1346 | `					/* Insert attribute name in the array */` |
|     148 | 1347 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     148 | 1348 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|      72 | 1349 | `				}` |
|     148 | 1350 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1351 | `				/* Reset the cursor */` |
|     148 | 1352 | `				ph7_value_reset_string_cursor(pName);` |
|      72 | 1353 | `			}` |
|      97 | 1354 | `		}` |
|     111 | 1355 | `		SySetRelease(&sNames);` |
|       - | 1356 | `	}` |
|       - | 1357 | `	/* Return the created array */` |
|     111 | 1358 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1359 | `	/*` |
|       - | 1360 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1361 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1362 | `	 */` |
|     111 | 1363 | `	return PH7_OK;` |
|      58 | 1364 | `}` |
|       - | 1365 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1366 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1367 | ` * detection should reject them up front. */` |
|       - | 1368 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1369 | `/*` |
|       - | 1370 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1371 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1372 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1373 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1374 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1375 | ` */` |
| 2479788 | 1376 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1377 | `{` |
| 2503345 | 1378 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1379 | `		ph7_class **apParent;` |
|       - | 1380 | `		sxu32 n;` |
| 2489593 | 1381 | `		if( pIface == pTarget ){` |
| 2466039 | 1382 | `			return TRUE;` |
|       - | 1383 | `		}` |
|       - | 1384 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1385 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|   23559 | 1386 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   23563 | 1387 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       7 | 1388 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|       3 | 1389 | `				return TRUE;` |
|       - | 1390 | `			}` |
|       3 | 1391 | `		}` |
|   23557 | 1392 | `		pIface = pIface->pBase;` |
|   23557 | 1393 | `		iDepth++;` |
|       5 | 1394 | `	}` |
|   13757 | 1395 | `	return FALSE;` |
| 1239899 | 1396 | `}` |
|       - | 1397 | `/*` |
|       - | 1398 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1399 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1400 | ` */` |
| 2703888 | 1401 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1402 | `{` |
|       - | 1403 | `	ph7_class **apInterface;` |
|       - | 1404 | `	sxu32 n;` |
| 2703893 | 1405 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1406 | `		/* Empty interface container */` |
|  228987 | 1407 | `		return FALSE;` |
|       - | 1408 | `	}` |
|       - | 1409 | `	/* Point to the set of implemented interfaces */` |
| 2474911 | 1410 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1411 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1412 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2488659 | 1413 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2479787 | 1414 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2466039 | 1415 | `			return TRUE;` |
|       - | 1416 | `		}` |
|    6879 | 1417 | `	}` |
|    8877 | 1418 | `	return FALSE;` |
| 1351949 | 1419 | `}` |
|       - | 1420 | `/*` |
|       - | 1421 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1422 | ` * is an instance of the main class (second argument).` |
|       - | 1423 | ` * Otherwise FALSE is returned.` |
|       - | 1424 | ` */` |
| 4017390 | 1425 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1426 | `{` |
|       - | 1427 | `	ph7_class *pParent;` |
|       - | 1428 | `	sxi32 rc;` |
| 4017395 | 1429 | `	if( pThis == pClass ){` |
|       - | 1430 | `		/* Instance of the same class */` |
| 1430275 | 1431 | `		return TRUE;` |
|       - | 1432 | `	}` |
|       - | 1433 | `	/* Check implemented interfaces */` |
| 2587125 | 1434 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2587125 | 1435 | `	if( rc ){` |
| 2354875 | 1436 | `		return TRUE;` |
|       - | 1437 | `	}` |
|       - | 1438 | `	/* Check parent classes */` |
|  232255 | 1439 | `	pParent = pThis->pBase;` |
|  237835 | 1440 | `	while( pParent ){` |
|  117267 | 1441 | `		if( pParent == pClass ){` |
|       - | 1442 | `			/* Same instance */` |
|     533 | 1443 | `			return TRUE;` |
|       - | 1444 | `		}` |
|       - | 1445 | `		/* Check the implemented interfaces */` |
|  116739 | 1446 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  116739 | 1447 | `		if( rc ){` |
|  111159 | 1448 | `			return TRUE;` |
|       - | 1449 | `		}` |
|       - | 1450 | `		/* Point to the parent class */` |
|    5585 | 1451 | `		pParent = pParent->pBase;` |
|       5 | 1452 | `	}` |
|       - | 1453 | `	/* Not an instance of the the given class */` |
|  120573 | 1454 | `	return FALSE;` |
| 2008700 | 1455 | `}` |
|       - | 1456 | `/*` |
|       - | 1457 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1458 | ` * is a subclass of the main class (second argument).` |
|       - | 1459 | ` * Otherwise FALSE is returned.` |
|       - | 1460 | ` */` |
|      36 | 1461 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       3 | 1462 | `{` |
|       - | 1463 | `	SyHashEntry *pEntry;` |
|       - | 1464 | `	SyString *pName;` |
|      63 | 1465 | `	while( pClass ){` |
|      59 | 1466 | `		pName = &pClass->sName;` |
|       - | 1467 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      59 | 1468 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      59 | 1469 | `		if( pEntry ){` |
|      25 | 1470 | `			return TRUE;` |
|       - | 1471 | `		}` |
|       - | 1472 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1473 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1474 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1475 | `		 * class's aInterface, missing inherited interfaces. */` |
|      35 | 1476 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1477 | `			return TRUE;` |
|       - | 1478 | `		}` |
|      25 | 1479 | `		pClass = pClass->pBase;` |
|       1 | 1480 | `	}` |
|       - | 1481 | `	/* Not a subclass */` |
|       5 | 1482 | `	return FALSE;` |
|      21 | 1483 | `}` |
|       - | 1484 | `/*` |
|       - | 1485 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1486 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1487 | ` *   as one of its parents.` |
|       - | 1488 | ` * Parameters` |
|       - | 1489 | ` *  object_or_class` |
|       - | 1490 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1491 | ` * class` |
|       - | 1492 | ` *  The class or interface name to test against.` |
|       - | 1493 | ` * allow_string` |
|       - | 1494 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1495 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1496 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1497 | ` * Return` |
|       - | 1498 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1499 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1500 | ` */` |
|      32 | 1501 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1502 | `{` |
|      34 | 1503 | `	int res = 0; /* Assume FALSE by default */` |
|      34 | 1504 | `	if( nArg > 1 ){` |
|      34 | 1505 | `		ph7_class *pThisClass = 0;` |
|      34 | 1506 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1507 | `			/* An object first argument: allow_string is ignored (php). */` |
|      16 | 1508 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      26 | 1509 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1510 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1511 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1512 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1513 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      15 | 1514 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       7 | 1515 | `		}` |
|      34 | 1516 | `		if( pThisClass ){` |
|       - | 1517 | `			/* Extract the given class */` |
|      28 | 1518 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      28 | 1519 | `			if( pClass ){` |
|       - | 1520 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1521 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      28 | 1522 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      13 | 1523 | `			}` |
|      13 | 1524 | `		}` |
|      16 | 1525 | `	}` |
|       - | 1526 | `	/* Query result */` |
|      34 | 1527 | `	ph7_result_bool(pCtx,res);` |
|      34 | 1528 | `	return PH7_OK;` |
|       2 | 1529 | `}` |
|       - | 1530 | `/*` |
|       - | 1531 | ` * int spl_object_id(object $object)` |
|       - | 1532 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1533 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1534 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1535 | ` */` |
|      18 | 1536 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1537 | `{` |
|       - | 1538 | `	ph7_class_instance *pThis;` |
|      21 | 1539 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1540 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1541 | `		return PH7_OK;` |
|       - | 1542 | `	}` |
|      21 | 1543 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      21 | 1544 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      21 | 1545 | `	return PH7_OK;` |
|      12 | 1546 | `}` |
|       - | 1547 | `/*` |
|       - | 1548 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 1549 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 1550 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 1551 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 1552 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 1553 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 1554 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 1555 | ` *` |
|       - | 1556 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 1557 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 1558 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 1559 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 1560 | ` */` |
|      48 | 1561 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1562 | `{` |
|      49 | 1563 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1564 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1565 | `	char zGiven[64];` |
|       - | 1566 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 1567 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      49 | 1568 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1569 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1570 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 1571 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 1572 | `	}` |
|      49 | 1573 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1574 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 1575 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 1576 | `	 * resource, and Generator/Fiber. */` |
|      48 | 1577 | `	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM\|PH7_CLASS_NOCLONE))` |
|      48 | 1578 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      10 | 1579 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       6 | 1580 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sName);` |
|       - | 1581 | `	}` |
|      43 | 1582 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 1583 | `	if( pClone == 0 ){` |
|     ! 0 | 1584 | `		return PH7_VmMemoryError(pVm);` |
|       - | 1585 | `	}` |
|       - | 1586 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 1587 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 1588 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 1589 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 1590 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 1591 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 1592 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 1593 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 1594 | `		sxu32 n;` |
|      53 | 1595 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 1596 | `			ph7_value *pVal,sVal;` |
|       - | 1597 | `			const char *zName;` |
|       - | 1598 | `			sxu32 nName;` |
|       - | 1599 | `			char zKeyBuf[64];` |
|       - | 1600 | `			sxi32 rc;` |
|      31 | 1601 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 1602 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 1603 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 1604 | `				zName = zKeyBuf;` |
|     ! 0 | 1605 | `			}else{` |
|      31 | 1606 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 1607 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 1608 | `			}` |
|      31 | 1609 | `			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 1610 | `			if( pVal ){` |
|       - | 1611 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 1612 | `				 * property, whose slot reservation can reallocate pVm->aMemObj and` |
|       - | 1613 | `				 * dangle pVal (a pointer into it). */` |
|      31 | 1614 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 1615 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 1616 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 1617 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 1618 | `				if( rc != SXRET_OK ){` |
|       7 | 1619 | `					return rc;` |
|       - | 1620 | `				}` |
|      12 | 1621 | `			}` |
|      25 | 1622 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 1623 | `		}` |
|      11 | 1624 | `	}` |
|      37 | 1625 | `	return PH7_OK;` |
|      25 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * string spl_object_hash(object $object)` |
|       - | 1629 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 1630 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 1631 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 1632 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 1633 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 1634 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 1635 | ` */` |
|      18 | 1636 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1637 | `{` |
|       - | 1638 | `	ph7_class_instance *pThis;` |
|      20 | 1639 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1640 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1641 | `		return PH7_OK;` |
|       - | 1642 | `	}` |
|      20 | 1643 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 1644 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 1645 | `	return PH7_OK;` |
|      11 | 1646 | `}` |
|       - | 1647 | `/*` |
|       - | 1648 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 1649 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 1650 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 1651 | ` * Parameters` |
|       - | 1652 | ` *  object_or_class` |
|       - | 1653 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 1654 | ` * class` |
|       - | 1655 | ` *  The class or interface name to test against.` |
|       - | 1656 | ` * allow_string` |
|       - | 1657 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 1658 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 1659 | ` *  object first argument (php).` |
|       - | 1660 | ` * Return` |
|       - | 1661 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 1662 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 1663 | ` */` |
|      48 | 1664 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1665 | `{` |
|      51 | 1666 | `	int res = 0; /* Assume FALSE by default */` |
|      51 | 1667 | `	if( nArg > 1 ){` |
|      51 | 1668 | `		ph7_class *pClass = 0;` |
|      51 | 1669 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1670 | `			/* An object first argument: allow_string is ignored (php). */` |
|      21 | 1671 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      42 | 1672 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 1673 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 1674 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 1675 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 1676 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      25 | 1677 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      11 | 1678 | `		}` |
|      51 | 1679 | `		if( pClass ){` |
|       - | 1680 | `			/* Extract the target class */` |
|      41 | 1681 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      41 | 1682 | `			if( pMain ){` |
|       - | 1683 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      39 | 1684 | `				res = VmSubclassOf(pClass,pMain);` |
|      18 | 1685 | `			}` |
|      19 | 1686 | `		}` |
|      24 | 1687 | `	}` |
|       - | 1688 | `	/* Query result */` |
|      51 | 1689 | `	ph7_result_bool(pCtx,res);` |
|      51 | 1690 | `	return PH7_OK;` |
|       3 | 1691 | `}` |
|     208 | 1692 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1693 | `{` |
|       - | 1694 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 1695 | `	sxi32 rc;` |
|     211 | 1696 | `	if( nArg < 1 ){` |
|       - | 1697 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1698 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1699 | `		return PH7_OK;` |
|       - | 1700 | `	}` |
|       - | 1701 | `	{` |
|       - | 1702 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 1703 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     211 | 1704 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     211 | 1705 | `		if( rcCb != PH7_OK ){` |
|      61 | 1706 | `			return rcCb;` |
|       - | 1707 | `		}` |
|       - | 1708 | `	}` |
|     151 | 1709 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     151 | 1710 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1711 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     151 | 1712 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 1713 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 1714 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 1715 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 1716 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 1717 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 1718 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     161 | 1719 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      21 | 1720 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 1721 | `		VmCallArgMap sInner;` |
|       - | 1722 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 1723 | `		 * unset when forwarded, not as stack garbage. */` |
|      21 | 1724 | `		SyZero(&sInner,sizeof(sInner));` |
|      21 | 1725 | `		sInner.bHasNamed = 1;` |
|      21 | 1726 | `		sInner.bIsNamespaced = 0;` |
|       - | 1727 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 1728 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 1729 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 1730 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      21 | 1731 | `		sInner.bStrict = 0;` |
|      21 | 1732 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      21 | 1733 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|      21 | 1734 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      11 | 1735 | `	}else{` |
|       - | 1736 | `		/* call_user_func is one of php's two FORWARDS: the callback binds under the` |
|       - | 1737 | `		 * mode of the file that wrote the call_user_func, not weakly like every other` |
|       - | 1738 | `		 * internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 1739 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 1740 | `		 * other internal invocation, and wrong here). */` |
|       - | 1741 | `		VmCallArgMap sFwd;` |
|     131 | 1742 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     131 | 1743 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     131 | 1744 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 1745 | `	}` |
|     151 | 1746 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1747 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 1748 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      18 | 1749 | `		PH7_MemObjRelease(&sResult);` |
|      18 | 1750 | `		return PH7_EXCEPTION;` |
|       - | 1751 | `	}` |
|     134 | 1752 | `	if( rc != SXRET_OK ){` |
|       - | 1753 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1754 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1755 | `	}else{` |
|       - | 1756 | `		/* Callback result */` |
|     134 | 1757 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1758 | `	}` |
|     134 | 1759 | `	PH7_MemObjRelease(&sResult);` |
|     134 | 1760 | `	return PH7_OK;` |
|     107 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 1764 | ` *  Call a callback with an array of parameters.` |
|       - | 1765 | ` * Parameter` |
|       - | 1766 | ` *  $callback` |
|       - | 1767 | ` *   The callable to be called.` |
|       - | 1768 | ` * $param_arr` |
|       - | 1769 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 1770 | ` * Return` |
|       - | 1771 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 1772 | ` */` |
|     198 | 1773 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1774 | `{` |
|       - | 1775 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 1776 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 1777 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 1778 | `	SySet aArg;               /* Argument value pointers */` |
|     200 | 1779 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     200 | 1780 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     200 | 1781 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 1782 | `	sxi32 rc;` |
|       - | 1783 | `	sxu32 n;` |
|     200 | 1784 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1785 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1786 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1787 | `		return PH7_OK;` |
|       - | 1788 | `	}` |
|       - | 1789 | `	{` |
|     200 | 1790 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     200 | 1791 | `		if( rcCb != PH7_OK ){` |
|       5 | 1792 | `			return rcCb;` |
|       - | 1793 | `		}` |
|       - | 1794 | `	}` |
|     196 | 1795 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     196 | 1796 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1797 | `	/* Initialize the arguments container */` |
|     196 | 1798 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1799 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 1800 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 1801 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 1802 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 1803 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 1804 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     196 | 1805 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     196 | 1806 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     582 | 1807 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1808 | `		/* Extract node value */` |
|     388 | 1809 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     388 | 1810 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      29 | 1811 | `				if( aNames == 0 ){` |
|       - | 1812 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 1813 | `					 * not-yet-seen slot defaults to positional. */` |
|      19 | 1814 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      19 | 1815 | `					if( aNames == 0 ){` |
|     ! 0 | 1816 | `						SySetRelease(&aArg);` |
|     ! 0 | 1817 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1818 | `						if( apNode ){` |
|     ! 0 | 1819 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 1820 | `						}` |
|     ! 0 | 1821 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 1822 | `					}` |
|      19 | 1823 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|       9 | 1824 | `				}` |
|      29 | 1825 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      14 | 1826 | `			}` |
|     388 | 1827 | `			if( apNode == 0 ){` |
|     227 | 1828 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     150 | 1829 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     152 | 1830 | `				if( apNode ){` |
|     152 | 1831 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|      75 | 1832 | `				}` |
|      75 | 1833 | `			}` |
|     388 | 1834 | `			if( apNode ){` |
|     388 | 1835 | `				apNode[nSlot] = pEntry;` |
|     193 | 1836 | `			}` |
|     388 | 1837 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     388 | 1838 | `			nSlot++;` |
|     193 | 1839 | `		}` |
|       - | 1840 | `		/* Point to the next entry */` |
|     388 | 1841 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     195 | 1842 | `	}` |
|       - | 1843 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 1844 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 1845 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 1846 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     196 | 1847 | `	if( apNode ){` |
|     152 | 1848 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     152 | 1849 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     152 | 1850 | `		apNode = 0;` |
|      75 | 1851 | `	}` |
|       - | 1852 | `	/* Try to invoke the callback */` |
|     196 | 1853 | `	if( aNames ){` |
|       - | 1854 | `		VmCallArgMap sMap;` |
|      19 | 1855 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      19 | 1856 | `		sMap.bHasNamed = 1;` |
|      19 | 1857 | `		sMap.bIsNamespaced = 0;` |
|       - | 1858 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 1859 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      19 | 1860 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      19 | 1861 | `		sMap.nTotal = nSlot;` |
|      19 | 1862 | `		sMap.aNames = aNames;` |
|      28 | 1863 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      18 | 1864 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      19 | 1865 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      10 | 1866 | `	}else{` |
|       - | 1867 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 1868 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 1869 | `		VmCallArgMap sFwd;` |
|     178 | 1870 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     178 | 1871 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     266 | 1872 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     176 | 1873 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 1874 | `	}` |
|     196 | 1875 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1876 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     113 | 1877 | `		PH7_MemObjRelease(&sResult);` |
|     113 | 1878 | `		SySetRelease(&aArg);` |
|     113 | 1879 | `		return PH7_EXCEPTION;` |
|       - | 1880 | `	}` |
|      84 | 1881 | `	if( rc != SXRET_OK ){` |
|       - | 1882 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1883 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1884 | `	}else{` |
|       - | 1885 | `		/* Callback result */` |
|      84 | 1886 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1887 | `	}` |
|       - | 1888 | `	/* Cleanup the mess left behind */` |
|      84 | 1889 | `	PH7_MemObjRelease(&sResult);` |
|      84 | 1890 | `	SySetRelease(&aArg);` |
|      84 | 1891 | `	return PH7_OK;` |
|     101 | 1892 | `}` |
|       - | 1893 |  |
