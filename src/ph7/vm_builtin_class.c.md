# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1206/1313 lines (91.85%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|   16695 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|   16700 |   11 | `	if( nArg < 1 ){` |
|       - |   12 | `		/* php reads the EXECUTED scope, the class the running code was written in --` |
|       - |   13 | `		 * not the late-static-bound one: a parent's method answers the parent from a` |
|       - |   14 | `		 * child instance. With no scope at all it is an Error, never FALSE. */` |
|      21 |   15 | `		pClass = PH7_VmCallerScope(pCtx->pVm);` |
|      21 |   16 | `		if( pClass == 0 ){` |
|       9 |   17 | `			return PH7_VmThrowException(pCtx,"Error",` |
|       - |   18 | `				"get_class() without arguments must be called from within a class");` |
|       - |   19 | `		}` |
|       - |   20 | `		/* php 8.3 deprecated the spelling, and warns only once a scope is found */` |
|      13 |   21 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,"Calling get_class() without arguments is deprecated");` |
|      13 |   22 | `		pName = &pClass->sName;` |
|      13 |   23 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       7 |   24 | `	}else{` |
|       - |   25 | `		/* Extract the target class */` |
|   16680 |   26 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   16680 |   27 | `		if( pClass ){` |
|   16680 |   28 | `			pName = &pClass->sName;` |
|       - |   29 | `			/* Return the class name */` |
|   16680 |   30 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    8317 |   31 | `		}else{` |
|       - |   32 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   33 | `			ph7_result_bool(pCtx,0);` |
|       - |   34 | `		}` |
|       - |   35 | `	}` |
|   16692 |   36 | `	return PH7_OK;` |
|    8327 |   37 | `}` |
|       - |   38 | `/*` |
|       - |   39 | ` * string get_parent_class([object $object = NULL ] )` |
|       - |   40 | ` *   Returns the name of the parent class of an object` |
|       - |   41 | ` * Parameters` |
|       - |   42 | ` *  object` |
|       - |   43 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|       - |   44 | ` * Return` |
|       - |   45 | ` *  The name of the parent class of which object is an instance.` |
|       - |   46 | ` *  Returns FALSE if object is not an object or if the object does` |
|       - |   47 | ` *  not have a parent.` |
|       - |   48 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|       - |   49 | ` */` |
|     140 |   50 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   51 | `{` |
|       - |   52 | `	ph7_class *pClass;` |
|       - |   53 | `	SyString *pName;` |
|     145 |   54 | `	if( nArg < 1 ){` |
|       - |   55 | `		/* The executed scope's parent, as get_class() reads it; FALSE outside a class.` |
|       - |   56 | `		 * php 8.3 deprecated the spelling, and warns before it looks, scope or not. */` |
|      11 |   57 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,"Calling get_parent_class() without arguments is deprecated");` |
|      11 |   58 | `		pClass = PH7_VmCallerScope(pCtx->pVm);` |
|      11 |   59 | `		if( pClass && pClass->pBase ){` |
|       - |   60 | `			/* Point to the class name */` |
|       5 |   61 | `			pName = &pClass->pBase->sName;` |
|       5 |   62 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       3 |   63 | `		}else{` |
|       - |   64 | `			/* Not inside class,return FALSE */` |
|       7 |   65 | `			ph7_result_bool(pCtx,0);` |
|       - |   66 | `		}` |
|       6 |   67 | `	}else{` |
|       - |   68 | `		/* Extract the target class */` |
|     135 |   69 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     135 |   70 | `		if( pClass == 0 ){` |
|       - |   71 | `			/* php's Z_PARAM_OBJ_OR_CLASS_NAME: anything that is not an object or the` |
|       - |   72 | `			 * name of a class -- an int, null, '' or a typo -- is one TypeError in` |
|       - |   73 | `` 			 * either file mode, never FALSE. Its row in vm_arg_check.c is marked `~` `` |
|       - |   74 | ``			 * so the declared `object\|string` screen stands aside for this one. */`` |
|       - |   75 | `			char zGiven[64];` |
|      43 |   76 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |   77 | `				"get_parent_class(): Argument #1 ($object_or_class) must be an object "` |
|      14 |   78 | `				"or a valid class name, %s given",VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - |   79 | `		}` |
|     107 |   80 | `		if( pClass->pBase ){` |
|      97 |   81 | `			pName = &pClass->pBase->sName;` |
|       - |   82 | `			/* Return the parent class name */` |
|      97 |   83 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      51 |   84 | `		}else{` |
|       - |   85 | `			/* Object does not have a parent class */` |
|      12 |   86 | `			ph7_result_bool(pCtx,0);` |
|       - |   87 | `		}` |
|       - |   88 | `	}` |
|     117 |   89 | `	return PH7_OK;` |
|      75 |   90 | `}` |
|       - |   91 | `/*` |
|       - |   92 | ` * string get_called_class(void)` |
|       - |   93 | ` *   Gets the name of the class the static method is called in.` |
|       - |   94 | ` * Parameters` |
|       - |   95 | ` *  None.` |
|       - |   96 | ` * Return` |
|       - |   97 | ` *  Returns the class name. Throws Error if called from outside a class.` |
|       - |   98 | ` */` |
|      24 |   99 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  100 | `{` |
|       - |  101 | `	ph7_class *pClass;` |
|       - |  102 | `	/* Check if we are inside a class [i.e: a method call] */` |
|      25 |  103 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      25 |  104 | `	if( pClass ){` |
|       - |  105 | `		SyString *pName;` |
|       - |  106 | `		/* Point to the class name */` |
|      19 |  107 | `		pName = &pClass->sName;` |
|      19 |  108 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      10 |  109 | `	}else{` |
|       3 |  110 | `		SXUNUSED(nArg); /* cc warning */` |
|       3 |  111 | `		SXUNUSED(apArg);` |
|       - |  112 | `		/* php refuses rather than answering FALSE */` |
|       7 |  113 | `		return PH7_VmThrowException(pCtx,"Error","get_called_class() must be called from within a class");` |
|       - |  114 | `	}` |
|      19 |  115 | `	return PH7_OK;` |
|      13 |  116 | `}` |
|       - |  117 | `/*` |
|       - |  118 | ` * Extract a ph7_class from the given ph7_value.` |
|       - |  119 | ` * The given value must be of type object [i.e: class instance] or` |
|       - |  120 | ` * string which hold the class name.` |
|       - |  121 | ` */` |
|  229300 |  122 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  123 | `{` |
|  229305 |  124 | `	ph7_class *pClass = 0;` |
|  229305 |  125 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  126 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  217674 |  127 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  120445 |  128 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  129 | `		const char *zClass;` |
|       - |  130 | `		int nLen;` |
|       - |  131 | `		/* Extract class name */` |
|   11612 |  132 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  133 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  134 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  135 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|   11612 |  136 | `		if( nLen > 0 ){` |
|       - |  137 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  138 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  139 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  140 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  141 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  142 | `			 * is a valid callable). */` |
|   11604 |  143 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|    5791 |  144 | `		}` |
|    5795 |  145 | `	}` |
|  229305 |  146 | `	return pClass;` |
|       5 |  147 | `}` |
|       - |  148 | `/*` |
|       - |  149 | `` * php's screen for a `$object_or_class` its ZPP takes as a bare zval: anything that`` |
|       - |  150 | ` * is neither an object nor a string is one TypeError naming the type given, in` |
|       - |  151 | ` * either file mode. method_exists() and property_exists() spell it in their C` |
|       - |  152 | ` * bodies, the three relation functions through Z_PARAM_OBJ_OR_STR; none of them` |
|       - |  153 | ` * DECLARES the type, which is why their aBuiltinSig[] rows leave it bare and the` |
|       - |  154 | ` * screen lives here. Answers the throw's status when it refused, PH7_OK otherwise.` |
|       - |  155 | ` */` |
|     466 |  156 | `static sxi32 VmObjectOrClassScreen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  157 | `{` |
|       - |  158 | `	char zGiven[64];` |
|     466 |  159 | `	if( nArg > 0` |
|     466 |  160 | `	 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) != 0` |
|     442 |  161 | `	 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     413 |  162 | `		return PH7_OK;` |
|       - |  163 | `	}` |
|     117 |  164 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  165 | `		"%s(): Argument #1 ($object_or_class) must be of type object\|string, %s given",` |
|      29 |  166 | `		ph7_function_name(pCtx),` |
|      58 |  167 | `		nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|     238 |  168 | `}` |
|       - |  169 | `/*` |
|       - |  170 | ` * bool property_exists(mixed $class,string $property)` |
|       - |  171 | ` *   Checks if the object or class has a property.` |
|       - |  172 | ` * Parameters` |
|       - |  173 | ` *  class` |
|       - |  174 | ` *   The class name or an object of the class to test for` |
|       - |  175 | ` * property` |
|       - |  176 | ` *  The name of the property` |
|       - |  177 | ` * Return` |
|       - |  178 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|       - |  179 | ` */` |
|     164 |  180 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  181 | `{` |
|     169 |  182 | `	int res = 0; /* Assume attribute does not exists */` |
|     169 |  183 | `	if( nArg > 1 ){` |
|       - |  184 | `		ph7_class *pClass;` |
|     169 |  185 | `		sxi32 rc = VmObjectOrClassScreen(pCtx,nArg,apArg);` |
|     169 |  186 | `		if( rc != PH7_OK ){` |
|      19 |  187 | `			return rc;` |
|       - |  188 | `		}` |
|     146 |  189 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|     119 |  190 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  191 | `			/* An incomplete OBJECT: php's has_property probe is the access warning` |
|       - |  192 | `			 * (qualified with this builtin's own name) answering false. The class` |
|       - |  193 | `			 * asked about by NAME stays an ordinary lookup. */` |
|       - |  194 | `			SyBlob sIncMsg;` |
|       3 |  195 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  196 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  197 | `				"access a property",&sIncMsg);` |
|       4 |  198 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%.*s",` |
|       2 |  199 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  200 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  201 | `			ph7_result_bool(pCtx,0);` |
|       3 |  202 | `			return PH7_OK;` |
|       - |  203 | `		}` |
|     149 |  204 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     149 |  205 | `		if( pClass ){` |
|       - |  206 | `			const char *zName;` |
|       - |  207 | `			int nLen;` |
|       - |  208 | `			/* Extract attribute name */` |
|     143 |  209 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     143 |  210 | `			if( nLen > 0 ){` |
|       - |  211 | `				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this` |
|       - |  212 | ``				 * name is not a property (`property_exists('C','someMethod')` is false),`` |
|       - |  213 | `				 * and neither is a class CONSTANT. PHL searched the method table too and` |
|       - |  214 | `				 * answered true for both. */` |
|     143 |  215 | `				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);` |
|     143 |  216 | `				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;` |
|     138 |  217 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND)` |
|      42 |  218 | `				 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - |  219 | `					/* An ON-DEMAND property is on the objects that took it and on no` |
|       - |  220 | `					 * other: php declares none of them, so the class table cannot be` |
|       - |  221 | `					 * what answers here. */` |
|       5 |  222 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       4 |  223 | `					if( pThis == 0` |
|       5 |  224 | `					 \|\| SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) == 0 ){` |
|       3 |  225 | `						pAttr = 0;` |
|       2 |  226 | `					}` |
|     141 |  227 | `				}else if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) ){` |
|     ! 0 |  228 | `					pAttr = 0;   /* asked about the CLASS: php declares no such name */` |
|     ! 0 |  229 | `				}` |
|     143 |  230 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN) != 0 ){` |
|       - |  231 | `					/* A native class's ENGINE SLOT is php's own C struct, not a` |
|       - |  232 | ``					 * declared property: `property_exists($doc, '__res')` is false`` |
|       - |  233 | `					 * under php because no such name is in its properties_info.` |
|       - |  234 | `					 * They are already hidden from Reflection, var_dump and the` |
|       - |  235 | `					 * (array) cast; this was the one door that still showed them. */` |
|       3 |  236 | `					pAttr = 0;` |
|       1 |  237 | `				}` |
|     143 |  238 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       - |  239 | `					/* A base's PRIVATE property is invisible to the child it was asked` |
|       - |  240 | ``					 * about, php's `property_info->ce == ce` rule: PHL copies one down`` |
|       - |  241 | `					 * onto every child (its own methods read it through $this), so the` |
|       - |  242 | `					 * table alone said true where php says false. Static or instance,` |
|       - |  243 | `					 * the rule is the same; protected and public are inherited outright.` |
|       - |  244 | `					 * A trait's property belongs to the class that COMPOSED it. */` |
|      66 |  245 | `					res = 1;` |
|      62 |  246 | `					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      38 |  247 | `					 && pAttr->pDeclClass != 0` |
|      18 |  248 | `					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){` |
|       5 |  249 | `						res = 0;` |
|       2 |  250 | `					}` |
|      31 |  251 | `				}` |
|       - |  252 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  253 | `				 * attribute table, not the class's — php reports those too` |
|       - |  254 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|     143 |  255 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|      50 |  256 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      50 |  257 | `					SyHashEntry *pObjE = pThis` |
|      46 |  258 | `						? SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) : 0;` |
|      50 |  259 | `					VmClassAttr *pObjAttr = pObjE ? (VmClassAttr *)pObjE->pUserData : 0;` |
|       - |  260 | `					/* Only a genuinely DYNAMIC one: every instance carries a slot for every` |
|       - |  261 | `					 * DECLARED member too, so an unqualified instance lookup answered true` |
|       - |  262 | `					 * for the base private the class-table rule above had just refused. */` |
|      46 |  263 | `					if( pObjAttr && pObjAttr->pAttr` |
|       8 |  264 | `					 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|       3 |  265 | `						res = 1;` |
|       1 |  266 | `					}` |
|      50 |  267 | `					if( res == 0 && pThis ){` |
|       - |  268 | `						/* php asks the class's has_property handler here too, which` |
|       - |  269 | `						 * is what reports a PDORow's COLUMNS -- a name no table` |
|       - |  270 | `						 * carries and every read answers. Its verdict is the value's` |
|       - |  271 | `						 * (a column holding SQL NULL reports false), which is what` |
|       - |  272 | `						 * php's own handler answers to this question. */` |
|       - |  273 | `						PH7_NativePropCtx sNat;` |
|       - |  274 | `						SyString sNatName;` |
|       - |  275 | `						ph7_value sNatVal;` |
|      48 |  276 | `						SyStringInitFromBuf(&sNatName,zName,(sxu32)nLen);` |
|      48 |  277 | `						PH7_MemObjInit(pCtx->pVm,&sNatVal);` |
|      44 |  278 | `						if( PH7_ClassNativePropAsk(pThis,&sNat,PH7_NATIVE_PROP_EXISTS,` |
|       - |  279 | `								&sNatName,&sNatVal)` |
|      40 |  280 | `						 && sNat.zThrowClass == 0 && ph7_value_to_bool(&sNatVal) ){` |
|      14 |  281 | `							res = 1;` |
|       6 |  282 | `						}` |
|      48 |  283 | `						PH7_MemObjRelease(&sNatVal);` |
|      22 |  284 | `					}` |
|      23 |  285 | `				}` |
|      69 |  286 | `			}` |
|      69 |  287 | `		}` |
|      72 |  288 | `	}` |
|     149 |  289 | `	ph7_result_bool(pCtx,res);` |
|     149 |  290 | `	return PH7_OK;` |
|      87 |  291 | `}` |
|       - |  292 | `/*` |
|       - |  293 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  294 | ` *   Checks if the given method is a class member.` |
|       - |  295 | ` * Parameters` |
|       - |  296 | ` *  class` |
|       - |  297 | ` *   The class name or an object of the class to test for` |
|       - |  298 | ` * property` |
|       - |  299 | ` *  The name of the method` |
|       - |  300 | ` * Return` |
|       - |  301 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  302 | ` */` |
|     142 |  303 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  304 | `{` |
|     145 |  305 | `	int res = 0; /* Assume method does not exists */` |
|     145 |  306 | `	if( nArg > 1 ){` |
|       - |  307 | `		ph7_class *pClass;` |
|     145 |  308 | `		sxi32 rc = VmObjectOrClassScreen(pCtx,nArg,apArg);` |
|     145 |  309 | `		if( rc != PH7_OK ){` |
|      17 |  310 | `			return rc;` |
|       - |  311 | `		}` |
|     126 |  312 | `		if( (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|      79 |  313 | `		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){` |
|       - |  314 | `			/* An incomplete OBJECT consults its method resolution, which the` |
|       - |  315 | `			 * carrier refuses with php's catchable call Error (probe-verified;` |
|       - |  316 | `			 * the class asked about by NAME answers false the ordinary way). */` |
|       - |  317 | `			SyBlob sIncMsg;` |
|       - |  318 | `			sxi32 rcInc;` |
|       3 |  319 | `			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);` |
|       3 |  320 | `			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,` |
|       - |  321 | `				"call a method",&sIncMsg);` |
|       4 |  322 | `			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",` |
|       2 |  323 | `				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));` |
|       3 |  324 | `			SyBlobRelease(&sIncMsg);` |
|       3 |  325 | `			return rcInc;` |
|       - |  326 | `		}` |
|     127 |  327 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     127 |  328 | `		if( pClass ){` |
|       - |  329 | `			const char *zName;` |
|       - |  330 | `			int nLen;` |
|       - |  331 | `			/* Extract method name */` |
|     117 |  332 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     117 |  333 | `			if( nLen > 0 ){` |
|       - |  334 | `				/* Perform the lookup in the method table */` |
|     117 |  335 | `				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);` |
|     117 |  336 | `				if( pEntry ){` |
|       - |  337 | ``					/* ...and apply php's one visibility rule here (`func->common.scope`` |
|       - |  338 | ``					 * == ce`): a PRIVATE method is only a method of the class that`` |
|       - |  339 | `					 * declares it. PHL copies a base's private down so an inherited` |
|       - |  340 | `					 * public method can still dispatch it, which made` |
|       - |  341 | ``					 * `method_exists('Child','basePrivate')` answer true where php`` |
|       - |  342 | `					 * answers false — the same shape property_exists() had. Nothing` |
|       - |  343 | `					 * about the CALLING scope enters into it: php answers false for the` |
|       - |  344 | `					 * child from inside the BASE too. A trait's method belongs to the` |
|       - |  345 | `					 * class that COMPOSED it. */` |
|      87 |  346 | `					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      87 |  347 | `					res = 1;` |
|      84 |  348 | `					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      55 |  349 | `					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){` |
|      15 |  350 | `						res = 0;` |
|       7 |  351 | `					}` |
|      42 |  352 | `				}` |
|      57 |  353 | `			}` |
|      57 |  354 | `		}` |
|      62 |  355 | `	}` |
|     127 |  356 | `	ph7_result_bool(pCtx,res);` |
|     127 |  357 | `	return PH7_OK;` |
|      74 |  358 | `}` |
|       - |  359 | `/*` |
|       - |  360 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  361 | ` *   Checks if the class has been defined.` |
|       - |  362 | ` * Parameters` |
|       - |  363 | ` *  class_name` |
|       - |  364 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  365 | ` *   unlinke the standard PHP engine.` |
|       - |  366 | ` *  autoload` |
|       - |  367 | ` *   Whether or not to call __autoload by default.` |
|       - |  368 | ` * Return` |
|       - |  369 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  370 | ` */` |
|     366 |  371 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  372 | `{` |
|     371 |  373 | `	int res = 0; /* Assume class does not exist */` |
|     371 |  374 | `	if( nArg > 0 ){` |
|     371 |  375 | `		SyHashEntry *pEntry = 0;` |
|       - |  376 | `		const char *zName;` |
|       - |  377 | `		int nLen;` |
|     371 |  378 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  379 | `		sxu32 nName;` |
|       - |  380 | `		/* Extract given name */` |
|     371 |  381 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     371 |  382 | `		if( nArg >= 2 ){` |
|     179 |  383 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|      87 |  384 | `		}` |
|       - |  385 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  386 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     371 |  387 | `		nName = (sxu32)nLen;` |
|     371 |  388 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     371 |  389 | `		if( nName > 0 ){` |
|       - |  390 | `			/* Perform a hash lookup first */` |
|     367 |  391 | `			pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|     181 |  392 | `		}` |
|       - |  393 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|     371 |  394 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  395 | `			/* Try autoload, then re-check */` |
|      83 |  396 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      83 |  397 | `			if( pClass ){` |
|      33 |  398 | `				pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|      14 |  399 | `			}` |
|      39 |  400 | `		}` |
|     371 |  401 | `		if( pEntry ){` |
|       - |  402 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  403 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|     283 |  404 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     295 |  405 | `			while( pClass ){` |
|     283 |  406 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|     271 |  407 | `					res = 1;` |
|     271 |  408 | `					break;` |
|       - |  409 | `				}` |
|      13 |  410 | `				pClass = pClass->pNextName;` |
|       1 |  411 | `			}` |
|     139 |  412 | `		}` |
|     183 |  413 | `	}` |
|     371 |  414 | `	ph7_result_bool(pCtx,res);` |
|     371 |  415 | `	return PH7_OK;` |
|       5 |  416 | `}` |
|       - |  417 | `/*` |
|       - |  418 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  419 | ` *   Checks if the interface has been defined.` |
|       - |  420 | ` * Parameters` |
|       - |  421 | ` *  class_name` |
|       - |  422 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  423 | ` *   unlinke the standard PHP engine.` |
|       - |  424 | ` *  autoload` |
|       - |  425 | ` *   Whether or not to call __autoload by default.` |
|       - |  426 | ` * Return` |
|       - |  427 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  428 | ` */` |
|      82 |  429 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  430 | `{` |
|      87 |  431 | `	int res = 0; /* Assume interface does not exist */` |
|      87 |  432 | `	if( nArg > 0 ){` |
|      87 |  433 | `		SyHashEntry *pEntry = 0;` |
|       - |  434 | `		const char *zName;` |
|       - |  435 | `		int nLen;` |
|      87 |  436 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  437 | `		sxu32 nName;` |
|       - |  438 | `		/* Extract given name */` |
|      87 |  439 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      87 |  440 | `		if( nArg >= 2 ){` |
|      23 |  441 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|      10 |  442 | `		}` |
|       - |  443 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  444 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      87 |  445 | `		nName = (sxu32)nLen;` |
|      87 |  446 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  447 | `		/* Perform a hash lookup */` |
|      87 |  448 | `		if( nName > 0 ){` |
|      84 |  449 | `			pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|      40 |  450 | `		}` |
|       - |  451 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      87 |  452 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  453 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|      13 |  454 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      13 |  455 | `			if( pClass ){` |
|       3 |  456 | `				pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|       1 |  457 | `			}` |
|       5 |  458 | `		}` |
|      87 |  459 | `		if( pEntry ){` |
|      70 |  460 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      72 |  461 | `			while( pClass ){` |
|      70 |  462 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  463 | `					/* interface is available */` |
|      68 |  464 | `					res = 1;` |
|      68 |  465 | `					break;` |
|       - |  466 | `				}` |
|       - |  467 | `				/* Next with the same name */` |
|       3 |  468 | `				pClass = pClass->pNextName;` |
|       1 |  469 | `			}` |
|      33 |  470 | `		}` |
|      41 |  471 | `	}` |
|      87 |  472 | `	ph7_result_bool(pCtx,res);` |
|      87 |  473 | `	return PH7_OK;` |
|       5 |  474 | `}` |
|       - |  475 | `/*` |
|       - |  476 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  477 | ` *   Checks if the trait has been defined.` |
|       - |  478 | ` * Parameters` |
|       - |  479 | ` *  trait` |
|       - |  480 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  481 | ` *  autoload` |
|       - |  482 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  483 | ` * Return` |
|       - |  484 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  485 | ` */` |
|      24 |  486 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  487 | `{` |
|      28 |  488 | `	int res = 0; /* Assume trait does not exist */` |
|      28 |  489 | `	if( nArg > 0 ){` |
|      28 |  490 | `		SyHashEntry *pEntry = 0;` |
|       - |  491 | `		const char *zName;` |
|       - |  492 | `		int nLen;` |
|      28 |  493 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  494 | `		sxu32 nName;` |
|       - |  495 | `		/* Extract given name */` |
|      28 |  496 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      28 |  497 | `		if( nArg >= 2 ){` |
|       5 |  498 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  499 | `		}` |
|       - |  500 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  501 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      28 |  502 | `		nName = (sxu32)nLen;` |
|      28 |  503 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  504 | `		/* Perform a hash lookup */` |
|      28 |  505 | `		if( nName > 0 ){` |
|      25 |  506 | `			pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|      11 |  507 | `		}` |
|       - |  508 | `		/* A lone "\" strips to no name, and php hands no name to the autoloader. */` |
|      28 |  509 | `		if( pEntry == 0 && nName > 0 && iAutoload ){` |
|       - |  510 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|      10 |  511 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      10 |  512 | `			if( pClass ){` |
|     ! 0 |  513 | `				pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|     ! 0 |  514 | `			}` |
|       4 |  515 | `		}` |
|      28 |  516 | `		if( pEntry ){` |
|      12 |  517 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  518 | `			while( pClass ){` |
|      12 |  519 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  520 | `					/* trait is available */` |
|       6 |  521 | `					res = 1;` |
|       6 |  522 | `					break;` |
|       - |  523 | `				}` |
|       - |  524 | `				/* Next with the same name */` |
|       7 |  525 | `				pClass = pClass->pNextName;` |
|       1 |  526 | `			}` |
|       5 |  527 | `		}` |
|      12 |  528 | `	}` |
|      28 |  529 | `	ph7_result_bool(pCtx,res);` |
|      28 |  530 | `	return PH7_OK;` |
|       4 |  531 | `}` |
|       - |  532 | `/*` |
|       - |  533 | ` * bool class_alias(string $class, string $alias, bool $autoload = true)` |
|       - |  534 | ` *   Creates an alias for a class.` |
|       - |  535 | ` * Parameters` |
|       - |  536 | ` *  class` |
|       - |  537 | ` *    The original class (interface, trait or enum — php aliases all four).` |
|       - |  538 | ` *  alias` |
|       - |  539 | ` *   The alias name for the class.` |
|       - |  540 | ` *  autoload` |
|       - |  541 | ` *   Whether to autoload $class when it is not declared yet. php's default is TRUE,` |
|       - |  542 | ` *   and this is not a detail: the ordinary use of this builtin is a compatibility` |
|       - |  543 | ` *   shim written against a class the AUTOLOADER owns —` |
|       - |  544 | ``  *   `class_alias('\\PHPUnit\\Framework\\TestCase','\\PHPUnit_Framework_TestCase')` `` |
|       - |  545 | ` *   in monolog's test bootstrap is exactly that, and with no autoload it silently` |
|       - |  546 | ` *   answered false, so every test class extending the alias was undefined.` |
|       - |  547 | ` * Return` |
|       - |  548 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  549 | ` */` |
|      22 |  550 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  551 | `{` |
|       - |  552 | `	const char *zOld,*zNew,*zOldRaw,*zNewRaw;` |
|       - |  553 | `	int nOldLen,nNewLen;` |
|       - |  554 | `	sxu32 nOld,nNew;` |
|      26 |  555 | `	int iAutoload = 1;` |
|       - |  556 | `	SyHashEntry *pEntry;` |
|       - |  557 | `	ph7_class *pClass;` |
|       - |  558 | `	char *zDup;` |
|       - |  559 | `	sxi32 rc;` |
|      26 |  560 | `	if( nArg < 2 ){` |
|       - |  561 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  562 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  563 | `		return PH7_OK;` |
|       - |  564 | `	}` |
|       - |  565 | `	/* Extract old class name */` |
|      26 |  566 | `	zOld = zOldRaw = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  567 | `	/* Extract alias name */` |
|      26 |  568 | `	zNew = zNewRaw = ph7_value_to_string(apArg[1],&nNewLen);` |
|      26 |  569 | `	if( nArg >= 3 ){` |
|       3 |  570 | `		iAutoload = ph7_value_to_bool(apArg[2]);` |
|       1 |  571 | `	}` |
|       - |  572 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  573 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  574 | `	 * on the plain name then matches. */` |
|      26 |  575 | `	nOld = (sxu32)nOldLen;` |
|      26 |  576 | `	nNew = (sxu32)nNewLen;` |
|      26 |  577 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|      26 |  578 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       - |  579 | `	/* Perform a hash lookup */` |
|      26 |  580 | `	pEntry = nOld > 0 ? PH7_VmClassEntry(pCtx->pVm,zOld,nOld) : 0;` |
|      26 |  581 | `	if( pEntry == 0 && iAutoload && nOld > 0 ){` |
|       - |  582 | `		/* Not declared yet: ask the autoloader, exactly as class_exists() does.` |
|       - |  583 | `		 * iLoadable is FALSE so an interface or a trait comes back too — php` |
|       - |  584 | `		 * aliases those as readily as a class. */` |
|       7 |  585 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zOld,nOld,FALSE) ){` |
|       5 |  586 | `			pEntry = PH7_VmClassEntry(pCtx->pVm,zOld,nOld);` |
|       2 |  587 | `		}` |
|       3 |  588 | `	}` |
|      26 |  589 | `	if( pEntry ==  0 ){` |
|       - |  590 | `		/* php names the class it could not find and answers false. The sentence` |
|       - |  591 | ``		 * carries no `class_alias(): ` prefix, so it is raised on the VM rather`` |
|       - |  592 | `		 * than through the context. */` |
|       5 |  593 | `		VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,"Class \"%.*s\" not found",nOldLen,zOldRaw);` |
|       5 |  594 | `		ph7_result_bool(pCtx,0);` |
|       5 |  595 | `		return PH7_OK;` |
|       - |  596 | `	}` |
|       - |  597 | `	/* Point to the class */` |
|      22 |  598 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|      22 |  599 | `	if( nNew > 0 && PH7_VmClassEntry(pCtx->pVm,zNew,nNew) != 0 ){` |
|       - |  600 | `		/* The alias name is already a declared class/interface/trait/enum. php` |
|       - |  601 | `		 * refuses and — this is php's own quirk, not a slip — names the file and` |
|       - |  602 | `		 * line of the class being ALIASED, not of the name already taken. */` |
|       3 |  603 | `		if( pClass->sFile.nByte > 0 ){` |
|       4 |  604 | `			VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,` |
|       - |  605 | `				"Cannot redeclare class %.*s (previously declared in %.*s:%u)",` |
|       1 |  606 | `				nNewLen,zNewRaw,` |
|       2 |  607 | `				(int)pClass->sFile.nByte,pClass->sFile.zString,pClass->nLine);` |
|       2 |  608 | `		}else{` |
|       - |  609 | `			/* No defining file: php's own wording for a class it did not compile. */` |
|     ! 0 |  610 | `			VmErrorFormat(pCtx->pVm,PH7_CTX_WARNING,` |
|     ! 0 |  611 | `				"Cannot redeclare class %.*s",nNewLen,zNewRaw);` |
|       - |  612 | `		}` |
|       3 |  613 | `		ph7_result_bool(pCtx,0);` |
|       3 |  614 | `		return PH7_OK;` |
|       - |  615 | `	}` |
|      20 |  616 | `	if( nNew < 1 ){` |
|       - |  617 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  618 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  619 | `		return PH7_OK;` |
|       - |  620 | `	}` |
|       - |  621 | `	/* Duplicate alias name */` |
|      20 |  622 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|      20 |  623 | `	if( zDup == 0 ){` |
|       - |  624 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  625 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  626 | `		return PH7_OK;` |
|       - |  627 | `	}` |
|       - |  628 | `	/* Create the alias */` |
|      20 |  629 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|      20 |  630 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  631 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  632 | `	}` |
|      20 |  633 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      20 |  634 | `	return PH7_OK;` |
|      15 |  635 | `}` |
|       - |  636 | `/*` |
|       - |  637 | ` * The three KINDS hClass holds. php keeps classes, interfaces, traits and enums in one` |
|       - |  638 | ` * table too and each of its three list builtins filters that table down to its own kind:` |
|       - |  639 | `` * an ENUM is a class (`get_declared_classes()` reports it), an interface and a trait are`` |
|       - |  640 | ` * not.` |
|       - |  641 | ` */` |
|       - |  642 | `#define VM_DECLARED_CLASS      0` |
|       - |  643 | `#define VM_DECLARED_INTERFACE  1` |
|       - |  644 | `#define VM_DECLARED_TRAIT      2` |
|       - |  645 |  |
|    9554 |  646 | `static int VmDeclaredEntryKind(ph7_class *pClass)` |
|       4 |  647 | `{` |
|    9558 |  648 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     950 |  649 | `		return VM_DECLARED_INTERFACE;` |
|       - |  650 | `	}` |
|    8612 |  651 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      63 |  652 | `		return VM_DECLARED_TRAIT;` |
|       - |  653 | `	}` |
|    8552 |  654 | `	return VM_DECLARED_CLASS;` |
|    4781 |  655 | `}` |
|       - |  656 | `struct VmDeclaredList {` |
|       - |  657 | `	int iKind;           /* Which VM_DECLARED_* kind this list wants */` |
|       - |  658 | `	ph7_value *pArray;   /* The array being built */` |
|       - |  659 | `	ph7_value *pName;    /* Scratch name */` |
|       - |  660 | `};` |
|       - |  661 | `/*` |
|       - |  662 | ` * One row of a get_declared_*() answer.` |
|       - |  663 | ` *` |
|       - |  664 | ` * The NAME reported is the table KEY, not the class struct's own name — a distinction` |
|       - |  665 | `` * only `class_alias()` makes visible, since it puts a second key over the same class.`` |
|       - |  666 | ` * php reports the class's declared spelling for the key that IS its name and the ALIAS` |
|       - |  667 | `` * for the other, so `class_alias('C1','C1Alias')` answers both `C1` and `c1alias`,`` |
|       - |  668 | ` * lower-cased because that is the spelling php's own alias key is stored under. PHL` |
|       - |  669 | ` * keeps the declared spelling in every key, so the fold is applied here.` |
|       - |  670 | ` */` |
|    9554 |  671 | `static sxi32 VmDeclaredNameStep(SyHashEntry *pEntry,void *pUserData)` |
|       4 |  672 | `{` |
|    9558 |  673 | `	struct VmDeclaredList *pList = (struct VmDeclaredList *)pUserData;` |
|    9558 |  674 | `	ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    9558 |  675 | `	SyString *pDecl = &pClass->sName;` |
|    9558 |  676 | `	if( VmDeclaredEntryKind(pClass) != pList->iKind \|\| (pClass->iFlags & PH7_CLASS_HIDDEN) ){` |
|       - |  677 | `		/* ...and a class whose statement has not run is not declared yet */` |
|    3748 |  678 | `		return SXRET_OK;` |
|       - |  679 | `	}` |
|    5810 |  680 | `	if( pEntry->nKeyLen == pDecl->nByte` |
|    5798 |  681 | `		&& SyStrnmicmp((const char *)pEntry->pKey,pDecl->zString,pEntry->nKeyLen) == 0 ){` |
|       - |  682 | `		/* The key this class was DECLARED under */` |
|    5782 |  683 | `		ph7_value_string(pList->pName,pDecl->zString,(int)pDecl->nByte);` |
|    2893 |  684 | `	}else{` |
|       - |  685 | `		/* A class_alias() key: php reports it folded */` |
|      36 |  686 | `		const char *zKey = (const char *)pEntry->pKey;` |
|       - |  687 | `		sxu32 n;` |
|     550 |  688 | `		for( n = 0 ; n < pEntry->nKeyLen ; ++n ){` |
|     518 |  689 | `			char c = (char)SyToLower(zKey[n]);` |
|     518 |  690 | `			ph7_value_string(pList->pName,&c,1);` |
|     261 |  691 | `		}` |
|       - |  692 | `	}` |
|    5814 |  693 | `	ph7_array_add_elem(pList->pArray,0/*Automatic index assign*/,pList->pName); /* Will make it's own copy */` |
|    5814 |  694 | `	ph7_value_reset_string_cursor(pList->pName);` |
|    5814 |  695 | `	return SXRET_OK;` |
|    4781 |  696 | `}` |
|       - |  697 |  |
|       - |  698 | `/*` |
|       - |  699 | ` * Build php's get_declared_classes()/get_declared_interfaces()/get_declared_traits()` |
|       - |  700 | ` * answer for one kind.` |
|       - |  701 | ` */` |
|      32 |  702 | `static int VmDeclaredNameList(ph7_context *pCtx,int iKind)` |
|       4 |  703 | `{` |
|       - |  704 | `	struct VmDeclaredList sList;` |
|       - |  705 | `	/* Create a new array first */` |
|      36 |  706 | `	sList.iKind = iKind;` |
|      36 |  707 | `	sList.pArray = ph7_context_new_array(pCtx);` |
|      36 |  708 | `	sList.pName = ph7_context_new_scalar(pCtx);` |
|      36 |  709 | `	if( sList.pArray == 0 \|\| sList.pName == 0 ){` |
|       - |  710 | `		/* Out of memory,return NULL */` |
|     ! 0 |  711 | `		ph7_result_null(pCtx);` |
|     ! 0 |  712 | `		return PH7_OK;` |
|       - |  713 | `	}` |
|       - |  714 | `	/* hClass is head-pushed, so its forward order is reverse-insertion; php reports` |
|       - |  715 | `	 * these lists in DECLARATION order (its own class table is append-ordered), which` |
|       - |  716 | `	 * is what the backward walk yields. */` |
|      36 |  717 | `	SyHashForEachReverse(&pCtx->pVm->hClass,VmDeclaredNameStep,(void *)&sList);` |
|       - |  718 | `	/* Return the created array */` |
|      36 |  719 | `	ph7_result_value(pCtx,sList.pArray);` |
|      36 |  720 | `	return PH7_OK;` |
|      20 |  721 | `}` |
|       - |  722 | `/*` |
|       - |  723 | ` * array get_declared_classes(void)` |
|       - |  724 | ` *   Returns an array with the name of the defined classes` |
|       - |  725 | ` * Parameters` |
|       - |  726 | ` *  None` |
|       - |  727 | ` * Return` |
|       - |  728 | ` *   Returns an array of the names of the declared classes` |
|       - |  729 | ` *   in the current script.` |
|       - |  730 | ` * Note:` |
|       - |  731 | ` *   NULL is returned on failure.` |
|       - |  732 | ` */` |
|      22 |  733 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  734 | `{` |
|      11 |  735 | `	SXUNUSED(nArg); /* cc warning */` |
|      11 |  736 | `	SXUNUSED(apArg);` |
|      26 |  737 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_CLASS);` |
|       4 |  738 | `}` |
|       - |  739 | `/*` |
|       - |  740 | ` * array get_declared_interfaces(void)` |
|       - |  741 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  742 | ` * Parameters` |
|       - |  743 | ` *  None` |
|       - |  744 | ` * Return` |
|       - |  745 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  746 | ` *   in the current script.` |
|       - |  747 | ` * Note:` |
|       - |  748 | ` *   NULL is returned on failure.` |
|       - |  749 | ` */` |
|       6 |  750 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  751 | `{` |
|       3 |  752 | `	SXUNUSED(nArg); /* cc warning */` |
|       3 |  753 | `	SXUNUSED(apArg);` |
|       9 |  754 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_INTERFACE);` |
|       3 |  755 | `}` |
|       - |  756 | `/*` |
|       - |  757 | ` * array get_declared_traits(void)` |
|       - |  758 | ` *   Returns an array with the name of the defined traits.` |
|       - |  759 | ` */` |
|       4 |  760 | `PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  761 | `{` |
|       2 |  762 | `	SXUNUSED(nArg); /* cc warning */` |
|       2 |  763 | `	SXUNUSED(apArg);` |
|       6 |  764 | `	return VmDeclaredNameList(pCtx,VM_DECLARED_TRAIT);` |
|       2 |  765 | `}` |
|       - |  766 | `/*` |
|       - |  767 | ` * Does this method-table entry answer to the method's OWN name (rather than to an` |
|       - |  768 | ` * adaptation alias made from it)? Method names fold case, so the comparison does too.` |
|       - |  769 | ` */` |
|     122 |  770 | `static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       2 |  771 | `{` |
|     183 |  772 | `	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte` |
|     122 |  773 | `		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;` |
|       2 |  774 | `}` |
|       - |  775 | `/*` |
|       - |  776 | ` * Which inheritance LEVEL does this method-table entry belong to — the class php would` |
|       - |  777 | ` * have added it under? A method declared in a class body is its own; a trait's is the` |
|       - |  778 | ` * class that COMPOSED it, which is two different questions depending on the entry. An` |
|       - |  779 | ` * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still` |
|       - |  780 | `` * holding this key over this very struct is the one whose `use` block wrote it. A trait`` |
|       - |  781 | ` * method under its own name is the same struct in every class that uses the trait, and` |
|       - |  782 | ` * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait` |
|       - |  783 | ` * composes its own copy, and the parent's inherited entry never replaces it.` |
|       - |  784 | ` */` |
|    2094 |  785 | `static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)` |
|       3 |  786 | `{` |
|    2097 |  787 | `	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;` |
|    2097 |  788 | `	ph7_class *pWalk,*pHigh = 0;` |
|    2097 |  789 | `	if( pDecl == 0 \|\| (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2041 |  790 | `		return pDecl;` |
|       - |  791 | `	}` |
|      58 |  792 | `	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){` |
|      62 |  793 | `		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      38 |  794 | `			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);` |
|      38 |  795 | `			if( pE && pE->pUserData == (void *)pMeth ){` |
|      34 |  796 | `				pHigh = pWalk;` |
|      16 |  797 | `			}` |
|      20 |  798 | `		}` |
|      26 |  799 | `		if( pHigh ){` |
|      26 |  800 | `			return pHigh;` |
|       - |  801 | `		}` |
|     ! 0 |  802 | `	}` |
|      34 |  803 | `	return PH7_VmComposingClass(pClass,pDecl);` |
|    1050 |  804 | `}` |
|       - |  805 | `/*` |
|       - |  806 | ` * Append one method-table entry's name to the result array. The name is the entry's HASH` |
|       - |  807 | `` * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in`` |
|       - |  808 | ` * its method struct while the key carries the alias — php lists the alias.` |
|       - |  809 | ` */` |
|    1142 |  810 | `static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)` |
|       3 |  811 | `{` |
|    1145 |  812 | `	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    1145 |  813 | `	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    1145 |  814 | `	ph7_value_reset_string_cursor(pName);` |
|    1145 |  815 | `}` |
|       - |  816 | `/*` |
|       - |  817 | ` * The three functions that answer what a class is RELATED to: the ancestors it` |
|       - |  818 | ` * extends, the interfaces it carries and the traits it composed. All three` |
|       - |  819 | ` * resolve the same argument and say the same two things when they cannot, so the` |
|       - |  820 | ` * lookup is written once here.` |
|       - |  821 | ` *` |
|       - |  822 | `` * php's ZPP for `$object_or_class` is Z_PARAM_OBJ_OR_STR, which screens without`` |
|       - |  823 | ` * DECLARING -- VmObjectOrClassScreen() above.` |
|       - |  824 | ` */` |
|     160 |  825 | `static ph7_class * VmClassRelationTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  826 | `{` |
|       - |  827 | `	const char *zName,*zLook;` |
|       - |  828 | `	int nLen,nShow,bAutoload;` |
|       - |  829 | `	sxu32 nLook;` |
|       - |  830 | `	SyHashEntry *pEntry;` |
|     165 |  831 | `	if( VmObjectOrClassScreen(pCtx,nArg,apArg) != PH7_OK ){` |
|      25 |  832 | `		return 0;` |
|       - |  833 | `	}` |
|     141 |  834 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      21 |  835 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      21 |  836 | `		return pInst ? pInst->pClass : 0;` |
|       - |  837 | `	}` |
|     122 |  838 | `	bAutoload = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     122 |  839 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|     122 |  840 | `	zLook = zName;` |
|     122 |  841 | `	nLook = (sxu32)nLen;` |
|       - |  842 | `	/* A leading '\' is the global-namespace anchor for the LOOKUP and part of the` |
|       - |  843 | `	 * name for the DIAGNOSTIC: php reports back what it was handed. */` |
|     122 |  844 | `	PH7_VmClassNameAnchor(&zLook,&nLook);` |
|     122 |  845 | `	pEntry = nLook > 0 ? PH7_VmClassEntry(pCtx->pVm,zLook,nLook) : 0;` |
|     122 |  846 | `	if( pEntry == 0 && nLook > 0 && bAutoload ){` |
|      10 |  847 | `		if( PH7_VmTriggerAutoload(pCtx->pVm,zLook,nLook,FALSE) ){` |
|     ! 0 |  848 | `			pEntry = PH7_VmClassEntry(pCtx->pVm,zLook,nLook);` |
|     ! 0 |  849 | `		}` |
|       4 |  850 | `	}` |
|     122 |  851 | `	if( pEntry ){` |
|       - |  852 | `		/* Anything a class name can stand for answers, php's zend_lookup_class` |
|       - |  853 | `		 * included: an interface, a trait and an enum all have relations to` |
|       - |  854 | `		 * report, and PHL's class_exists() gate said FALSE for every one of them` |
|       - |  855 | ``		 * -- so `class_implements('Countable')` was false rather than a list. */`` |
|     101 |  856 | `		return (ph7_class *)pEntry->pUserData;` |
|       - |  857 | `	}` |
|       - |  858 | `	/* php has TWO sentences here and the difference is whether it was allowed to` |
|       - |  859 | `	 * look: the autoloading form says the name could not be LOADED, the other only` |
|       - |  860 | `	 * that it does not exist. PHL said neither -- all three answered a bare false,` |
|       - |  861 | `	 * so a typo in a class name was completely silent. The name is printed as php` |
|       - |  862 | `	 * prints it, which stops at the first NUL. */` |
|     110 |  863 | `	for( nShow = 0 ; nShow < nLen && zName[nShow] ; ++nShow ){}` |
|       - |  864 | ``	/* The context prints `class_uses(): ` in front of this itself. */`` |
|      32 |  865 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      10 |  866 | `		bAutoload ? "Class %.*s does not exist and could not be loaded"` |
|       - |  867 | `		          : "Class %.*s does not exist",` |
|      10 |  868 | `		nShow,zName);` |
|      22 |  869 | `	return 0;` |
|      85 |  870 | `}` |
|       - |  871 | ``/* `[$name => $name, ...]` over a list of classes, php's shape for all three. */`` |
|     116 |  872 | `static int VmClassRelationList(ph7_context *pCtx,ph7_class **apClass,sxu32 nClass)` |
|       4 |  873 | `{` |
|       - |  874 | `	ph7_value *pArray,*pName;` |
|       - |  875 | `	sxu32 n;` |
|     120 |  876 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 |  877 | `	pName = ph7_context_new_scalar(pCtx);` |
|     120 |  878 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  879 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  880 | `	}` |
|     278 |  881 | `	for( n = 0 ; n < nClass ; ++n ){` |
|     162 |  882 | `		SyString *pStr = &apClass[n]->sName;` |
|       - |  883 | `		/* The name is its own key. A ph7_value key rather than the strkey door,` |
|       - |  884 | `		 * which wants a NUL-terminated C string a SyString does not promise. */` |
|     162 |  885 | `		ph7_value_string(pName,SyStringData(pStr),(int)SyStringLength(pStr));` |
|     162 |  886 | `		ph7_array_add_elem(pArray,pName,pName);` |
|     162 |  887 | `		ph7_value_reset_string_cursor(pName);` |
|      83 |  888 | `	}` |
|     120 |  889 | `	ph7_result_value(pCtx,pArray);` |
|     120 |  890 | `	return PH7_OK;` |
|      62 |  891 | `}` |
|       - |  892 | `/*` |
|       - |  893 | ` * array\|false class_parents($object_or_class, bool $autoload = true)` |
|       - |  894 | ` *  Every ancestor of the class, nearest first, keyed by its own name.` |
|       - |  895 | ` */` |
|      44 |  896 | `PH7_PRIVATE int vm_builtin_class_parents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  897 | `{` |
|      47 |  898 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  899 | `	SySet aOut;` |
|       - |  900 | `	int rc;` |
|      47 |  901 | `	if( pClass == 0 ){` |
|      15 |  902 | `		ph7_result_bool(pCtx,0);` |
|      15 |  903 | `		return PH7_OK;` |
|       - |  904 | `	}` |
|      33 |  905 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|       - |  906 | `	/* An INTERFACE keeps its first parent on pBase here where php keeps none at` |
|       - |  907 | `	 * all, so an interface answers the empty list the way php's does. */` |
|      33 |  908 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      29 |  909 | `		ph7_class *pUp = pClass->pBase;` |
|      29 |  910 | `		sxu32 nGuard = 0;` |
|      49 |  911 | `		while( pUp && nGuard++ < 1024 ){` |
|      23 |  912 | `			SySetPut(&aOut,(const void *)&pUp);` |
|      23 |  913 | `			pUp = pUp->pBase;` |
|       3 |  914 | `		}` |
|      13 |  915 | `	}` |
|      33 |  916 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      33 |  917 | `	SySetRelease(&aOut);` |
|      33 |  918 | `	return rc;` |
|      25 |  919 | `}` |
|       - |  920 | `/*` |
|       - |  921 | ` * array\|false class_implements($object_or_class, bool $autoload = true)` |
|       - |  922 | ` *  Every interface the class carries, in the order zend linked them (the one` |
|       - |  923 | ` *  Reflection publishes too -- see PH7_ReflectInterfacesOf).` |
|       - |  924 | ` */` |
|      72 |  925 | `PH7_PRIVATE int vm_builtin_class_implements(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  926 | `{` |
|      76 |  927 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|       - |  928 | `	SySet aOut;` |
|       - |  929 | `	int rc;` |
|      76 |  930 | `	if( pClass == 0 ){` |
|      18 |  931 | `		ph7_result_bool(pCtx,0);` |
|      18 |  932 | `		return PH7_OK;` |
|       - |  933 | `	}` |
|      59 |  934 | `	SySetInit(&aOut,&pCtx->pVm->sAllocator,sizeof(ph7_class *));` |
|      59 |  935 | `	PH7_ReflectInterfacesOf(pCtx->pVm,pClass,&aOut);` |
|      59 |  936 | `	rc = VmClassRelationList(pCtx,(ph7_class **)SySetBasePtr(&aOut),SySetUsed(&aOut));` |
|      59 |  937 | `	SySetRelease(&aOut);` |
|      59 |  938 | `	return rc;` |
|      40 |  939 | `}` |
|       - |  940 | `/*` |
|       - |  941 | ` * array\|false class_uses($object_or_class, bool $autoload = true)` |
|       - |  942 | `` *  The traits this class composed ITSELF, in the order its `use` clauses named`` |
|       - |  943 | ` *  them. Not the ancestors' -- php answers the empty list for a subclass of a` |
|       - |  944 | ` *  class that uses a trait -- and not the traits those traits flattened in.` |
|       - |  945 | ` */` |
|      44 |  946 | `PH7_PRIVATE int vm_builtin_class_uses(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  947 | `{` |
|      46 |  948 | `	ph7_class *pClass = VmClassRelationTarget(pCtx,nArg,apArg);` |
|      46 |  949 | `	if( pClass == 0 ){` |
|      15 |  950 | `		ph7_result_bool(pCtx,0);` |
|      15 |  951 | `		return PH7_OK;` |
|       - |  952 | `	}` |
|      47 |  953 | `	return VmClassRelationList(pCtx,` |
|      30 |  954 | `		(ph7_class **)SySetBasePtr(&pClass->aTrait),SySetUsed(&pClass->aTrait));` |
|      24 |  955 | `}` |
|       - |  956 | `/*` |
|       - |  957 | ` * array get_class_methods(object\|string $object_or_class)` |
|       - |  958 | ` *   Returns an array with the names of the class methods the CALLING SCOPE can reach,` |
|       - |  959 | ` *   in php's order: each class's own body methods, then its trait composition, then the` |
|       - |  960 | ` *   same again for every ancestor.` |
|       - |  961 | ` * Parameters` |
|       - |  962 | ` *  object_or_class` |
|       - |  963 | ` *   The class name or a class instance. Anything that does not resolve to a class is a` |
|       - |  964 | ` *   TypeError naming the type given — this builtin never answers NULL.` |
|       - |  965 | ` */` |
|      90 |  966 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  967 | `{` |
|       - |  968 | `	ph7_value *pName,*pArray;` |
|       - |  969 | `	SyHashEntry *pEntry;` |
|       - |  970 | `	ph7_class *pClass;` |
|       - |  971 | `	/* Extract the target class first */` |
|      93 |  972 | `	pClass = 0;` |
|      93 |  973 | `	if( nArg > 0 ){` |
|      93 |  974 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      45 |  975 | `	}` |
|      93 |  976 | `	if( pClass == 0 ){` |
|       - |  977 | `		/* php screens the VALUE, not the type: anything that does not resolve to a` |
|       - |  978 | `		 * class — a name nothing declares, an int, an array, null — is ONE TypeError` |
|       - |  979 | `		 * naming the type given. PHL answered NULL for most of them (and the shared` |
|       - |  980 | ``		 * ZPP screen's `must be of type object\|string` for the rest), so a typo in a`` |
|       - |  981 | `		 * class name silently listed nothing. This is why get_class_methods() joins` |
|       - |  982 | `		 * get_class_vars() on the self-checked list in vm_arg_check.c. */` |
|       - |  983 | `		char zGiven[64];` |
|       9 |  984 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  985 | `			"get_class_methods(): Argument #1 ($object_or_class) must be an object "` |
|       - |  986 | `			"or a valid class name, %s given",` |
|       4 |  987 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - |  988 | `	}` |
|       - |  989 | `	/* Create a new array  */` |
|      89 |  990 | `	pArray = ph7_context_new_array(pCtx);` |
|      89 |  991 | `	pName = ph7_context_new_scalar(pCtx);` |
|      89 |  992 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  993 | `		/* Out of memory,return NULL */` |
|     ! 0 |  994 | `		ph7_result_null(pCtx);` |
|     ! 0 |  995 | `		return PH7_OK;` |
|       - |  996 | `	}` |
|       - |  997 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  998 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  999 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - | 1000 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - | 1001 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - | 1002 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - | 1003 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - | 1004 | `	{` |
|       - | 1005 | `		SySet aTmp;` |
|       - | 1006 | `		SyHashEntry **apEntry;` |
|       - | 1007 | `		ph7_class *pLevel;` |
|       - | 1008 | `		sxu32 n;` |
|       - | 1009 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - | 1010 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - | 1011 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      89 | 1012 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      89 | 1013 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    1285 | 1014 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    1199 | 1015 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       3 | 1016 | `		}` |
|      89 | 1017 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|     215 | 1018 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - | 1019 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - | 1020 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - | 1021 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - | 1022 | `			SySet aLvl;` |
|       - | 1023 | `			SyHashEntry **apLvl;` |
|       - | 1024 | `			sxu32 i,j;` |
|     129 | 1025 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - | 1026 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - | 1027 | `			 * come out in declaration order when walked newest-first, while` |
|       - | 1028 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - | 1029 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    2223 | 1030 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    2097 | 1031 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    2097 | 1032 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|       - | 1033 | `				/* The level a method belongs to is the class that OWNS it — for a trait` |
|       - | 1034 | `				 * method the class that composed it, not the trait. Reading sFunc.pUserData` |
|       - | 1035 | `				 * raw put every trait method on the CLASS's own level even when a BASE was` |
|       - | 1036 | `				 * the one that used the trait, so a subclass listed its inherited trait` |
|       - | 1037 | `				 * methods before its own. */` |
|    2097 | 1038 | `				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);` |
|       - | 1039 | `				/* A FABRICATED method is not in php's function table, and this walk is` |
|       - | 1040 | `` 				 * that table: `get_class_methods($closure)` does not name `__invoke` `` |
|       - | 1041 | ``				 * even though `method_exists($closure,'__invoke')` is true. */`` |
|    2097 | 1042 | `				if( pMethod->iFlags & PH7_CLASS_ATTR_FABRICATED ){` |
|       3 | 1043 | `					continue;` |
|       - | 1044 | `				}` |
|       - | 1045 | `				/* php lists only what the CALLING scope could reach: public always,` |
|       - | 1046 | `				 * protected within the hierarchy, private only from the class that` |
|       - | 1047 | `				 * declares it. PHL listed the whole table, so global-scope code was handed` |
|       - | 1048 | `				 * every private and protected name a class holds. Same decision` |
|       - | 1049 | `				 * get_class_vars() already makes for properties. */` |
|    2095 | 1050 | `				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 1051 | `					SyString sMName;` |
|      98 | 1052 | `					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,` |
|       - | 1053 | `						apEntry[nPick]->nKeyLen);` |
|       - | 1054 | `					/* The DECISION is the owning class's, which is not always the LEVEL` |
|       - | 1055 | ``					 * above: an inherited alias is listed with the class whose `use` block`` |
|       - | 1056 | `					 * wrote it, and judged against the class that composed the method. */` |
|     146 | 1057 | `					if( !PH7_VmClassMemberAccess(pCtx->pVm,` |
|      48 | 1058 | `							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,` |
|      48 | 1059 | `							pMethod->iProtection,FALSE) ){` |
|      84 | 1060 | `						continue;` |
|       - | 1061 | `					}` |
|       7 | 1062 | `				}` |
|    2013 | 1063 | `				if( pDecl != pLevel ){` |
|       - | 1064 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - | 1065 | `					 * counts as the class's own level, like php. */` |
|       - | 1066 | `					ph7_class *pWalk;` |
|     869 | 1067 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     283 | 1068 | `						continue;` |
|       - | 1069 | `					}` |
|    1219 | 1070 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|    1219 | 1071 | `						if( pWalk == pDecl ){` |
|     587 | 1072 | `							break;` |
|       - | 1073 | `						}` |
|     317 | 1074 | `					}` |
|     587 | 1075 | `					if( pWalk != 0 ){` |
|     587 | 1076 | `						continue; /* in-chain: its own level emits it */` |
|       - | 1077 | `					}` |
|     ! 0 | 1078 | `				}` |
|    1145 | 1079 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|     574 | 1080 | `			}` |
|     129 | 1081 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - | 1082 | `			/* Insertion sort by declaration line (stable) */` |
|    1153 | 1083 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    1027 | 1084 | `				SyHashEntry *pKey = apLvl[i];` |
|    1091 | 1085 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|    1111 | 1086 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|      65 | 1087 | `					apLvl[j] = apLvl[j-1];` |
|      33 | 1088 | `				}` |
|    1027 | 1089 | `				apLvl[j] = pKey;` |
|     515 | 1090 | `			}` |
|       - | 1091 | `			/* php's order INSIDE a level is not the line order: the class's own BODY methods` |
|       - | 1092 | ``			 * come first, then each USED trait in `use` order, and within a trait each of its`` |
|       - | 1093 | `			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —` |
|       - | 1094 | ``			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,`` |
|       - | 1095 | `			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the` |
|       - | 1096 | `			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line` |
|       - | 1097 | `			 * sort above still decides the body's order and, being stable, leaves two aliases` |
|       - | 1098 | `			 * of the same method in their adaptation-block order for the walk below. An emitted` |
|       - | 1099 | `			 * entry is cleared, so each name is listed once and anything these walks do not` |
|       - | 1100 | `			 * claim still goes out at the end. */` |
|    1271 | 1101 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|    1145 | 1102 | `				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|    1145 | 1103 | `				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;` |
|    1145 | 1104 | `				if( pOwn == 0 \|\| pOwn == pLevel ){` |
|    1103 | 1105 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|    1103 | 1106 | `					apLvl[i] = 0;` |
|     550 | 1107 | `				}` |
|     574 | 1108 | `			}` |
|       - | 1109 | `			{` |
|     129 | 1110 | `				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);` |
|     129 | 1111 | `				sxu32 nTrait = SySetUsed(&pLevel->aTrait);` |
|       - | 1112 | `				sxu32 k;` |
|     145 | 1113 | `				for( k = 0 ; k < nTrait ; ++k ){` |
|      18 | 1114 | `					ph7_class *pTrait = apTrait[k];` |
|       - | 1115 | `					SySet aTr;` |
|       - | 1116 | `					SyHashEntry **apTr;` |
|       - | 1117 | `					SyHashEntry *pTrE;` |
|       - | 1118 | `					sxu32 t;` |
|      18 | 1119 | `					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      18 | 1120 | `					SyHashResetLoopCursor(&pTrait->hMethod);` |
|      48 | 1121 | `					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      32 | 1122 | `						SySetPut(&aTr,(const void *)&pTrE);` |
|       2 | 1123 | `					}` |
|      18 | 1124 | `					apTr = (SyHashEntry **)SySetBasePtr(&aTr);` |
|       - | 1125 | `					/* The trait's own table walks newest-first, so backwards is its` |
|       - | 1126 | `					 * declaration order. */` |
|      48 | 1127 | `					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){` |
|      32 | 1128 | `						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;` |
|       - | 1129 | `						int bWantAlias;` |
|       - | 1130 | `						/* First pass emits the aliases made from this method, second the` |
|       - | 1131 | ``						 * method itself — php's order for `m1 as z1`. */`` |
|      92 | 1132 | `						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){` |
|     402 | 1133 | `							for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1134 | `								ph7_class_method *pM;` |
|     342 | 1135 | `								if( apLvl[i] == 0 ){` |
|     184 | 1136 | `									continue;` |
|       - | 1137 | `								}` |
|     160 | 1138 | `								pM = (ph7_class_method *)apLvl[i]->pUserData;` |
|     158 | 1139 | `								if( (ph7_class *)pM->sFunc.pUserData != pTrait` |
|     130 | 1140 | `								 \|\| pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte` |
|     102 | 1141 | `								 \|\| SyStrnmicmp(pM->sFunc.sName.zString,` |
|      98 | 1142 | `										pOrigin->sFunc.sName.zString,` |
|      98 | 1143 | `										pM->sFunc.sName.nByte) != 0 ){` |
|      94 | 1144 | `									continue;` |
|       - | 1145 | `								}` |
|      68 | 1146 | `								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){` |
|      26 | 1147 | `									continue;` |
|       - | 1148 | `								}` |
|      44 | 1149 | `								VmEmitMethodName(pArray,pName,apLvl[i]);` |
|      44 | 1150 | `								apLvl[i] = 0;` |
|      23 | 1151 | `							}` |
|      32 | 1152 | `						}` |
|      17 | 1153 | `					}` |
|      18 | 1154 | `					SySetRelease(&aTr);` |
|      10 | 1155 | `				}` |
|       - | 1156 | `			}` |
|    1271 | 1157 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - | 1158 | `				/* Whatever the two walks above did not claim — an alias made inside a trait` |
|       - | 1159 | `				 * that another trait then composed, say — keeps the line order. */` |
|    1145 | 1160 | `				if( apLvl[i] != 0 ){` |
|     ! 0 | 1161 | `					VmEmitMethodName(pArray,pName,apLvl[i]);` |
|     ! 0 | 1162 | `				}` |
|     574 | 1163 | `			}` |
|     129 | 1164 | `			SySetRelease(&aLvl);` |
|      66 | 1165 | `		}` |
|      89 | 1166 | `		SySetRelease(&aTmp);` |
|       - | 1167 | `	}` |
|       - | 1168 | `	/* Return the created array */` |
|      89 | 1169 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1170 | `	/*` |
|       - | 1171 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1172 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1173 | `	 */` |
|      89 | 1174 | `	return PH7_OK;` |
|      48 | 1175 | `}` |
|       - | 1176 | `/*` |
|       - | 1177 | ` * The class a TRAIT method's frame is really executing in: walk the receiver's ancestry` |
|       - | 1178 | `` * (or, with no receiver, the current `self`) to the first class that uses this trait.`` |
|       - | 1179 | `` * `class Base { use T; } class Kid extends Base {}` answers Base from a Kid instance, which`` |
|       - | 1180 | ` * is where php composed the method. The trait itself stands when nothing in the chain lists` |
|       - | 1181 | ` * it — a trait used by another trait reaching this through an unusual path; php has no such` |
|       - | 1182 | ` * scope, but a lie would be worse.` |
|       - | 1183 | ` */` |
|      96 | 1184 | `static ph7_class * VmTraitScopeFrom(ph7_vm *pVm,ph7_class *pTrait,VmFrame *pFrame)` |
|       4 | 1185 | `{` |
|     100 | 1186 | `	ph7_class *pWalk = pFrame ? pFrame->pSelfClass : 0;` |
|     100 | 1187 | `	if( pWalk == 0 ){` |
|       - | 1188 | `		/* No activation of its own (a closure body, an initializer): the ambient self. */` |
|     ! 0 | 1189 | `		pWalk = VmCurrentSelf(&(*pVm));` |
|     ! 0 | 1190 | `	}` |
|     100 | 1191 | `	return PH7_VmTraitUsingClass(&(*pVm),pTrait,pWalk ? pWalk : pTrait);` |
|       4 | 1192 | `}` |
|       - | 1193 | `/*` |
|       - | 1194 | ` * php's zend_get_executed_scope(): the class whose code is running, which is what every` |
|       - | 1195 | ` * visibility decision is made against — and what php NAMES in the Error when it refuses` |
|       - | 1196 | ` * ("... from scope C", or "from global scope" when this answers 0).` |
|       - | 1197 | ` *` |
|       - | 1198 | ` * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message` |
|       - | 1199 | ` * sites hardcoded "from global scope" and so reported the wrong scope for every` |
|       - | 1200 | ` * private/protected refusal raised from inside a class.` |
|       - | 1201 | ` */` |
|  105274 | 1202 | `PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)` |
|       5 | 1203 | `{` |
|  105279 | 1204 | `	VmFrame *pFrame = pVm->pFrame;` |
|       - | 1205 | `	ph7_vm_func *pVmFunc;` |
|  105279 | 1206 | `	ph7_class *pScope = 0;` |
|  205551 | 1207 | `	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - | 1208 | `		/* Safely ignore the exception frame */` |
|  100277 | 1209 | `		pFrame = pFrame->pParent;` |
|       5 | 1210 | `	}` |
|  105279 | 1211 | `	if( pFrame == 0 ){` |
|     ! 0 | 1212 | `		return 0;` |
|       - | 1213 | `	}` |
|  105279 | 1214 | `	pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - | 1215 | `	/* An INITIALIZER -- a class constant's, or an instance property's default -- is a` |
|       - | 1216 | `	 * mini-program run by VmLocalExec, which pushes no frame of its own. The frame still` |
|       - | 1217 | ``	 * current is therefore the one the `new` executed in, and every branch below would`` |
|       - | 1218 | `	 * answer the CONSTRUCTING class. php runs an initializer in its DECLARING class's` |
|       - | 1219 | ``	 * scope: `private const A;` beside `private string $x = self::A;` is readable from`` |
|       - | 1220 | ``	 * that initializer no matter where the `new` happens to be, and reading it as the`` |
|       - | 1221 | `	 * caller made a library's own tunable default an Error the moment the object was` |
|       - | 1222 | `	 * built from inside another class (phpcs's Tokens::WEIGHTINGS, Collision's` |
|       - | 1223 | `	 * Highlighter::ARROW_SYMBOL_UTF8 -- neither tool could START).` |
|       - | 1224 | `	 *` |
|       - | 1225 | `	 * pConstEvalClass alone is not the test: it stays set while the initializer runs, so` |
|       - | 1226 | `	 * a METHOD the initializer calls would inherit the initializer's scope. The marker is` |
|       - | 1227 | `	 * the frame the eval began in -- the same test PH7_VmPeekDeclaringClass makes for` |
|       - | 1228 | ``	 * `self::` -- and once a method pushes a frame it stops matching. */`` |
|  105274 | 1229 | `	if( pVm->pConstEvalClass` |
|   52643 | 1230 | `	 && pVm->pConstEvalFrame == (void *)VmSkipExceptionFrames(pVm->pFrame) ){` |
|       3 | 1231 | `		pScope = pVm->pConstEvalClass;` |
|       3 | 1232 | `		goto normalize_trait;` |
|       - | 1233 | `	}` |
|       - | 1234 | `	/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - | 1235 | `	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
|  105277 | 1236 | `	if( pFrame->pBoundScope ){` |
|      42 | 1237 | `		return pFrame->pBoundScope; /* an explicit rebind names a CLASS; never a trait */` |
|       - | 1238 | `	}` |
|  105237 | 1239 | `	if( pFrame->iFlags & VM_FRAME_UNSCOPED ){` |
|      11 | 1240 | `		return 0;` |
|       - | 1241 | `	}` |
|  157838 | 1242 | `	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|    3599 | 1243 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|  103430 | 1244 | `	}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - | 1245 | `		/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - | 1246 | `		 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - | 1247 | ``		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - | 1248 | `		 * against inside the body). php binds that class as the closure's scope,` |
|       - | 1249 | ``		 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - | 1250 | `		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - | 1251 | `		 * pBoundScope. */` |
|     149 | 1252 | `		pScope = (ph7_class *)pVmFunc->pUserData;` |
|  101559 | 1253 | `	}else if( pVm->pConstEvalClass ){` |
|       - | 1254 | `		/* Constant/property initializer bytecode runs without a method` |
|       - | 1255 | `		 * frame; its scope is the class being initialized (php: a private` |
|       - | 1256 | `		 * constant is reachable from its own class's initializers). */` |
|     ! 0 | 1257 | `		pScope = pVm->pConstEvalClass;` |
|     ! 0 | 1258 | `	}` |
|   50740 | 1259 | `normalize_trait:` |
|  105229 | 1260 | `	if( pScope && (pScope->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1261 | `		/* php COMPOSES a trait method into the using class at compile time, so the scope` |
|       - | 1262 | ``		 * its code executes in IS that class — `protected` members of the class are its`` |
|       - | 1263 | `		 * own from in there, and so are the class's protected METHODS. PHL shares a trait` |
|       - | 1264 | `		 * method by pointer and its declaring class stays the trait, so this answered the` |
|       - | 1265 | `		 * trait and every protected access from a trait body was refused. */` |
|     100 | 1266 | `		pScope = VmTraitScopeFrom(&(*pVm),pScope,pFrame);` |
|      48 | 1267 | `	}` |
|  105229 | 1268 | `	return pScope;` |
|   52642 | 1269 | `}` |
|       - | 1270 | `/*` |
|       - | 1271 | ` * php's scope of a PLAIN closure (its func->common.scope), which is what php qualifies it` |
|       - | 1272 | `` * with wherever it names it -- the `class` of a backtrace frame, and the `C::` in front of`` |
|       - | 1273 | `` * `{closure:…}` in an argument diagnostic. In php's order: an explicit rebind's scope`` |
|       - | 1274 | ` * (pBound); else the class the closure was WRITTEN in, the using class for a trait's` |
|       - | 1275 | `` * (pFrom names the class the call went through); else, for a closure carrying a `$this`,`` |
|       - | 1276 | `` * `Closure` itself -- php gives a scopeless closure that dummy scope the moment an object`` |
|       - | 1277 | ` * is bound to it. 0 for a closure with neither.` |
|       - | 1278 | ` *` |
|       - | 1279 | `` * `bindTo($o, null)` on a closure written in a class is php's `Closure` too, and PHL cannot`` |
|       - | 1280 | ` * tell it from a keep-scope rebind (both leave $__scope empty), so that one answers the` |
|       - | 1281 | ` * written class.` |
|       - | 1282 | ` */` |
|   25948 | 1283 | `PH7_PRIVATE ph7_class * PH7_VmClosureFuncScope(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pBound,` |
|       - | 1284 | `	int bThis,ph7_class *pFrom)` |
|       5 | 1285 | `{` |
|   25953 | 1286 | `	ph7_class *pScope = pBound;` |
|   25953 | 1287 | `	if( pScope == 0 && pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) && pFunc->pUserData ){` |
|     384 | 1288 | `		pScope = (ph7_class *)pFunc->pUserData;` |
|     384 | 1289 | `		if( (pScope->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|      12 | 1290 | `			if( pFrom == 0 ){` |
|       5 | 1291 | `				pFrom = (ph7_class *)pFunc->pLsbClass;` |
|       2 | 1292 | `			}` |
|      12 | 1293 | `			if( pFrom ){` |
|      12 | 1294 | `				pScope = PH7_VmTraitUsingClass(&(*pVm),pScope,pFrom);` |
|       5 | 1295 | `			}` |
|       5 | 1296 | `		}` |
|     190 | 1297 | `	}` |
|   25953 | 1298 | `	if( pScope == 0 && bThis ){` |
|       3 | 1299 | `		pScope = pVm->pClosureClass;` |
|       1 | 1300 | `	}` |
|   25953 | 1301 | `	return pScope;` |
|       5 | 1302 | `}` |
|       - | 1303 | `/*` |
|       - | 1304 | ` * The DECLARING-side twin of PH7_VmCallerScope: the class php NAMES as a method's` |
|       - | 1305 | ` * owner. php composes a trait INTO the class that uses it — the composed method's scope` |
|       - | 1306 | `` * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with`` |
|       - | 1307 | ` * "Call to private method C::p()", from a subclass instance too, and never says T. PHL` |
|       - | 1308 | ` * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see` |
|       - | 1309 | ` * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the` |
|       - | 1310 | ` * class the lookup went through up its ancestry to the first one that uses this trait.` |
|       - | 1311 | ` *` |
|       - | 1312 | ` * pClass is the class the method was reached through (the receiver's, or the named one).` |
|       - | 1313 | ` * A non-trait declarer is returned unchanged, which is php too: a base's private method` |
|       - | 1314 | ` * refused on a child instance names the BASE.` |
|       - | 1315 | ` */` |
|    1446 | 1316 | `PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 | 1317 | `{` |
|     723 | 1318 | `	SXUNUSED(pVm);` |
|    2897 | 1319 | `	return PH7_VmComposingClass(pClass,` |
|    1446 | 1320 | `		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);` |
|       5 | 1321 | `}` |
|       - | 1322 | `/*` |
|       - | 1323 | ` * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's` |
|       - | 1324 | ` * property is composed into the using class exactly as its methods are, and` |
|       - | 1325 | ` * property_exists() asks the same "is this member's class the one I asked about"` |
|       - | 1326 | ` * question). A declarer that is not a trait is the answer; a trait resolves to the first` |
|       - | 1327 | ` * class in pClass's ancestry that uses it, and stands for itself when nothing does.` |
|       - | 1328 | ` */` |
|    1492 | 1329 | `PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)` |
|       5 | 1330 | `{` |
|       - | 1331 | `	/* One rule, one implementation: a trait's members belong to the class that composed` |
|       - | 1332 | `	 * them, found by walking pClass's ancestry -- through NESTED trait use as well, since` |
|       - | 1333 | ``	 * php flattens `class C { use Outer; } trait Outer { use Inner; }` into C whole. This`` |
|       - | 1334 | `	 * walk used to look at each class's OWN trait list only, so a member reached through` |
|       - | 1335 | `	 * two levels kept the inner trait as its owner and every visibility rule about it` |
|       - | 1336 | `	 * became a question about the trait. */` |
|    1497 | 1337 | `	return PH7_VmMemberOwnerClass(pDecl,pClass);` |
|       5 | 1338 | `}` |
|       - | 1339 | `/*` |
|       - | 1340 | ` * The name php prints for a method: the identity the class REGISTERED it under, not the` |
|       - | 1341 | `` * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files`` |
|       - | 1342 | `` * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias`` |
|       - | 1343 | `` * into a function of its own, names `pHi`. Falls back to the requested name when the class`` |
|       - | 1344 | ` * holds no entry for it.` |
|       - | 1345 | ` */` |
|     102 | 1346 | `PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)` |
|       4 | 1347 | `{` |
|     106 | 1348 | `	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;` |
|     106 | 1349 | `	if( pEntry ){` |
|     106 | 1350 | `		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      55 | 1351 | `	}else{` |
|     ! 0 | 1352 | `		SyStringInitFromBuf(pOut,zName,nByte);` |
|       - | 1353 | `	}` |
|     106 | 1354 | `}` |
|       - | 1355 | `/*` |
|       - | 1356 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - | 1357 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - | 1358 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - | 1359 | ` */` |
|  222211 | 1360 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - | 1361 | `	ph7_vm *pVm,               /* Target VM */` |
|       - | 1362 | `	ph7_class *pClass,         /* Target Class */` |
|       - | 1363 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1364 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - | 1365 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - | 1366 | `	)` |
|       5 | 1367 | `{` |
|  222216 | 1368 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    1471 | 1369 | `		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));` |
|    1471 | 1370 | `		if( pCallerScope == 0 ){` |
|     747 | 1371 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - | 1372 | `		}` |
|     729 | 1373 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1374 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - | 1375 | `			 * class must declare a private attribute of this name (a base` |
|       - | 1376 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - | 1377 | `			 * child method touching an inherited base-private fails). An attr` |
|       - | 1378 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - | 1379 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - | 1380 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - | 1381 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - | 1382 | `			 * match (dynamic props and other non-declared shapes). */` |
|     459 | 1383 | `			ph7_class *pCaller = pCallerScope;` |
|     686 | 1384 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|     454 | 1385 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|     459 | 1386 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|     459 | 1387 | `			int bGranted = 0;` |
|     459 | 1388 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       8 | 1389 | `				if( pOwn->pDeclClass == 0` |
|       8 | 1390 | `				 \|\| pOwn->pDeclClass == pCaller` |
|       6 | 1391 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|      10 | 1392 | `					bGranted = 1;` |
|       6 | 1393 | `				}` |
|     455 | 1394 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     295 | 1395 | `				bGranted = 1;` |
|     145 | 1396 | `			}` |
|     459 | 1397 | `			if( !bGranted ){` |
|       - | 1398 | `				/* Check if the caller is a trait used by pClass */` |
|       - | 1399 | `				ph7_class **apTrait;` |
|       - | 1400 | `				sxu32 nTrait,k;` |
|     161 | 1401 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     161 | 1402 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     161 | 1403 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1404 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 | 1405 | `						bGranted = 1;` |
|     ! 0 | 1406 | `						break;` |
|       - | 1407 | `					}` |
|     ! 0 | 1408 | `				}` |
|      78 | 1409 | `			}` |
|     459 | 1410 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1411 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - | 1412 | `				 * member behaves as if declared in the adopting class, so a` |
|       - | 1413 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - | 1414 | `				 * from a using class's static method reaching a trait-private` |
|       - | 1415 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - | 1416 | `				 * whose owner is the trait, not the class). */` |
|       - | 1417 | `				ph7_class **apTrait;` |
|       - | 1418 | `				sxu32 nTrait,k;` |
|       3 | 1419 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|       3 | 1420 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|       3 | 1421 | `				for(k = 0; k < nTrait; k++){` |
|       3 | 1422 | `					if( apTrait[k] == pClass ){` |
|       3 | 1423 | `						bGranted = 1;` |
|       3 | 1424 | `						break;` |
|       - | 1425 | `					}` |
|     ! 0 | 1426 | `				}` |
|       1 | 1427 | `			}` |
|     459 | 1428 | `			if( !bGranted ){` |
|     159 | 1429 | `				goto dis; /* Access is forbidden */` |
|       - | 1430 | `			}` |
|     155 | 1431 | `		}else{` |
|       - | 1432 | `			/* Protected */` |
|     275 | 1433 | `			ph7_class *pBase = pCallerScope;` |
|       - | 1434 | `			/* The walk below exists for one shape only -- a SIBLING scope, which reaches` |
|       - | 1435 | `			 * a protected member through the ancestor that INTRODUCES it. When the two` |
|       - | 1436 | `			 * classes are on one inheritance chain the walk cannot change the answer, so` |
|       - | 1437 | `			 * it is skipped: the introducing class is by construction an ancestor-or-self` |
|       - | 1438 | `			 * of pClass, so it lies on that same chain, and one of the two directions of` |
|       - | 1439 | `			 * the test below therefore holds however far the walk climbs. Asked over the` |
|       - | 1440 | `			 * pBase links alone rather than through PH7_VmInstanceOf, so an interface in` |
|       - | 1441 | `			 * the picture cannot make the shortcut claim more than the walk would. It is` |
|       - | 1442 | `			 * the ordinary case -- a class touching its own protected member -- and the` |
|       - | 1443 | `			 * walk it skips costs two hash lookups per ANCESTOR, per access. */` |
|       - | 1444 | `			{` |
|       - | 1445 | `				ph7_class *pChain;` |
|     385 | 1446 | `				for( pChain = pClass ; pChain ; pChain = pChain->pBase ){` |
|     313 | 1447 | `					if( pChain == pBase ){` |
|     203 | 1448 | `						return 1;` |
|       - | 1449 | `					}` |
|      59 | 1450 | `				}` |
|     154 | 1451 | `				for( pChain = pBase ; pChain ; pChain = pChain->pBase ){` |
|     138 | 1452 | `					if( pChain == pClass ){` |
|      60 | 1453 | `						return 1;` |
|       - | 1454 | `					}` |
|      43 | 1455 | `				}` |
|       - | 1456 | `			}` |
|       - | 1457 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - | 1458 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - | 1459 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - | 1460 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - | 1461 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - | 1462 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - | 1463 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - | 1464 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - | 1465 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - | 1466 | `			 * from a sibling, matching php. */` |
|      17 | 1467 | `			ph7_class *pIntro = pClass;` |
|       - | 1468 | `			ph7_class *pAnc;` |
|      39 | 1469 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
|      23 | 1470 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
|      23 | 1471 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|      23 | 1472 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
|      23 | 1473 | `				int bHere = 0;` |
|      23 | 1474 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|      15 | 1475 | `					bHere = 1;` |
|       7 | 1476 | `				}` |
|      23 | 1477 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
|       7 | 1478 | `					bHere = 1;` |
|       3 | 1479 | `				}` |
|      23 | 1480 | `				if( bHere ){` |
|      21 | 1481 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|      10 | 1482 | `				}` |
|      12 | 1483 | `			}` |
|       - | 1484 | `			/* Must be in the same class hierarchy as the introducing class */` |
|      17 | 1485 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      13 | 1486 | `				int bTraitGrant = 0;` |
|      13 | 1487 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - | 1488 | `					/* Same trait-target rule as the private branch above */` |
|       - | 1489 | `					ph7_class **apTrait;` |
|       - | 1490 | `					sxu32 nTrait,k;` |
|     ! 0 | 1491 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     ! 0 | 1492 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     ! 0 | 1493 | `					for(k = 0; k < nTrait; k++){` |
|     ! 0 | 1494 | `						if( apTrait[k] == pClass ){` |
|     ! 0 | 1495 | `							bTraitGrant = 1;` |
|     ! 0 | 1496 | `							break;` |
|       - | 1497 | `						}` |
|     ! 0 | 1498 | `					}` |
|     ! 0 | 1499 | `				}` |
|      13 | 1500 | `				if( !bTraitGrant ){` |
|      13 | 1501 | `					goto dis; /* Access is forbidden */` |
|       - | 1502 | `				}` |
|     ! 0 | 1503 | `			}` |
|       - | 1504 | `		}` |
|     152 | 1505 | `	}` |
|  221054 | 1506 | `	return 1; /* Access is granted */` |
|     454 | 1507 | `dis:` |
|     913 | 1508 | `	if( bLog ){` |
|     ! 0 | 1509 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1510 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1511 | `			&pClass->sDisp,pAttrName);` |
|     ! 0 | 1512 | `	}` |
|     913 | 1513 | `	return 0; /* Access is forbidden */` |
|  111108 | 1514 | `}` |
|       - | 1515 | `/*` |
|       - | 1516 | ` * The same question about one PROPERTY rather than about a NAME.` |
|       - | 1517 | ` *` |
|       - | 1518 | ` * A private member belongs to a SLOT, not to a name: once a base's private` |
|       - | 1519 | ` * instance property is carried into a subclass under php's mangled storage name` |
|       - | 1520 | ` * (PH7_ClassAttrStorageName), one object can hold two of them, and the name-based` |
|       - | 1521 | ` * rule above would hand the executing scope whichever it was shown -- a subclass` |
|       - | 1522 | `` * method reading its own `$q` would be granted its base's `$q` just as readily.`` |
|       - | 1523 | ` * php decides an inherited private by identity alone: the executing scope IS the` |
|       - | 1524 | ` * declaring class, or the slot is not reachable at all.` |
|       - | 1525 | ` *` |
|       - | 1526 | ` * A property the reflected class DECLARED is left to the name-based rule, whose` |
|       - | 1527 | ` * trait grants and legacy fallbacks are what every other caller has always had.` |
|       - | 1528 | ` */` |
|  223493 | 1529 | `PH7_PRIVATE int PH7_VmClassAttrAccess(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bLog)` |
|       5 | 1530 | `{` |
|  223493 | 1531 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  113115 | 1532 | `	 && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|    2737 | 1533 | `		ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|    2737 | 1534 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    2737 | 1535 | `		int bDeny = 0;` |
|    2737 | 1536 | `		if( pScope && pOwner ){` |
|    2595 | 1537 | `			if( pScope == pOwner ){` |
|    2567 | 1538 | `				return 1;   /* php's rule, stated positively */` |
|       - | 1539 | `			}` |
|      30 | 1540 | `			if( pOwner != pClass ){` |
|       - | 1541 | `				/* An INHERITED private: this slot is the declaring class's and no` |
|       - | 1542 | `				 * one else's, so identity is the whole rule. */` |
|      27 | 1543 | `				bDeny = 1;` |
|      14 | 1544 | `			}else{` |
|       - | 1545 | `				/* The scope declares a private of this NAME, but it is a member of` |
|       - | 1546 | `				 * its own -- granting on the name would hand a base method the` |
|       - | 1547 | `				 * CHILD's property (and the other way round). Properties and` |
|       - | 1548 | `				 * constants are php's two separate namespaces, so ask the table` |
|       - | 1549 | `				 * this member belongs to. */` |
|       4 | 1550 | `				SyHash *pTab = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|       2 | 1551 | `					? &pScope->hConst : &pScope->hAttr;` |
|       4 | 1552 | `				SyHashEntry *pE = SyHashGet(pTab,` |
|       2 | 1553 | `					(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|       3 | 1554 | `				ph7_class_attr *pOwn = pE ? (ph7_class_attr *)pE->pUserData : 0;` |
|       3 | 1555 | `				if( pOwn && pOwn != pAttr && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 1556 | `					bDeny = 1;` |
|     ! 0 | 1557 | `				}` |
|       - | 1558 | `			}` |
|      14 | 1559 | `		}` |
|     174 | 1560 | `		if( bDeny ){` |
|      27 | 1561 | `			if( bLog ){` |
|     ! 0 | 1562 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1563 | `					"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 | 1564 | `					&pClass->sDisp,&pAttr->sName);` |
|     ! 0 | 1565 | `			}` |
|      27 | 1566 | `			return 0;` |
|       - | 1567 | `		}` |
|      72 | 1568 | `	}` |
|  220910 | 1569 | `	return PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,bLog);` |
|  111749 | 1570 | `}` |
|       - | 1571 | `/*` |
|       - | 1572 | ` * Where in the inheritance chain a property was DECLARED, counted from the class` |
|       - | 1573 | ` * being listed: 0 for its own declarations, 1 for its parent's, and so on. A` |
|       - | 1574 | ` * property copied in from a TRAIT reports 0, because php compiles a trait's` |
|       - | 1575 | ` * properties into the using class itself.` |
|       - | 1576 | ` *` |
|       - | 1577 | ` * php's property table is built own-declarations-first and then APPENDED to by` |
|       - | 1578 | ` * each inheritance step, so this depth is the order get_class_vars() answers in.` |
|       - | 1579 | ` * The engine's own table is keyed by name and carries the inherited entries` |
|       - | 1580 | ` * first, which is why listing it has to sort rather than walk.` |
|       - | 1581 | ` */` |
|     150 | 1582 | `static int VmClassAttrDeclDepth(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       4 | 1583 | `{` |
|     154 | 1584 | `	ph7_class *pCls = pClass;` |
|     154 | 1585 | `	int iDepth = 0;` |
|     206 | 1586 | `	while( pCls ){` |
|     206 | 1587 | `		if( pAttr->pDeclClass == pCls ){` |
|     154 | 1588 | `			return iDepth;` |
|       - | 1589 | `		}` |
|      54 | 1590 | `		pCls = pCls->pBase;` |
|      54 | 1591 | `		++iDepth;` |
|       2 | 1592 | `	}` |
|     ! 0 | 1593 | `	return 0;` |
|      79 | 1594 | `}` |
|       - | 1595 | `/*` |
|       - | 1596 | ` * array get_class_vars(string/object $class_name)` |
|       - | 1597 | ` *   Get the default properties of the class` |
|       - | 1598 | ` * Parameters` |
|       - | 1599 | ` *  class_name` |
|       - | 1600 | ` *   The class name or class instance` |
|       - | 1601 | ` * Return` |
|       - | 1602 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - | 1603 | ` *  with their default value. The resulting array elements are in the form` |
|       - | 1604 | ` *  of varname => value.` |
|       - | 1605 | ` * Note:` |
|       - | 1606 | ` *   NULL is returned on failure.` |
|       - | 1607 | ` */` |
|      38 | 1608 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1609 | `{` |
|       - | 1610 | `	ph7_value *pName,*pArray,sValue;` |
|       - | 1611 | `	SyHashEntry *pEntry;` |
|       - | 1612 | `	ph7_class *pClass,*pWalk;` |
|       - | 1613 | `	int iPass,iDepth,iMaxDepth;` |
|       - | 1614 | `	/* Extract the target class first */` |
|      42 | 1615 | `	pClass = 0;` |
|      42 | 1616 | `	if( nArg > 0 ){` |
|      42 | 1617 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      19 | 1618 | `	}` |
|      42 | 1619 | `	if( pClass == 0 ){` |
|       - | 1620 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - | 1621 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - | 1622 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - | 1623 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 | 1624 | `		int nLen = 0;` |
|     ! 0 | 1625 | `		const char *zVal = "";` |
|     ! 0 | 1626 | `		if( nArg > 0 ){` |
|     ! 0 | 1627 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 | 1628 | `				zVal = "Array";` |
|     ! 0 | 1629 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 | 1630 | `			}else{` |
|     ! 0 | 1631 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1632 | `			}` |
|     ! 0 | 1633 | `		}` |
|     ! 0 | 1634 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1635 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 | 1636 | `			nLen,zVal);` |
|       - | 1637 | `	}` |
|      42 | 1638 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - | 1639 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - | 1640 | `		 * class's static table: a default that threw at the declaration raises` |
|       - | 1641 | `		 * here, as it does in php. */` |
|      15 | 1642 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|      15 | 1643 | `		if( rcMat != SXRET_OK ){` |
|      10 | 1644 | `			return rcMat;` |
|       - | 1645 | `		}` |
|       2 | 1646 | `	}` |
|       - | 1647 | `	/* Create a new array  */` |
|      34 | 1648 | `	pArray = ph7_context_new_array(pCtx);` |
|      34 | 1649 | `	pName = ph7_context_new_scalar(pCtx);` |
|      34 | 1650 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      34 | 1651 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1652 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1653 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1654 | `		return PH7_OK;` |
|       - | 1655 | `	}` |
|       - | 1656 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1657 | `	 *` |
|       - | 1658 | `	 * php walks the property table TWICE — every non-static first, then every` |
|       - | 1659 | `	 * static — so the answer is the instance properties in declaration order` |
|       - | 1660 | `	 * followed by the statics in declaration order, never the two interleaved` |
|       - | 1661 | `	 * the way one pass over a single table produces them.` |
|       - | 1662 | `	 *` |
|       - | 1663 | `	 * And what a static contributes is its DEFAULT, read out of the class's` |
|       - | 1664 | `	 * default table: php never looks at the live static slot here, so a` |
|       - | 1665 | ``	 * `C::$s = 'live'` before the call does not change the answer. The class`` |
|       - | 1666 | `	 * DEFAULT is the compiled initializer, which is what the non-static arm` |
|       - | 1667 | `	 * has always evaluated, so both arms now go through the same one. */` |
|      34 | 1668 | `	iMaxDepth = 0;` |
|      70 | 1669 | `	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){` |
|      40 | 1670 | `		++iMaxDepth;` |
|      22 | 1671 | `	}` |
|      94 | 1672 | `	for( iPass = 0 ; iPass < 2 ; ++iPass ){` |
|     136 | 1673 | `	  for( iDepth = 0 ; iDepth < iMaxDepth ; ++iDepth ){` |
|      76 | 1674 | `		SyHashResetLoopCursor(&pClass->hAttr);` |
|     380 | 1675 | `		while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     308 | 1676 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       - | 1677 | `			int bStatic;` |
|     308 | 1678 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1679 | `				/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - | 1680 | `				 * get_class_vars() excludes them (raw surface) */` |
|       5 | 1681 | `				continue;` |
|       - | 1682 | `			}` |
|     304 | 1683 | `			bStatic = (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) != 0;` |
|     304 | 1684 | `			if( bStatic != iPass \|\| VmClassAttrDeclDepth(pClass,pAttr) != iDepth ){` |
|     196 | 1685 | `				continue;` |
|       - | 1686 | `			}` |
|       - | 1687 | `			/* A PRIVATE property is listed only by the class that DECLARED it.` |
|       - | 1688 | `			 * The shared visibility screen answers the question a member ACCESS` |
|       - | 1689 | `			 * asks — may this scope reach the name — and grants a subclass its` |
|       - | 1690 | `			 * inherited copy of a base private, which is the right answer for a` |
|       - | 1691 | `			 * read through the base's own methods and the wrong one here: php` |
|       - | 1692 | `			 * compares the property's declaring class against the scope and` |
|       - | 1693 | `			 * drops it, so a parent's private never appears in a subclass's` |
|       - | 1694 | `			 * listing. */` |
|     108 | 1695 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      66 | 1696 | `			 && pAttr->pDeclClass != 0` |
|      28 | 1697 | `			 && pAttr->pDeclClass != PH7_VmCallerScope(pCtx->pVm) ){` |
|      22 | 1698 | `				continue;` |
|       - | 1699 | `			}` |
|       - | 1700 | `			/* Check if the access is allowed */` |
|      92 | 1701 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      86 | 1702 | `				SyString *pAttrName = &pAttr->sName;` |
|      86 | 1703 | `				ph7_value *pValue = 0;` |
|      86 | 1704 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       - | 1705 | `					/* Constants are materialized lazily and read from their slot */` |
|     ! 0 | 1706 | `					PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     ! 0 | 1707 | `					pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|      86 | 1708 | `				}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      64 | 1709 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1710 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      64 | 1711 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|      64 | 1712 | `					PH7_VmResolvedDefault(pCtx->pVm,pClass,pAttr,&sValue);` |
|      64 | 1713 | `					pValue = &sValue;` |
|      30 | 1714 | `				}` |
|       - | 1715 | `				/* Fill in the array */` |
|      86 | 1716 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      86 | 1717 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1718 | `				/* Reset the cursor */` |
|      86 | 1719 | `				ph7_value_reset_string_cursor(pName);` |
|      41 | 1720 | `			}` |
|       4 | 1721 | `		}` |
|      40 | 1722 | `	  }` |
|      34 | 1723 | `	}` |
|      34 | 1724 | `	PH7_MemObjRelease(&sValue);` |
|       - | 1725 | `	/* Return the created array */` |
|      34 | 1726 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1727 | `	/*` |
|       - | 1728 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1729 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1730 | `	 */` |
|      34 | 1731 | `	return PH7_OK;` |
|      23 | 1732 | `}` |
|       - | 1733 | `/*` |
|       - | 1734 | ` * array get_object_vars(object $this)` |
|       - | 1735 | ` *   Gets the properties of the given object` |
|       - | 1736 | ` * Parameters` |
|       - | 1737 | ` *  this` |
|       - | 1738 | ` *   A class instance` |
|       - | 1739 | ` * Return` |
|       - | 1740 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - | 1741 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - | 1742 | ` *  it will be returned with a NULL value.` |
|       - | 1743 | ` * Note:` |
|       - | 1744 | ` *   NULL is returned on failure.` |
|       - | 1745 | ` */` |
|     390 | 1746 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1747 | `{` |
|     395 | 1748 | `	ph7_class_instance *pThis = 0;` |
|       - | 1749 | `	ph7_value *pName,*pArray;` |
|       - | 1750 | `	SyHashEntry *pEntry;` |
|     395 | 1751 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1752 | `		/* Extract the target instance */` |
|     395 | 1753 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     195 | 1754 | `	}` |
|     395 | 1755 | `	if( pThis == 0 ){` |
|       - | 1756 | `		/* No such instance,return NULL */` |
|     ! 0 | 1757 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1758 | `		return PH7_OK;` |
|       - | 1759 | `	}` |
|       - | 1760 | `	/* Create a new array  */` |
|     395 | 1761 | `	pArray = ph7_context_new_array(pCtx);` |
|     395 | 1762 | `	pName = ph7_context_new_scalar(pCtx);` |
|     395 | 1763 | `	if( pArray == 0 \|\| pName == 0){` |
|       - | 1764 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1765 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1766 | `		return PH7_OK;` |
|       - | 1767 | `	}` |
|       - | 1768 | `	/* A class whose get_properties handler answers this purpose too: php asks` |
|       - | 1769 | `	 * the handler with ZEND_PROP_PURPOSE_GET_OBJECT_VARS, and SimpleXMLElement` |
|       - | 1770 | `	 * is the one native class here that answers it with the same table it shows` |
|       - | 1771 | `	 * var_dump. Every other one answers its real (empty) slots, which is the` |
|       - | 1772 | `	 * walk below. */` |
|     390 | 1773 | `	if( PH7_ClassVarsFromPresent(pThis->pClass)` |
|     201 | 1774 | `	 && PH7_ClassInstancePresent(pThis,pArray,0) ){` |
|       3 | 1775 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 1776 | `		return PH7_OK;` |
|       - | 1777 | `	}` |
|       - | 1778 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - | 1779 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - | 1780 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - | 1781 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - | 1782 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - | 1783 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - | 1784 | `	 * skipped instead of read after free. The name snapshotted is the STORAGE` |
|       - | 1785 | `	 * key -- an inherited private lives under php's mangled one -- while the key` |
|       - | 1786 | `	 * php puts in the answer is the property's plain name. */` |
|       - | 1787 | `	{` |
|       - | 1788 | `		SySet sNames;` |
|       - | 1789 | `		SyString *aName;` |
|       - | 1790 | `		sxu32 iName,nName;` |
|     393 | 1791 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     393 | 1792 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    1809 | 1793 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    1421 | 1794 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    1421 | 1795 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|       - | 1796 | `				/* Only non-static/constant attributes are extracted */` |
|     531 | 1797 | `				continue;` |
|       - | 1798 | `			}` |
|     895 | 1799 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      51 | 1800 | `				continue; /* typed, never written: not there yet (php) */` |
|       - | 1801 | `			}` |
|     842 | 1802 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     426 | 1803 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     ! 0 | 1804 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - | 1805 | `			}` |
|     847 | 1806 | `			if( PH7_ClassInstanceAttrShadowed(pCtx->pVm,pThis,pEntry) ){` |
|       3 | 1807 | `				continue; /* an earlier accessible slot already answers for this name */` |
|       - | 1808 | `			}` |
|       - | 1809 | `			{` |
|       - | 1810 | `				SyString sKey;` |
|     845 | 1811 | `				SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|     845 | 1812 | `				SySetPut(&sNames,(const void *)&sKey);` |
|       - | 1813 | `			}` |
|       5 | 1814 | `		}` |
|     393 | 1815 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|     393 | 1816 | `		nName = SySetUsed(&sNames);` |
|    1233 | 1817 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     845 | 1818 | `			SyString *pAttrName = &aName[iName];` |
|       - | 1819 | `			VmClassAttr *pVmAttr;` |
|     845 | 1820 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     845 | 1821 | `			if( pEntry == 0 ){` |
|     ! 0 | 1822 | `				continue; /* unset by an earlier hook */` |
|       - | 1823 | `			}` |
|     845 | 1824 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1825 | `			/* Check if the access is allowed */` |
|     845 | 1826 | `			if( PH7_VmClassAttrAccess(pCtx->pVm,pThis->pClass,pVmAttr->pAttr,FALSE) ){` |
|     687 | 1827 | `				ph7_value *pValue = 0;` |
|       - | 1828 | `				ph7_value sHookVal;` |
|       - | 1829 | `				sxi32 rcHk;` |
|       - | 1830 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1831 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     687 | 1832 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     687 | 1833 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     687 | 1834 | `				if( rcHk == SXRET_OK ){` |
|      17 | 1835 | `					pValue = &sHookVal;` |
|     679 | 1836 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1837 | `					/* Extract attribute */` |
|     671 | 1838 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     338 | 1839 | `				}else{` |
|       - | 1840 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1841 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1842 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1843 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1844 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1845 | `					break;` |
|       - | 1846 | `				}` |
|     687 | 1847 | `				if( pValue ){` |
|       - | 1848 | `					/* Insert attribute name in the array -- php unmangles it. */` |
|    1028 | 1849 | `					ph7_value_string(pName,SyStringData(&pVmAttr->pAttr->sName),` |
|     682 | 1850 | `						(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     682 | 1851 | `					if( rcHk == SXERR_NOTFOUND` |
|     679 | 1852 | `					 && PH7_ClassAttrIsRef(pThis,pVmAttr) ){` |
|       - | 1853 | `						/* php hands out the property's own REFERENCE for a property that` |
|       - | 1854 | `						 * IS one, so a write through the returned element reaches the` |
|       - | 1855 | `						 * object. A HOOK's answer is a computed value with no slot behind` |
|       - | 1856 | `						 * it and stays a copy. */` |
|      74 | 1857 | `						PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,` |
|      24 | 1858 | `							pName,pVmAttr->nIdx);` |
|      26 | 1859 | `					}else{` |
|     639 | 1860 | `						ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - | 1861 | `					}` |
|     341 | 1862 | `				}` |
|     687 | 1863 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1864 | `				/* Reset the cursor */` |
|     687 | 1865 | `				ph7_value_reset_string_cursor(pName);` |
|     341 | 1866 | `			}` |
|     425 | 1867 | `		}` |
|     393 | 1868 | `		SySetRelease(&sNames);` |
|       - | 1869 | `	}` |
|       - | 1870 | `	/* Return the created array */` |
|     393 | 1871 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1872 | `	/*` |
|       - | 1873 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1874 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1875 | `	 */` |
|     393 | 1876 | `	return PH7_OK;` |
|     200 | 1877 | `}` |
|       - | 1878 | `/*` |
|       - | 1879 | ` * array get_mangled_object_vars(object $object)` |
|       - | 1880 | ` *  The object's own property table, with php's visibility MANGLING left on the keys:` |
|       - | 1881 | `` *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".`` |
|       - | 1882 | ` *` |
|       - | 1883 | ` *  It is get_object_vars()'s opposite in both of that function's decisions — no` |
|       - | 1884 | ` *  visibility screen (every property is reported, from every level of the chain) and` |
|       - | 1885 | `` *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,`` |
|       - | 1886 | ` *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not` |
|       - | 1887 | ` *  the (array) cast either: the cast asks a native class's own handler, so` |
|       - | 1888 | `` *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.`` |
|       - | 1889 | ` */` |
|      46 | 1890 | `PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1891 | `{` |
|      49 | 1892 | `	ph7_class_instance *pThis = 0;` |
|       - | 1893 | `	ph7_value *pArray;` |
|      49 | 1894 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - | 1895 | `		/* Extract the target instance */` |
|      49 | 1896 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      23 | 1897 | `	}` |
|      49 | 1898 | `	if( pThis == 0 ){` |
|       - | 1899 | ``		/* The `object $object` signature row refuses everything else before we run */`` |
|     ! 0 | 1900 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1901 | `		return PH7_OK;` |
|       - | 1902 | `	}` |
|      49 | 1903 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 1904 | `	if( pArray == 0 ){` |
|       - | 1905 | `		/* Out of memory,return NULL */` |
|     ! 0 | 1906 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1907 | `		return PH7_OK;` |
|       - | 1908 | `	}` |
|      49 | 1909 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);` |
|      49 | 1910 | `	ph7_result_value(pCtx,pArray);` |
|      49 | 1911 | `	return PH7_OK;` |
|      26 | 1912 | `}` |
|       - | 1913 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1914 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1915 | ` * detection should reject them up front. */` |
|       - | 1916 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1917 | `/*` |
|       - | 1918 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1919 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1920 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1921 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1922 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1923 | ` */` |
| 2667328 | 1924 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1925 | `{` |
| 2876171 | 1926 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1927 | `		ph7_class **apParent;` |
|       - | 1928 | `		sxu32 n;` |
| 2706663 | 1929 | `		if( pIface == pTarget ){` |
| 2497815 | 1930 | `			return TRUE;` |
|       - | 1931 | `		}` |
|       - | 1932 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1933 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|  208853 | 1934 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|  208871 | 1935 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|      29 | 1936 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|      11 | 1937 | `				return TRUE;` |
|       - | 1938 | `			}` |
|      10 | 1939 | `		}` |
|  208843 | 1940 | `		pIface = pIface->pBase;` |
|  208843 | 1941 | `		iDepth++;` |
|       5 | 1942 | `	}` |
|  169513 | 1943 | `	return FALSE;` |
| 1333595 | 1944 | `}` |
|       - | 1945 | `/*` |
|       - | 1946 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1947 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1948 | ` */` |
| 2952301 | 1949 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1950 | `{` |
|       - | 1951 | `	ph7_class **apInterface;` |
|       - | 1952 | `	sxu32 n;` |
| 2952306 | 1953 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1954 | `		/* Empty interface container */` |
|  339194 | 1955 | `		return FALSE;` |
|       - | 1956 | `	}` |
|       - | 1957 | `	/* Point to the set of implemented interfaces */` |
| 2613117 | 1958 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1959 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1960 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2782607 | 1961 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2667305 | 1962 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2497815 | 1963 | `			return TRUE;` |
|       - | 1964 | `		}` |
|   84733 | 1965 | `	}` |
|  115307 | 1966 | `	return FALSE;` |
| 1475895 | 1967 | `}` |
|       - | 1968 | `/*` |
|       - | 1969 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1970 | ` * is an instance of the main class (second argument).` |
|       - | 1971 | ` * Otherwise FALSE is returned.` |
|       - | 1972 | ` */` |
| 4201766 | 1973 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1974 | `{` |
|       - | 1975 | `	ph7_class *pParent;` |
|       - | 1976 | `	sxi32 rc;` |
| 4201771 | 1977 | `	if( pThis == pClass ){` |
|       - | 1978 | `		/* Instance of the same class */` |
| 1477080 | 1979 | `		return TRUE;` |
|       - | 1980 | `	}` |
|       - | 1981 | `	/* Check implemented interfaces */` |
| 2724696 | 1982 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2724696 | 1983 | `	if( rc ){` |
| 2363991 | 1984 | `		return TRUE;` |
|       - | 1985 | `	}` |
|       - | 1986 | `	/* Check parent classes */` |
|  360710 | 1987 | `	pParent = pThis->pBase;` |
|  454470 | 1988 | `	while( pParent ){` |
|  234107 | 1989 | `		if( pParent == pClass ){` |
|       - | 1990 | `			/* Same instance */` |
|    6533 | 1991 | `			return TRUE;` |
|       - | 1992 | `		}` |
|       - | 1993 | `		/* Check the implemented interfaces */` |
|  227579 | 1994 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  227579 | 1995 | `		if( rc ){` |
|  133819 | 1996 | `			return TRUE;` |
|       - | 1997 | `		}` |
|       - | 1998 | `		/* Point to the parent class */` |
|   93765 | 1999 | `		pParent = pParent->pBase;` |
|       5 | 2000 | `	}` |
|       - | 2001 | `	/* Not an instance of the the given class */` |
|  220368 | 2002 | `	return FALSE;` |
| 2100698 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 2006 | ` * is a subclass of the main class (second argument).` |
|       - | 2007 | ` * Otherwise FALSE is returned.` |
|       - | 2008 | ` */` |
|      46 | 2009 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       4 | 2010 | `{` |
|       - | 2011 | `	SyHashEntry *pEntry;` |
|       - | 2012 | `	SyString *pName;` |
|      76 | 2013 | `	while( pClass ){` |
|      70 | 2014 | `		pName = &pClass->sName;` |
|       - | 2015 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      70 | 2016 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      70 | 2017 | `		if( pEntry ){` |
|      33 | 2018 | `			return TRUE;` |
|       - | 2019 | `		}` |
|       - | 2020 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 2021 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 2022 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 2023 | `		 * class's aInterface, missing inherited interfaces. */` |
|      38 | 2024 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 2025 | `			return TRUE;` |
|       - | 2026 | `		}` |
|      28 | 2027 | `		pClass = pClass->pBase;` |
|       2 | 2028 | `	}` |
|       - | 2029 | `	/* Not a subclass */` |
|       8 | 2030 | `	return FALSE;` |
|      27 | 2031 | `}` |
|       - | 2032 | `/*` |
|       - | 2033 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 2034 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 2035 | ` *   as one of its parents.` |
|       - | 2036 | ` * Parameters` |
|       - | 2037 | ` *  object_or_class` |
|       - | 2038 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 2039 | ` * class` |
|       - | 2040 | ` *  The class or interface name to test against.` |
|       - | 2041 | ` * allow_string` |
|       - | 2042 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 2043 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 2044 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 2045 | ` * Return` |
|       - | 2046 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 2047 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 2048 | ` */` |
|      56 | 2049 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2050 | `{` |
|      60 | 2051 | `	int res = 0; /* Assume FALSE by default */` |
|      60 | 2052 | `	if( nArg > 1 ){` |
|      60 | 2053 | `		ph7_class *pThisClass = 0;` |
|      60 | 2054 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 2055 | `			/* An object first argument: allow_string is ignored (php). */` |
|      17 | 2056 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      52 | 2057 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 2058 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 2059 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 2060 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 2061 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      41 | 2062 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      19 | 2063 | `		}` |
|      60 | 2064 | `		if( pThisClass ){` |
|       - | 2065 | `			/* Extract the given class */` |
|      49 | 2066 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      49 | 2067 | `			if( pClass ){` |
|       - | 2068 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 2069 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      49 | 2070 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      23 | 2071 | `			}` |
|      23 | 2072 | `		}` |
|      28 | 2073 | `	}` |
|       - | 2074 | `	/* Query result */` |
|      60 | 2075 | `	ph7_result_bool(pCtx,res);` |
|      60 | 2076 | `	return PH7_OK;` |
|       4 | 2077 | `}` |
|       - | 2078 | `/*` |
|       - | 2079 | ` * int spl_object_id(object $object)` |
|       - | 2080 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 2081 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 2082 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 2083 | ` */` |
|      26 | 2084 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2085 | `{` |
|       - | 2086 | `	ph7_class_instance *pThis;` |
|      30 | 2087 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2088 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2089 | `		return PH7_OK;` |
|       - | 2090 | `	}` |
|      30 | 2091 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      30 | 2092 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      30 | 2093 | `	return PH7_OK;` |
|      17 | 2094 | `}` |
|       - | 2095 | `/*` |
|       - | 2096 | ` * object clone(object $object, array $withProperties = [])` |
|       - | 2097 | `` *  php 8.5's clone-with: `clone` is a real internal function there, so every`` |
|       - | 2098 | `` *  indirect spelling reaches it — `clone(...)` as a first-class callable,`` |
|       - | 2099 | `` *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type`` |
|       - | 2100 | ` *  refusals are the ordinary runtime ones, not a compile error. The direct` |
|       - | 2101 | `` *  `clone($o, [...])` source form compiles to a CALL of this function, and the`` |
|       - | 2102 | `` *  `clone $o` OPERATOR keeps its own opcode.`` |
|       - | 2103 | ` *` |
|       - | 2104 | ` *  The property updates are applied AFTER __clone(), each as a scope-aware write` |
|       - | 2105 | ` *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared` |
|       - | 2106 | ` *  with nothing else now. A host function runs on the CALLER's frame, so the` |
|       - | 2107 | ` *  scope those writes are judged against is php's: the scope that called clone().` |
|       - | 2108 | ` */` |
|      50 | 2109 | `PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2110 | `{` |
|      51 | 2111 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2112 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2113 | `	char zGiven[64];` |
|       - | 2114 | `	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that` |
|       - | 2115 | `	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */` |
|      51 | 2116 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2117 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2118 | `			"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     ! 0 | 2119 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");` |
|       - | 2120 | `	}` |
|      51 | 2121 | `	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       - | 2122 | `	/* The uncloneable classes, same rule and wording as the operator: an enum case` |
|       - | 2123 | `	 * (the singleton identity would break), a class whose instances own a C-side` |
|       - | 2124 | `	 * resource, and Generator/Fiber. */` |
|      50 | 2125 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|      48 | 2126 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      13 | 2127 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       8 | 2128 | `			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sDisp);` |
|       - | 2129 | `	}` |
|      43 | 2130 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|      43 | 2131 | `	if( pClone == 0 ){` |
|     ! 0 | 2132 | `		return PH7_VmMemoryError(pVm);` |
|       - | 2133 | `	}` |
|       - | 2134 | `	/* Hand the clone to the caller BEFORE the updates run: an update that throws` |
|       - | 2135 | `	 * leaves the object owned by the return slot, which releases it. */` |
|      43 | 2136 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      43 | 2137 | `	pCtx->pRet->x.pOther = pClone;` |
|      43 | 2138 | `	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);` |
|      43 | 2139 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      29 | 2140 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      29 | 2141 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|       - | 2142 | `		sxu32 n;` |
|      53 | 2143 | `		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){` |
|       - | 2144 | `			ph7_value *pVal,sVal;` |
|       - | 2145 | `			const char *zName;` |
|       - | 2146 | `			sxu32 nName;` |
|       - | 2147 | `			char zKeyBuf[64];` |
|       - | 2148 | `			sxi32 rc;` |
|      31 | 2149 | `			if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 2150 | ``				/* An int key becomes the property name (php: `$5`). */`` |
|     ! 0 | 2151 | `				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 | 2152 | `				zName = zKeyBuf;` |
|     ! 0 | 2153 | `			}else{` |
|      31 | 2154 | `				zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      31 | 2155 | `				nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - | 2156 | `			}` |
|      31 | 2157 | `			pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      31 | 2158 | `			if( pVal ){` |
|       - | 2159 | `				/* Snapshot the update value first: applying it may create a dynamic` |
|       - | 2160 | `				 * property, whose slot reservation used to reallocate pVm->aMemObj` |
|       - | 2161 | `				 * and dangle pVal. Redundant since P1 (fixed segments); left for the` |
|       - | 2162 | `				 * harvest sweep. */` |
|      31 | 2163 | `				PH7_MemObjInit(pVm,&sVal);` |
|      31 | 2164 | `				PH7_MemObjLoad(pVal,&sVal);` |
|      31 | 2165 | `				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      31 | 2166 | `				PH7_MemObjRelease(&sVal);` |
|      31 | 2167 | `				if( rc != SXRET_OK ){` |
|       7 | 2168 | `					return rc;` |
|       - | 2169 | `				}` |
|      12 | 2170 | `			}` |
|      25 | 2171 | `			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */` |
|      13 | 2172 | `		}` |
|      11 | 2173 | `	}` |
|      37 | 2174 | `	return PH7_OK;` |
|      26 | 2175 | `}` |
|       - | 2176 | `/*` |
|       - | 2177 | ` * string spl_object_hash(object $object)` |
|       - | 2178 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 2179 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 2180 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 2181 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 2182 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 2183 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 2184 | ` */` |
|      18 | 2185 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2186 | `{` |
|       - | 2187 | `	ph7_class_instance *pThis;` |
|      20 | 2188 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 2189 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2190 | `		return PH7_OK;` |
|       - | 2191 | `	}` |
|      20 | 2192 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      20 | 2193 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      20 | 2194 | `	return PH7_OK;` |
|      11 | 2195 | `}` |
|       - | 2196 | `/*` |
|       - | 2197 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 2198 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 2199 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 2200 | ` * Parameters` |
|       - | 2201 | ` *  object_or_class` |
|       - | 2202 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 2203 | ` * class` |
|       - | 2204 | ` *  The class or interface name to test against.` |
|       - | 2205 | ` * allow_string` |
|       - | 2206 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 2207 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 2208 | ` *  object first argument (php).` |
|       - | 2209 | ` * Return` |
|       - | 2210 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 2211 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 2212 | ` */` |
|      60 | 2213 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2214 | `{` |
|      64 | 2215 | `	int res = 0; /* Assume FALSE by default */` |
|      64 | 2216 | `	if( nArg > 1 ){` |
|      64 | 2217 | `		ph7_class *pClass = 0;` |
|      64 | 2218 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 2219 | `			/* An object first argument: allow_string is ignored (php). */` |
|      21 | 2220 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      55 | 2221 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 2222 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 2223 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 2224 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 2225 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      38 | 2226 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      17 | 2227 | `		}` |
|      64 | 2228 | `		if( pClass ){` |
|       - | 2229 | `			/* Extract the target class */` |
|      52 | 2230 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      52 | 2231 | `			if( pMain ){` |
|       - | 2232 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      50 | 2233 | `				res = VmSubclassOf(pClass,pMain);` |
|      23 | 2234 | `			}` |
|      24 | 2235 | `		}` |
|      30 | 2236 | `	}` |
|       - | 2237 | `	/* Query result */` |
|      64 | 2238 | `	ph7_result_bool(pCtx,res);` |
|      64 | 2239 | `	return PH7_OK;` |
|       4 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | `` * php folds `call_user_func('f', ...)` / `call_user_func_array('f', $a)` into a DIRECT`` |
|       - | 2243 | ` * call of f when the callable is a literal string naming a function its compiler can` |
|       - | 2244 | ` * bind; any other callable (a variable, a Closure, an array pair) goes through` |
|       - | 2245 | ` * ZEND_INIT_USER_CALL, which marks the call DYNAMIC. A literal reaches the C body as` |
|       - | 2246 | ` * a constant-marked operand (nIdx == SXU32_HIGH, the mark OP_CALL reads for its own` |
|       - | 2247 | ` * callee), so the same question is answerable here. See ph7_vm::bDynamicForward.` |
|       - | 2248 | ` */` |
|     754 | 2249 | `static int VmForwardIsDynamic(const ph7_value *pCallable)` |
|       5 | 2250 | `{` |
|     759 | 2251 | `	return pCallable->nIdx != SXU32_HIGH \|\| (pCallable->iFlags & MEMOBJ_STRING) == 0;` |
|       5 | 2252 | `}` |
|       - | 2253 | `/*` |
|       - | 2254 | ` * Did php's compiler fold THIS forward away? Only a compile-time bound call` |
|       - | 2255 | `` * (PH7_CTX_CALL_CT_BOUND: no spread, no `name:` argument, not unqualified inside a`` |
|       - | 2256 | ` * namespace) of the two names zend_compile_func_cuf knows. forward_static_call and` |
|       - | 2257 | ` * _array share these C bodies but are never folded. Anything else is a real internal` |
|       - | 2258 | ` * call: the forward keeps a frame of its own, its callback's frame has no call site,` |
|       - | 2259 | ` * the callback runs DYNAMIC, and a dropped answer is the forward's, not the callback's.` |
|       - | 2260 | ` */` |
|    1998 | 2261 | `static int VmForwardFolded(ph7_context *pCtx)` |
|       5 | 2262 | `{` |
|    2003 | 2263 | `	const SyString *pName = &pCtx->pFunc->sName;` |
|    2003 | 2264 | `	if( (pCtx->iFlags & PH7_CTX_CALL_CT_BOUND) == 0 ){` |
|     280 | 2265 | `		return 0;` |
|       - | 2266 | `	}` |
|    2588 | 2267 | `	return (pName->nByte == sizeof("call_user_func") - 1` |
|    1387 | 2268 | `	        && SyStrnicmp(pName->zString,"call_user_func",pName->nByte) == 0)` |
|    2840 | 2269 | `	    \|\| (pName->nByte == sizeof("call_user_func_array") - 1` |
|     631 | 2270 | `	        && SyStrnicmp(pName->zString,"call_user_func_array",pName->nByte) == 0);` |
|    1004 | 2271 | `}` |
|       - | 2272 | `/*` |
|       - | 2273 | ` * Arm the latches the callback's OP_CALL consumes, and answer whether the forward was` |
|       - | 2274 | ` * folded: only a folded one binds its callback under the caller's strict_types, since a` |
|       - | 2275 | ` * real internal call leaves php no calling file to take the mode from.` |
|       - | 2276 | ` */` |
|     914 | 2277 | `static int VmForwardArm(ph7_context *pCtx,const ph7_value *pCallable)` |
|       5 | 2278 | `{` |
|     919 | 2279 | `	ph7_vm *pVm = pCtx->pVm;` |
|     919 | 2280 | `	if( VmForwardFolded(pCtx) ){` |
|       - | 2281 | `		/* php's compiler rewrites the forward into a direct call of the callback, so` |
|       - | 2282 | `		 * a DROPPED answer here is a dropped answer for the callback: hand the bit` |
|       - | 2283 | `		 * on, one call deep. */` |
|     759 | 2284 | `		pVm->bDiscardCallback = pVm->bHostDiscard;` |
|     759 | 2285 | `		pVm->bDynamicForward = VmForwardIsDynamic(pCallable);` |
|     759 | 2286 | `		return 1;` |
|       - | 2287 | `	}` |
|     164 | 2288 | `	pVm->bDiscardCallback = 0;` |
|     164 | 2289 | `	pVm->bDynamicForward = 1;` |
|     164 | 2290 | `	pVm->pNativeFrameName = &pCtx->pFunc->sName;` |
|     164 | 2291 | `	return 0;` |
|     462 | 2292 | `}` |
|       - | 2293 | `/*` |
|       - | 2294 | ` * Screen a forward's callback. A call php's compiler bound (PH7_CTX_CALL_CT_BOUND) is` |
|       - | 2295 | ` * compiled into ZEND_INIT_USER_CALL, and it is that opcode which resolves the callable: an` |
|       - | 2296 | ` * error handler throwing on the scope deprecation leaves no reason behind, so php raises` |
|       - | 2297 | `` * `must be a valid callback, (null)` with the handler's exception as its previous. Any other`` |
|       - | 2298 | ` * shape runs the function's own parameter screen, where the handler's exception is bare.` |
|       - | 2299 | ` */` |
|    1084 | 2300 | `static sxi32 VmForwardScreen(ph7_context *pCtx,ph7_value *pCb)` |
|       5 | 2301 | `{` |
|       - | 2302 | `	ph7_class_instance *pExc;` |
|    1089 | 2303 | `	VmNativeCall *pRec = pCtx->pVm->pNativeCall;` |
|       - | 2304 | `	sxi32 rc;` |
|    1089 | 2305 | `	if( pRec && pRec->pName == &pCtx->pFunc->sName && VmForwardFolded(pCtx) ){` |
|       - | 2306 | `		/* A folded forward is no call in php, so it has no frame of its own for a` |
|       - | 2307 | `		 * trace to show: not under this screen's refusal, and not above whatever` |
|       - | 2308 | `		 * its callback runs (VmNativeCall.bElided). */` |
|     895 | 2309 | `		pRec->bElided = 1;` |
|     445 | 2310 | `	}` |
|    1089 | 2311 | `	if( (pCtx->iFlags & PH7_CTX_CALL_CT_BOUND) == 0 ){` |
|     154 | 2312 | `		return PH7_CheckCallbackArg(pCtx,pCb,1,"callback",0);` |
|       - | 2313 | `	}` |
|     939 | 2314 | `	rc = PH7_VmCallableDeprecationFenced(pCtx->pVm,pCb,&pExc);` |
|     939 | 2315 | `	if( pExc ){` |
|      10 | 2316 | `		rc = PH7_VmThrowExceptionPrev(pCtx,pExc,"TypeError",` |
|       3 | 2317 | `			"%s(): Argument #1 ($callback) must be a valid callback, (null)",ph7_function_name(pCtx));` |
|       7 | 2318 | `		PH7_ClassInstanceUnref(pExc);` |
|       7 | 2319 | `		return rc;` |
|       - | 2320 | `	}` |
|     933 | 2321 | `	if( rc != PH7_OK ){` |
|     ! 0 | 2322 | `		return rc;` |
|       - | 2323 | `	}` |
|     933 | 2324 | `	return PH7_CheckCallbackReason(pCtx,pCb,1,"callback",0);` |
|     547 | 2325 | `}` |
|       - | 2326 | `/*` |
|       - | 2327 | ` * Does the code that called this internal function run in a class scope? php asks it of` |
|       - | 2328 | ` * the frame IMMEDIATELY above (prev_execute_data), never walking past it: a userland` |
|       - | 2329 | ` * frame answers its own scope (a method's class, a closure's creation-site or bound` |
|       - | 2330 | ` * scope), and an internal caller answers ITS class -- so array_map() has none while` |
|       - | 2331 | ` * ReflectionFunction::invoke() has one wherever it is called from. A call_user_func php's` |
|       - | 2332 | ` * compiler folded is no frame, so the walk steps over it.` |
|       - | 2333 | ` */` |
|      48 | 2334 | `static int VmCallerHasClassScope(ph7_context *pCtx)` |
|       2 | 2335 | `{` |
|      50 | 2336 | `	VmNativeCall *pRec = pCtx->pVm->pNativeCall;` |
|       - | 2337 | `	VmNativeCall *pPrev;` |
|      50 | 2338 | `	if( pRec && pRec->pName == &pCtx->pFunc->sName ){` |
|      54 | 2339 | `		for( pPrev = pRec->pPrev ; pPrev && pPrev->bElided ; pPrev = pPrev->pPrev ){` |
|       3 | 2340 | `		}` |
|      50 | 2341 | `		if( pPrev && pPrev->pFrame == pRec->pFrame ){` |
|       - | 2342 | `			/* Entered with no userland activation in between: an internal caller. */` |
|       9 | 2343 | `			return pPrev->pClass != 0;` |
|       - | 2344 | `		}` |
|      20 | 2345 | `	}` |
|      40 | 2346 | `	if( (pCtx->pVm->pFrame->iFlags & (VM_FRAME_FIBER\|VM_FRAME_EXCEPTION))` |
|      22 | 2347 | `			== (VM_FRAME_FIBER\|VM_FRAME_EXCEPTION) ){` |
|       - | 2348 | `		/* Run AS a fiber's body, on the trampoline's transparent frame: php's caller` |
|       - | 2349 | `		 * is the fiber's own entry frame, which has no scope wherever it was started. */` |
|       3 | 2350 | `		return 0;` |
|       - | 2351 | `	}` |
|      40 | 2352 | `	return PH7_VmPeekDeclaringClass(pCtx->pVm) != 0;` |
|      26 | 2353 | `}` |
|     760 | 2354 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2355 | `{` |
|       - | 2356 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 2357 | `	sxi32 rc;` |
|     765 | 2358 | `	if( nArg < 1 ){` |
|       - | 2359 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2360 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2361 | `		return PH7_OK;` |
|       - | 2362 | `	}` |
|       - | 2363 | `	{` |
|       - | 2364 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 2365 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     765 | 2366 | `		sxi32 rcCb = VmForwardScreen(pCtx,apArg[0]);` |
|     765 | 2367 | `		if( rcCb != PH7_OK ){` |
|     147 | 2368 | `			return rcCb;` |
|       - | 2369 | `		}` |
|       - | 2370 | `	}` |
|     616 | 2371 | `	if( pCtx->pFunc->sName.nByte == sizeof("forward_static_call") - 1` |
|     332 | 2372 | `	    && SyStrnicmp(pCtx->pFunc->sName.zString,"forward_static_call",pCtx->pFunc->sName.nByte) == 0` |
|      53 | 2373 | `	    && !VmCallerHasClassScope(pCtx) ){` |
|       - | 2374 | `		/* Refused after the callback screen, as php's body does after its ZPP. Only this` |
|       - | 2375 | `		 * one of the two forwards asks; forward_static_call_array() runs anywhere. */` |
|      17 | 2376 | `		return PH7_VmThrowException(pCtx,"Error",` |
|       - | 2377 | `			"Cannot call forward_static_call() when no class scope is active");` |
|       - | 2378 | `	}` |
|     605 | 2379 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     605 | 2380 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2381 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     905 | 2382 | `	rc = PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1],` |
|     600 | 2383 | `		(pCtx->pArgMap && pCtx->pArgMap->bHasNamed && pCtx->pArgMap->nTotal > 1)` |
|      58 | 2384 | `			? &pCtx->pArgMap->aNames[1] : 0);` |
|     605 | 2385 | `	if( rc != SXRET_OK ){` |
|       3 | 2386 | `		PH7_MemObjRelease(&sResult);` |
|       3 | 2387 | `		return rc;` |
|       - | 2388 | `	}` |
|       - | 2389 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 2390 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 2391 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 2392 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 2393 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 2394 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     632 | 2395 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      61 | 2396 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 2397 | `		VmCallArgMap sInner;` |
|       - | 2398 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 2399 | `		 * unset when forwarded, not as stack garbage. */` |
|      61 | 2400 | `		SyZero(&sInner,sizeof(sInner));` |
|      61 | 2401 | `		sInner.bHasNamed = 1;` |
|      61 | 2402 | `		sInner.bIsNamespaced = 0;` |
|       - | 2403 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 2404 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 2405 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 2406 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      61 | 2407 | `		sInner.bStrict = 0;` |
|      61 | 2408 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      61 | 2409 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|       - | 2410 | ``		/* A `name:` argument is one php's compiler does not fold, so this is`` |
|       - | 2411 | `		 * always a real internal call (VmForwardFolded). */` |
|      61 | 2412 | `		VmForwardArm(pCtx,apArg[0]);` |
|      61 | 2413 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      32 | 2414 | `	}else{` |
|       - | 2415 | `		/* call_user_func is one of php's two FORWARDS: once folded, the callback binds` |
|       - | 2416 | `		 * under the mode of the file that wrote the call_user_func, not weakly like every` |
|       - | 2417 | `		 * other internal callback. Carry that one bit on a map of its own — the positional` |
|       - | 2418 | `		 * wrapper would latch the call weak (which is right for array_map and every` |
|       - | 2419 | `		 * other internal invocation, and wrong here). */` |
|       - | 2420 | `		VmCallArgMap sFwd;` |
|     545 | 2421 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     545 | 2422 | `		sFwd.bStrict = (VmForwardArm(pCtx,apArg[0]) && pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     545 | 2423 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);` |
|       - | 2424 | `	}` |
|       - | 2425 | `	/* The latches are consumed by the OP_CALL the dispatch builds; clear them for` |
|       - | 2426 | `	 * the paths that never reach one, so they cannot describe some later call. */` |
|     603 | 2427 | `	pCtx->pVm->bDiscardCallback = 0;` |
|     603 | 2428 | `	pCtx->pVm->bDynamicForward = 0;` |
|     603 | 2429 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2430 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 2431 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      95 | 2432 | `		PH7_MemObjRelease(&sResult);` |
|      95 | 2433 | `		return PH7_EXCEPTION;` |
|       - | 2434 | `	}` |
|     513 | 2435 | `	if( rc != SXRET_OK ){` |
|       - | 2436 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2437 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2438 | `	}else{` |
|       - | 2439 | `		/* Callback result */` |
|     513 | 2440 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2441 | `	}` |
|     513 | 2442 | `	PH7_MemObjRelease(&sResult);` |
|     513 | 2443 | `	return PH7_OK;` |
|     385 | 2444 | `}` |
|       - | 2445 | `/*` |
|       - | 2446 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 2447 | ` *  Call a callback with an array of parameters.` |
|       - | 2448 | ` * Parameter` |
|       - | 2449 | ` *  $callback` |
|       - | 2450 | ` *   The callable to be called.` |
|       - | 2451 | ` * $param_arr` |
|       - | 2452 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 2453 | ` * Return` |
|       - | 2454 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 2455 | ` */` |
|     324 | 2456 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2457 | `{` |
|       - | 2458 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 2459 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 2460 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 2461 | `	SySet aArg;               /* Argument value pointers */` |
|     329 | 2462 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     329 | 2463 | `	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */` |
|     329 | 2464 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 2465 | `	int bFolded;              /* php's compiler folded this forward (VmForwardFolded) */` |
|       - | 2466 | `	sxi32 rc;` |
|       - | 2467 | `	sxu32 n;` |
|     329 | 2468 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 2469 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2470 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2471 | `		return PH7_OK;` |
|       - | 2472 | `	}` |
|       - | 2473 | `	{` |
|     329 | 2474 | `		sxi32 rcCb = VmForwardScreen(pCtx,apArg[0]);` |
|     329 | 2475 | `		if( rcCb != PH7_OK ){` |
|      11 | 2476 | `			return rcCb;` |
|       - | 2477 | `		}` |
|       - | 2478 | `	}` |
|     321 | 2479 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     321 | 2480 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 2481 | `	/* Initialize the arguments container */` |
|     321 | 2482 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 2483 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 2484 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 2485 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 2486 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 2487 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 2488 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     321 | 2489 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     321 | 2490 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     833 | 2491 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 2492 | `		/* Extract node value */` |
|     517 | 2493 | `		if( (pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     517 | 2494 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      52 | 2495 | `				if( aNames == 0 ){` |
|       - | 2496 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 2497 | `					 * not-yet-seen slot defaults to positional. */` |
|      42 | 2498 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      42 | 2499 | `					if( aNames == 0 ){` |
|     ! 0 | 2500 | `						SySetRelease(&aArg);` |
|     ! 0 | 2501 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 2502 | `						if( apNode ){` |
|     ! 0 | 2503 | `							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     ! 0 | 2504 | `						}` |
|     ! 0 | 2505 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 2506 | `					}` |
|      42 | 2507 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|      20 | 2508 | `				}` |
|      52 | 2509 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      25 | 2510 | `			}` |
|     517 | 2511 | `			if( apNode == 0 ){` |
|     341 | 2512 | `				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     224 | 2513 | `					pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     229 | 2514 | `				if( apNode ){` |
|     229 | 2515 | `					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     112 | 2516 | `				}` |
|     112 | 2517 | `			}` |
|     517 | 2518 | `			if( apNode ){` |
|     517 | 2519 | `				apNode[nSlot] = pEntry;` |
|     256 | 2520 | `			}` |
|     517 | 2521 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     517 | 2522 | `			nSlot++;` |
|     256 | 2523 | `		}` |
|       - | 2524 | `		/* Point to the next entry */` |
|     517 | 2525 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     261 | 2526 | `	}` |
|       - | 2527 | `	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is` |
|       - | 2528 | `		 * itself a reference; a plain element is copied, and php says so. The values were` |
|       - | 2529 | `		 * already php-exact (the callee aliases the array's own element) — the diagnostic` |
|       - | 2530 | `		 * was the whole gap. Raised before the invoke, which is where php raises it. */` |
|     321 | 2531 | `	if( apNode ){` |
|     229 | 2532 | `		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);` |
|     229 | 2533 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);` |
|     229 | 2534 | `		apNode = 0;` |
|     112 | 2535 | `	}` |
|       - | 2536 | `	/* Try to invoke the callback. Like call_user_func, this is a php FORWARD, and` |
|       - | 2537 | `	 * folded or not it arms the same latches. */` |
|     321 | 2538 | `	bFolded = VmForwardArm(pCtx,apArg[0]);` |
|     321 | 2539 | `	if( aNames ){` |
|       - | 2540 | `		VmCallArgMap sMap;` |
|      42 | 2541 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      42 | 2542 | `		sMap.bHasNamed = 1;` |
|      42 | 2543 | `		sMap.bIsNamespaced = 0;` |
|       - | 2544 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 2545 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      42 | 2546 | `		sMap.bStrict = (bFolded && pCtx->pArgMap) ? pCtx->pArgMap->bStrict : 0;` |
|      42 | 2547 | `		sMap.nTotal = nSlot;` |
|      42 | 2548 | `		sMap.aNames = aNames;` |
|      62 | 2549 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      40 | 2550 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      42 | 2551 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|      22 | 2552 | `	}else{` |
|       - | 2553 | `		/* The other FORWARD: same rule as call_user_func above — the caller's file` |
|       - | 2554 | `		 * mode reaches the callback, where every other internal invocation is weak. */` |
|       - | 2555 | `		VmCallArgMap sFwd;` |
|     281 | 2556 | `		SyZero(&sFwd,sizeof(sFwd));` |
|     281 | 2557 | `		sFwd.bStrict = (bFolded && pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|     419 | 2558 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|     276 | 2559 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);` |
|       - | 2560 | `	}` |
|     321 | 2561 | `	pCtx->pVm->bDiscardCallback = 0;   /* see the call_user_func sibling */` |
|     321 | 2562 | `	pCtx->pVm->bDynamicForward = 0;` |
|     321 | 2563 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2564 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     159 | 2565 | `		PH7_MemObjRelease(&sResult);` |
|     159 | 2566 | `		SySetRelease(&aArg);` |
|     159 | 2567 | `		return PH7_EXCEPTION;` |
|       - | 2568 | `	}` |
|     166 | 2569 | `	if( rc != SXRET_OK ){` |
|       - | 2570 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 2571 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 2572 | `	}else{` |
|       - | 2573 | `		/* Callback result */` |
|     166 | 2574 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 2575 | `	}` |
|       - | 2576 | `	/* Cleanup the mess left behind */` |
|     166 | 2577 | `	PH7_MemObjRelease(&sResult);` |
|     166 | 2578 | `	SySetRelease(&aArg);` |
|     166 | 2579 | `	return PH7_OK;` |
|     167 | 2580 | `}` |
|       - | 2581 |  |
