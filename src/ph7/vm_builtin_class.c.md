# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1058/1183 lines (89.43%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|   13115 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|   13120 |   11 | `	if( nArg < 1 ){` |
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
|   13120 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   13120 |   25 | `		if( pClass ){` |
|   13120 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|   13120 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    6537 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|   13120 |   34 | `	return PH7_OK;` |
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
|       4 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      84 |   52 | `	if( nArg < 1 ){` |
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
|      82 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      82 |   66 | `		if( pClass ){` |
|      82 |   67 | `			if( pClass->pBase ){` |
|      76 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      76 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      40 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       7 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      43 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      84 |   80 | `	return PH7_OK;` |
|       4 |   81 | `}` |
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
|  220614 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  220619 |  115 | `	ph7_class *pClass = 0;` |
|  220619 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  213520 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  113836 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    7100 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    7100 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    7098 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    3538 |  135 | `		}` |
|    3539 |  136 | `	}` |
|  220619 |  137 | `	return pClass;` |
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
|      90 |  269 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  270 | `{` |
|      93 |  271 | `	int res = 0; /* Assume method does not exists */` |
|      93 |  272 | `	if( nArg > 1 ){` |
|       - |  273 | `		ph7_class *pClass;` |
|      90 |  274 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      52 |  275 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
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
|      91 |  289 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      91 |  290 | `		if( pClass ){` |
|       - |  291 | `			const char *zName;` |
|       - |  292 | `			int nLen;` |
|       - |  293 | `			/* Extract method name */` |
|      86 |  294 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      86 |  295 | `			if( nLen > 0 ){` |
|       - |  296 | `				/* Perform the lookup in the method table */` |
|      86 |  297 | `				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);` |
|      86 |  298 | `				if( pEntry ){` |
|       - |  299 | ``					/* ...and apply php's one visibility rule here (`func->common.scope`` |
|       - |  300 | ``					 * == ce`): a PRIVATE method is only a method of the class that`` |
|       - |  301 | `					 * declares it. PHL copies a base's private down so an inherited` |
|       - |  302 | `					 * public method can still dispatch it, which made` |
|       - |  303 | ``					 * `method_exists('Child','basePrivate')` answer true where php`` |
|       - |  304 | `					 * answers false — the same shape property_exists() had. Nothing` |
|       - |  305 | `					 * about the CALLING scope enters into it: php answers false for the` |
|       - |  306 | `					 * child from inside the BASE too. A trait's method belongs to the` |
|       - |  307 | `					 * class that COMPOSED it. */` |
|      64 |  308 | `					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      64 |  309 | `					res = 1;` |
|      62 |  310 | `					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      43 |  311 | `					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){` |
|      15 |  312 | `						res = 0;` |
|       7 |  313 | `					}` |
|      31 |  314 | `				}` |
|      42 |  315 | `			}` |
|      42 |  316 | `		}` |
|      44 |  317 | `	}` |
|      91 |  318 | `	ph7_result_bool(pCtx,res);` |
|      91 |  319 | `	return PH7_OK;` |
|      48 |  320 | `}` |
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
|     262 |  333 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  334 | `{` |
|     267 |  335 | `	int res = 0; /* Assume class does not exist */` |
|     267 |  336 | `	if( nArg > 0 ){` |
|     267 |  337 | `		SyHashEntry *pEntry = 0;` |
|       - |  338 | `		const char *zName;` |
|       - |  339 | `		int nLen;` |
|     267 |  340 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  341 | `		sxu32 nName;` |
|       - |  342 | `		/* Extract given name */` |
|     267 |  343 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     267 |  344 | `		if( nArg >= 2 ){` |
|     136 |  345 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|      66 |  346 | `		}` |
|       - |  347 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  348 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     267 |  349 | `		nName = (sxu32)nLen;` |
|     267 |  350 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     267 |  351 | `		if( nName > 0 ){` |
|       - |  352 | `			/* Perform a hash lookup first */` |
|     263 |  353 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     129 |  354 | `		}` |
|       - |  355 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|     267 |  356 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  357 | `			/* Try autoload, then re-check */` |
|      28 |  358 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      28 |  359 | `			if( pClass ){` |
|       9 |  360 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  361 | `			}` |
|      12 |  362 | `		}` |
|     267 |  363 | `		if( pEntry ){` |
|       - |  364 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  365 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|     233 |  366 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     245 |  367 | `			while( pClass ){` |
|     233 |  368 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|     221 |  369 | `					res = 1;` |
|     221 |  370 | `					break;` |
|       - |  371 | `				}` |
|      13 |  372 | `				pClass = pClass->pNextName;` |
|       1 |  373 | `			}` |
|     114 |  374 | `		}` |
|     131 |  375 | `	}` |
|     267 |  376 | `	ph7_result_bool(pCtx,res);` |
|     267 |  377 | `	return PH7_OK;` |
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
|      19 |  403 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
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
|      57 |  422 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      59 |  423 | `			while( pClass ){` |
|      57 |  424 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  425 | `					/* interface is available */` |
|      55 |  426 | `					res = 1;` |
|      55 |  427 | `					break;` |
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
|       4 |  513 | `{` |
|       - |  514 | `	const char *zOld,*zNew,*zOldRaw,*zNewRaw;` |
|       - |  515 | `	int nOldLen,nNewLen;` |
|       - |  516 | `	sxu32 nOld,nNew;` |
|      26 |  517 | `	int iAutoload = 1;` |
|       - |  518 | `	SyHashEntry *pEntry;` |
|       - |  519 | `	ph7_class *pClass;` |
|       - |  520 | `	char *zDup;` |
|       - |  521 | `	sxi32 rc;` |
|      26 |  522 | `	if( nArg < 2 ){` |
|       - |  523 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  524 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  525 | `		return PH7_OK;` |
|       - |  526 | `	}` |
|       - |  527 | `	/* Extract old class name */` |
|      26 |  528 | `	zOld = zOldRaw = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  529 | `	/* Extract alias name */` |
|      26 |  530 | `	zNew = zNewRaw = ph7_value_to_string(apArg[1],&nNewLen);` |
|      26 |  531 | `	if( nArg >= 3 ){` |
|       3 |  532 | `		iAutoload = ph7_value_to_bool(apArg[2]);` |
|       1 |  533 | `	}` |
|       - |  534 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  535 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  536 | `	 * on the plain name then matches. */` |
|      26 |  537 | `	nOld = (sxu32)nOldLen;` |
|      26 |  538 | `	nNew = (sxu32)nNewLen;` |
|      26 |  539 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|      26 |  540 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       - |  541 | `	/* Perform a hash lookup */` |
|      26 |  542 | `	pEntry = nOld > 0 ? SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld) : 0;` |
|      26 |  543 | `	if( pEntry == 0 && iAutoload && nOld > 0 ){` |
|       - |  544 | `		/* Not declared yet: ask the autoloader, exactly as class_exists() does.` |
|       - |  545 | `		 * iLoadable is FALSE so an interface or a trait comes back too — php` |
|       - |  546 | `		 * aliases those as readily as a class. */` |
|       7 |  547 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zOld,nOld,FALSE) ){` |
|       5 |  548 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|       2 |  549 | `		}` |
|       3 |  550 | `	}` |
|      26 |  551 | `	if( pEntry ==  0 ){` |
|       - |  552 | `		/* php names the class it could not find and answers false. The sentence` |
|       - |  553 | ``		 * carries no `class_alias(): ` prefix, so it is raised on the VM rather`` |
|       - |  554 | `		 * than through the context. */` |
|       5 |  555 | `		VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,"Class \"%.*s\" not found",nOldLen,zOldRaw);` |
|       5 |  556 | `		ph7_result_bool(pCtx,0);` |
|       5 |  557 | `		return PH7_OK;` |
|       - |  558 | `	}` |
|       - |  559 | `	/* Point to the class */` |
|      22 |  560 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|      22 |  561 | `	if( nNew > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zNew,nNew) != 0 ){` |
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
|      20 |  578 | `	if( nNew < 1 ){` |
|       - |  579 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  580 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  581 | `		return PH7_OK;` |
|       - |  582 | `	}` |
|       - |  583 | `	/* Duplicate alias name */` |
|      20 |  584 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|      20 |  585 | `	if( zDup == 0 ){` |
|       - |  586 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  587 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  588 | `		return PH7_OK;` |
|       - |  589 | `	}` |
|       - |  590 | `	/* Create the alias */` |
|      20 |  591 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|      20 |  592 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  593 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  594 | `	}` |
|      20 |  595 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      20 |  596 | `	return PH7_OK;` |
|      15 |  597 | `}` |
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
|    2082 |  746 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  747 | `{` |
|    2084 |  748 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|    2084 |  749 | `	ph7_class *pWalk,*pHigh = 0;` |
|    2084 |  750 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2028 |  751 | `		return pDecl;` |
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
|    1043 |  765 | `}` |
|       - |  766 | `/*` |
|       - |  767 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  768 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  769 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  770 | ` */` |
|    1134 |  771 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       2 |  772 | `{` |
|    1136 |  773 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    1136 |  774 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    1136 |  775 | `	ph7_value_reset_string_cursor(pName);` |
|    1136 |  776 | `}` |
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
|     152 |  788 | `static ph7_class * VmClassRelationTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  789 | `{` |
|       - |  790 | `	const char *zName,*zLook;` |
|       - |  791 | `	int nLen,nShow,bAutoload;` |
|       - |  792 | `	sxu32 nLook;` |
|       - |  793 | `	SyHashEntry *pEntry;` |
|     152 |  794 | `	if( nArg < 1` |
|     152 |  795 | `	 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|     144 |  796 | `	 \|\| (apArg[0]->iFlags & MEMOBJ_NULL) != 0 ){` |
|       - |  797 | `		char zGiven[64];` |
|      49 |  798 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  799 | `			"%s(): Argument #1 ($object_or_class) must be of type object\|string, %s given",` |
|      12 |  800 | `			ph7_function_name(pCtx),` |
|      24 |  801 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|      25 |  802 | `		return 0;` |
|       - |  803 | `	}` |
|     132 |  804 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      21 |  805 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      21 |  806 | `		return pInst ? pInst->pClass : 0;` |
|       - |  807 | `	}` |
|     113 |  808 | `	bAutoload = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     113 |  809 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     113 |  810 | `	zLook = zName;` |
|     113 |  811 | `	nLook = (sxu32)nLen;` |
|       - |  812 | `	/* A leading '\' is the global-namespace anchor for the LOOKUP and part of the` |
|       - |  813 | `	 * name for the DIAGNOSTIC: php reports back what it was handed. */` |
|     113 |  814 | `	PH7_VmClassNameAnchor(&zLook,&nLook);` |
|     113 |  815 | `	pEntry = nLook > 0 ? SyHashGet(&pCtx->pVm->hClass,(const void *)zLook,nLook) : 0;` |
|     113 |  816 | `	if( pEntry == 0 && nLook > 0 && bAutoload ){` |
|       7 |  817 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zLook,nLook,FALSE) ){` |
|     ! 0 |  818 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zLook,nLook);` |
|     ! 0 |  819 | `		}` |
|       3 |  820 | `	}` |
|     113 |  821 | `	if( pEntry ){` |
|       - |  822 | `		/* Anything a class name can stand for answers, php's zend_lookup_class` |
|       - |  823 | `		 * included: an interface, a trait and an enum all have relations to` |
|       - |  824 | `		 * report, and PHL's class_exists() gate said FALSE for every one of them` |
|       - |  825 | ``		 * -- so `class_implements('Countable')` was false rather than a list. */`` |
|      95 |  826 | `		return (ph7_class *)pEntry->pUserData;` |
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
|     110 |  842 | `static int VmClassRelationList(ph7_context *pCtx,ph7_class **apClass,sxu32 nClass)` |
|       4 |  843 | `{` |
|       - |  844 | `	ph7_value *pArray,*pName;` |
|       - |  845 | `	sxu32 n;` |
|     114 |  846 | `	pArray = ph7_context_new_array(pCtx);` |
|     114 |  847 | `	pName = ph7_context_new_scalar(pCtx);` |
|     114 |  848 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  849 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  850 | `	}` |
|     266 |  851 | `	for( n = 0 ; n < nClass ; ++n ){` |
|     155 |  852 | `		SyString *pStr = &apClass[n]->sName;` |
|       - |  853 | `		/* The name is its own key. A ph7_value key rather than the strkey door,` |
|       - |  854 | `		 * which wants a NUL-terminated C string a SyString does not promise. */` |
|     155 |  855 | `		ph7_value_string(pName,SyStringData(pStr),(int)SyStringLength(pStr));` |
|     155 |  856 | `		ph7_array_add_elem(pArray,pName,pName);` |
|     155 |  857 | `		ph7_value_reset_string_cursor(pName);` |
|      79 |  858 | `	}` |
|     114 |  859 | `	ph7_result_value(pCtx,pArray);` |
|     114 |  860 | `	return PH7_OK;` |
|      59 |  861 | `}` |
|       - |  862 | `/*` |
|       - |  863 | ` * array\|false class_parents($object_or_class, bool $autoload = true)` |
|       - |  864 | ` *  Every ancestor of the class, nearest first, keyed by its own name.` |
|       - |  865 | ` */` |
|      44 |  866 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  867 | `{` |
|      47 |  868 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  869 | `	SySet aOut;` |
|       - |  870 | `	int rc;` |
|      47 |  871 | `	if( pClass == 0 ){` |
|      15 |  872 | `		ph7_result_bool(pCtx,0);` |
|      15 |  873 | `		return PH7_OK;` |
|       - |  874 | `	}` |
|      33 |  875 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|       - |  876 | `	/* An INTERFACE keeps its first parent on pBase here where php keeps none at` |
|       - |  877 | `	 * all, so an interface answers the empty list the way php's does. */` |
|      33 |  878 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      29 |  879 | `		ph7_class *pUp = pClass->pBase;` |
|      29 |  880 | `		sxu32 nGuard = 0;` |
|      49 |  881 | `		while( pUp && nGuard++ < 1024 ){` |
|      23 |  882 | `			SySetPut(&aOut,(const void *)&pUp);` |
|      23 |  883 | `			pUp = pUp->pBase;` |
|       3 |  884 | `		}` |
|      13 |  885 | `	}` |
|      33 |  886 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      33 |  887 | `	SySetRelease(&aOut);` |
|      33 |  888 | `	return rc;` |
|      25 |  889 | `}` |
|       - |  890 | `/*` |
|       - |  891 | ` * array\|false class_implements($object_or_class, bool $autoload = true)` |
|       - |  892 | ` *  Every interface the class carries, in the order zend linked them (the one` |
|       - |  893 | ` *  Reflection publishes too -- see PH7_ReflectInterfacesOf).` |
|       - |  894 | ` */` |
|      66 |  895 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  896 | `{` |
|      68 |  897 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  898 | `	SySet aOut;` |
|       - |  899 | `	int rc;` |
|      68 |  900 | `	if( pClass == 0 ){` |
|      15 |  901 | `		ph7_result_bool(pCtx,0);` |
|      15 |  902 | `		return PH7_OK;` |
|       - |  903 | `	}` |
|      54 |  904 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|      54 |  905 | `	PH7_ReflectInterfacesOf(pCtx->pVm,pClass,&aOut);` |
|      54 |  906 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      54 |  907 | `	SySetRelease(&aOut);` |
|      54 |  908 | `	return rc;` |
|      35 |  909 | `}` |
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
|      88 |  936 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  937 | `{` |
|       - |  938 | `	ph7_value *pName,*pArray;` |
|       - |  939 | `	SyHashEntry *pEntry;` |
|       - |  940 | `	ph7_class *pClass;` |
|       - |  941 | `	/* Extract the target class first */` |
|      90 |  942 | `	pClass = 0;` |
|      90 |  943 | `	if( nArg > 0 ){` |
|      90 |  944 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      44 |  945 | `	}` |
|      90 |  946 | `	if( pClass == 0 ){` |
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
|      86 |  960 | `	pArray = ph7_context_new_array(pCtx);` |
|      86 |  961 | `	pName = ph7_context_new_scalar(pCtx);` |
|      86 |  962 | `	if( pArray == 0 \|\| pName == 0){` |
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
|      86 |  982 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      86 |  983 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    1270 |  984 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    1186 |  985 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       2 |  986 | `		}` |
|      86 |  987 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     210 |  988 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  989 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  990 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  991 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  992 | `			SySet aLvl;` |
|       - |  993 | `			SyHashEntry **apLvl;` |
|       - |  994 | `			sxu32 i,j;` |
|     126 |  995 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  996 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  997 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  998 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  999 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    2208 | 1000 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    2084 | 1001 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    2084 | 1002 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - | 1003 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - | 1004 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - | 1005 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - | 1006 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - | 1007 | `				 * methods before its own. */` |
|    2084 | 1008 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - | 1009 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - | 1010 | `				 * protected within the hierarchy, private only from the class that` |
|       - | 1011 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - | 1012 | `				 * every private and protected name a class holds. Same decision` |
|       - | 1013 | `				 * get_class_vars() already makes for properties. */` |
|    2084 | 1014 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 1015 | `					SyString sMName;` |
|      95 | 1016 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - | 1017 | `						apEntry[nPick]->nKeyLen);` |
|       - | 1018 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - | 1019 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - | 1020 | `					 * wrote it, and judged against the class that composed the method. */` |
|     142 | 1021 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      47 | 1022 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      47 | 1023 | `							pMethod->iProtection,FALSE) ){` |
|      81 | 1024 | `						continue;` |
|       - | 1025 | `					}` |
|       7 | 1026 | `				}` |
|    2004 | 1027 | `				if( pDecl != pLevel ){` |
|       - | 1028 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - | 1029 | `					 * counts as the class's own level, like php. */` |
|       - | 1030 | `					ph7_class *pWalk;` |
|     869 | 1031 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     283 | 1032 | `						continue;` |
|       - | 1033 | `					}` |
|    1219 | 1034 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|    1219 | 1035 | `						if( pWalk == pDecl ){` |
|     587 | 1036 | `							break;` |
|       - | 1037 | `						}` |
|     317 | 1038 | `					}` |
|     587 | 1039 | `					if( pWalk != 0 ){` |
|     587 | 1040 | `						continue; /* in-chain: its own level emits it */` |
|       - | 1041 | `					}` |
|     ! 0 | 1042 | `				}` |
|    1136 | 1043 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     569 | 1044 | `			}` |
|     126 | 1045 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - | 1046 | `			/* Insertion sort by declaration line (stable) */` |
|    1144 | 1047 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    1020 | 1048 | `				SyHashEntry *pKey = apLvl[i];` |
|    1084 | 1049 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|    1105 | 1050 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 | 1051 | `					apLvl[j] = apLvl[j-1];` |
|      33 | 1052 | `				}` |
|    1020 | 1053 | `				apLvl[j] = pKey;` |
|     511 | 1054 | `			}` |
|       - | 1055 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - | 1056 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - | 1057 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - | 1058 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - | 1059 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - | 1060 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - | 1061 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - | 1062 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - | 1063 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - | 1064 | `			 * claim still goes out at the end. */` |
|    1260 | 1065 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|    1136 | 1066 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|    1136 | 1067 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|    1136 | 1068 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|    1094 | 1069 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|    1094 | 1070 | `					apLvl[i] = 0;` |
|     546 | 1071 | `				}` |
|     569 | 1072 | `			}` |
|       - | 1073 | `			{` |
|     126 | 1074 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|     126 | 1075 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - | 1076 | `				sxu32 k;` |
|     142 | 1077 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 | 1078 | `					ph7_class *pTrait = apTrait[k];` |
|       - | 1079 | `					SySet aTr;` |
|       - | 1080 | `					SyHashEntry **apTr;` |
|       - | 1081 | `					SyHashEntry *pTrE;` |
|       - | 1082 | `					sxu32 t;` |
|      18 | 1083 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 | 1084 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 | 1085 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 | 1086 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 | 1087 | `					}` |
|      18 | 1088 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - | 1089 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - | 1090 | `					 * declaration order. */` |
|      48 | 1091 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 | 1092 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - | 1093 | `						int bWantAlias;` |
|       - | 1094 | `						/* First pass emits the aliases made from this method, second the` |
|       - | 1095 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 | 1096 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 | 1097 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1098 | `								ph7_class_method *pM;` |
|     342 | 1099 | `								if( apLvl[i] == 0 ){` |
|     184 | 1100 | `									continue;` |
|       - | 1101 | `								}` |
|     160 | 1102 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 | 1103 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 | 1104 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 | 1105 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 | 1106 | `										pOrigin->sFunc.sName.zString,` |
|      98 | 1107 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 | 1108 | `									continue;` |
|       - | 1109 | `								}` |
|      68 | 1110 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 | 1111 | `									continue;` |
|       - | 1112 | `								}` |
|      44 | 1113 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 | 1114 | `								apLvl[i] = 0;` |
|      23 | 1115 | `							}` |
|      32 | 1116 | `						}` |
|      17 | 1117 | `					}` |
|      18 | 1118 | `					SySetRelease(&aTr);` |
|      10 | 1119 | `				}` |
|       - | 1120 | `			}` |
|    1260 | 1121 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1122 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - | 1123 | `				 * that another trait then composed, say — keeps the line order. */` |
|    1136 | 1124 | `				if( apLvl[i] != 0 ){` |
|     ! 0 | 1125 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 | 1126 | `				}` |
|     569 | 1127 | `			}` |
|     126 | 1128 | `			SySetRelease(&aLvl);` |
|      64 | 1129 | `		}` |
|      86 | 1130 | `		SySetRelease(&aTmp);` |
|       - | 1131 | `	}` |
|       - | 1132 | `	/* Return the created array */` |
|      86 | 1133 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1134 | `	/*` |
|       - | 1135 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1136 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1137 | `	 */` |
|      86 | 1138 | `	return PH7_OK;` |
|      46 | 1139 | `}` |
|       - | 1140 | `/*` |
|       - | 1141 | ` * The class a TRAIT method's frame is really executing in: walk the receiver's ancestry` |
|       - | 1142 | `` * (or, with no receiver, the current `self`) to the first class that uses this trait.`` |
|       - | 1143 | `` * `class Base { use T; } class Kid extends Base {}` answers Base from a Kid instance, which`` |
|       - | 1144 | ` * is where php composed the method. The trait itself stands when nothing in the chain lists` |
|       - | 1145 | ` * it — a trait used by another trait reaching this through an unusual path; php has no such` |
|       - | 1146 | ` * scope, but a lie would be worse.` |
|       - | 1147 | ` */` |
|      88 | 1148 | `static ph7_class * VmTraitScopeFrom(ph7_vm *pVm,ph7_class *pTrait,VmFrame *pFrame)` |
|       4 | 1149 | `{` |
|      92 | 1150 | `	ph7_class *pWalk = pFrame ? pFrame->pSelfClass : 0;` |
|      92 | 1151 | `	if( pWalk == 0 ){` |
|       - | 1152 | `		/* No activation of its own (a closure body, an initializer): the ambient self. */` |
|     ! 0 | 1153 | `		pWalk = VmCurrentSelf(&(*pVm));` |
|     ! 0 | 1154 | `	}` |
|      92 | 1155 | `	return PH7_VmTraitUsingClass(&(*pVm),pTrait,pWalk ? pWalk : pTrait);` |
|       4 | 1156 | `}` |
|       - | 1157 | `/*` |
|       - | 1158 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - | 1159 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - | 1160 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - | 1161 | ` *` |
|       - | 1162 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - | 1163 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - | 1164 | ` * private/protected refusal raised from inside a class.` |
|       - | 1165 | ` */` |
|    4032 | 1166 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 | 1167 | `{` |
|    4037 | 1168 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1169 | `	ph7_vm_func *pVmFunc;` |
|    4037 | 1170 | `	ph7_class *pScope = 0;` |
|    4155 | 1171 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - | 1172 | `		/* Safely ignore the exception frame */` |
|     122 | 1173 | `		pFrame = pFrame->pParent;` |
|       4 | 1174 | `	}` |
|    4037 | 1175 | `	if( pFrame == 0 ){` |
|     ! 0 | 1176 | `		return 0;` |
|       - | 1177 | `	}` |
|    4037 | 1178 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 1179 | `	/* An INITIALIZER -- a class constant's, or an instance property's default -- is a` |
|       - | 1180 | `	 * mini-program run by VmLocalExec, which pushes no frame of its own. The frame still` |
|       - | 1181 | ``	 * current is therefore the one the `new` executed in, and every branch below would`` |
|       - | 1182 | `	 * answer the CONSTRUCTING class. php runs an initializer in its DECLARING class's` |
|       - | 1183 | ``	 * scope: `private const A;` beside `private string $x = self::A;` is readable from`` |
|       - | 1184 | ``	 * that initializer no matter where the `new` happens to be, and reading it as the`` |
|       - | 1185 | `	 * caller made a library's own tunable default an Error the moment the object was` |
|       - | 1186 | `	 * built from inside another class (phpcs's Tokens::WEIGHTINGS, Collision's` |
|       - | 1187 | `	 * Highlighter::ARROW_SYMBOL_UTF8 -- neither tool could START).` |
|       - | 1188 | `	 *` |
|       - | 1189 | `	 * pConstEvalClass alone is not the test: it stays set while the initializer runs, so` |
|       - | 1190 | `	 * a METHOD the initializer calls would inherit the initializer's scope. The marker is` |
|       - | 1191 | `	 * the frame the eval began in -- the same test PH7_VmPeekDeclaringClass makes for` |
|       - | 1192 | ``	 * `self::` -- and once a method pushes a frame it stops matching. */`` |
|    4032 | 1193 | `	if( pVm->pConstEvalClass` |
|    2022 | 1194 | `	 && pVm->pConstEvalFrame == (void *)VmSkipExceptionFrames(pVm->pFrame) ){` |
|       3 | 1195 | `		pScope = pVm->pConstEvalClass;` |
|       3 | 1196 | `		goto normalize_trait;` |
|       - | 1197 | `	}` |
|       - | 1198 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - | 1199 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|    4035 | 1200 | `	if( pFrame->pBoundScope ){` |
|      36 | 1201 | `		return pFrame->pBoundScope; /* an explicit rebind names a CLASS; never a trait */` |
|       - | 1202 | `	}` |
|    5999 | 1203 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    3091 | 1204 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|    2458 | 1205 | `	}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1206 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - | 1207 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - | 1208 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - | 1209 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - | 1210 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - | 1211 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - | 1212 | `		 * pBoundScope. */` |
|      71 | 1213 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|     880 | 1214 | `	}else if( pVm->pConstEvalClass ){` |
|       - | 1215 | `		/* Constant/property initializer bytecode runs without a method` |
|       - | 1216 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - | 1217 | `		 * constant is reachable from its own class's initializers). */` |
|     ! 0 | 1218 | `		pScope = pVm->pConstEvalClass;` |
|     ! 0 | 1219 | `	}` |
|     420 | 1220 | `normalize_trait:` |
|    4003 | 1221 | `	if( pScope && (pScope->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1222 | `		/* php COMPOSES a trait method into the using class at compile time, so the scope` |
|       - | 1223 | ``		 * its code executes in IS that class — `protected` members of the class are its`` |
|       - | 1224 | `		 * own from in there, and so are the class's protected METHODS. PHL shares a trait` |
|       - | 1225 | `		 * method by pointer and its declaring class stays the trait, so this answered the` |
|       - | 1226 | `		 * trait and every protected access from a trait body was refused. */` |
|      92 | 1227 | `		pScope = VmTraitScopeFrom(&(*pVm),pScope,pFrame);` |
|      44 | 1228 | `	}` |
|    4003 | 1229 | `	return pScope;` |
|    2021 | 1230 | `}` |
|       - | 1231 | `/*` |
|       - | 1232 | ` * The DECLARING-side twin of PH7_VmCallerScope: the class php NAMES as a method's` |
|       - | 1233 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - | 1234 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - | 1235 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - | 1236 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - | 1237 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - | 1238 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - | 1239 | ` *` |
|       - | 1240 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - | 1241 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - | 1242 | ` * refused on a child instance names the BASE.` |
|       - | 1243 | ` */` |
|     822 | 1244 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 | 1245 | `{` |
|     411 | 1246 | `	SXUNUSED(pVm);` |
|    1649 | 1247 | `	return PH7_VmComposingClass(pClass,` |
|     822 | 1248 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 | 1249 | `}` |
|       - | 1250 | `/*` |
|       - | 1251 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - | 1252 | ` * property is composed into the using class exactly as its methods are, and` |
|       - | 1253 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - | 1254 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - | 1255 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - | 1256 | ` */` |
|     868 | 1257 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 | 1258 | `{` |
|       - | 1259 | `	/* One rule, one implementation: a trait's members belong to the class that composed` |
|       - | 1260 | `	 * them, found by walking pClass's ancestry -- through NESTED trait use as well, since` |
|       - | 1261 | ``	 * php flattens `class C { use Outer; } trait Outer { use Inner; }` into C whole. This`` |
|       - | 1262 | `	 * walk used to look at each class's OWN trait list only, so a member reached through` |
|       - | 1263 | `	 * two levels kept the inner trait as its owner and every visibility rule about it` |
|       - | 1264 | `	 * became a question about the trait. */` |
|     873 | 1265 | `	return PH7_VmMemberOwnerClass(pDecl,pClass);` |
|       5 | 1266 | `}` |
|       - | 1267 | `/*` |
|       - | 1268 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1269 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1270 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1271 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1272 | ` * holds no entry for it.` |
|       - | 1273 | ` */` |
|      40 | 1274 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       2 | 1275 | `{` |
|      42 | 1276 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|      42 | 1277 | `	if( pEntry ){` |
|      42 | 1278 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      22 | 1279 | `	}else{` |
|     ! 0 | 1280 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1281 | `	}` |
|      42 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1285 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1286 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1287 | ` */` |
|  218444 | 1288 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1289 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1290 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1291 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1292 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1293 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1294 | `	)` |
|       5 | 1295 | `{` |
|  218449 | 1296 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    1153 | 1297 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    1153 | 1298 | `		if( pCallerScope == 0 ){` |
|     589 | 1299 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1300 | `		}` |
|     569 | 1301 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1302 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1303 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1304 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1305 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1306 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1307 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1308 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1309 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1310 | `			 * match (dynamic props and other non-declared shapes). */` |
|     329 | 1311 | `			ph7_class *pCaller = pCallerScope;` |
|     491 | 1312 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|     324 | 1313 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|     329 | 1314 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|     329 | 1315 | `			int bGranted = 0;` |
|     329 | 1316 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       4 | 1317 | `				if( pOwn->pDeclClass == 0` |
|       4 | 1318 | `				 \|\| pOwn->pDeclClass == pCaller` |
|       3 | 1319 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       5 | 1320 | `					bGranted = 1;` |
|       3 | 1321 | `				}` |
|     327 | 1322 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     221 | 1323 | `				bGranted = 1;` |
|     108 | 1324 | `			}` |
|     329 | 1325 | `			if( !bGranted ){` |
|       - | 1326 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1327 | `				ph7_class **apTrait;` |
|       - | 1328 | `				sxu32 nTrait,k;` |
|     107 | 1329 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     107 | 1330 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     107 | 1331 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1332 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1333 | `						bGranted = 1;` |
|     ! 0 | 1334 | `						break;` |
|       - | 1335 | `					}` |
|     ! 0 | 1336 | `				}` |
|      52 | 1337 | `			}` |
|     329 | 1338 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1339 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1340 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1341 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1342 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1343 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1344 | `				 * whose owner is the trait, not the class). */` |
|       - | 1345 | `				ph7_class **apTrait;` |
|       - | 1346 | `				sxu32 nTrait,k;` |
|     ! 0 | 1347 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     ! 0 | 1348 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     ! 0 | 1349 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1350 | `					if( apTrait[k] == pClass ){` |
|     ! 0 | 1351 | `						bGranted = 1;` |
|     ! 0 | 1352 | `						break;` |
|       - | 1353 | `					}` |
|     ! 0 | 1354 | `				}` |
|     ! 0 | 1355 | `			}` |
|     329 | 1356 | `			if( !bGranted ){` |
|     107 | 1357 | `				goto dis; /* Access is forbidden */` |
|       - | 1358 | `			}` |
|     115 | 1359 | `		}else{` |
|       - | 1360 | `			/* Protected */` |
|     245 | 1361 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1362 | `			/* The walk below exists for one shape only -- a SIBLING scope, which reaches` |
|       - | 1363 | `			 * a protected member through the ancestor that INTRODUCES it. When the two` |
|       - | 1364 | `			 * classes are on one inheritance chain the walk cannot change the answer, so` |
|       - | 1365 | `			 * it is skipped: the introducing class is by construction an ancestor-or-self` |
|       - | 1366 | `			 * of pClass, so it lies on that same chain, and one of the two directions of` |
|       - | 1367 | `			 * the test below therefore holds however far the walk climbs. Asked over the` |
|       - | 1368 | `			 * pBase links alone rather than through PH7_VmInstanceOf, so an interface in` |
|       - | 1369 | `			 * the picture cannot make the shortcut claim more than the walk would. It is` |
|       - | 1370 | `			 * the ordinary case -- a class touching its own protected member -- and the` |
|       - | 1371 | `			 * walk it skips costs two hash lookups per ANCESTOR, per access. */` |
|       - | 1372 | `			{` |
|       - | 1373 | `				ph7_class *pChain;` |
|     337 | 1374 | `				for( pChain = pClass ; pChain ; pChain = pChain->pBase ){` |
|     283 | 1375 | `					if( pChain == pBase ){` |
|     191 | 1376 | `						return 1;` |
|       - | 1377 | `					}` |
|      50 | 1378 | `				}` |
|     117 | 1379 | `				for( pChain = pBase ; pChain ; pChain = pChain->pBase ){` |
|     101 | 1380 | `					if( pChain == pClass ){` |
|      41 | 1381 | `						return 1;` |
|       - | 1382 | `					}` |
|      33 | 1383 | `				}` |
|       - | 1384 | `			}` |
|       - | 1385 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1386 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1387 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1388 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1389 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1390 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1391 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1392 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1393 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1394 | `			 * from a sibling, matching php. */` |
|      17 | 1395 | `			ph7_class *pIntro = pClass;` |
|       - | 1396 | `			ph7_class *pAnc;` |
|      39 | 1397 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|      23 | 1398 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|      23 | 1399 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|      23 | 1400 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|      23 | 1401 | `				int bHere = 0;` |
|      23 | 1402 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      15 | 1403 | `					bHere = 1;` |
|       7 | 1404 | `				}` |
|      23 | 1405 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|       7 | 1406 | `					bHere = 1;` |
|       3 | 1407 | `				}` |
|      23 | 1408 | `				if( bHere ){` |
|      21 | 1409 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      10 | 1410 | `				}` |
|      12 | 1411 | `			}` |
|       - | 1412 | `			/* Must be in the same class hierarchy as the introducing class */` |
|      17 | 1413 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      13 | 1414 | `				int bTraitGrant = 0;` |
|      13 | 1415 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1416 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1417 | `					ph7_class **apTrait;` |
|       - | 1418 | `					sxu32 nTrait,k;` |
|     ! 0 | 1419 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     ! 0 | 1420 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     ! 0 | 1421 | `					for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1422 | `						if( apTrait[k] == pClass ){` |
|     ! 0 | 1423 | `							bTraitGrant = 1;` |
|     ! 0 | 1424 | `							break;` |
|       - | 1425 | `						}` |
|     ! 0 | 1426 | `					}` |
|     ! 0 | 1427 | `				}` |
|      13 | 1428 | `				if( !bTraitGrant ){` |
|      13 | 1429 | `					goto dis; /* Access is forbidden */` |
|       - | 1430 | `				}` |
|     ! 0 | 1431 | `			}` |
|       - | 1432 | `		}` |
|     112 | 1433 | `	}` |
|  217525 | 1434 | `	return 1; /* Access is granted */` |
|     350 | 1435 | `dis:` |
|     705 | 1436 | `	if( bLog ){` |
|     ! 0 | 1437 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1438 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1439 | `			&pClass->sName,pAttrName);` |
|     ! 0 | 1440 | `	}` |
|     705 | 1441 | `	return 0; /* Access is forbidden */` |
|  109227 | 1442 | `}` |
|       - | 1443 | `/*` |
|       - | 1444 | ` * The same question about one PROPERTY rather than about a NAME.` |
|       - | 1445 | ` *` |
|       - | 1446 | ` * A private member belongs to a SLOT, not to a name: once a base's private` |
|       - | 1447 | ` * instance property is carried into a subclass under php's mangled storage name` |
|       - | 1448 | ` * (PH7_ClassAttrStorageName), one object can hold two of them, and the name-based` |
|       - | 1449 | ` * rule above would hand the executing scope whichever it was shown -- a subclass` |
|       - | 1450 | `` * method reading its own `$q` would be granted its base's `$q` just as readily.`` |
|       - | 1451 | ` * php decides an inherited private by identity alone: the executing scope IS the` |
|       - | 1452 | ` * declaring class, or the slot is not reachable at all.` |
|       - | 1453 | ` *` |
|       - | 1454 | ` * A property the reflected class DECLARED is left to the name-based rule, whose` |
|       - | 1455 | ` * trait grants and legacy fallbacks are what every other caller has always had.` |
|       - | 1456 | ` */` |
|  220000 | 1457 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog)` |
|       5 | 1458 | `{` |
|  220000 | 1459 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  111315 | 1460 | `	 && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2625 | 1461 | `		ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|    2625 | 1462 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    2625 | 1463 | `		int bDeny = 0;` |
|    2625 | 1464 | `		if( pScope && pOwner ){` |
|    2489 | 1465 | `			if( pScope == pOwner ){` |
|    2461 | 1466 | `				return 1;   /* php's rule, stated positively */` |
|       - | 1467 | `			}` |
|      30 | 1468 | `			if( pOwner != pClass ){` |
|       - | 1469 | `				/* An INHERITED private: this slot is the declaring class's and no` |
|       - | 1470 | `				 * one else's, so identity is the whole rule. */` |
|      27 | 1471 | `				bDeny = 1;` |
|      14 | 1472 | `			}else{` |
|       - | 1473 | `				/* The scope declares a private of this NAME, but it is a member of` |
|       - | 1474 | `				 * its own -- granting on the name would hand a base method the` |
|       - | 1475 | `				 * CHILD's property (and the other way round). Properties and` |
|       - | 1476 | `				 * constants are php's two separate namespaces, so ask the table` |
|       - | 1477 | `				 * this member belongs to. */` |
|       4 | 1478 | `				SyHash *pTab = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|       2 | 1479 | `					? &pScope->hConst : &pScope->hAttr;` |
|       4 | 1480 | `				SyHashEntry *pE = SyHashGet(pTab,` |
|       2 | 1481 | `					(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|       3 | 1482 | `				ph7_class_attr *pOwn = pE ? (ph7_class_attr *)pE->pUserData : 0;` |
|       3 | 1483 | `				if( pOwn && pOwn != pAttr && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 1484 | `					bDeny = 1;` |
|     ! 0 | 1485 | `				}` |
|       - | 1486 | `			}` |
|      14 | 1487 | `		}` |
|     167 | 1488 | `		if( bDeny ){` |
|      27 | 1489 | `			if( bLog ){` |
|     ! 0 | 1490 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1491 | `					"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1492 | `					&pClass->sName,&pAttr->sName);` |
|     ! 0 | 1493 | `			}` |
|      27 | 1494 | `			return 0;` |
|       - | 1495 | `		}` |
|      69 | 1496 | `	}` |
|  217523 | 1497 | `	return PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,bLog);` |
|  110005 | 1498 | `}` |
|       - | 1499 | `/*` |
|       - | 1500 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1501 | ` *   Get the default properties of the class` |
|       - | 1502 | ` * Parameters` |
|       - | 1503 | ` *  class_name` |
|       - | 1504 | ` *   The class name or class instance` |
|       - | 1505 | ` * Return` |
|       - | 1506 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1507 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1508 | ` *  of varname => value.` |
|       - | 1509 | ` * Note:` |
|       - | 1510 | ` *   NULL is returned on failure.` |
|       - | 1511 | ` */` |
|      22 | 1512 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1513 | `{` |
|       - | 1514 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1515 | `	SyHashEntry *pEntry;` |
|       - | 1516 | `	ph7_class *pClass;` |
|       - | 1517 | `	/* Extract the target class first */` |
|      24 | 1518 | `	pClass = 0;` |
|      24 | 1519 | `	if( nArg > 0 ){` |
|      24 | 1520 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      11 | 1521 | `	}` |
|      24 | 1522 | `	if( pClass == 0 ){` |
|       - | 1523 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1524 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1525 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1526 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1527 | `		int nLen = 0;` |
|     ! 0 | 1528 | `		const char *zVal = "";` |
|     ! 0 | 1529 | `		if( nArg > 0 ){` |
|     ! 0 | 1530 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1531 | `				zVal = "Array";` |
|     ! 0 | 1532 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1533 | `			}else{` |
|     ! 0 | 1534 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1535 | `			}` |
|     ! 0 | 1536 | `		}` |
|     ! 0 | 1537 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1538 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1539 | `			nLen,zVal);` |
|       - | 1540 | `	}` |
|      24 | 1541 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1542 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1543 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1544 | `		 * here, as it does in php. */` |
|       5 | 1545 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 | 1546 | `		if( rcMat != SXRET_OK ){` |
|       5 | 1547 | `			return rcMat;` |
|       - | 1548 | `		}` |
|     ! 0 | 1549 | `	}` |
|       - | 1550 | `	/* Create a new array  */` |
|      20 | 1551 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 1552 | `	pName = ph7_context_new_scalar(pCtx);` |
|      20 | 1553 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      20 | 1554 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1555 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1556 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1557 | `		return PH7_OK;` |
|       - | 1558 | `	}` |
|       - | 1559 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|      20 | 1560 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      58 | 1561 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      40 | 1562 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      40 | 1563 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1564 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1565 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 | 1566 | `			continue;` |
|       - | 1567 | `		}` |
|       - | 1568 | `		/* Check if the access is allowed */` |
|      38 | 1569 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      30 | 1570 | `			SyString *pAttrName = &pAttr->sName;` |
|      30 | 1571 | `			ph7_value *pValue = 0;` |
|      30 | 1572 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1573 | `				/* Static slots are computed at mount; constants lazily */` |
|       8 | 1574 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       8 | 1575 | `				pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       5 | 1576 | `			}else{` |
|      24 | 1577 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 | 1578 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1579 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 | 1580 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 | 1581 | `					pValue = &sValue;` |
|       2 | 1582 | `				}` |
|       - | 1583 | `			}` |
|       - | 1584 | `			/* Fill in the array */` |
|      30 | 1585 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      30 | 1586 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1587 | `			/* Reset the cursor */` |
|      30 | 1588 | `			ph7_value_reset_string_cursor(pName);` |
|      14 | 1589 | `		}` |
|       2 | 1590 | `	}` |
|      20 | 1591 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1592 | `	/* Return the created array */` |
|      20 | 1593 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1594 | `	/*` |
|       - | 1595 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1596 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1597 | `	 */` |
|      20 | 1598 | `	return PH7_OK;` |
|      13 | 1599 | `}` |
|       - | 1600 | `/*` |
|       - | 1601 | ` * array get_object_vars(object $this)` |
|       - | 1602 | ` *   Gets the properties of the given object` |
|       - | 1603 | ` * Parameters` |
|       - | 1604 | ` *  this` |
|       - | 1605 | ` *   A class instance` |
|       - | 1606 | ` * Return` |
|       - | 1607 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1608 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1609 | ` *  it will be returned with a NULL value.` |
|       - | 1610 | ` * Note:` |
|       - | 1611 | ` *   NULL is returned on failure.` |
|       - | 1612 | ` */` |
|     354 | 1613 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1614 | `{` |
|     358 | 1615 | `	ph7_class_instance *pThis = 0;` |
|       - | 1616 | `	ph7_value *pName,*pArray;` |
|       - | 1617 | `	SyHashEntry *pEntry;` |
|     358 | 1618 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1619 | `		/* Extract the target instance */` |
|     358 | 1620 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     177 | 1621 | `	}` |
|     358 | 1622 | `	if( pThis == 0 ){` |
|       - | 1623 | `		/* No such instance,return NULL */` |
|     ! 0 | 1624 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1625 | `		return PH7_OK;` |
|       - | 1626 | `	}` |
|       - | 1627 | `	/* Create a new array  */` |
|     358 | 1628 | `	pArray = ph7_context_new_array(pCtx);` |
|     358 | 1629 | `	pName = ph7_context_new_scalar(pCtx);` |
|     358 | 1630 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1631 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1632 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1633 | `		return PH7_OK;` |
|       - | 1634 | `	}` |
|       - | 1635 | `	/* A class whose get_properties handler answers this purpose too: php asks` |
|       - | 1636 | `	 * the handler with ZEND_PROP_PURPOSE_GET_OBJECT_VARS, and SimpleXMLElement` |
|       - | 1637 | `	 * is the one native class here that answers it with the same table it shows` |
|       - | 1638 | `	 * var_dump. Every other one answers its real (empty) slots, which is the` |
|       - | 1639 | `	 * walk below. */` |
|     354 | 1640 | `	if( PH7_ClassVarsFromPresent(pThis->pClass)` |
|     182 | 1641 | `	 && PH7_ClassInstancePresent(pThis,pArray,0) ){` |
|       3 | 1642 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 1643 | `		return PH7_OK;` |
|       - | 1644 | `	}` |
|       - | 1645 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1646 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1647 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1648 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1649 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1650 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1651 | `	 * skipped instead of read after free. The name snapshotted is the STORAGE` |
|       - | 1652 | `	 * key -- an inherited private lives under php's mangled one -- while the key` |
|       - | 1653 | `	 * php puts in the answer is the property's plain name. */` |
|       - | 1654 | `	{` |
|       - | 1655 | `		SySet sNames;` |
|       - | 1656 | `		SyString *aName;` |
|       - | 1657 | `		sxu32 iName,nName;` |
|     356 | 1658 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     356 | 1659 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    1564 | 1660 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    1212 | 1661 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    1212 | 1662 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|       - | 1663 | `				/* Only non-static/constant attributes are extracted */` |
|     388 | 1664 | `				continue;` |
|       - | 1665 | `			}` |
|     828 | 1666 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      34 | 1667 | `				continue; /* typed, never written: not there yet (php) */` |
|       - | 1668 | `			}` |
|     792 | 1669 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     400 | 1670 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     ! 0 | 1671 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1672 | `			}` |
|     796 | 1673 | `			if( PH7_ClassInstanceAttrShadowed(pCtx->pVm,pThis,pEntry) ){` |
|       3 | 1674 | `				continue; /* an earlier accessible slot already answers for this name */` |
|       - | 1675 | `			}` |
|       - | 1676 | `			{` |
|       - | 1677 | `				SyString sKey;` |
|     794 | 1678 | `				SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     794 | 1679 | `				SySetPut(&sNames,(const void *)&sKey);` |
|       - | 1680 | `			}` |
|       4 | 1681 | `		}` |
|     356 | 1682 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     356 | 1683 | `		nName = SySetUsed(&sNames);` |
|    1146 | 1684 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     794 | 1685 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1686 | `			VmClassAttr *pVmAttr;` |
|     794 | 1687 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     794 | 1688 | `			if( pEntry == 0 ){` |
|     ! 0 | 1689 | `				continue; /* unset by an earlier hook */` |
|       - | 1690 | `			}` |
|     794 | 1691 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1692 | `			/* Check if the access is allowed */` |
|     794 | 1693 | `			if( PH7_VmClassAttrAccess(pCtx->pVm,pThis->pClass,pVmAttr->pAttr,FALSE) ){` |
|     636 | 1694 | `				ph7_value *pValue = 0;` |
|       - | 1695 | `				ph7_value sHookVal;` |
|       - | 1696 | `				sxi32 rcHk;` |
|       - | 1697 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1698 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     636 | 1699 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     636 | 1700 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     636 | 1701 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1702 | `					pValue = &sHookVal;` |
|     629 | 1703 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1704 | `					/* Extract attribute */` |
|     622 | 1705 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     313 | 1706 | `				}else{` |
|       - | 1707 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1708 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1709 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1710 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1711 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1712 | `					break;` |
|       - | 1713 | `				}` |
|     636 | 1714 | `				if( pValue ){` |
|       - | 1715 | `					/* Insert attribute name in the array -- php unmangles it. */` |
|     952 | 1716 | `					ph7_value_string(pName,SyStringData(&pVmAttr->pAttr->sName),` |
|     632 | 1717 | `						(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     632 | 1718 | `					if( rcHk == SXERR_NOTFOUND` |
|     629 | 1719 | `					 && PH7_ClassAttrIsRef(pThis,pVmAttr) ){` |
|       - | 1720 | `						/* php hands out the property's own REFERENCE for a property that` |
|       - | 1721 | `						 * IS one, so a write through the returned element reaches the` |
|       - | 1722 | `						 * object. A HOOK's answer is a computed value with no slot behind` |
|       - | 1723 | `						 * it and stays a copy. */` |
|      74 | 1724 | `						PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,` |
|      24 | 1725 | `							pName,pVmAttr->nIdx);` |
|      26 | 1726 | `					}else{` |
|     588 | 1727 | `						ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1728 | `					}` |
|     316 | 1729 | `				}` |
|     636 | 1730 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1731 | `				/* Reset the cursor */` |
|     636 | 1732 | `				ph7_value_reset_string_cursor(pName);` |
|     316 | 1733 | `			}` |
|     399 | 1734 | `		}` |
|     356 | 1735 | `		SySetRelease(&sNames);` |
|       - | 1736 | `	}` |
|       - | 1737 | `	/* Return the created array */` |
|     356 | 1738 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1739 | `	/*` |
|       - | 1740 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1741 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1742 | `	 */` |
|     356 | 1743 | `	return PH7_OK;` |
|     181 | 1744 | `}` |
|       - | 1745 | `/*` |
|       - | 1746 | ` * array get_mangled_object_vars(object $object)` |
|       - | 1747 | ` *  The object's own property table, with php's visibility MANGLING left on the keys:` |
|       - | 1748 | `` *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".`` |
|       - | 1749 | ` *` |
|       - | 1750 | ` *  It is get_object_vars()'s opposite in both of that function's decisions — no` |
|       - | 1751 | ` *  visibility screen (every property is reported, from every level of the chain) and` |
|       - | 1752 | `` *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,`` |
|       - | 1753 | ` *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not` |
|       - | 1754 | ` *  the (array) cast either: the cast asks a native class's own handler, so` |
|       - | 1755 | `` *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.`` |
|       - | 1756 | ` */` |
|      46 | 1757 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1758 | `{` |
|      48 | 1759 | `	ph7_class_instance *pThis = 0;` |
|       - | 1760 | `	ph7_value *pArray;` |
|      48 | 1761 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1762 | `		/* Extract the target instance */` |
|      48 | 1763 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      23 | 1764 | `	}` |
|      48 | 1765 | `	if( pThis == 0 ){` |
|       - | 1766 | ``		/* The `object $object` signature row refuses everything else before we run */`` |
|     ! 0 | 1767 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1768 | `		return PH7_OK;` |
|       - | 1769 | `	}` |
|      48 | 1770 | `	pArray = ph7_context_new_array(pCtx);` |
|      48 | 1771 | `	if( pArray == 0 ){` |
|       - | 1772 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1773 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1774 | `		return PH7_OK;` |
|       - | 1775 | `	}` |
|      48 | 1776 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);` |
|      48 | 1777 | `	ph7_result_value(pCtx,pArray);` |
|      48 | 1778 | `	return PH7_OK;` |
|      25 | 1779 | `}` |
|       - | 1780 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1781 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1782 | ` * detection should reject them up front. */` |
|       - | 1783 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1784 | `/*` |
|       - | 1785 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1786 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1787 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1788 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1789 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1790 | ` */` |
| 2542058 | 1791 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1792 | `{` |
| 2622947 | 1793 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1794 | `		ph7_class **apParent;` |
|       - | 1795 | `		sxu32 n;` |
| 2570009 | 1796 | `		if( pIface == pTarget ){` |
| 2489115 | 1797 | `			return TRUE;` |
|       - | 1798 | `		}` |
|       - | 1799 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1800 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|   80899 | 1801 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   80917 | 1802 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|      29 | 1803 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|      11 | 1804 | `				return TRUE;` |
|       - | 1805 | `			}` |
|      10 | 1806 | `		}` |
|   80889 | 1807 | `		pIface = pIface->pBase;` |
|   80889 | 1808 | `		iDepth++;` |
|       5 | 1809 | `	}` |
|   52943 | 1810 | `	return FALSE;` |
| 1270960 | 1811 | `}` |
|       - | 1812 | `/*` |
|       - | 1813 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1814 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1815 | ` */` |
| 2808063 | 1816 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1817 | `{` |
|       - | 1818 | `	ph7_class **apInterface;` |
|       - | 1819 | `	sxu32 n;` |
| 2808068 | 1820 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1821 | `		/* Empty interface container */` |
|  281086 | 1822 | `		return FALSE;` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Point to the set of implemented interfaces */` |
| 2526987 | 1825 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1826 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1827 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2579907 | 1828 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2542035 | 1829 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2489115 | 1830 | `			return TRUE;` |
|       - | 1831 | `		}` |
|   26448 | 1832 | `	}` |
|   37877 | 1833 | `	return FALSE;` |
| 1403777 | 1834 | `}` |
|       - | 1835 | `/*` |
|       - | 1836 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1837 | ` * is an instance of the main class (second argument).` |
|       - | 1838 | ` * Otherwise FALSE is returned.` |
|       - | 1839 | ` */` |
| 4126929 | 1840 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1841 | `{` |
|       - | 1842 | `	ph7_class *pParent;` |
|       - | 1843 | `	sxi32 rc;` |
| 4126934 | 1844 | `	if( pThis == pClass ){` |
|       - | 1845 | `		/* Instance of the same class */` |
| 1471746 | 1846 | `		return TRUE;` |
|       - | 1847 | `	}` |
|       - | 1848 | `	/* Check implemented interfaces */` |
| 2655193 | 1849 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2655193 | 1850 | `	if( rc ){` |
| 2360539 | 1851 | `		return TRUE;` |
|       - | 1852 | `	}` |
|       - | 1853 | `	/* Check parent classes */` |
|  294659 | 1854 | `	pParent = pThis->pBase;` |
|  318934 | 1855 | `	while( pParent ){` |
|  155632 | 1856 | `		if( pParent == pClass ){` |
|       - | 1857 | `			/* Same instance */` |
|    2791 | 1858 | `			return TRUE;` |
|       - | 1859 | `		}` |
|       - | 1860 | `		/* Check the implemented interfaces */` |
|  152846 | 1861 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  152846 | 1862 | `		if( rc ){` |
|  128571 | 1863 | `			return TRUE;` |
|       - | 1864 | `		}` |
|       - | 1865 | `		/* Point to the parent class */` |
|   24280 | 1866 | `		pParent = pParent->pBase;` |
|       5 | 1867 | `	}` |
|       - | 1868 | `	/* Not an instance of the the given class */` |
|  163307 | 1869 | `	return FALSE;` |
| 2063280 | 1870 | `}` |
|       - | 1871 | `/*` |
|       - | 1872 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1873 | ` * is a subclass of the main class (second argument).` |
|       - | 1874 | ` * Otherwise FALSE is returned.` |
|       - | 1875 | ` */` |
|      44 | 1876 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       2 | 1877 | `{` |
|       - | 1878 | `	SyHashEntry *pEntry;` |
|       - | 1879 | `	SyString *pName;` |
|      70 | 1880 | `	while( pClass ){` |
|      66 | 1881 | `		pName = &pClass->sName;` |
|       - | 1882 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      66 | 1883 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      66 | 1884 | `		if( pEntry ){` |
|      32 | 1885 | `			return TRUE;` |
|       - | 1886 | `		}` |
|       - | 1887 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1888 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1889 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1890 | `		 * class's aInterface, missing inherited interfaces. */` |
|      35 | 1891 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1892 | `			return TRUE;` |
|       - | 1893 | `		}` |
|      25 | 1894 | `		pClass = pClass->pBase;` |
|       1 | 1895 | `	}` |
|       - | 1896 | `	/* Not a subclass */` |
|       5 | 1897 | `	return FALSE;` |
|      24 | 1898 | `}` |
|       - | 1899 | `/*` |
|       - | 1900 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1901 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1902 | ` *   as one of its parents.` |
|       - | 1903 | ` * Parameters` |
|       - | 1904 | ` *  object_or_class` |
|       - | 1905 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1906 | ` * class` |
|       - | 1907 | ` *  The class or interface name to test against.` |
|       - | 1908 | ` * allow_string` |
|       - | 1909 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1910 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1911 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1912 | ` * Return` |
|       - | 1913 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1914 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1915 | ` */` |
|      34 | 1916 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1917 | `{` |
|      37 | 1918 | `	int res = 0; /* Assume FALSE by default */` |
|      37 | 1919 | `	if( nArg > 1 ){` |
|      37 | 1920 | `		ph7_class *pThisClass = 0;` |
|      37 | 1921 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1922 | `			/* An object first argument: allow_string is ignored (php). */` |
|      17 | 1923 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      29 | 1924 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1925 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1926 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1927 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1928 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      18 | 1929 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       8 | 1930 | `		}` |
|      37 | 1931 | `		if( pThisClass ){` |
|       - | 1932 | `			/* Extract the given class */` |
|      31 | 1933 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      31 | 1934 | `			if( pClass ){` |
|       - | 1935 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1936 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      31 | 1937 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      14 | 1938 | `			}` |
|      14 | 1939 | `		}` |
|      17 | 1940 | `	}` |
|       - | 1941 | `	/* Query result */` |
|      37 | 1942 | `	ph7_result_bool(pCtx,res);` |
|      37 | 1943 | `	return PH7_OK;` |
|       3 | 1944 | `}` |
|       - | 1945 | `/*` |
|       - | 1946 | ` * int spl_object_id(object $object)` |
|       - | 1947 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1948 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1949 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1950 | ` */` |
|      26 | 1951 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1952 | `{` |
|       - | 1953 | `	ph7_class_instance *pThis;` |
|      30 | 1954 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1955 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1956 | `		return PH7_OK;` |
|       - | 1957 | `	}` |
|      30 | 1958 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      30 | 1959 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      30 | 1960 | `	return PH7_OK;` |
|      17 | 1961 | `}` |
|       - | 1962 | `/*` |
|       - | 1963 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 1964 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 1965 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 1966 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 1967 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 1968 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 1969 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 1970 | ` *` |
|       - | 1971 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 1972 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 1973 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 1974 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 1975 | ` */` |
|      50 | 1976 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1977 | `{` |
|      51 | 1978 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 1979 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1980 | `	char zGiven[64];` |
|       - | 1981 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 1982 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      51 | 1983 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1984 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1985 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 1986 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 1987 | `	}` |
|      51 | 1988 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 1989 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 1990 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 1991 | `	 * resource, and Generator/Fiber. */` |
|      50 | 1992 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|      48 | 1993 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      13 | 1994 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       8 | 1995 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sName);` |
|       - | 1996 | `	}` |
|      43 | 1997 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 1998 | `	if( pClone == 0 ){` |
|     ! 0 | 1999 | `		return PH7_VmMemoryError(pVm);` |
|       - | 2000 | `	}` |
|       - | 2001 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 2002 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 2003 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 2004 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 2005 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 2006 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 2007 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 2008 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 2009 | `		sxu32 n;` |
|      53 | 2010 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 2011 | `			ph7_value *pVal,sVal;` |
|       - | 2012 | `			const char *zName;` |
|       - | 2013 | `			sxu32 nName;` |
|       - | 2014 | `			char zKeyBuf[64];` |
|       - | 2015 | `			sxi32 rc;` |
|      31 | 2016 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 2017 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 2018 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 2019 | `				zName = zKeyBuf;` |
|     ! 0 | 2020 | `			}else{` |
|      31 | 2021 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 2022 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 2023 | `			}` |
|      31 | 2024 | `			pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 2025 | `			if( pVal ){` |
|       - | 2026 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 2027 | `				 * property, whose slot reservation used to reallocate pVm->aMemObj` |
|       - | 2028 | `				 * and dangle pVal. Redundant since P1 (fixed segments); left for the` |
|       - | 2029 | `				 * harvest sweep (PERF.md P1). */` |
|      31 | 2030 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 2031 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 2032 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 2033 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 2034 | `				if( rc != SXRET_OK ){` |
|       7 | 2035 | `					return rc;` |
|       - | 2036 | `				}` |
|      12 | 2037 | `			}` |
|      25 | 2038 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 2039 | `		}` |
|      11 | 2040 | `	}` |
|      37 | 2041 | `	return PH7_OK;` |
|      26 | 2042 | `}` |
|       - | 2043 | `/*` |
|       - | 2044 | ` * string spl_object_hash(object $object)` |
|       - | 2045 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 2046 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 2047 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 2048 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 2049 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 2050 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 2051 | ` */` |
|      18 | 2052 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2053 | `{` |
|       - | 2054 | `	ph7_class_instance *pThis;` |
|      20 | 2055 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2056 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2057 | `		return PH7_OK;` |
|       - | 2058 | `	}` |
|      20 | 2059 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 2060 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 2061 | `	return PH7_OK;` |
|      11 | 2062 | `}` |
|       - | 2063 | `/*` |
|       - | 2064 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 2065 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 2066 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 2067 | ` * Parameters` |
|       - | 2068 | ` *  object_or_class` |
|       - | 2069 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 2070 | ` * class` |
|       - | 2071 | ` *  The class or interface name to test against.` |
|       - | 2072 | ` * allow_string` |
|       - | 2073 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 2074 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 2075 | ` *  object first argument (php).` |
|       - | 2076 | ` * Return` |
|       - | 2077 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 2078 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 2079 | ` */` |
|      56 | 2080 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2081 | `{` |
|      58 | 2082 | `	int res = 0; /* Assume FALSE by default */` |
|      58 | 2083 | `	if( nArg > 1 ){` |
|      58 | 2084 | `		ph7_class *pClass = 0;` |
|      58 | 2085 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 2086 | `			/* An object first argument: allow_string is ignored (php). */` |
|      20 | 2087 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      49 | 2088 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 2089 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 2090 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 2091 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 2092 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      32 | 2093 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      15 | 2094 | `		}` |
|      58 | 2095 | `		if( pClass ){` |
|       - | 2096 | `			/* Extract the target class */` |
|      48 | 2097 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      48 | 2098 | `			if( pMain ){` |
|       - | 2099 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      46 | 2100 | `				res = VmSubclassOf(pClass,pMain);` |
|      22 | 2101 | `			}` |
|      23 | 2102 | `		}` |
|      28 | 2103 | `	}` |
|       - | 2104 | `	/* Query result */` |
|      58 | 2105 | `	ph7_result_bool(pCtx,res);` |
|      58 | 2106 | `	return PH7_OK;` |
|       2 | 2107 | `}` |
|     248 | 2108 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2109 | `{` |
|       - | 2110 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 2111 | `	sxi32 rc;` |
|     253 | 2112 | `	if( nArg < 1 ){` |
|       - | 2113 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2114 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2115 | `		return PH7_OK;` |
|       - | 2116 | `	}` |
|       - | 2117 | `	{` |
|       - | 2118 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 2119 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     253 | 2120 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     253 | 2121 | `		if( rcCb != PH7_OK ){` |
|      72 | 2122 | `			return rcCb;` |
|       - | 2123 | `		}` |
|       - | 2124 | `	}` |
|     183 | 2125 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     183 | 2126 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2127 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     183 | 2128 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 2129 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 2130 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 2131 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 2132 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 2133 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 2134 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     193 | 2135 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      21 | 2136 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 2137 | `		VmCallArgMap sInner;` |
|       - | 2138 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 2139 | `		 * unset when forwarded, not as stack garbage. */` |
|      21 | 2140 | `		SyZero(&sInner,sizeof(sInner));` |
|      21 | 2141 | `		sInner.bHasNamed = 1;` |
|      21 | 2142 | `		sInner.bIsNamespaced = 0;` |
|       - | 2143 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 2144 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 2145 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 2146 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      21 | 2147 | `		sInner.bStrict = 0;` |
|      21 | 2148 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      21 | 2149 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|       - | 2150 | ``		/* php's compiler rewrites `call_user_func(f, ...)` into a direct call to`` |
|       - | 2151 | `		 * f, so a DROPPED answer here is a dropped answer for the callback: hand` |
|       - | 2152 | `		 * the bit on, one call deep. */` |
|      21 | 2153 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|      21 | 2154 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      11 | 2155 | `	}else{` |
|       - | 2156 | `		/* call_user_func is one of php's two FORWARDS: the callback binds under the` |
|       - | 2157 | `		 * mode of the file that wrote the call_user_func, not weakly like every other` |
|       - | 2158 | `		 * internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 2159 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 2160 | `		 * other internal invocation, and wrong here). */` |
|       - | 2161 | `		VmCallArgMap sFwd;` |
|     163 | 2162 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     163 | 2163 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     163 | 2164 | `		pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;   /* see above */` |
|     163 | 2165 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 2166 | `	}` |
|       - | 2167 | `	/* The latch is consumed by the OP_CALL the dispatch builds; clear it for the` |
|       - | 2168 | `	 * paths that never reach one, so it cannot describe some later call. */` |
|     183 | 2169 | `	pCtx->pVm->bDiscardCallback = 0;` |
|     183 | 2170 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2171 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 2172 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      25 | 2173 | `		PH7_MemObjRelease(&sResult);` |
|      25 | 2174 | `		return PH7_EXCEPTION;` |
|       - | 2175 | `	}` |
|     161 | 2176 | `	if( rc != SXRET_OK ){` |
|       - | 2177 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2178 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2179 | `	}else{` |
|       - | 2180 | `		/* Callback result */` |
|     161 | 2181 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2182 | `	}` |
|     161 | 2183 | `	PH7_MemObjRelease(&sResult);` |
|     161 | 2184 | `	return PH7_OK;` |
|     129 | 2185 | `}` |
|       - | 2186 | `/*` |
|       - | 2187 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 2188 | ` *  Call a callback with an array of parameters.` |
|       - | 2189 | ` * Parameter` |
|       - | 2190 | ` *  $callback` |
|       - | 2191 | ` *   The callable to be called.` |
|       - | 2192 | ` * $param_arr` |
|       - | 2193 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 2194 | ` * Return` |
|       - | 2195 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 2196 | ` */` |
|     206 | 2197 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2198 | `{` |
|       - | 2199 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 2200 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 2201 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 2202 | `	SySet aArg;               /* Argument value pointers */` |
|     209 | 2203 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     209 | 2204 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     209 | 2205 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 2206 | `	sxi32 rc;` |
|       - | 2207 | `	sxu32 n;` |
|     209 | 2208 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 2209 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2210 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2211 | `		return PH7_OK;` |
|       - | 2212 | `	}` |
|       - | 2213 | `	{` |
|     209 | 2214 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     209 | 2215 | `		if( rcCb != PH7_OK ){` |
|       5 | 2216 | `			return rcCb;` |
|       - | 2217 | `		}` |
|       - | 2218 | `	}` |
|     205 | 2219 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     205 | 2220 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2221 | `	/* Initialize the arguments container */` |
|     205 | 2222 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2223 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 2224 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 2225 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 2226 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 2227 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 2228 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     205 | 2229 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     205 | 2230 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     599 | 2231 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 2232 | `		/* Extract node value */` |
|     397 | 2233 | `		if( (pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     397 | 2234 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      32 | 2235 | `				if( aNames == 0 ){` |
|       - | 2236 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 2237 | `					 * not-yet-seen slot defaults to positional. */` |
|      22 | 2238 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      22 | 2239 | `					if( aNames == 0 ){` |
|     ! 0 | 2240 | `						SySetRelease(&aArg);` |
|     ! 0 | 2241 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2242 | `						if( apNode ){` |
|     ! 0 | 2243 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 2244 | `						}` |
|     ! 0 | 2245 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 2246 | `					}` |
|      22 | 2247 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|      10 | 2248 | `				}` |
|      32 | 2249 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      15 | 2250 | `			}` |
|     397 | 2251 | `			if( apNode == 0 ){` |
|     237 | 2252 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     156 | 2253 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     159 | 2254 | `				if( apNode ){` |
|     159 | 2255 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|      78 | 2256 | `				}` |
|      78 | 2257 | `			}` |
|     397 | 2258 | `			if( apNode ){` |
|     397 | 2259 | `				apNode[nSlot] = pEntry;` |
|     197 | 2260 | `			}` |
|     397 | 2261 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     397 | 2262 | `			nSlot++;` |
|     197 | 2263 | `		}` |
|       - | 2264 | `		/* Point to the next entry */` |
|     397 | 2265 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     200 | 2266 | `	}` |
|       - | 2267 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 2268 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 2269 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 2270 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     205 | 2271 | `	if( apNode ){` |
|     159 | 2272 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     159 | 2273 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     159 | 2274 | `		apNode = 0;` |
|      78 | 2275 | `	}` |
|       - | 2276 | `	/* Try to invoke the callback. Like call_user_func, this is a php FORWARD: a` |
|       - | 2277 | `	 * dropped answer here is a dropped answer for the callback. */` |
|     205 | 2278 | `	pCtx->pVm->bDiscardCallback = pCtx->pVm->bHostDiscard;` |
|     205 | 2279 | `	if( aNames ){` |
|       - | 2280 | `		VmCallArgMap sMap;` |
|      22 | 2281 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      22 | 2282 | `		sMap.bHasNamed = 1;` |
|      22 | 2283 | `		sMap.bIsNamespaced = 0;` |
|       - | 2284 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 2285 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      22 | 2286 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      22 | 2287 | `		sMap.nTotal = nSlot;` |
|      22 | 2288 | `		sMap.aNames = aNames;` |
|      32 | 2289 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      20 | 2290 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      22 | 2291 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      12 | 2292 | `	}else{` |
|       - | 2293 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 2294 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 2295 | `		VmCallArgMap sFwd;` |
|     184 | 2296 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     184 | 2297 | `		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     275 | 2298 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     182 | 2299 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 2300 | `	}` |
|     205 | 2301 | `	pCtx->pVm->bDiscardCallback = 0;   /* see the call_user_func sibling */` |
|     205 | 2302 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2303 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     119 | 2304 | `		PH7_MemObjRelease(&sResult);` |
|     119 | 2305 | `		SySetRelease(&aArg);` |
|     119 | 2306 | `		return PH7_EXCEPTION;` |
|       - | 2307 | `	}` |
|      88 | 2308 | `	if( rc != SXRET_OK ){` |
|       - | 2309 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2310 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2311 | `	}else{` |
|       - | 2312 | `		/* Callback result */` |
|      88 | 2313 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2314 | `	}` |
|       - | 2315 | `	/* Cleanup the mess left behind */` |
|      88 | 2316 | `	PH7_MemObjRelease(&sResult);` |
|      88 | 2317 | `	SySetRelease(&aArg);` |
|      88 | 2318 | `	return PH7_OK;` |
|     106 | 2319 | `}` |
|       - | 2320 |  |
