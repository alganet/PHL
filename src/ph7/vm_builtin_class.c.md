# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 630/718 lines (87.74%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   750 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   755 |   11 | `	if( nArg < 1 ){` |
|     - |   12 | `		/* Check if we are inside a class */` |
|   ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|   ! 0 |   14 | `		if( pClass ){` |
|     - |   15 | `			/* Point to the class name */` |
|   ! 0 |   16 | `			pName = &pClass->sName;` |
|   ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   ! 0 |   18 | `		}else{` |
|     - |   19 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|     - |   21 | `		}` |
|   ! 0 |   22 | `	}else{` |
|     - |   23 | `		/* Extract the target class */` |
|   755 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   755 |   25 | `		if( pClass ){` |
|   753 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   753 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   379 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   755 |   34 | `	return PH7_OK;` |
|     5 |   35 | `}` |
|     - |   36 | `/*` |
|     - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|     - |   38 | ` *   Returns the name of the parent class of an object` |
|     - |   39 | ` * Parameters` |
|     - |   40 | ` *  object` |
|     - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|     - |   42 | ` * Return` |
|     - |   43 | ` *  The name of the parent class of which object is an instance.` |
|     - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|     - |   45 | ` *  not have a parent.` |
|     - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|     - |   47 | ` */` |
|    40 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   49 | `{` |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    42 |   52 | `	if( nArg < 1 ){` |
|     - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|     3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     3 |   55 | `		if( pClass && pClass->pBase ){` |
|     - |   56 | `			/* Point to the class name */` |
|     3 |   57 | `			pName = &pClass->pBase->sName;` |
|     3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     2 |   59 | `		}else{` |
|     - |   60 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|     - |   62 | `		}` |
|     2 |   63 | `	}else{` |
|     - |   64 | `		/* Extract the target class */` |
|    40 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    40 |   66 | `		if( pClass ){` |
|    40 |   67 | `			if( pClass->pBase ){` |
|    38 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    38 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    20 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    21 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    42 |   80 | `	return PH7_OK;` |
|     2 |   81 | `}` |
|     - |   82 | `/*` |
|     - |   83 | ` * string get_called_class(void)` |
|     - |   84 | ` *   Gets the name of the class the static method is called in.` |
|     - |   85 | ` * Parameters` |
|     - |   86 | ` *  None.` |
|     - |   87 | ` * Return` |
|     - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|     - |   89 | ` */` |
|     4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   91 | `{` |
|     - |   92 | `	ph7_class *pClass;` |
|     - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|     5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     5 |   95 | `	if( pClass ){` |
|     - |   96 | `		SyString *pName;` |
|     - |   97 | `		/* Point to the class name */` |
|     5 |   98 | `		pName = &pClass->sName;` |
|     5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     3 |  100 | `	}else{` |
|   ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  102 | `		SXUNUSED(apArg);` |
|     - |  103 | `		/* Not inside class,return FALSE */` |
|   ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|     - |  105 | `	}` |
|     5 |  106 | `	return PH7_OK;` |
|     1 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|     - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|     - |  111 | ` * string which hold the class name.` |
|     - |  112 | ` */` |
|  1802 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  1807 |  115 | `	ph7_class *pClass = 0;` |
|  1807 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   919 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1349 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|   889 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|     - |  124 | `		/* php: a leading '\' anchors the name to the global namespace. */` |
|   889 |  125 | `		if( nLen > 0 && zClass[0] == '\\' ){ zClass++; nLen--; }` |
|   889 |  126 | `		if( nLen > 0 ){` |
|     - |  127 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|     - |  128 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|     - |  129 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|     - |  130 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|     - |  131 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|     - |  132 | `			 * is a valid callable). */` |
|   889 |  133 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|   443 |  134 | `		}` |
|   443 |  135 | `	}` |
|  1807 |  136 | `	return pClass;` |
|     5 |  137 | `}` |
|     - |  138 | `/*` |
|     - |  139 | ` * bool property_exists(mixed $class,string $property)` |
|     - |  140 | ` *   Checks if the object or class has a property.` |
|     - |  141 | ` * Parameters` |
|     - |  142 | ` *  class` |
|     - |  143 | ` *   The class name or an object of the class to test for` |
|     - |  144 | ` * property` |
|     - |  145 | ` *  The name of the property` |
|     - |  146 | ` * Return` |
|     - |  147 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|     - |  148 | ` */` |
|    16 |  149 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  150 | `{` |
|    17 |  151 | `	int res = 0; /* Assume attribute does not exists */` |
|    17 |  152 | `	if( nArg > 1 ){` |
|     - |  153 | `		ph7_class *pClass;` |
|    17 |  154 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    17 |  155 | `		if( pClass ){` |
|     - |  156 | `			const char *zName;` |
|     - |  157 | `			int nLen;` |
|     - |  158 | `			/* Extract attribute name */` |
|    17 |  159 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    17 |  160 | `			if( nLen > 0 ){` |
|     - |  161 | `				/* Perform the lookup in the attribute and method table */` |
|    16 |  162 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    11 |  163 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  164 | `						/* property exists,flag that */` |
|    13 |  165 | `						res = 1;` |
|     6 |  166 | `				}` |
|     - |  167 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  168 | `				 * attribute table, not the class's — php reports those too` |
|     - |  169 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    17 |  170 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  171 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  172 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  173 | `						res = 1;` |
|   ! 0 |  174 | `					}` |
|     1 |  175 | `				}` |
|     8 |  176 | `			}` |
|     8 |  177 | `		}` |
|     8 |  178 | `	}` |
|    17 |  179 | `	ph7_result_bool(pCtx,res);` |
|    17 |  180 | `	return PH7_OK;` |
|     1 |  181 | `}` |
|     - |  182 | `/*` |
|     - |  183 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  184 | ` *   Checks if the given method is a class member.` |
|     - |  185 | ` * Parameters` |
|     - |  186 | ` *  class` |
|     - |  187 | ` *   The class name or an object of the class to test for` |
|     - |  188 | ` * property` |
|     - |  189 | ` *  The name of the method` |
|     - |  190 | ` * Return` |
|     - |  191 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  192 | ` */` |
|     4 |  193 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  194 | `{` |
|     5 |  195 | `	int res = 0; /* Assume method does not exists */` |
|     5 |  196 | `	if( nArg > 1 ){` |
|     - |  197 | `		ph7_class *pClass;` |
|     5 |  198 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     5 |  199 | `		if( pClass ){` |
|     - |  200 | `			const char *zName;` |
|     - |  201 | `			int nLen;` |
|     - |  202 | `			/* Extract method name */` |
|     5 |  203 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     5 |  204 | `			if( nLen > 0 ){` |
|     - |  205 | `				/* Perform the lookup in the method table */` |
|     5 |  206 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  207 | `					/* method exists,flag that */` |
|     3 |  208 | `					res = 1;` |
|     1 |  209 | `				}` |
|     2 |  210 | `			}` |
|     2 |  211 | `		}` |
|     2 |  212 | `	}` |
|     5 |  213 | `	ph7_result_bool(pCtx,res);` |
|     5 |  214 | `	return PH7_OK;` |
|     1 |  215 | `}` |
|     - |  216 | `/*` |
|     - |  217 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  218 | ` *   Checks if the class has been defined.` |
|     - |  219 | ` * Parameters` |
|     - |  220 | ` *  class_name` |
|     - |  221 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  222 | ` *   unlinke the standard PHP engine.` |
|     - |  223 | ` *  autoload` |
|     - |  224 | ` *   Whether or not to call __autoload by default.` |
|     - |  225 | ` * Return` |
|     - |  226 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  227 | ` */` |
|    82 |  228 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  229 | `{` |
|    86 |  230 | `	int res = 0; /* Assume class does not exist */` |
|    86 |  231 | `	if( nArg > 0 ){` |
|    86 |  232 | `		SyHashEntry *pEntry = 0;` |
|     - |  233 | `		const char *zName;` |
|     - |  234 | `		int nLen;` |
|    86 |  235 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  236 | `		/* Extract given name */` |
|    86 |  237 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    86 |  238 | `		if( nArg >= 2 ){` |
|     6 |  239 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  240 | `		}` |
|    86 |  241 | `		if( nLen > 0 ){` |
|     - |  242 | `			/* Perform a hash lookup first */` |
|    86 |  243 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    41 |  244 | `		}` |
|    86 |  245 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  246 | `			/* Try autoload, then re-check */` |
|    24 |  247 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    24 |  248 | `			if( pClass ){` |
|     9 |  249 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     3 |  250 | `			}` |
|    10 |  251 | `		}` |
|    86 |  252 | `		if( pEntry ){` |
|     - |  253 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  254 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    70 |  255 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    72 |  256 | `			while( pClass ){` |
|    70 |  257 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    68 |  258 | `					res = 1;` |
|    68 |  259 | `					break;` |
|     - |  260 | `				}` |
|     3 |  261 | `				pClass = pClass->pNextName;` |
|     1 |  262 | `			}` |
|    33 |  263 | `		}` |
|    41 |  264 | `	}` |
|    86 |  265 | `	ph7_result_bool(pCtx,res);` |
|    86 |  266 | `	return PH7_OK;` |
|     4 |  267 | `}` |
|     - |  268 | `/*` |
|     - |  269 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  270 | ` *   Checks if the interface has been defined.` |
|     - |  271 | ` * Parameters` |
|     - |  272 | ` *  class_name` |
|     - |  273 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  274 | ` *   unlinke the standard PHP engine.` |
|     - |  275 | ` *  autoload` |
|     - |  276 | ` *   Whether or not to call __autoload by default.` |
|     - |  277 | ` * Return` |
|     - |  278 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  279 | ` */` |
|    30 |  280 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  281 | `{` |
|    31 |  282 | `	int res = 0; /* Assume interface does not exist */` |
|    31 |  283 | `	if( nArg > 0 ){` |
|    31 |  284 | `		SyHashEntry *pEntry = 0;` |
|     - |  285 | `		const char *zName;` |
|     - |  286 | `		int nLen;` |
|    31 |  287 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  288 | `		/* Extract given name */` |
|    31 |  289 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    31 |  290 | `		if( nArg >= 2 ){` |
|   ! 0 |  291 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  292 | `		}` |
|     - |  293 | `		/* Perform a hash lookup */` |
|    31 |  294 | `		if( nLen > 0 ){` |
|    31 |  295 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    15 |  296 | `		}` |
|    31 |  297 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  298 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  299 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  300 | `			if( pClass ){` |
|   ! 0 |  301 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  302 | `			}` |
|     1 |  303 | `		}` |
|    31 |  304 | `		if( pEntry ){` |
|    29 |  305 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    31 |  306 | `			while( pClass ){` |
|    29 |  307 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  308 | `					/* interface is available */` |
|    27 |  309 | `					res = 1;` |
|    27 |  310 | `					break;` |
|     - |  311 | `				}` |
|     - |  312 | `				/* Next with the same name */` |
|     3 |  313 | `				pClass = pClass->pNextName;` |
|     1 |  314 | `			}` |
|    14 |  315 | `		}` |
|    15 |  316 | `	}` |
|    31 |  317 | `	ph7_result_bool(pCtx,res);` |
|    31 |  318 | `	return PH7_OK;` |
|     1 |  319 | `}` |
|     - |  320 | `/*` |
|     - |  321 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|     - |  322 | ` *   Checks if the trait has been defined.` |
|     - |  323 | ` * Parameters` |
|     - |  324 | ` *  trait` |
|     - |  325 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|     - |  326 | ` *  autoload` |
|     - |  327 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|     - |  328 | ` * Return` |
|     - |  329 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|     - |  330 | ` */` |
|    12 |  331 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  332 | `{` |
|    13 |  333 | `	int res = 0; /* Assume trait does not exist */` |
|    13 |  334 | `	if( nArg > 0 ){` |
|    13 |  335 | `		SyHashEntry *pEntry = 0;` |
|     - |  336 | `		const char *zName;` |
|     - |  337 | `		int nLen;` |
|    13 |  338 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  339 | `		/* Extract given name */` |
|    13 |  340 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    13 |  341 | `		if( nArg >= 2 ){` |
|     3 |  342 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     1 |  343 | `		}` |
|     - |  344 | `		/* Perform a hash lookup */` |
|    13 |  345 | `		if( nLen > 0 ){` |
|    13 |  346 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     6 |  347 | `		}` |
|    13 |  348 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  349 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|     3 |  350 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  351 | `			if( pClass ){` |
|   ! 0 |  352 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  353 | `			}` |
|     1 |  354 | `		}` |
|    13 |  355 | `		if( pEntry ){` |
|     9 |  356 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    15 |  357 | `			while( pClass ){` |
|     9 |  358 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|     - |  359 | `					/* trait is available */` |
|     3 |  360 | `					res = 1;` |
|     3 |  361 | `					break;` |
|     - |  362 | `				}` |
|     - |  363 | `				/* Next with the same name */` |
|     7 |  364 | `				pClass = pClass->pNextName;` |
|     1 |  365 | `			}` |
|     4 |  366 | `		}` |
|     6 |  367 | `	}` |
|    13 |  368 | `	ph7_result_bool(pCtx,res);` |
|    13 |  369 | `	return PH7_OK;` |
|     1 |  370 | `}` |
|     - |  371 | `/*` |
|     - |  372 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  373 | ` *   Creates an alias for a class.` |
|     - |  374 | ` * Parameters` |
|     - |  375 | ` *  original` |
|     - |  376 | ` *    The original class.` |
|     - |  377 | ` *  alias` |
|     - |  378 | ` *   The alias name for the class.` |
|     - |  379 | ` * Return` |
|     - |  380 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  381 | ` */` |
|     2 |  382 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  383 | `{` |
|     - |  384 | `	const char *zOld,*zNew;` |
|     - |  385 | `	int nOldLen,nNewLen;` |
|     - |  386 | `	SyHashEntry *pEntry;` |
|     - |  387 | `	ph7_class *pClass;` |
|     - |  388 | `	char *zDup;` |
|     - |  389 | `	sxi32 rc;` |
|     3 |  390 | `	if( nArg < 2 ){` |
|     - |  391 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  392 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  393 | `		return PH7_OK;` |
|     - |  394 | `	}` |
|     - |  395 | `	/* Extract old class name */` |
|     3 |  396 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  397 | `	/* Extract alias name */` |
|     3 |  398 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  399 | `	if( nNewLen < 1 ){` |
|     - |  400 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  401 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  402 | `		return PH7_OK;` |
|     - |  403 | `	}` |
|     - |  404 | `	/* Perform a hash lookup */` |
|     3 |  405 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  406 | `	if( pEntry ==  0 ){` |
|     - |  407 | `		/* No such class,return FALSE */` |
|   ! 0 |  408 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  409 | `		return PH7_OK;` |
|     - |  410 | `	}` |
|     - |  411 | `	/* Point to the class */` |
|     3 |  412 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  413 | `	/* Duplicate alias name */` |
|     3 |  414 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  415 | `	if( zDup == 0 ){` |
|     - |  416 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  417 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  418 | `		return PH7_OK;` |
|     - |  419 | `	}` |
|     - |  420 | `	/* Create the alias */` |
|     3 |  421 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  422 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  423 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  424 | `	}` |
|     3 |  425 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  426 | `	return PH7_OK;` |
|     2 |  427 | `}` |
|     - |  428 | `/*` |
|     - |  429 | ` * array get_declared_classes(void)` |
|     - |  430 | ` *   Returns an array with the name of the defined classes` |
|     - |  431 | ` * Parameters` |
|     - |  432 | ` *  None` |
|     - |  433 | ` * Return` |
|     - |  434 | ` *   Returns an array of the names of the declared classes` |
|     - |  435 | ` *   in the current script.` |
|     - |  436 | ` * Note:` |
|     - |  437 | ` *   NULL is returned on failure.` |
|     - |  438 | ` */` |
|     2 |  439 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  440 | `{` |
|     - |  441 | `	ph7_value *pName,*pArray;` |
|     - |  442 | `	SyHashEntry *pEntry;` |
|     - |  443 | `	/* Create a new array first */` |
|     3 |  444 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  445 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  446 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  447 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  448 | `		SXUNUSED(apArg);` |
|     - |  449 | `		/* Out of memory,return NULL */` |
|   ! 0 |  450 | `		ph7_result_null(pCtx);` |
|   ! 0 |  451 | `		return PH7_OK;` |
|     - |  452 | `	}` |
|     - |  453 | `	/* Fill the array with the defined classes */` |
|     3 |  454 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   344 |  455 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   341 |  456 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  457 | `		/* Do not register classes defined as interfaces */` |
|   341 |  458 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   303 |  459 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  460 | `			/* insert class name */` |
|   303 |  461 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  462 | `			/* Reset the cursor */` |
|   303 |  463 | `			ph7_value_reset_string_cursor(pName);` |
|   151 |  464 | `		}` |
|     1 |  465 | `	}` |
|     - |  466 | `	/* Return the created array */` |
|     3 |  467 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  468 | `	return PH7_OK;` |
|     2 |  469 | `}` |
|     - |  470 | `/*` |
|     - |  471 | ` * array get_declared_interfaces(void)` |
|     - |  472 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  473 | ` * Parameters` |
|     - |  474 | ` *  None` |
|     - |  475 | ` * Return` |
|     - |  476 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  477 | ` *   in the current script.` |
|     - |  478 | ` * Note:` |
|     - |  479 | ` *   NULL is returned on failure.` |
|     - |  480 | ` */` |
|     2 |  481 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  482 | `{` |
|     - |  483 | `	ph7_value *pName,*pArray;` |
|     - |  484 | `	SyHashEntry *pEntry;` |
|     - |  485 | `	/* Create a new array first */` |
|     3 |  486 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  487 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  488 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  489 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  490 | `		SXUNUSED(apArg);` |
|     - |  491 | `		/* Out of memory,return NULL */` |
|   ! 0 |  492 | `		ph7_result_null(pCtx);` |
|   ! 0 |  493 | `		return PH7_OK;` |
|     - |  494 | `	}` |
|     - |  495 | `	/* Fill the array with the defined classes */` |
|     3 |  496 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   346 |  497 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   343 |  498 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  499 | `		/* Register classes defined as interfaces only */` |
|   343 |  500 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    41 |  501 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  502 | `			/* insert interface name */` |
|    41 |  503 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  504 | `			/* Reset the cursor */` |
|    41 |  505 | `			ph7_value_reset_string_cursor(pName);` |
|    20 |  506 | `		}` |
|     1 |  507 | `	}` |
|     - |  508 | `	/* Return the created array */` |
|     3 |  509 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  510 | `	return PH7_OK;` |
|     2 |  511 | `}` |
|     - |  512 | `/*` |
|     - |  513 | ` * array get_class_methods(string/object $class_name)` |
|     - |  514 | ` *   Returns an array with the name of the class methods` |
|     - |  515 | ` * Parameters` |
|     - |  516 | ` *  class_name` |
|     - |  517 | ` *  The class name or class instance` |
|     - |  518 | ` * Return` |
|     - |  519 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  520 | ` *  In case of an error, it returns NULL.` |
|     - |  521 | ` * Note:` |
|     - |  522 | ` *   NULL is returned on failure.` |
|     - |  523 | ` */` |
|     8 |  524 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  525 | `{` |
|     - |  526 | `	ph7_value *pName,*pArray;` |
|     - |  527 | `	SyHashEntry *pEntry;` |
|     - |  528 | `	ph7_class *pClass;` |
|     - |  529 | `	/* Extract the target class first */` |
|     9 |  530 | `	pClass = 0;` |
|     9 |  531 | `	if( nArg > 0 ){` |
|     9 |  532 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     4 |  533 | `	}` |
|     9 |  534 | `	if( pClass == 0 ){` |
|     - |  535 | `		/* No such class,return NULL */` |
|     3 |  536 | `		ph7_result_null(pCtx);` |
|     3 |  537 | `		return PH7_OK;` |
|     - |  538 | `	}` |
|     - |  539 | `	/* Create a new array  */` |
|     7 |  540 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  541 | `	pName = ph7_context_new_scalar(pCtx);` |
|     7 |  542 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  543 | `		/* Out of memory,return NULL */` |
|   ! 0 |  544 | `		ph7_result_null(pCtx);` |
|   ! 0 |  545 | `		return PH7_OK;` |
|     - |  546 | `	}` |
|     - |  547 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|     - |  548 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|     - |  549 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|     - |  550 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|     - |  551 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|     - |  552 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|     - |  553 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|     - |  554 | `	{` |
|     - |  555 | `		SySet aTmp;` |
|     - |  556 | `		SyHashEntry **apEntry;` |
|     - |  557 | `		ph7_class *pLevel;` |
|     - |  558 | `		sxu32 n;` |
|     7 |  559 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|     7 |  560 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    27 |  561 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    21 |  562 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|     1 |  563 | `		}` |
|     7 |  564 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|    15 |  565 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|     - |  566 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|     - |  567 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|     - |  568 | `			 * hash-order fallback covers line-less internal methods). */` |
|     - |  569 | `			SySet aLvl;` |
|     - |  570 | `			ph7_class_method **apLvl;` |
|     - |  571 | `			sxu32 i,j;` |
|     9 |  572 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     - |  573 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|     - |  574 | `			 * come out in declaration order when walked newest-first, while` |
|     - |  575 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|     - |  576 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    37 |  577 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    29 |  578 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    29 |  579 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|    29 |  580 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|    29 |  581 | `				if( pDecl != pLevel ){` |
|     - |  582 | `					/* A declarer outside the base chain (a used trait, or none)` |
|     - |  583 | `					 * counts as the class's own level, like php. */` |
|     - |  584 | `					ph7_class *pWalk;` |
|     9 |  585 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     7 |  586 | `						continue;` |
|     - |  587 | `					}` |
|     9 |  588 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     9 |  589 | `						if( pWalk == pDecl ){` |
|     5 |  590 | `							break;` |
|     - |  591 | `						}` |
|     3 |  592 | `					}` |
|     5 |  593 | `					if( pWalk != 0 ){` |
|     5 |  594 | `						continue; /* in-chain: its own level emits it */` |
|     - |  595 | `					}` |
|   ! 0 |  596 | `				}` |
|    21 |  597 | `				SySetPut(&aLvl,(const void *)&pMethod);` |
|    11 |  598 | `			}` |
|     9 |  599 | `			apLvl = (ph7_class_method **)SySetBasePtr(&aLvl);` |
|     - |  600 | `			/* Insertion sort by declaration line (stable) */` |
|    21 |  601 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    13 |  602 | `				ph7_class_method *pKey = apLvl[i];` |
|    13 |  603 | `				for( j = i; j > 0 && apLvl[j-1]->nLine > pKey->nLine; j-- ){` |
|   ! 0 |  604 | `					apLvl[j] = apLvl[j-1];` |
|   ! 0 |  605 | `				}` |
|    13 |  606 | `				apLvl[j] = pKey;` |
|     7 |  607 | `			}` |
|    29 |  608 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     - |  609 | `				/* Insert method name */` |
|    21 |  610 | `				ph7_value_string(pName,SyStringData(&apLvl[i]->sFunc.sName),(int)SyStringLength(&apLvl[i]->sFunc.sName));` |
|    21 |  611 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  612 | `				/* Reset the cursor */` |
|    21 |  613 | `				ph7_value_reset_string_cursor(pName);` |
|    11 |  614 | `			}` |
|     9 |  615 | `			SySetRelease(&aLvl);` |
|     5 |  616 | `		}` |
|     7 |  617 | `		SySetRelease(&aTmp);` |
|     - |  618 | `	}` |
|     - |  619 | `	/* Return the created array */` |
|     7 |  620 | `	ph7_result_value(pCtx,pArray);` |
|     - |  621 | `	/*` |
|     - |  622 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  623 | `	 * automatically as soon we return from this foreign function.` |
|     - |  624 | `	 */` |
|     7 |  625 | `	return PH7_OK;` |
|     5 |  626 | `}` |
|     - |  627 | `/*` |
|     - |  628 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  629 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  630 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  631 | ` */` |
| 32984 |  632 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  633 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  634 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  635 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  636 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  637 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  638 | `	)` |
|     5 |  639 | `{` |
| 32989 |  640 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 24759 |  641 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  642 | `		ph7_vm_func *pVmFunc;` |
|     - |  643 | `		ph7_class *pCallerScope;` |
| 24773 |  644 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  645 | `			/* Safely ignore the exception frame */` |
|    16 |  646 | `			pFrame = pFrame->pParent;` |
|     2 |  647 | `		}` |
| 24759 |  648 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  649 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  650 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 24759 |  651 | `		if( pFrame->pBoundScope ){` |
|    15 |  652 | `			pCallerScope = pFrame->pBoundScope;` |
| 24752 |  653 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 24649 |  654 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 12421 |  655 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  656 | `			/* A closure/arrow-fn defined inside a class carries its creation-site` |
|     - |  657 | `			 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|     - |  658 | ``			 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|     - |  659 | `			 * against inside the body). php binds that class as the closure's scope,` |
|     - |  660 | ``			 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|     - |  661 | `			 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|     - |  662 | `			 * pBoundScope. */` |
|    39 |  663 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|    80 |  664 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  665 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  666 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  667 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  668 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  669 | `		}else{` |
|    59 |  670 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  671 | `		}` |
| 24703 |  672 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  673 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  674 | `			 * class must declare a private attribute of this name (a base` |
|     - |  675 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  676 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  677 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  678 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  679 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  680 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  681 | `			 * match (dynamic props and other non-declared shapes). */` |
| 11119 |  682 | `			ph7_class *pCaller = pCallerScope;` |
| 16676 |  683 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
| 11114 |  684 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
| 11119 |  685 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
| 11119 |  686 | `			int bGranted = 0;` |
| 11119 |  687 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  9272 |  688 | `				if( pOwn->pDeclClass == 0` |
|  9272 |  689 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  5583 |  690 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  9271 |  691 | `					bGranted = 1;` |
|  4638 |  692 | `				}` |
|  6480 |  693 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   963 |  694 | `				bGranted = 1;` |
|   481 |  695 | `			}` |
| 11119 |  696 | `			if( !bGranted ){` |
|     - |  697 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  698 | `				ph7_class **apTrait;` |
|     - |  699 | `				sxu32 nTrait,k;` |
|   889 |  700 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   889 |  701 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   889 |  702 | `				for(k = 0; k < nTrait; k++){` |
|   ! 0 |  703 | `					if( apTrait[k] == pCaller ){` |
|   ! 0 |  704 | `						bGranted = 1;` |
|   ! 0 |  705 | `						break;` |
|     - |  706 | `					}` |
|   ! 0 |  707 | `				}` |
|   443 |  708 | `			}` |
| 11119 |  709 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  710 | `				/* The target "class" is itself a trait: a trait-copied private` |
|     - |  711 | `				 * member behaves as if declared in the adopting class, so a` |
|     - |  712 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|     - |  713 | `				 * from a using class's static method reaching a trait-private` |
|     - |  714 | `				 * static — the callee resolves via the shared trait VmFunc` |
|     - |  715 | `				 * whose owner is the trait, not the class). */` |
|     - |  716 | `				ph7_class **apTrait;` |
|     - |  717 | `				sxu32 nTrait,k;` |
|   878 |  718 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|   878 |  719 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|   878 |  720 | `				for(k = 0; k < nTrait; k++){` |
|   878 |  721 | `					if( apTrait[k] == pClass ){` |
|   878 |  722 | `						bGranted = 1;` |
|   878 |  723 | `						break;` |
|     - |  724 | `					}` |
|   ! 0 |  725 | `				}` |
|   438 |  726 | `			}` |
| 11119 |  727 | `			if( !bGranted ){` |
|    13 |  728 | `				goto dis; /* Access is forbidden */` |
|     - |  729 | `			}` |
|  5557 |  730 | `		}else{` |
|     - |  731 | `			/* Protected */` |
| 13589 |  732 | `			ph7_class *pBase = pCallerScope;` |
|     - |  733 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|     - |  734 | `			 * not the one that (re)declares the override we resolved. A protected` |
|     - |  735 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|     - |  736 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|     - |  737 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|     - |  738 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|     - |  739 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|     - |  740 | `			 * true declaring class) and test the hierarchy against that introducing` |
|     - |  741 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|     - |  742 | `			 * from a sibling, matching php. */` |
| 13589 |  743 | `			ph7_class *pIntro = pClass;` |
|     - |  744 | `			ph7_class *pAnc;` |
| 37213 |  745 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
| 23629 |  746 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
| 23629 |  747 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
| 23629 |  748 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
| 23629 |  749 | `				int bHere = 0;` |
| 23629 |  750 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|  3578 |  751 | `					bHere = 1;` |
|  1787 |  752 | `				}` |
| 23629 |  753 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
| 10559 |  754 | `					bHere = 1;` |
|  5277 |  755 | `				}` |
| 23629 |  756 | `				if( bHere ){` |
| 14133 |  757 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|  7064 |  758 | `				}` |
| 11817 |  759 | `			}` |
|     - |  760 | `			/* Must be in the same class hierarchy as the introducing class */` |
| 13589 |  761 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|    12 |  762 | `				int bTraitGrant = 0;` |
|    12 |  763 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  764 | `					/* Same trait-target rule as the private branch above */` |
|     - |  765 | `					ph7_class **apTrait;` |
|     - |  766 | `					sxu32 nTrait,k;` |
|     8 |  767 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     8 |  768 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     8 |  769 | `					for(k = 0; k < nTrait; k++){` |
|     6 |  770 | `						if( apTrait[k] == pClass ){` |
|     6 |  771 | `							bTraitGrant = 1;` |
|     6 |  772 | `							break;` |
|     - |  773 | `						}` |
|   ! 0 |  774 | `					}` |
|     3 |  775 | `				}` |
|    12 |  776 | `				if( !bTraitGrant ){` |
|     8 |  777 | `					goto dis; /* Access is forbidden */` |
|     - |  778 | `				}` |
|     2 |  779 | `			}` |
|     - |  780 | `		}` |
| 12341 |  781 | `	}` |
| 32917 |  782 | `	return 1; /* Access is granted */` |
|    36 |  783 | `dis:` |
|    75 |  784 | `	if( bLog ){` |
|   ! 0 |  785 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  786 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  787 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  788 | `	}` |
|    75 |  789 | `	return 0; /* Access is forbidden */` |
| 16497 |  790 | `}` |
|     - |  791 | `/*` |
|     - |  792 | ` * array get_class_vars(string/object $class_name)` |
|     - |  793 | ` *   Get the default properties of the class` |
|     - |  794 | ` * Parameters` |
|     - |  795 | ` *  class_name` |
|     - |  796 | ` *   The class name or class instance` |
|     - |  797 | ` * Return` |
|     - |  798 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  799 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  800 | ` *  of varname => value.` |
|     - |  801 | ` * Note:` |
|     - |  802 | ` *   NULL is returned on failure.` |
|     - |  803 | ` */` |
|     4 |  804 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  805 | `{` |
|     - |  806 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  807 | `	SyHashEntry *pEntry;` |
|     - |  808 | `	ph7_class *pClass;` |
|     - |  809 | `	/* Extract the target class first */` |
|     5 |  810 | `	pClass = 0;` |
|     5 |  811 | `	if( nArg > 0 ){` |
|     5 |  812 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     2 |  813 | `	}` |
|     5 |  814 | `	if( pClass == 0 ){` |
|     - |  815 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|     - |  816 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|     - |  817 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|     - |  818 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|   ! 0 |  819 | `		int nLen = 0;` |
|   ! 0 |  820 | `		const char *zVal = "";` |
|   ! 0 |  821 | `		if( nArg > 0 ){` |
|   ! 0 |  822 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|   ! 0 |  823 | `				zVal = "Array";` |
|   ! 0 |  824 | `				nLen = (int)sizeof("Array") - 1;` |
|   ! 0 |  825 | `			}else{` |
|   ! 0 |  826 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  827 | `			}` |
|   ! 0 |  828 | `		}` |
|   ! 0 |  829 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  830 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|   ! 0 |  831 | `			nLen,zVal);` |
|     - |  832 | `	}` |
|     - |  833 | `	/* Create a new array  */` |
|     5 |  834 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  835 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  836 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  837 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  838 | `		/* Out of memory,return NULL */` |
|   ! 0 |  839 | `		ph7_result_null(pCtx);` |
|   ! 0 |  840 | `		return PH7_OK;` |
|     - |  841 | `	}` |
|     - |  842 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  843 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  844 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  845 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  846 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  847 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  848 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  849 | `			continue;` |
|     - |  850 | `		}` |
|     - |  851 | `		/* Check if the access is allowed */` |
|     7 |  852 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  853 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  854 | `			ph7_value *pValue = 0;` |
|     7 |  855 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  856 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  857 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  858 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  859 | `			}else{` |
|     3 |  860 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  861 | `					PH7_MemObjRelease(&sValue);` |
|     - |  862 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  863 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  864 | `					pValue = &sValue;` |
|     1 |  865 | `				}` |
|     - |  866 | `			}` |
|     - |  867 | `			/* Fill in the array */` |
|     7 |  868 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  869 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  870 | `			/* Reset the cursor */` |
|     7 |  871 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  872 | `		}` |
|     1 |  873 | `	}` |
|     5 |  874 | `	PH7_MemObjRelease(&sValue);` |
|     - |  875 | `	/* Return the created array */` |
|     5 |  876 | `	ph7_result_value(pCtx,pArray);` |
|     - |  877 | `	/*` |
|     - |  878 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  879 | `	 * automatically as soon we return from this foreign function.` |
|     - |  880 | `	 */` |
|     5 |  881 | `	return PH7_OK;` |
|     3 |  882 | `}` |
|     - |  883 | `/*` |
|     - |  884 | ` * array get_object_vars(object $this)` |
|     - |  885 | ` *   Gets the properties of the given object` |
|     - |  886 | ` * Parameters` |
|     - |  887 | ` *  this` |
|     - |  888 | ` *   A class instance` |
|     - |  889 | ` * Return` |
|     - |  890 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  891 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  892 | ` *  it will be returned with a NULL value.` |
|     - |  893 | ` * Note:` |
|     - |  894 | ` *   NULL is returned on failure.` |
|     - |  895 | ` */` |
|    26 |  896 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  897 | `{` |
|    27 |  898 | `	ph7_class_instance *pThis = 0;` |
|     - |  899 | `	ph7_value *pName,*pArray;` |
|     - |  900 | `	SyHashEntry *pEntry;` |
|    27 |  901 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  902 | `		/* Extract the target instance */` |
|    27 |  903 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    13 |  904 | `	}` |
|    27 |  905 | `	if( pThis == 0 ){` |
|     - |  906 | `		/* No such instance,return NULL */` |
|   ! 0 |  907 | `		ph7_result_null(pCtx);` |
|   ! 0 |  908 | `		return PH7_OK;` |
|     - |  909 | `	}` |
|     - |  910 | `	/* Create a new array  */` |
|    27 |  911 | `	pArray = ph7_context_new_array(pCtx);` |
|    27 |  912 | `	pName = ph7_context_new_scalar(pCtx);` |
|    27 |  913 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  914 | `		/* Out of memory,return NULL */` |
|   ! 0 |  915 | `		ph7_result_null(pCtx);` |
|   ! 0 |  916 | `		return PH7_OK;` |
|     - |  917 | `	}` |
|     - |  918 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  919 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  920 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  921 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  922 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  923 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  924 | `	 * skipped instead of read after free. */` |
|     - |  925 | `	{` |
|     - |  926 | `		SySet sNames;` |
|     - |  927 | `		SyString *aName;` |
|     - |  928 | `		sxu32 iName,nName;` |
|    27 |  929 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    27 |  930 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|   103 |  931 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    77 |  932 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    77 |  933 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  934 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  935 | `				continue;` |
|     - |  936 | `			}` |
|    66 |  937 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    34 |  938 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  939 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  940 | `			}` |
|    65 |  941 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  942 | `		}` |
|    27 |  943 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    27 |  944 | `		nName = SySetUsed(&sNames);` |
|    91 |  945 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    65 |  946 | `			SyString *pAttrName = &aName[iName];` |
|     - |  947 | `			VmClassAttr *pVmAttr;` |
|    65 |  948 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    65 |  949 | `			if( pEntry == 0 ){` |
|   ! 0 |  950 | `				continue; /* unset by an earlier hook */` |
|     - |  951 | `			}` |
|    65 |  952 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  953 | `			/* Check if the access is allowed */` |
|    65 |  954 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    51 |  955 | `				ph7_value *pValue = 0;` |
|     - |  956 | `				ph7_value sHookVal;` |
|     - |  957 | `				sxi32 rcHk;` |
|     - |  958 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  959 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    51 |  960 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    51 |  961 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    51 |  962 | `				if( rcHk == SXRET_OK ){` |
|    15 |  963 | `					pValue = &sHookVal;` |
|    44 |  964 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  965 | `					/* Extract attribute */` |
|    37 |  966 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    19 |  967 | `				}else{` |
|     - |  968 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  969 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  970 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  971 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  972 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  973 | `					break;` |
|     - |  974 | `				}` |
|    51 |  975 | `				if( pValue ){` |
|     - |  976 | `					/* Insert attribute name in the array */` |
|    51 |  977 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    51 |  978 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    25 |  979 | `				}` |
|    51 |  980 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  981 | `				/* Reset the cursor */` |
|    51 |  982 | `				ph7_value_reset_string_cursor(pName);` |
|    25 |  983 | `			}` |
|    33 |  984 | `		}` |
|    27 |  985 | `		SySetRelease(&sNames);` |
|     - |  986 | `	}` |
|     - |  987 | `	/* Return the created array */` |
|    27 |  988 | `	ph7_result_value(pCtx,pArray);` |
|     - |  989 | `	/*` |
|     - |  990 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  991 | `	 * automatically as soon we return from this foreign function.` |
|     - |  992 | `	 */` |
|    27 |  993 | `	return PH7_OK;` |
|    14 |  994 | `}` |
|     - |  995 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  996 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  997 | ` * detection should reject them up front. */` |
|     - |  998 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  999 | `/*` |
|     - | 1000 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|     - | 1001 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|     - | 1002 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|     - | 1003 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|     - | 1004 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|     - | 1005 | ` */` |
| 14110 | 1006 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|     5 | 1007 | `{` |
| 26037 | 1008 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|     - | 1009 | `		ph7_class **apParent;` |
|     - | 1010 | `		sxu32 n;` |
| 16701 | 1011 | `		if( pIface == pTarget ){` |
|  4777 | 1012 | `			return TRUE;` |
|     - | 1013 | `		}` |
|     - | 1014 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|     - | 1015 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
| 11929 | 1016 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
| 11933 | 1017 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|     7 | 1018 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|     3 | 1019 | `				return TRUE;` |
|     - | 1020 | `			}` |
|     3 | 1021 | `		}` |
| 11927 | 1022 | `		pIface = pIface->pBase;` |
| 11927 | 1023 | `		iDepth++;` |
|     5 | 1024 | `	}` |
|  9341 | 1025 | `	return FALSE;` |
|  7060 | 1026 | `}` |
|     - | 1027 | `/*` |
|     - | 1028 | ` * This function returns TRUE if the given class is an implemented` |
|     - | 1029 | ` * interface.Otherwise FALSE is returned.` |
|     - | 1030 | ` */` |
| 16404 | 1031 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 | 1032 | `{` |
|     - | 1033 | `	ph7_class **apInterface;` |
|     - | 1034 | `	sxu32 n;` |
| 16409 | 1035 | `	if( SySetUsed(pSet) < 1 ){` |
|     - | 1036 | `		/* Empty interface container */` |
|  4247 | 1037 | `		return FALSE;` |
|     - | 1038 | `	}` |
|     - | 1039 | `	/* Point to the set of implemented interfaces */` |
| 12167 | 1040 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - | 1041 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - | 1042 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 21499 | 1043 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 14109 | 1044 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
|  4777 | 1045 | `			return TRUE;` |
|     - | 1046 | `		}` |
|  4671 | 1047 | `	}` |
|  7395 | 1048 | `	return FALSE;` |
|  8207 | 1049 | `}` |
|     - | 1050 | `/*` |
|     - | 1051 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1052 | ` * is an instance of the main class (second argument).` |
|     - | 1053 | ` * Otherwise FALSE is returned.` |
|     - | 1054 | ` */` |
| 26960 | 1055 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 | 1056 | `{` |
|     - | 1057 | `	ph7_class *pParent;` |
|     - | 1058 | `	sxi32 rc;` |
| 26965 | 1059 | `	if( pThis == pClass ){` |
|     - | 1060 | `		/* Instance of the same class */` |
| 15017 | 1061 | `		return TRUE;` |
|     - | 1062 | `	}` |
|     - | 1063 | `	/* Check implemented interfaces */` |
| 11953 | 1064 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 11953 | 1065 | `	if( rc ){` |
|  2029 | 1066 | `		return TRUE;` |
|     - | 1067 | `	}` |
|     - | 1068 | `	/* Check parent classes */` |
|  9929 | 1069 | `	pParent = pThis->pBase;` |
| 11627 | 1070 | `	while( pParent ){` |
|  5209 | 1071 | `		if( pParent == pClass ){` |
|     - | 1072 | `			/* Same instance */` |
|   763 | 1073 | `			return TRUE;` |
|     - | 1074 | `		}` |
|     - | 1075 | `		/* Check the implemented interfaces */` |
|  4451 | 1076 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4451 | 1077 | `		if( rc ){` |
|  2753 | 1078 | `			return TRUE;` |
|     - | 1079 | `		}` |
|     - | 1080 | `		/* Point to the parent class */` |
|  1703 | 1081 | `		pParent = pParent->pBase;` |
|     5 | 1082 | `	}` |
|     - | 1083 | `	/* Not an instance of the the given class */` |
|  6423 | 1084 | `	return FALSE;` |
| 13485 | 1085 | `}` |
|     - | 1086 | `/*` |
|     - | 1087 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1088 | ` * is a subclass of the main class (second argument).` |
|     - | 1089 | ` * Otherwise FALSE is returned.` |
|     - | 1090 | ` */` |
|    16 | 1091 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 | 1092 | `{` |
|    17 | 1093 | `	SySet *pInterface = &pClass->aInterface;` |
|     - | 1094 | `	SyHashEntry *pEntry;` |
|     - | 1095 | `	SyString *pName;` |
|     - | 1096 | `	sxi32 rc;` |
|    27 | 1097 | `	while( pClass ){` |
|    17 | 1098 | `		pName = &pClass->sName;` |
|     - | 1099 | `		/* Query the derived hashtable */` |
|    17 | 1100 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    17 | 1101 | `		if( pEntry ){` |
|     7 | 1102 | `			return TRUE;` |
|     - | 1103 | `		}` |
|    11 | 1104 | `		pClass = pClass->pBase;` |
|     1 | 1105 | `	}` |
|    11 | 1106 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|    11 | 1107 | `	if( rc ){` |
|   ! 0 | 1108 | `		return TRUE;` |
|     - | 1109 | `	}` |
|     - | 1110 | `	/* Not a subclass */` |
|    11 | 1111 | `	return FALSE;` |
|     9 | 1112 | `}` |
|     - | 1113 | `/*` |
|     - | 1114 | ` * bool is_a(object $object,string $class_name)` |
|     - | 1115 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - | 1116 | ` * Parameters` |
|     - | 1117 | ` *  object` |
|     - | 1118 | ` *   The tested object` |
|     - | 1119 | ` * class_name` |
|     - | 1120 | ` *  The class name` |
|     - | 1121 | ` * Return` |
|     - | 1122 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - | 1123 | ` *   parents, FALSE otherwise.` |
|     - | 1124 | ` */` |
|    18 | 1125 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1126 | `{` |
|    19 | 1127 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1128 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 | 1129 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - | 1130 | `		ph7_class *pClass;` |
|     - | 1131 | `		/* Extract the given class */` |
|    19 | 1132 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1133 | `		if( pClass ){` |
|     - | 1134 | `			/* Perform the query */` |
|    19 | 1135 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 | 1136 | `		}` |
|     9 | 1137 | `	}` |
|     - | 1138 | `	/* Query result */` |
|    19 | 1139 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1140 | `	return PH7_OK;` |
|     1 | 1141 | `}` |
|     - | 1142 | `/*` |
|     - | 1143 | ` * int spl_object_id(object $object)` |
|     - | 1144 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1145 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1146 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1147 | ` */` |
|    58 | 1148 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1149 | `{` |
|     - | 1150 | `	ph7_class_instance *pThis;` |
|    62 | 1151 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1152 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1153 | `		return PH7_OK;` |
|     - | 1154 | `	}` |
|    62 | 1155 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1156 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1157 | `	return PH7_OK;` |
|    33 | 1158 | `}` |
|     - | 1159 | `/*` |
|     - | 1160 | ` * string spl_object_hash(object $object)` |
|     - | 1161 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1162 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1163 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1164 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1165 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1166 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1167 | ` */` |
|    14 | 1168 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1169 | `{` |
|     - | 1170 | `	ph7_class_instance *pThis;` |
|    16 | 1171 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1172 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1173 | `		return PH7_OK;` |
|     - | 1174 | `	}` |
|    16 | 1175 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1176 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1177 | `	return PH7_OK;` |
|     9 | 1178 | `}` |
|     - | 1179 | `/*` |
|     - | 1180 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1181 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1182 | ` * Parameters` |
|     - | 1183 | ` *  object` |
|     - | 1184 | ` *   The tested object` |
|     - | 1185 | ` * class_name` |
|     - | 1186 | ` *  The class name` |
|     - | 1187 | ` * Return` |
|     - | 1188 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1189 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1190 | ` */` |
|    18 | 1191 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1192 | `{` |
|    19 | 1193 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1194 | `	if( nArg > 1 ){` |
|     - | 1195 | `		ph7_class *pClass,*pMain;` |
|     - | 1196 | `		/* Extract the given classes */` |
|    19 | 1197 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    19 | 1198 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1199 | `		if( pClass && pMain ){` |
|     - | 1200 | `			/* Perform the query */` |
|    17 | 1201 | `			res = VmSubclassOf(pClass,pMain);` |
|     8 | 1202 | `		}` |
|     9 | 1203 | `	}` |
|     - | 1204 | `	/* Query result */` |
|    19 | 1205 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1206 | `	return PH7_OK;` |
|     1 | 1207 | `}` |
|    80 | 1208 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1209 | `{` |
|     - | 1210 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1211 | `	sxi32 rc;` |
|    81 | 1212 | `	if( nArg < 1 ){` |
|     - | 1213 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1214 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1215 | `		return PH7_OK;` |
|     - | 1216 | `	}` |
|     - | 1217 | `	{` |
|     - | 1218 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|     - | 1219 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|    81 | 1220 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    81 | 1221 | `		if( rcCb != PH7_OK ){` |
|    11 | 1222 | `			return rcCb;` |
|     - | 1223 | `		}` |
|     - | 1224 | `	}` |
|    71 | 1225 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    71 | 1226 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1227 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|    71 | 1228 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|     - | 1229 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1230 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1231 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1232 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1233 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1234 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    80 | 1235 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1236 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1237 | `		VmCallArgMap sInner;` |
|    19 | 1238 | `		sInner.bHasNamed = 1;` |
|    19 | 1239 | `		sInner.bIsNamespaced = 0;` |
|     - | 1240 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1241 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1242 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1243 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1244 | `		sInner.bStrict = 0;` |
|    19 | 1245 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1246 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1247 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1248 | `	}else{` |
|    53 | 1249 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1250 | `	}` |
|    71 | 1251 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1252 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1253 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1254 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1255 | `		return PH7_EXCEPTION;` |
|     - | 1256 | `	}` |
|    65 | 1257 | `	if( rc != SXRET_OK ){` |
|     - | 1258 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1259 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1260 | `	}else{` |
|     - | 1261 | `		/* Callback result */` |
|    65 | 1262 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1263 | `	}` |
|    65 | 1264 | `	PH7_MemObjRelease(&sResult);` |
|    65 | 1265 | `	return PH7_OK;` |
|    41 | 1266 | `}` |
|     - | 1267 | `/*` |
|     - | 1268 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1269 | ` *  Call a callback with an array of parameters.` |
|     - | 1270 | ` * Parameter` |
|     - | 1271 | ` *  $callback` |
|     - | 1272 | ` *   The callable to be called.` |
|     - | 1273 | ` * $param_arr` |
|     - | 1274 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1275 | ` * Return` |
|     - | 1276 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1277 | ` */` |
|    36 | 1278 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1279 | `{` |
|     - | 1280 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1281 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1282 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1283 | `	SySet aArg;               /* Argument value pointers */` |
|    37 | 1284 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    37 | 1285 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1286 | `	sxi32 rc;` |
|     - | 1287 | `	sxu32 n;` |
|    37 | 1288 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1289 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1290 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1291 | `		return PH7_OK;` |
|     - | 1292 | `	}` |
|     - | 1293 | `	{` |
|    37 | 1294 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    37 | 1295 | `		if( rcCb != PH7_OK ){` |
|     3 | 1296 | `			return rcCb;` |
|     - | 1297 | `		}` |
|     - | 1298 | `	}` |
|    35 | 1299 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1300 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1301 | `	/* Initialize the arguments container */` |
|    35 | 1302 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1303 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1304 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1305 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1306 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1307 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1308 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1309 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1310 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1311 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1312 | `		/* Extract node value */` |
|   155 | 1313 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1314 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1315 | `				if( aNames == 0 ){` |
|     - | 1316 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1317 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1318 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1319 | `					if( aNames == 0 ){` |
|   ! 0 | 1320 | `						SySetRelease(&aArg);` |
|   ! 0 | 1321 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1322 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1323 | `					}` |
|    13 | 1324 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1325 | `				}` |
|    23 | 1326 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1327 | `			}` |
|   155 | 1328 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1329 | `			nSlot++;` |
|    77 | 1330 | `		}` |
|     - | 1331 | `		/* Point to the next entry */` |
|   155 | 1332 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1333 | `	}` |
|     - | 1334 | `	/* Try to invoke the callback */` |
|    35 | 1335 | `	if( aNames ){` |
|     - | 1336 | `		VmCallArgMap sMap;` |
|    13 | 1337 | `		sMap.bHasNamed = 1;` |
|    13 | 1338 | `		sMap.bIsNamespaced = 0;` |
|     - | 1339 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1340 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1341 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1342 | `		sMap.nTotal = nSlot;` |
|    13 | 1343 | `		sMap.aNames = aNames;` |
|    19 | 1344 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1345 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1346 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1347 | `	}else{` |
|    34 | 1348 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1349 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1350 | `	}` |
|    35 | 1351 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1352 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1353 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1354 | `		SySetRelease(&aArg);` |
|     5 | 1355 | `		return PH7_EXCEPTION;` |
|     - | 1356 | `	}` |
|    31 | 1357 | `	if( rc != SXRET_OK ){` |
|     - | 1358 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1359 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1360 | `	}else{` |
|     - | 1361 | `		/* Callback result */` |
|    31 | 1362 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1363 | `	}` |
|     - | 1364 | `	/* Cleanup the mess left behind */` |
|    31 | 1365 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1366 | `	SySetRelease(&aArg);` |
|    31 | 1367 | `	return PH7_OK;` |
|    19 | 1368 | `}` |
|     - | 1369 |  |
