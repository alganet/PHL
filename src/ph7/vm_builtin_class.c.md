# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 901/1010 lines (89.21%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|    3876 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|    3881 |   11 | `	if( nArg < 1 ){` |
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
|    3881 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    3881 |   25 | `		if( pClass ){` |
|    3881 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|    3881 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    1943 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|    3881 |   34 | `	return PH7_OK;` |
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
|  207252 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  207257 |  115 | `	ph7_class *pClass = 0;` |
|  207257 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  204177 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  105171 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    3081 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    3081 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    3079 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    1537 |  135 | `		}` |
|    1538 |  136 | `	}` |
|  207257 |  137 | `	return pClass;` |
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
|      58 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  151 | `{` |
|      61 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|      61 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|      58 |  155 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      44 |  156 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
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
|      59 |  170 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      59 |  171 | `		if( pClass ){` |
|       - |  172 | `			const char *zName;` |
|       - |  173 | `			int nLen;` |
|       - |  174 | `			/* Extract attribute name */` |
|      59 |  175 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      59 |  176 | `			if( nLen > 0 ){` |
|       - |  177 | `				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this` |
|       - |  178 | ``				 * name is not a property (`property_exists('C','someMethod')` is false),`` |
|       - |  179 | `				 * and neither is a class CONSTANT. PHL searched the method table too and` |
|       - |  180 | `				 * answered true for both. */` |
|      59 |  181 | `				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);` |
|      59 |  182 | `				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;` |
|      59 |  183 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - |  184 | `					/* A base's PRIVATE property is invisible to the child it was asked` |
|       - |  185 | ``					 * about, php's `property_info->ce == ce` rule: PHL copies one down`` |
|       - |  186 | `					 * onto every child (its own methods read it through $this), so the` |
|       - |  187 | `					 * table alone said true where php says false. Static or instance,` |
|       - |  188 | `					 * the rule is the same; protected and public are inherited outright.` |
|       - |  189 | `					 * A trait's property belongs to the class that COMPOSED it. */` |
|      37 |  190 | `					res = 1;` |
|      34 |  191 | `					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      25 |  192 | `					 && pAttr->pDeclClass != 0` |
|      19 |  193 | `					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){` |
|       9 |  194 | `						res = 0;` |
|       4 |  195 | `					}` |
|      17 |  196 | `				}` |
|       - |  197 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  198 | `				 * attribute table, not the class's — php reports those too` |
|       - |  199 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|      59 |  200 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
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
|      28 |  213 | `			}` |
|      28 |  214 | `		}` |
|      28 |  215 | `	}` |
|      59 |  216 | `	ph7_result_bool(pCtx,res);` |
|      59 |  217 | `	return PH7_OK;` |
|      32 |  218 | `}` |
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
|       3 |  231 | `{` |
|      53 |  232 | `	int res = 0; /* Assume method does not exists */` |
|      53 |  233 | `	if( nArg > 1 ){` |
|       - |  234 | `		ph7_class *pClass;` |
|      50 |  235 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      29 |  236 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
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
|      51 |  250 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      51 |  251 | `		if( pClass ){` |
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
|      51 |  279 | `	ph7_result_bool(pCtx,res);` |
|      51 |  280 | `	return PH7_OK;` |
|      28 |  281 | `}` |
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
|     108 |  294 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  295 | `{` |
|     113 |  296 | `	int res = 0; /* Assume class does not exist */` |
|     113 |  297 | `	if( nArg > 0 ){` |
|     113 |  298 | `		SyHashEntry *pEntry = 0;` |
|       - |  299 | `		const char *zName;` |
|       - |  300 | `		int nLen;` |
|     113 |  301 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  302 | `		sxu32 nName;` |
|       - |  303 | `		/* Extract given name */` |
|     113 |  304 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     113 |  305 | `		if( nArg >= 2 ){` |
|       6 |  306 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  307 | `		}` |
|       - |  308 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  309 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     113 |  310 | `		nName = (sxu32)nLen;` |
|     113 |  311 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     113 |  312 | `		if( nName > 0 ){` |
|       - |  313 | `			/* Perform a hash lookup first */` |
|     109 |  314 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      52 |  315 | `		}` |
|       - |  316 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|     113 |  317 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  318 | `			/* Try autoload, then re-check */` |
|      27 |  319 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      27 |  320 | `			if( pClass ){` |
|       9 |  321 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  322 | `			}` |
|      11 |  323 | `		}` |
|     113 |  324 | `		if( pEntry ){` |
|       - |  325 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  326 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|      91 |  327 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      95 |  328 | `			while( pClass ){` |
|      91 |  329 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|      87 |  330 | `					res = 1;` |
|      87 |  331 | `					break;` |
|       - |  332 | `				}` |
|       5 |  333 | `				pClass = pClass->pNextName;` |
|       1 |  334 | `			}` |
|      43 |  335 | `		}` |
|      54 |  336 | `	}` |
|     113 |  337 | `	ph7_result_bool(pCtx,res);` |
|     113 |  338 | `	return PH7_OK;` |
|       5 |  339 | `}` |
|       - |  340 | `/*` |
|       - |  341 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  342 | ` *   Checks if the interface has been defined.` |
|       - |  343 | ` * Parameters` |
|       - |  344 | ` *  class_name` |
|       - |  345 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  346 | ` *   unlinke the standard PHP engine.` |
|       - |  347 | ` *  autoload` |
|       - |  348 | ` *   Whether or not to call __autoload by default.` |
|       - |  349 | ` * Return` |
|       - |  350 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  351 | ` */` |
|      38 |  352 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  353 | `{` |
|      42 |  354 | `	int res = 0; /* Assume interface does not exist */` |
|      42 |  355 | `	if( nArg > 0 ){` |
|      42 |  356 | `		SyHashEntry *pEntry = 0;` |
|       - |  357 | `		const char *zName;` |
|       - |  358 | `		int nLen;` |
|      42 |  359 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  360 | `		sxu32 nName;` |
|       - |  361 | `		/* Extract given name */` |
|      42 |  362 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      42 |  363 | `		if( nArg >= 2 ){` |
|     ! 0 |  364 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     ! 0 |  365 | `		}` |
|       - |  366 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  367 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      42 |  368 | `		nName = (sxu32)nLen;` |
|      42 |  369 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  370 | `		/* Perform a hash lookup */` |
|      42 |  371 | `		if( nName > 0 ){` |
|      39 |  372 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      18 |  373 | `		}` |
|       - |  374 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      42 |  375 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  376 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  377 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  378 | `			if( pClass ){` |
|     ! 0 |  379 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  380 | `			}` |
|       1 |  381 | `		}` |
|      42 |  382 | `		if( pEntry ){` |
|      37 |  383 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      39 |  384 | `			while( pClass ){` |
|      37 |  385 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  386 | `					/* interface is available */` |
|      35 |  387 | `					res = 1;` |
|      35 |  388 | `					break;` |
|       - |  389 | `				}` |
|       - |  390 | `				/* Next with the same name */` |
|       3 |  391 | `				pClass = pClass->pNextName;` |
|       1 |  392 | `			}` |
|      17 |  393 | `		}` |
|      19 |  394 | `	}` |
|      42 |  395 | `	ph7_result_bool(pCtx,res);` |
|      42 |  396 | `	return PH7_OK;` |
|       4 |  397 | `}` |
|       - |  398 | `/*` |
|       - |  399 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  400 | ` *   Checks if the trait has been defined.` |
|       - |  401 | ` * Parameters` |
|       - |  402 | ` *  trait` |
|       - |  403 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  404 | ` *  autoload` |
|       - |  405 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  406 | ` * Return` |
|       - |  407 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  408 | ` */` |
|      18 |  409 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  410 | `{` |
|      21 |  411 | `	int res = 0; /* Assume trait does not exist */` |
|      21 |  412 | `	if( nArg > 0 ){` |
|      21 |  413 | `		SyHashEntry *pEntry = 0;` |
|       - |  414 | `		const char *zName;` |
|       - |  415 | `		int nLen;` |
|      21 |  416 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  417 | `		sxu32 nName;` |
|       - |  418 | `		/* Extract given name */` |
|      21 |  419 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      21 |  420 | `		if( nArg >= 2 ){` |
|       3 |  421 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       1 |  422 | `		}` |
|       - |  423 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  424 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      21 |  425 | `		nName = (sxu32)nLen;` |
|      21 |  426 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  427 | `		/* Perform a hash lookup */` |
|      21 |  428 | `		if( nName > 0 ){` |
|      18 |  429 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       8 |  430 | `		}` |
|       - |  431 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      21 |  432 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  433 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       5 |  434 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       5 |  435 | `			if( pClass ){` |
|     ! 0 |  436 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  437 | `			}` |
|       2 |  438 | `		}` |
|      21 |  439 | `		if( pEntry ){` |
|      12 |  440 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  441 | `			while( pClass ){` |
|      12 |  442 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  443 | `					/* trait is available */` |
|       6 |  444 | `					res = 1;` |
|       6 |  445 | `					break;` |
|       - |  446 | `				}` |
|       - |  447 | `				/* Next with the same name */` |
|       7 |  448 | `				pClass = pClass->pNextName;` |
|       1 |  449 | `			}` |
|       5 |  450 | `		}` |
|       9 |  451 | `	}` |
|      21 |  452 | `	ph7_result_bool(pCtx,res);` |
|      21 |  453 | `	return PH7_OK;` |
|       3 |  454 | `}` |
|       - |  455 | `/*` |
|       - |  456 | ` * bool class_alias([string $original[,string $alias ]])` |
|       - |  457 | ` *   Creates an alias for a class.` |
|       - |  458 | ` * Parameters` |
|       - |  459 | ` *  original` |
|       - |  460 | ` *    The original class.` |
|       - |  461 | ` *  alias` |
|       - |  462 | ` *   The alias name for the class.` |
|       - |  463 | ` * Return` |
|       - |  464 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  465 | ` */` |
|      10 |  466 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  467 | `{` |
|       - |  468 | `	const char *zOld,*zNew;` |
|       - |  469 | `	int nOldLen,nNewLen;` |
|       - |  470 | `	sxu32 nOld,nNew;` |
|       - |  471 | `	SyHashEntry *pEntry;` |
|       - |  472 | `	ph7_class *pClass;` |
|       - |  473 | `	char *zDup;` |
|       - |  474 | `	sxi32 rc;` |
|      13 |  475 | `	if( nArg < 2 ){` |
|       - |  476 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  477 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  478 | `		return PH7_OK;` |
|       - |  479 | `	}` |
|       - |  480 | `	/* Extract old class name */` |
|      13 |  481 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  482 | `	/* Extract alias name */` |
|      13 |  483 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|       - |  484 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  485 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  486 | `	 * on the plain name then matches. */` |
|      13 |  487 | `	nOld = (sxu32)nOldLen;` |
|      13 |  488 | `	nNew = (sxu32)nNewLen;` |
|      13 |  489 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|      13 |  490 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|      13 |  491 | `	if( nNew < 1 ){` |
|       - |  492 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  493 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  494 | `		return PH7_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Perform a hash lookup */` |
|      13 |  497 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|      13 |  498 | `	if( pEntry ==  0 ){` |
|       - |  499 | `		/* No such class,return FALSE */` |
|     ! 0 |  500 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  501 | `		return PH7_OK;` |
|       - |  502 | `	}` |
|       - |  503 | `	/* Point to the class */` |
|      13 |  504 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  505 | `	/* Duplicate alias name */` |
|      13 |  506 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|      13 |  507 | `	if( zDup == 0 ){` |
|       - |  508 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  509 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  510 | `		return PH7_OK;` |
|       - |  511 | `	}` |
|       - |  512 | `	/* Create the alias */` |
|      13 |  513 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|      13 |  514 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  515 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  516 | `	}` |
|      13 |  517 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      13 |  518 | `	return PH7_OK;` |
|       8 |  519 | `}` |
|       - |  520 | `/*` |
|       - |  521 | ` * The three KINDS hClass holds. php keeps classes, interfaces, traits and enums in one` |
|       - |  522 | ` * table too and each of its three list builtins filters that table down to its own kind:` |
|       - |  523 | `` * an ENUM is a class (`get_declared_classes()` reports it), an interface and a trait are`` |
|       - |  524 | ` * not.` |
|       - |  525 | ` */` |
|       - |  526 | `#define VM_DECLARED_CLASS      0` |
|       - |  527 | `#define VM_DECLARED_INTERFACE  1` |
|       - |  528 | `#define VM_DECLARED_TRAIT      2` |
|       - |  529 |  |
|    2772 |  530 | `static int VmDeclaredEntryKind(ph7_class *pClass)` |
|       2 |  531 | `{` |
|    2774 |  532 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     344 |  533 | `		return VM_DECLARED_INTERFACE;` |
|       - |  534 | `	}` |
|    2432 |  535 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      24 |  536 | `		return VM_DECLARED_TRAIT;` |
|       - |  537 | `	}` |
|    2410 |  538 | `	return VM_DECLARED_CLASS;` |
|    1388 |  539 | `}` |
|       - |  540 | `struct VmDeclaredList {` |
|       - |  541 | `	int iKind;           /* Which VM_DECLARED_* kind this list wants */` |
|       - |  542 | `	ph7_value *pArray;   /* The array being built */` |
|       - |  543 | `	ph7_value *pName;    /* Scratch name */` |
|       - |  544 | `};` |
|       - |  545 | `/*` |
|       - |  546 | ` * One row of a get_declared_*() answer.` |
|       - |  547 | ` *` |
|       - |  548 | ` * The NAME reported is the table KEY, not the class struct's own name — a distinction` |
|       - |  549 | `` * only `class_alias()` makes visible, since it puts a second key over the same class.`` |
|       - |  550 | ` * php reports the class's declared spelling for the key that IS its name and the ALIAS` |
|       - |  551 | `` * for the other, so `class_alias('C1','C1Alias')` answers both `C1` and `c1alias`,`` |
|       - |  552 | ` * lower-cased because that is the spelling php's own alias key is stored under. PHL` |
|       - |  553 | ` * keeps the declared spelling in every key, so the fold is applied here.` |
|       - |  554 | ` */` |
|    2772 |  555 | `static sxi32 VmDeclaredNameStep(SyHashEntry *pEntry,void *pUserData)` |
|       2 |  556 | `{` |
|    2774 |  557 | `	struct VmDeclaredList *pList = (struct VmDeclaredList *)pUserData;` |
|    2774 |  558 | `	ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    2774 |  559 | `	SyString *pDecl = &pClass->sName;` |
|    2774 |  560 | `	if( VmDeclaredEntryKind(pClass) != pList->iKind ){` |
|    1590 |  561 | `		return SXRET_OK;` |
|       - |  562 | `	}` |
|    1184 |  563 | `	if( pEntry->nKeyLen == pDecl->nByte` |
|    1181 |  564 | `		&& SyStrnmicmp((const char *)pEntry->pKey,pDecl->zString,pEntry->nKeyLen) == 0 ){` |
|       - |  565 | `		/* The key this class was DECLARED under */` |
|    1176 |  566 | `		ph7_value_string(pList->pName,pDecl->zString,(int)pDecl->nByte);` |
|     589 |  567 | `	}else{` |
|       - |  568 | `		/* A class_alias() key: php reports it folded */` |
|      12 |  569 | `		const char *zKey = (const char *)pEntry->pKey;` |
|       - |  570 | `		sxu32 n;` |
|     174 |  571 | `		for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|     164 |  572 | `			char c = (char)SyToLower(zKey[n]);` |
|     164 |  573 | `			ph7_value_string(pList->pName,&c,1);` |
|      83 |  574 | `		}` |
|       - |  575 | `	}` |
|    1186 |  576 | `	ph7_array_add_elem(pList->pArray,0/*Automatic index assign*/,pList->pName); /* Will make it's own copy */` |
|    1186 |  577 | `	ph7_value_reset_string_cursor(pList->pName);` |
|    1186 |  578 | `	return SXRET_OK;` |
|    1388 |  579 | `}` |
|       - |  580 |  |
|       - |  581 | `/*` |
|       - |  582 | ` * Build php's get_declared_classes()/get_declared_interfaces()/get_declared_traits()` |
|       - |  583 | ` * answer for one kind.` |
|       - |  584 | ` */` |
|      14 |  585 | `static int VmDeclaredNameList(ph7_context *pCtx,int iKind)` |
|       2 |  586 | `{` |
|       - |  587 | `	struct VmDeclaredList sList;` |
|       - |  588 | `	/* Create a new array first */` |
|      16 |  589 | `	sList.iKind = iKind;` |
|      16 |  590 | `	sList.pArray = ph7_context_new_array(pCtx);` |
|      16 |  591 | `	sList.pName = ph7_context_new_scalar(pCtx);` |
|      16 |  592 | `	if( sList.pArray == 0 \|\| sList.pName == 0 ){` |
|       - |  593 | `		/* Out of memory,return NULL */` |
|     ! 0 |  594 | `		ph7_result_null(pCtx);` |
|     ! 0 |  595 | `		return PH7_OK;` |
|       - |  596 | `	}` |
|       - |  597 | `	/* hClass is head-pushed, so its forward order is reverse-insertion; php reports` |
|       - |  598 | `	 * these lists in DECLARATION order (its own class table is append-ordered), which` |
|       - |  599 | `	 * is what the backward walk yields. */` |
|      16 |  600 | `	SyHashForEachReverse(&pCtx->pVm->hClass,VmDeclaredNameStep,(void *)&sList);` |
|       - |  601 | `	/* Return the created array */` |
|      16 |  602 | `	ph7_result_value(pCtx,sList.pArray);` |
|      16 |  603 | `	return PH7_OK;` |
|       9 |  604 | `}` |
|       - |  605 | `/*` |
|       - |  606 | ` * array get_declared_classes(void)` |
|       - |  607 | ` *   Returns an array with the name of the defined classes` |
|       - |  608 | ` * Parameters` |
|       - |  609 | ` *  None` |
|       - |  610 | ` * Return` |
|       - |  611 | ` *   Returns an array of the names of the declared classes` |
|       - |  612 | ` *   in the current script.` |
|       - |  613 | ` * Note:` |
|       - |  614 | ` *   NULL is returned on failure.` |
|       - |  615 | ` */` |
|       6 |  616 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  617 | `{` |
|       3 |  618 | `	SXUNUSED(nArg); /* cc warning */` |
|       3 |  619 | `	SXUNUSED(apArg);` |
|       8 |  620 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_CLASS);` |
|       2 |  621 | `}` |
|       - |  622 | `/*` |
|       - |  623 | ` * array get_declared_interfaces(void)` |
|       - |  624 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  625 | ` * Parameters` |
|       - |  626 | ` *  None` |
|       - |  627 | ` * Return` |
|       - |  628 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  629 | ` *   in the current script.` |
|       - |  630 | ` * Note:` |
|       - |  631 | ` *   NULL is returned on failure.` |
|       - |  632 | ` */` |
|       4 |  633 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  634 | `{` |
|       2 |  635 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  636 | `	SXUNUSED(apArg);` |
|       6 |  637 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_INTERFACE);` |
|       2 |  638 | `}` |
|       - |  639 | `/*` |
|       - |  640 | ` * array get_declared_traits(void)` |
|       - |  641 | ` *   Returns an array with the name of the defined traits.` |
|       - |  642 | ` */` |
|       4 |  643 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  644 | `{` |
|       2 |  645 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  646 | `	SXUNUSED(apArg);` |
|       6 |  647 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_TRAIT);` |
|       2 |  648 | `}` |
|       - |  649 | `/*` |
|       - |  650 | ` * Does this method-table entry answer to the method's OWN name (rather than to an` |
|       - |  651 | ` * adaptation alias made from it)? Method names fold case, so the comparison does too.` |
|       - |  652 | ` */` |
|     122 |  653 | `static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  654 | `{` |
|     183 |  655 | `	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte` |
|     122 |  656 | `		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;` |
|       2 |  657 | `}` |
|       - |  658 | `/*` |
|       - |  659 | ` * Which inheritance LEVEL does this method-table entry belong to — the class php would` |
|       - |  660 | ` * have added it under? A method declared in a class body is its own; a trait's is the` |
|       - |  661 | ` * class that COMPOSED it, which is two different questions depending on the entry. An` |
|       - |  662 | ` * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still` |
|       - |  663 | `` * holding this key over this very struct is the one whose `use` block wrote it. A trait`` |
|       - |  664 | ` * method under its own name is the same struct in every class that uses the trait, and` |
|       - |  665 | ` * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait` |
|       - |  666 | ` * composes its own copy, and the parent's inherited entry never replaces it.` |
|       - |  667 | ` */` |
|     392 |  668 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       3 |  669 | `{` |
|     395 |  670 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|     395 |  671 | `	ph7_class *pWalk,*pHigh = 0;` |
|     395 |  672 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     338 |  673 | `		return pDecl;` |
|       - |  674 | `	}` |
|      58 |  675 | `	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){` |
|      62 |  676 | `		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 |  677 | `			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);` |
|      38 |  678 | `			if( pE && pE->pUserData == (void *)pMeth ){` |
|      34 |  679 | `				pHigh = pWalk;` |
|      16 |  680 | `			}` |
|      20 |  681 | `		}` |
|      26 |  682 | `		if( pHigh ){` |
|      26 |  683 | `			return pHigh;` |
|       - |  684 | `		}` |
|     ! 0 |  685 | `	}` |
|      34 |  686 | `	return PH7_VmComposingClass(pClass,pDecl);` |
|     199 |  687 | `}` |
|       - |  688 | `/*` |
|       - |  689 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  690 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  691 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  692 | ` */` |
|     226 |  693 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       3 |  694 | `{` |
|     229 |  695 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|     229 |  696 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     229 |  697 | `	ph7_value_reset_string_cursor(pName);` |
|     229 |  698 | `}` |
|       - |  699 | `/*` |
|       - |  700 | ` * array get_class_methods(object\|string $object_or_class)` |
|       - |  701 | ` *   Returns an array with the names of the class methods the CALLING SCOPE can reach,` |
|       - |  702 | ` *   in php's order: each class's own body methods, then its trait composition, then the` |
|       - |  703 | ` *   same again for every ancestor.` |
|       - |  704 | ` * Parameters` |
|       - |  705 | ` *  object_or_class` |
|       - |  706 | ` *   The class name or a class instance. Anything that does not resolve to a class is a` |
|       - |  707 | ` *   TypeError naming the type given — this builtin never answers NULL.` |
|       - |  708 | ` */` |
|      48 |  709 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  710 | `{` |
|       - |  711 | `	ph7_value *pName,*pArray;` |
|       - |  712 | `	SyHashEntry *pEntry;` |
|       - |  713 | `	ph7_class *pClass;` |
|       - |  714 | `	/* Extract the target class first */` |
|      51 |  715 | `	pClass = 0;` |
|      51 |  716 | `	if( nArg > 0 ){` |
|      51 |  717 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      24 |  718 | `	}` |
|      51 |  719 | `	if( pClass == 0 ){` |
|       - |  720 | `		/* php screens the VALUE, not the type: anything that does not resolve to a` |
|       - |  721 | `		 * class — a name nothing declares, an int, an array, null — is ONE TypeError` |
|       - |  722 | `		 * naming the type given. PHL answered NULL for most of them (and the shared` |
|       - |  723 | ``		 * ZPP screen's `must be of type object\|string` for the rest), so a typo in a`` |
|       - |  724 | `		 * class name silently listed nothing. This is why get_class_methods() joins` |
|       - |  725 | `		 * get_class_vars() on the self-checked list in vm_arg_check.c. */` |
|       - |  726 | `		char zGiven[64];` |
|       9 |  727 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  728 | `			"get_class_methods(): Argument #1 ($object_or_class) must be an object "` |
|       - |  729 | `			"or a valid class name, %s given",` |
|       4 |  730 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - |  731 | `	}` |
|       - |  732 | `	/* Create a new array  */` |
|      47 |  733 | `	pArray = ph7_context_new_array(pCtx);` |
|      47 |  734 | `	pName = ph7_context_new_scalar(pCtx);` |
|      47 |  735 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  736 | `		/* Out of memory,return NULL */` |
|     ! 0 |  737 | `		ph7_result_null(pCtx);` |
|     ! 0 |  738 | `		return PH7_OK;` |
|       - |  739 | `	}` |
|       - |  740 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  741 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  742 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  743 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  744 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  745 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  746 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  747 | `	{` |
|       - |  748 | `		SySet aTmp;` |
|       - |  749 | `		SyHashEntry **apEntry;` |
|       - |  750 | `		ph7_class *pLevel;` |
|       - |  751 | `		sxu32 n;` |
|       - |  752 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  753 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  754 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      47 |  755 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      47 |  756 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|     321 |  757 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|     277 |  758 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       3 |  759 | `		}` |
|      47 |  760 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     107 |  761 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  762 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  763 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  764 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  765 | `			SySet aLvl;` |
|       - |  766 | `			SyHashEntry **apLvl;` |
|       - |  767 | `			sxu32 i,j;` |
|      63 |  768 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  769 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  770 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  771 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  772 | `			 * hash) come out in declaration order walked oldest-first. */` |
|     455 |  773 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|     395 |  774 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|     395 |  775 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - |  776 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - |  777 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - |  778 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - |  779 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - |  780 | `				 * methods before its own. */` |
|     395 |  781 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - |  782 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - |  783 | `				 * protected within the hierarchy, private only from the class that` |
|       - |  784 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - |  785 | `				 * every private and protected name a class holds. Same decision` |
|       - |  786 | `				 * get_class_vars() already makes for properties. */` |
|     395 |  787 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  788 | `					SyString sMName;` |
|      93 |  789 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - |  790 | `						apEntry[nPick]->nKeyLen);` |
|       - |  791 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - |  792 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - |  793 | `					 * wrote it, and judged against the class that composed the method. */` |
|     139 |  794 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      46 |  795 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      46 |  796 | `							pMethod->iProtection,FALSE) ){` |
|      79 |  797 | `						continue;` |
|       - |  798 | `					}` |
|       7 |  799 | `				}` |
|     317 |  800 | `				if( pDecl != pLevel ){` |
|       - |  801 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - |  802 | `					 * counts as the class's own level, like php. */` |
|       - |  803 | `					ph7_class *pWalk;` |
|      89 |  804 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|      35 |  805 | `						continue;` |
|       - |  806 | `					}` |
|     109 |  807 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     109 |  808 | `						if( pWalk == pDecl ){` |
|      55 |  809 | `							break;` |
|       - |  810 | `						}` |
|      28 |  811 | `					}` |
|      55 |  812 | `					if( pWalk != 0 ){` |
|      55 |  813 | `						continue; /* in-chain: its own level emits it */` |
|       - |  814 | `					}` |
|     ! 0 |  815 | `				}` |
|     229 |  816 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     116 |  817 | `			}` |
|      63 |  818 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - |  819 | `			/* Insertion sort by declaration line (stable) */` |
|     231 |  820 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|     171 |  821 | `				SyHashEntry *pKey = apLvl[i];` |
|     235 |  822 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|     255 |  823 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 |  824 | `					apLvl[j] = apLvl[j-1];` |
|      33 |  825 | `				}` |
|     171 |  826 | `				apLvl[j] = pKey;` |
|      87 |  827 | `			}` |
|       - |  828 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - |  829 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - |  830 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - |  831 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - |  832 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - |  833 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - |  834 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - |  835 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - |  836 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - |  837 | `			 * claim still goes out at the end. */` |
|     289 |  838 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     229 |  839 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     229 |  840 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|     229 |  841 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|     186 |  842 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     186 |  843 | `					apLvl[i] = 0;` |
|      92 |  844 | `				}` |
|     116 |  845 | `			}` |
|       - |  846 | `			{` |
|      63 |  847 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|      63 |  848 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - |  849 | `				sxu32 k;` |
|      79 |  850 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 |  851 | `					ph7_class *pTrait = apTrait[k];` |
|       - |  852 | `					SySet aTr;` |
|       - |  853 | `					SyHashEntry **apTr;` |
|       - |  854 | `					SyHashEntry *pTrE;` |
|       - |  855 | `					sxu32 t;` |
|      18 |  856 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 |  857 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 |  858 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 |  859 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 |  860 | `					}` |
|      18 |  861 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - |  862 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - |  863 | `					 * declaration order. */` |
|      48 |  864 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 |  865 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - |  866 | `						int bWantAlias;` |
|       - |  867 | `						/* First pass emits the aliases made from this method, second the` |
|       - |  868 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 |  869 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 |  870 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  871 | `								ph7_class_method *pM;` |
|     342 |  872 | `								if( apLvl[i] == 0 ){` |
|     184 |  873 | `									continue;` |
|       - |  874 | `								}` |
|     160 |  875 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 |  876 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 |  877 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 |  878 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 |  879 | `										pOrigin->sFunc.sName.zString,` |
|      98 |  880 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 |  881 | `									continue;` |
|       - |  882 | `								}` |
|      68 |  883 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 |  884 | `									continue;` |
|       - |  885 | `								}` |
|      44 |  886 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 |  887 | `								apLvl[i] = 0;` |
|      23 |  888 | `							}` |
|      32 |  889 | `						}` |
|      17 |  890 | `					}` |
|      18 |  891 | `					SySetRelease(&aTr);` |
|      10 |  892 | `				}` |
|       - |  893 | `			}` |
|     289 |  894 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  895 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - |  896 | `				 * that another trait then composed, say — keeps the line order. */` |
|     229 |  897 | `				if( apLvl[i] != 0 ){` |
|     ! 0 |  898 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 |  899 | `				}` |
|     116 |  900 | `			}` |
|      63 |  901 | `			SySetRelease(&aLvl);` |
|      33 |  902 | `		}` |
|      47 |  903 | `		SySetRelease(&aTmp);` |
|       - |  904 | `	}` |
|       - |  905 | `	/* Return the created array */` |
|      47 |  906 | `	ph7_result_value(pCtx,pArray);` |
|       - |  907 | `	/*` |
|       - |  908 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  909 | `	 * automatically as soon we return from this foreign function.` |
|       - |  910 | `	 */` |
|      47 |  911 | `	return PH7_OK;` |
|      27 |  912 | `}` |
|       - |  913 | `/*` |
|       - |  914 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - |  915 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - |  916 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - |  917 | ` *` |
|       - |  918 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - |  919 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - |  920 | ` * private/protected refusal raised from inside a class.` |
|       - |  921 | ` */` |
|    2054 |  922 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 |  923 | `{` |
|    2059 |  924 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - |  925 | `	ph7_vm_func *pVmFunc;` |
|    2155 |  926 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - |  927 | `		/* Safely ignore the exception frame */` |
|     101 |  928 | `		pFrame = pFrame->pParent;` |
|       5 |  929 | `	}` |
|    2059 |  930 | `	if( pFrame == 0 ){` |
|     ! 0 |  931 | `		return 0;` |
|       - |  932 | `	}` |
|    2059 |  933 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  934 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - |  935 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|    2059 |  936 | `	if( pFrame->pBoundScope ){` |
|      32 |  937 | `		return pFrame->pBoundScope;` |
|       - |  938 | `	}` |
|    2029 |  939 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    1503 |  940 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  941 | `	}` |
|     531 |  942 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  943 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - |  944 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - |  945 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - |  946 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - |  947 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - |  948 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - |  949 | `		 * pBoundScope. */` |
|      67 |  950 | `		return (ph7_class *)pVmFunc->pUserData;` |
|       - |  951 | `	}` |
|     465 |  952 | `	if( pVm->pConstEvalClass ){` |
|       - |  953 | `		/* Constant/property initializer bytecode runs without a method` |
|       - |  954 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - |  955 | `		 * constant is reachable from its own class's initializers). */` |
|       3 |  956 | `		return pVm->pConstEvalClass;` |
|       - |  957 | `	}` |
|     463 |  958 | `	return 0;` |
|    1032 |  959 | `}` |
|       - |  960 | `/*` |
|       - |  961 | ` * The scope php NAMES in a visibility Error. PH7_VmCallerScope with one adjustment: php` |
|       - |  962 | ` * flattens a TRAIT into the class that uses it, so code running in a trait method reports` |
|       - |  963 | ` * the USING class ("from scope Base"), never the trait — and not the RECEIVER's class` |
|       - |  964 | `` * either, so `class Kid extends Base` (Base being the one that composed the trait) still`` |
|       - |  965 | ` * reports Base. Walk the receiver's ancestry to the first class that uses this trait; the` |
|       - |  966 | ` * trait itself stands when nothing does (nothing php would print, but better than a lie).` |
|       - |  967 | ` *` |
|       - |  968 | ` * Kept apart from PH7_VmCallerScope because the ACCESS decision genuinely wants the trait:` |
|       - |  969 | ` * its private/protected branches grant on "the caller is a trait used by the target class"` |
|       - |  970 | ` * and on the reverse, and both compare against the trait itself.` |
|       - |  971 | ` */` |
|      92 |  972 | `PH7_PRIVATE ph7_class * PH7_VmCallerScopeName(ph7_vm *pVm)` |
|       4 |  973 | `{` |
|      96 |  974 | `	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       - |  975 | `	VmFrame *pFrame;` |
|       - |  976 | `	ph7_class *pWalk;` |
|      96 |  977 | `	if( pScope == 0 \|\| (pScope->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      92 |  978 | `		return pScope;` |
|       - |  979 | `	}` |
|       5 |  980 | `	pFrame = pVm->pFrame;` |
|       5 |  981 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|     ! 0 |  982 | `		pFrame = pFrame->pParent;` |
|     ! 0 |  983 | `	}` |
|       5 |  984 | `	pWalk = (pFrame && pFrame->pThis) ? pFrame->pThis->pClass : VmCurrentSelf(&(*pVm));` |
|       7 |  985 | `	for( ; pWalk ; pWalk = pWalk->pBase ){` |
|       7 |  986 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|       7 |  987 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - |  988 | `		sxu32 k;` |
|       7 |  989 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|       5 |  990 | `			if( apTrait[k] == pScope ){` |
|       5 |  991 | `				return pWalk;` |
|       - |  992 | `			}` |
|     ! 0 |  993 | `		}` |
|       2 |  994 | `	}` |
|     ! 0 |  995 | `	return pScope;` |
|      50 |  996 | `}` |
|       - |  997 | `/*` |
|       - |  998 | ` * The DECLARING-side twin of PH7_VmCallerScopeName: the class php NAMES as a method's` |
|       - |  999 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - | 1000 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - | 1001 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - | 1002 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - | 1003 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - | 1004 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - | 1005 | ` *` |
|       - | 1006 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - | 1007 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - | 1008 | ` * refused on a child instance names the BASE.` |
|       - | 1009 | ` */` |
|     786 | 1010 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 | 1011 | `{` |
|     393 | 1012 | `	SXUNUSED(pVm);` |
|    1577 | 1013 | `	return PH7_VmComposingClass(pClass,` |
|     786 | 1014 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 | 1015 | `}` |
|       - | 1016 | `/*` |
|       - | 1017 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - | 1018 | ` * property is composed into the using class exactly as its methods are, and` |
|       - | 1019 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - | 1020 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - | 1021 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - | 1022 | ` */` |
|     834 | 1023 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 | 1024 | `{` |
|       - | 1025 | `	ph7_class *pWalk;` |
|     839 | 1026 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     740 | 1027 | `		return pDecl;` |
|       - | 1028 | `	}` |
|     139 | 1029 | `	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|     137 | 1030 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);` |
|     137 | 1031 | `		sxu32 nTrait = SySetUsed(&pWalk->aTrait);` |
|       - | 1032 | `		sxu32 k;` |
|     159 | 1033 | `		for( k = 0 ; k < nTrait ; ++k ){` |
|     121 | 1034 | `			if( apTrait[k] == pDecl ){` |
|      99 | 1035 | `				return pWalk;` |
|       - | 1036 | `			}` |
|      13 | 1037 | `		}` |
|      21 | 1038 | `	}` |
|       3 | 1039 | `	return pDecl;` |
|     422 | 1040 | `}` |
|       - | 1041 | `/*` |
|       - | 1042 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1043 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1044 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1045 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1046 | ` * holds no entry for it.` |
|       - | 1047 | ` */` |
|      38 | 1048 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       2 | 1049 | `{` |
|      40 | 1050 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|      40 | 1051 | `	if( pEntry ){` |
|      40 | 1052 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      21 | 1053 | `	}else{` |
|     ! 0 | 1054 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1055 | `	}` |
|      40 | 1056 | `}` |
|       - | 1057 | `/*` |
|       - | 1058 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1059 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1060 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1061 | ` */` |
|  208284 | 1062 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1063 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1064 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1065 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1066 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1067 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1068 | `	)` |
|       5 | 1069 | `{` |
|  208289 | 1070 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    1967 | 1071 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    1967 | 1072 | `		if( pCallerScope == 0 ){` |
|     389 | 1073 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1074 | `		}` |
|    1583 | 1075 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1076 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1077 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1078 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1079 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1080 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1081 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1082 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1083 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1084 | `			 * match (dynamic props and other non-declared shapes). */` |
|    1399 | 1085 | `			ph7_class *pCaller = pCallerScope;` |
|    2096 | 1086 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|    1394 | 1087 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|    1399 | 1088 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|    1399 | 1089 | `			int bGranted = 0;` |
|    1399 | 1090 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    1074 | 1091 | `				if( pOwn->pDeclClass == 0` |
|    1074 | 1092 | `				 \|\| pOwn->pDeclClass == pCaller` |
|     558 | 1093 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|    1049 | 1094 | `					bGranted = 1;` |
|     527 | 1095 | `				}` |
|     862 | 1096 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     221 | 1097 | `				bGranted = 1;` |
|     108 | 1098 | `			}` |
|    1399 | 1099 | `			if( !bGranted ){` |
|       - | 1100 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1101 | `				ph7_class **apTrait;` |
|       - | 1102 | `				sxu32 nTrait,k;` |
|     138 | 1103 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     138 | 1104 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     138 | 1105 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1106 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1107 | `						bGranted = 1;` |
|     ! 0 | 1108 | `						break;` |
|       - | 1109 | `					}` |
|     ! 0 | 1110 | `				}` |
|      67 | 1111 | `			}` |
|    1399 | 1112 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1113 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1114 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1115 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1116 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1117 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1118 | `				 * whose owner is the trait, not the class). */` |
|       - | 1119 | `				ph7_class **apTrait;` |
|       - | 1120 | `				sxu32 nTrait,k;` |
|     ! 0 | 1121 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     ! 0 | 1122 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     ! 0 | 1123 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1124 | `					if( apTrait[k] == pClass ){` |
|     ! 0 | 1125 | `						bGranted = 1;` |
|     ! 0 | 1126 | `						break;` |
|       - | 1127 | `					}` |
|     ! 0 | 1128 | `				}` |
|     ! 0 | 1129 | `			}` |
|    1399 | 1130 | `			if( !bGranted ){` |
|     138 | 1131 | `				goto dis; /* Access is forbidden */` |
|       - | 1132 | `			}` |
|     635 | 1133 | `		}else{` |
|       - | 1134 | `			/* Protected */` |
|     189 | 1135 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1136 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1137 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1138 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1139 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1140 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1141 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1142 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1143 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1144 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1145 | `			 * from a sibling, matching php. */` |
|     189 | 1146 | `			ph7_class *pIntro = pClass;` |
|       - | 1147 | `			ph7_class *pAnc;` |
|     463 | 1148 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|     279 | 1149 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|     279 | 1150 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     279 | 1151 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|     279 | 1152 | `				int bHere = 0;` |
|     279 | 1153 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      55 | 1154 | `					bHere = 1;` |
|      27 | 1155 | `				}` |
|     279 | 1156 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|      99 | 1157 | `					bHere = 1;` |
|      48 | 1158 | `				}` |
|     279 | 1159 | `				if( bHere ){` |
|     153 | 1160 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      75 | 1161 | `				}` |
|     142 | 1162 | `			}` |
|       - | 1163 | `			/* Must be in the same class hierarchy as the introducing class */` |
|     189 | 1164 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      14 | 1165 | `				int bTraitGrant = 0;` |
|      14 | 1166 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1167 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1168 | `					ph7_class **apTrait;` |
|       - | 1169 | `					sxu32 nTrait,k;` |
|       3 | 1170 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|       3 | 1171 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|       3 | 1172 | `					for(k = 0; k < nTrait; k++){` |
|       3 | 1173 | `						if( apTrait[k] == pClass ){` |
|       3 | 1174 | `							bTraitGrant = 1;` |
|       3 | 1175 | `							break;` |
|       - | 1176 | `						}` |
|     ! 0 | 1177 | `					}` |
|       1 | 1178 | `				}` |
|      14 | 1179 | `				if( !bTraitGrant ){` |
|      11 | 1180 | `					goto dis; /* Access is forbidden */` |
|       - | 1181 | `				}` |
|       1 | 1182 | `			}` |
|       - | 1183 | `		}` |
|     717 | 1184 | `	}` |
|  207761 | 1185 | `	return 1; /* Access is granted */` |
|     264 | 1186 | `dis:` |
|     533 | 1187 | `	if( bLog ){` |
|     ! 0 | 1188 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1189 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1190 | `			&pClass->sName,pAttrName);` |
|     ! 0 | 1191 | `	}` |
|     533 | 1192 | `	return 0; /* Access is forbidden */` |
|  104147 | 1193 | `}` |
|       - | 1194 | `/*` |
|       - | 1195 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1196 | ` *   Get the default properties of the class` |
|       - | 1197 | ` * Parameters` |
|       - | 1198 | ` *  class_name` |
|       - | 1199 | ` *   The class name or class instance` |
|       - | 1200 | ` * Return` |
|       - | 1201 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1202 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1203 | ` *  of varname => value.` |
|       - | 1204 | ` * Note:` |
|       - | 1205 | ` *   NULL is returned on failure.` |
|       - | 1206 | ` */` |
|      16 | 1207 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1208 | `{` |
|       - | 1209 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1210 | `	SyHashEntry *pEntry;` |
|       - | 1211 | `	ph7_class *pClass;` |
|       - | 1212 | `	/* Extract the target class first */` |
|      19 | 1213 | `	pClass = 0;` |
|      19 | 1214 | `	if( nArg > 0 ){` |
|      19 | 1215 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       8 | 1216 | `	}` |
|      19 | 1217 | `	if( pClass == 0 ){` |
|       - | 1218 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1219 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1220 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1221 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1222 | `		int nLen = 0;` |
|     ! 0 | 1223 | `		const char *zVal = "";` |
|     ! 0 | 1224 | `		if( nArg > 0 ){` |
|     ! 0 | 1225 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1226 | `				zVal = "Array";` |
|     ! 0 | 1227 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1228 | `			}else{` |
|     ! 0 | 1229 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1230 | `			}` |
|     ! 0 | 1231 | `		}` |
|     ! 0 | 1232 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1233 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1234 | `			nLen,zVal);` |
|       - | 1235 | `	}` |
|      19 | 1236 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1237 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1238 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1239 | `		 * here, as it does in php. */` |
|       5 | 1240 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 | 1241 | `		if( rcMat != SXRET_OK ){` |
|       5 | 1242 | `			return rcMat;` |
|       - | 1243 | `		}` |
|     ! 0 | 1244 | `	}` |
|       - | 1245 | `	/* Create a new array  */` |
|      15 | 1246 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 1247 | `	pName = ph7_context_new_scalar(pCtx);` |
|      15 | 1248 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      15 | 1249 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1250 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1251 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1252 | `		return PH7_OK;` |
|       - | 1253 | `	}` |
|       - | 1254 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|      15 | 1255 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      41 | 1256 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      29 | 1257 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      29 | 1258 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1259 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1260 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 | 1261 | `			continue;` |
|       - | 1262 | `		}` |
|       - | 1263 | `		/* Check if the access is allowed */` |
|      27 | 1264 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      27 | 1265 | `			SyString *pAttrName = &pAttr->sName;` |
|      27 | 1266 | `			ph7_value *pValue = 0;` |
|      27 | 1267 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1268 | `				/* Static slots are computed at mount; constants lazily */` |
|       8 | 1269 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       8 | 1270 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       5 | 1271 | `			}else{` |
|      20 | 1272 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 | 1273 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1274 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 | 1275 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 | 1276 | `					pValue = &sValue;` |
|       2 | 1277 | `				}` |
|       - | 1278 | `			}` |
|       - | 1279 | `			/* Fill in the array */` |
|      27 | 1280 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      27 | 1281 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1282 | `			/* Reset the cursor */` |
|      27 | 1283 | `			ph7_value_reset_string_cursor(pName);` |
|      12 | 1284 | `		}` |
|       3 | 1285 | `	}` |
|      15 | 1286 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1287 | `	/* Return the created array */` |
|      15 | 1288 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1289 | `	/*` |
|       - | 1290 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1291 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1292 | `	 */` |
|      15 | 1293 | `	return PH7_OK;` |
|      11 | 1294 | `}` |
|       - | 1295 | `/*` |
|       - | 1296 | ` * array get_object_vars(object $this)` |
|       - | 1297 | ` *   Gets the properties of the given object` |
|       - | 1298 | ` * Parameters` |
|       - | 1299 | ` *  this` |
|       - | 1300 | ` *   A class instance` |
|       - | 1301 | ` * Return` |
|       - | 1302 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1303 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1304 | ` *  it will be returned with a NULL value.` |
|       - | 1305 | ` * Note:` |
|       - | 1306 | ` *   NULL is returned on failure.` |
|       - | 1307 | ` */` |
|     108 | 1308 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1309 | `{` |
|     113 | 1310 | `	ph7_class_instance *pThis = 0;` |
|       - | 1311 | `	ph7_value *pName,*pArray;` |
|       - | 1312 | `	SyHashEntry *pEntry;` |
|     113 | 1313 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1314 | `		/* Extract the target instance */` |
|     113 | 1315 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      54 | 1316 | `	}` |
|     113 | 1317 | `	if( pThis == 0 ){` |
|       - | 1318 | `		/* No such instance,return NULL */` |
|     ! 0 | 1319 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1320 | `		return PH7_OK;` |
|       - | 1321 | `	}` |
|       - | 1322 | `	/* Create a new array  */` |
|     113 | 1323 | `	pArray = ph7_context_new_array(pCtx);` |
|     113 | 1324 | `	pName = ph7_context_new_scalar(pCtx);` |
|     113 | 1325 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1326 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1327 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1328 | `		return PH7_OK;` |
|       - | 1329 | `	}` |
|       - | 1330 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1331 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1332 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1333 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1334 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1335 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1336 | `	 * skipped instead of read after free. */` |
|       - | 1337 | `	{` |
|       - | 1338 | `		SySet sNames;` |
|       - | 1339 | `		SyString *aName;` |
|       - | 1340 | `		sxu32 iName,nName;` |
|     113 | 1341 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     113 | 1342 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|     523 | 1343 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     415 | 1344 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     415 | 1345 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|       - | 1346 | `				/* Only non-static/constant attributes are extracted */` |
|     205 | 1347 | `				continue;` |
|       - | 1348 | `			}` |
|     212 | 1349 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      24 | 1350 | `				continue; /* typed, never written: not there yet (php) */` |
|       - | 1351 | `			}` |
|     186 | 1352 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      97 | 1353 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     ! 0 | 1354 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1355 | `			}` |
|     190 | 1356 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       4 | 1357 | `		}` |
|     113 | 1358 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     113 | 1359 | `		nName = SySetUsed(&sNames);` |
|     299 | 1360 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     190 | 1361 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1362 | `			VmClassAttr *pVmAttr;` |
|     190 | 1363 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     190 | 1364 | `			if( pEntry == 0 ){` |
|     ! 0 | 1365 | `				continue; /* unset by an earlier hook */` |
|       - | 1366 | `			}` |
|     190 | 1367 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1368 | `			/* Check if the access is allowed */` |
|     190 | 1369 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     148 | 1370 | `				ph7_value *pValue = 0;` |
|       - | 1371 | `				ph7_value sHookVal;` |
|       - | 1372 | `				sxi32 rcHk;` |
|       - | 1373 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1374 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     148 | 1375 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     148 | 1376 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     148 | 1377 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1378 | `					pValue = &sHookVal;` |
|     141 | 1379 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1380 | `					/* Extract attribute */` |
|     134 | 1381 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      69 | 1382 | `				}else{` |
|       - | 1383 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1384 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1385 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1386 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1387 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1388 | `					break;` |
|       - | 1389 | `				}` |
|     148 | 1390 | `				if( pValue ){` |
|       - | 1391 | `					/* Insert attribute name in the array */` |
|     148 | 1392 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     148 | 1393 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|      72 | 1394 | `				}` |
|     148 | 1395 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1396 | `				/* Reset the cursor */` |
|     148 | 1397 | `				ph7_value_reset_string_cursor(pName);` |
|      72 | 1398 | `			}` |
|      97 | 1399 | `		}` |
|     113 | 1400 | `		SySetRelease(&sNames);` |
|       - | 1401 | `	}` |
|       - | 1402 | `	/* Return the created array */` |
|     113 | 1403 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1404 | `	/*` |
|       - | 1405 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1406 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1407 | `	 */` |
|     113 | 1408 | `	return PH7_OK;` |
|      59 | 1409 | `}` |
|       - | 1410 | `/*` |
|       - | 1411 | ` * array get_mangled_object_vars(object $object)` |
|       - | 1412 | ` *  The object's own property table, with php's visibility MANGLING left on the keys:` |
|       - | 1413 | `` *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".`` |
|       - | 1414 | ` *` |
|       - | 1415 | ` *  It is get_object_vars()'s opposite in both of that function's decisions — no` |
|       - | 1416 | ` *  visibility screen (every property is reported, from every level of the chain) and` |
|       - | 1417 | `` *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,`` |
|       - | 1418 | ` *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not` |
|       - | 1419 | ` *  the (array) cast either: the cast asks a native class's own handler, so` |
|       - | 1420 | `` *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.`` |
|       - | 1421 | ` */` |
|      14 | 1422 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1423 | `{` |
|      17 | 1424 | `	ph7_class_instance *pThis = 0;` |
|       - | 1425 | `	ph7_value *pArray;` |
|      17 | 1426 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1427 | `		/* Extract the target instance */` |
|      17 | 1428 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       7 | 1429 | `	}` |
|      17 | 1430 | `	if( pThis == 0 ){` |
|       - | 1431 | ``		/* The `object $object` signature row refuses everything else before we run */`` |
|     ! 0 | 1432 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1433 | `		return PH7_OK;` |
|       - | 1434 | `	}` |
|      17 | 1435 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 1436 | `	if( pArray == 0 ){` |
|       - | 1437 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1438 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1439 | `		return PH7_OK;` |
|       - | 1440 | `	}` |
|      17 | 1441 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);` |
|      17 | 1442 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 1443 | `	return PH7_OK;` |
|      10 | 1444 | `}` |
|       - | 1445 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1446 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1447 | ` * detection should reject them up front. */` |
|       - | 1448 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1449 | `/*` |
|       - | 1450 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1451 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1452 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1453 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1454 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1455 | ` */` |
| 2481102 | 1456 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1457 | `{` |
| 2504861 | 1458 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1459 | `		ph7_class **apParent;` |
|       - | 1460 | `		sxu32 n;` |
| 2490959 | 1461 | `		if( pIface == pTarget ){` |
| 2467203 | 1462 | `			return TRUE;` |
|       - | 1463 | `		}` |
|       - | 1464 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1465 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|   23761 | 1466 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   23765 | 1467 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       7 | 1468 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|       3 | 1469 | `				return TRUE;` |
|       - | 1470 | `			}` |
|       3 | 1471 | `		}` |
|   23759 | 1472 | `		pIface = pIface->pBase;` |
|   23759 | 1473 | `		iDepth++;` |
|       5 | 1474 | `	}` |
|   13907 | 1475 | `	return FALSE;` |
| 1240556 | 1476 | `}` |
|       - | 1477 | `/*` |
|       - | 1478 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1479 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1480 | ` */` |
| 2708338 | 1481 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1482 | `{` |
|       - | 1483 | `	ph7_class **apInterface;` |
|       - | 1484 | `	sxu32 n;` |
| 2708343 | 1485 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1486 | `		/* Empty interface container */` |
|  232153 | 1487 | `		return FALSE;` |
|       - | 1488 | `	}` |
|       - | 1489 | `	/* Point to the set of implemented interfaces */` |
| 2476195 | 1490 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1491 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1492 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2490093 | 1493 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2481101 | 1494 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2467203 | 1495 | `			return TRUE;` |
|       - | 1496 | `		}` |
|    6954 | 1497 | `	}` |
|    8997 | 1498 | `	return FALSE;` |
| 1354174 | 1499 | `}` |
|       - | 1500 | `/*` |
|       - | 1501 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1502 | ` * is an instance of the main class (second argument).` |
|       - | 1503 | ` * Otherwise FALSE is returned.` |
|       - | 1504 | ` */` |
| 4022058 | 1505 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1506 | `{` |
|       - | 1507 | `	ph7_class *pParent;` |
|       - | 1508 | `	sxi32 rc;` |
| 4022063 | 1509 | `	if( pThis == pClass ){` |
|       - | 1510 | `		/* Instance of the same class */` |
| 1431549 | 1511 | `		return TRUE;` |
|       - | 1512 | `	}` |
|       - | 1513 | `	/* Check implemented interfaces */` |
| 2590519 | 1514 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2590519 | 1515 | `	if( rc ){` |
| 2355095 | 1516 | `		return TRUE;` |
|       - | 1517 | `	}` |
|       - | 1518 | `	/* Check parent classes */` |
|  235429 | 1519 | `	pParent = pThis->pBase;` |
|  241121 | 1520 | `	while( pParent ){` |
|  118421 | 1521 | `		if( pParent == pClass ){` |
|       - | 1522 | `			/* Same instance */` |
|     631 | 1523 | `			return TRUE;` |
|       - | 1524 | `		}` |
|       - | 1525 | `		/* Check the implemented interfaces */` |
|  117795 | 1526 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  117795 | 1527 | `		if( rc ){` |
|  112103 | 1528 | `			return TRUE;` |
|       - | 1529 | `		}` |
|       - | 1530 | `		/* Point to the parent class */` |
|    5697 | 1531 | `		pParent = pParent->pBase;` |
|       5 | 1532 | `	}` |
|       - | 1533 | `	/* Not an instance of the the given class */` |
|  122705 | 1534 | `	return FALSE;` |
| 2011034 | 1535 | `}` |
|       - | 1536 | `/*` |
|       - | 1537 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1538 | ` * is a subclass of the main class (second argument).` |
|       - | 1539 | ` * Otherwise FALSE is returned.` |
|       - | 1540 | ` */` |
|      36 | 1541 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       2 | 1542 | `{` |
|       - | 1543 | `	SyHashEntry *pEntry;` |
|       - | 1544 | `	SyString *pName;` |
|      62 | 1545 | `	while( pClass ){` |
|      58 | 1546 | `		pName = &pClass->sName;` |
|       - | 1547 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      58 | 1548 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      58 | 1549 | `		if( pEntry ){` |
|      24 | 1550 | `			return TRUE;` |
|       - | 1551 | `		}` |
|       - | 1552 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1553 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1554 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1555 | `		 * class's aInterface, missing inherited interfaces. */` |
|      35 | 1556 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1557 | `			return TRUE;` |
|       - | 1558 | `		}` |
|      25 | 1559 | `		pClass = pClass->pBase;` |
|       1 | 1560 | `	}` |
|       - | 1561 | `	/* Not a subclass */` |
|       5 | 1562 | `	return FALSE;` |
|      20 | 1563 | `}` |
|       - | 1564 | `/*` |
|       - | 1565 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1566 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1567 | ` *   as one of its parents.` |
|       - | 1568 | ` * Parameters` |
|       - | 1569 | ` *  object_or_class` |
|       - | 1570 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1571 | ` * class` |
|       - | 1572 | ` *  The class or interface name to test against.` |
|       - | 1573 | ` * allow_string` |
|       - | 1574 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1575 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1576 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1577 | ` * Return` |
|       - | 1578 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1579 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1580 | ` */` |
|      32 | 1581 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1582 | `{` |
|      35 | 1583 | `	int res = 0; /* Assume FALSE by default */` |
|      35 | 1584 | `	if( nArg > 1 ){` |
|      35 | 1585 | `		ph7_class *pThisClass = 0;` |
|      35 | 1586 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1587 | `			/* An object first argument: allow_string is ignored (php). */` |
|      17 | 1588 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      26 | 1589 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1590 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1591 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1592 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1593 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      15 | 1594 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       7 | 1595 | `		}` |
|      35 | 1596 | `		if( pThisClass ){` |
|       - | 1597 | `			/* Extract the given class */` |
|      29 | 1598 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      29 | 1599 | `			if( pClass ){` |
|       - | 1600 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1601 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      29 | 1602 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      13 | 1603 | `			}` |
|      13 | 1604 | `		}` |
|      16 | 1605 | `	}` |
|       - | 1606 | `	/* Query result */` |
|      35 | 1607 | `	ph7_result_bool(pCtx,res);` |
|      35 | 1608 | `	return PH7_OK;` |
|       3 | 1609 | `}` |
|       - | 1610 | `/*` |
|       - | 1611 | ` * int spl_object_id(object $object)` |
|       - | 1612 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1613 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1614 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1615 | ` */` |
|      18 | 1616 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1617 | `{` |
|       - | 1618 | `	ph7_class_instance *pThis;` |
|      21 | 1619 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1620 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1621 | `		return PH7_OK;` |
|       - | 1622 | `	}` |
|      21 | 1623 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      21 | 1624 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      21 | 1625 | `	return PH7_OK;` |
|      12 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 1629 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 1630 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 1631 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 1632 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 1633 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 1634 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 1635 | ` *` |
|       - | 1636 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 1637 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 1638 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 1639 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 1640 | ` */` |
|      48 | 1641 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1642 | `{` |
|      49 | 1643 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1644 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1645 | `	char zGiven[64];` |
|       - | 1646 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 1647 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      49 | 1648 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1649 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1650 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 1651 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 1652 | `	}` |
|      49 | 1653 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1654 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 1655 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 1656 | `	 * resource, and Generator/Fiber. */` |
|      48 | 1657 | `	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM\|PH7_CLASS_NOCLONE))` |
|      48 | 1658 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      10 | 1659 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       6 | 1660 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sName);` |
|       - | 1661 | `	}` |
|      43 | 1662 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 1663 | `	if( pClone == 0 ){` |
|     ! 0 | 1664 | `		return PH7_VmMemoryError(pVm);` |
|       - | 1665 | `	}` |
|       - | 1666 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 1667 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 1668 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 1669 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 1670 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 1671 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 1672 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 1673 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 1674 | `		sxu32 n;` |
|      53 | 1675 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 1676 | `			ph7_value *pVal,sVal;` |
|       - | 1677 | `			const char *zName;` |
|       - | 1678 | `			sxu32 nName;` |
|       - | 1679 | `			char zKeyBuf[64];` |
|       - | 1680 | `			sxi32 rc;` |
|      31 | 1681 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 1682 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 1683 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 1684 | `				zName = zKeyBuf;` |
|     ! 0 | 1685 | `			}else{` |
|      31 | 1686 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 1687 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 1688 | `			}` |
|      31 | 1689 | `			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 1690 | `			if( pVal ){` |
|       - | 1691 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 1692 | `				 * property, whose slot reservation can reallocate pVm->aMemObj and` |
|       - | 1693 | `				 * dangle pVal (a pointer into it). */` |
|      31 | 1694 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 1695 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 1696 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 1697 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 1698 | `				if( rc != SXRET_OK ){` |
|       7 | 1699 | `					return rc;` |
|       - | 1700 | `				}` |
|      12 | 1701 | `			}` |
|      25 | 1702 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 1703 | `		}` |
|      11 | 1704 | `	}` |
|      37 | 1705 | `	return PH7_OK;` |
|      25 | 1706 | `}` |
|       - | 1707 | `/*` |
|       - | 1708 | ` * string spl_object_hash(object $object)` |
|       - | 1709 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 1710 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 1711 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 1712 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 1713 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 1714 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 1715 | ` */` |
|      18 | 1716 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1717 | `{` |
|       - | 1718 | `	ph7_class_instance *pThis;` |
|      20 | 1719 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1720 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1721 | `		return PH7_OK;` |
|       - | 1722 | `	}` |
|      20 | 1723 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 1724 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 1725 | `	return PH7_OK;` |
|      11 | 1726 | `}` |
|       - | 1727 | `/*` |
|       - | 1728 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 1729 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 1730 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 1731 | ` * Parameters` |
|       - | 1732 | ` *  object_or_class` |
|       - | 1733 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 1734 | ` * class` |
|       - | 1735 | ` *  The class or interface name to test against.` |
|       - | 1736 | ` * allow_string` |
|       - | 1737 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 1738 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 1739 | ` *  object first argument (php).` |
|       - | 1740 | ` * Return` |
|       - | 1741 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 1742 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 1743 | ` */` |
|      48 | 1744 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1745 | `{` |
|      50 | 1746 | `	int res = 0; /* Assume FALSE by default */` |
|      50 | 1747 | `	if( nArg > 1 ){` |
|      50 | 1748 | `		ph7_class *pClass = 0;` |
|      50 | 1749 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1750 | `			/* An object first argument: allow_string is ignored (php). */` |
|      20 | 1751 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      41 | 1752 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 1753 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 1754 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 1755 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 1756 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      24 | 1757 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      11 | 1758 | `		}` |
|      50 | 1759 | `		if( pClass ){` |
|       - | 1760 | `			/* Extract the target class */` |
|      40 | 1761 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      40 | 1762 | `			if( pMain ){` |
|       - | 1763 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      38 | 1764 | `				res = VmSubclassOf(pClass,pMain);` |
|      18 | 1765 | `			}` |
|      19 | 1766 | `		}` |
|      24 | 1767 | `	}` |
|       - | 1768 | `	/* Query result */` |
|      50 | 1769 | `	ph7_result_bool(pCtx,res);` |
|      50 | 1770 | `	return PH7_OK;` |
|       2 | 1771 | `}` |
|     226 | 1772 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1773 | `{` |
|       - | 1774 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 1775 | `	sxi32 rc;` |
|     230 | 1776 | `	if( nArg < 1 ){` |
|       - | 1777 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1778 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1779 | `		return PH7_OK;` |
|       - | 1780 | `	}` |
|       - | 1781 | `	{` |
|       - | 1782 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 1783 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     230 | 1784 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     230 | 1785 | `		if( rcCb != PH7_OK ){` |
|      72 | 1786 | `			return rcCb;` |
|       - | 1787 | `		}` |
|       - | 1788 | `	}` |
|     160 | 1789 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     160 | 1790 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1791 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     160 | 1792 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 1793 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 1794 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 1795 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 1796 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 1797 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 1798 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     170 | 1799 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      21 | 1800 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 1801 | `		VmCallArgMap sInner;` |
|       - | 1802 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 1803 | `		 * unset when forwarded, not as stack garbage. */` |
|      21 | 1804 | `		SyZero(&sInner,sizeof(sInner));` |
|      21 | 1805 | `		sInner.bHasNamed = 1;` |
|      21 | 1806 | `		sInner.bIsNamespaced = 0;` |
|       - | 1807 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 1808 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 1809 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 1810 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      21 | 1811 | `		sInner.bStrict = 0;` |
|      21 | 1812 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      21 | 1813 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|      21 | 1814 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      11 | 1815 | `	}else{` |
|       - | 1816 | `		/* call_user_func is one of php's two FORWARDS: the callback binds under the` |
|       - | 1817 | `		 * mode of the file that wrote the call_user_func, not weakly like every other` |
|       - | 1818 | `		 * internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 1819 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 1820 | `		 * other internal invocation, and wrong here). */` |
|       - | 1821 | `		VmCallArgMap sFwd;` |
|     140 | 1822 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     140 | 1823 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     140 | 1824 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 1825 | `	}` |
|     160 | 1826 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1827 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 1828 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      18 | 1829 | `		PH7_MemObjRelease(&sResult);` |
|      18 | 1830 | `		return PH7_EXCEPTION;` |
|       - | 1831 | `	}` |
|     143 | 1832 | `	if( rc != SXRET_OK ){` |
|       - | 1833 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1834 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1835 | `	}else{` |
|       - | 1836 | `		/* Callback result */` |
|     143 | 1837 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1838 | `	}` |
|     143 | 1839 | `	PH7_MemObjRelease(&sResult);` |
|     143 | 1840 | `	return PH7_OK;` |
|     117 | 1841 | `}` |
|       - | 1842 | `/*` |
|       - | 1843 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 1844 | ` *  Call a callback with an array of parameters.` |
|       - | 1845 | ` * Parameter` |
|       - | 1846 | ` *  $callback` |
|       - | 1847 | ` *   The callable to be called.` |
|       - | 1848 | ` * $param_arr` |
|       - | 1849 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 1850 | ` * Return` |
|       - | 1851 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 1852 | ` */` |
|     198 | 1853 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1854 | `{` |
|       - | 1855 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 1856 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 1857 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 1858 | `	SySet aArg;               /* Argument value pointers */` |
|     200 | 1859 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     200 | 1860 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     200 | 1861 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 1862 | `	sxi32 rc;` |
|       - | 1863 | `	sxu32 n;` |
|     200 | 1864 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1865 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1866 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1867 | `		return PH7_OK;` |
|       - | 1868 | `	}` |
|       - | 1869 | `	{` |
|     200 | 1870 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     200 | 1871 | `		if( rcCb != PH7_OK ){` |
|       5 | 1872 | `			return rcCb;` |
|       - | 1873 | `		}` |
|       - | 1874 | `	}` |
|     196 | 1875 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     196 | 1876 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1877 | `	/* Initialize the arguments container */` |
|     196 | 1878 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1879 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 1880 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 1881 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 1882 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 1883 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 1884 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     196 | 1885 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     196 | 1886 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     582 | 1887 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1888 | `		/* Extract node value */` |
|     388 | 1889 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     388 | 1890 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      29 | 1891 | `				if( aNames == 0 ){` |
|       - | 1892 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 1893 | `					 * not-yet-seen slot defaults to positional. */` |
|      19 | 1894 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      19 | 1895 | `					if( aNames == 0 ){` |
|     ! 0 | 1896 | `						SySetRelease(&aArg);` |
|     ! 0 | 1897 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1898 | `						if( apNode ){` |
|     ! 0 | 1899 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 1900 | `						}` |
|     ! 0 | 1901 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 1902 | `					}` |
|      19 | 1903 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|       9 | 1904 | `				}` |
|      29 | 1905 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      14 | 1906 | `			}` |
|     388 | 1907 | `			if( apNode == 0 ){` |
|     227 | 1908 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     150 | 1909 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     152 | 1910 | `				if( apNode ){` |
|     152 | 1911 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|      75 | 1912 | `				}` |
|      75 | 1913 | `			}` |
|     388 | 1914 | `			if( apNode ){` |
|     388 | 1915 | `				apNode[nSlot] = pEntry;` |
|     193 | 1916 | `			}` |
|     388 | 1917 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     388 | 1918 | `			nSlot++;` |
|     193 | 1919 | `		}` |
|       - | 1920 | `		/* Point to the next entry */` |
|     388 | 1921 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     195 | 1922 | `	}` |
|       - | 1923 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 1924 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 1925 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 1926 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     196 | 1927 | `	if( apNode ){` |
|     152 | 1928 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     152 | 1929 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     152 | 1930 | `		apNode = 0;` |
|      75 | 1931 | `	}` |
|       - | 1932 | `	/* Try to invoke the callback */` |
|     196 | 1933 | `	if( aNames ){` |
|       - | 1934 | `		VmCallArgMap sMap;` |
|      19 | 1935 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      19 | 1936 | `		sMap.bHasNamed = 1;` |
|      19 | 1937 | `		sMap.bIsNamespaced = 0;` |
|       - | 1938 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 1939 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      19 | 1940 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      19 | 1941 | `		sMap.nTotal = nSlot;` |
|      19 | 1942 | `		sMap.aNames = aNames;` |
|      28 | 1943 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      18 | 1944 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      19 | 1945 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      10 | 1946 | `	}else{` |
|       - | 1947 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 1948 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 1949 | `		VmCallArgMap sFwd;` |
|     178 | 1950 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     178 | 1951 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     266 | 1952 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     176 | 1953 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 1954 | `	}` |
|     196 | 1955 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1956 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     113 | 1957 | `		PH7_MemObjRelease(&sResult);` |
|     113 | 1958 | `		SySetRelease(&aArg);` |
|     113 | 1959 | `		return PH7_EXCEPTION;` |
|       - | 1960 | `	}` |
|      84 | 1961 | `	if( rc != SXRET_OK ){` |
|       - | 1962 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1963 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1964 | `	}else{` |
|       - | 1965 | `		/* Callback result */` |
|      84 | 1966 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1967 | `	}` |
|       - | 1968 | `	/* Cleanup the mess left behind */` |
|      84 | 1969 | `	PH7_MemObjRelease(&sResult);` |
|      84 | 1970 | `	SySetRelease(&aArg);` |
|      84 | 1971 | `	return PH7_OK;` |
|     101 | 1972 | `}` |
|       - | 1973 |  |
