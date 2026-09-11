# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 596/686 lines (86.88%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   738 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   743 |   11 | `	if( nArg < 1 ){` |
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
|   743 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   743 |   25 | `		if( pClass ){` |
|   741 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   741 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   373 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   743 |   34 | `	return PH7_OK;` |
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
|    38 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   49 | `{` |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    40 |   52 | `	if( nArg < 1 ){` |
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
|    38 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    38 |   66 | `		if( pClass ){` |
|    38 |   67 | `			if( pClass->pBase ){` |
|    36 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    36 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    19 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    20 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    40 |   80 | `	return PH7_OK;` |
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
|  1742 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  1747 |  115 | `	ph7_class *pClass = 0;` |
|  1747 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   903 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1297 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|   846 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|     - |  124 | `		/* php: a leading '\' anchors the name to the global namespace. */` |
|   846 |  125 | `		if( nLen > 0 && zClass[0] == '\\' ){ zClass++; nLen--; }` |
|   846 |  126 | `		if( nLen > 0 ){` |
|     - |  127 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|     - |  128 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|     - |  129 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|     - |  130 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|     - |  131 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|     - |  132 | `			 * is a valid callable). */` |
|   846 |  133 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|   421 |  134 | `		}` |
|   421 |  135 | `	}` |
|  1747 |  136 | `	return pClass;` |
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
|    80 |  228 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  229 | `{` |
|    85 |  230 | `	int res = 0; /* Assume class does not exist */` |
|    85 |  231 | `	if( nArg > 0 ){` |
|    85 |  232 | `		SyHashEntry *pEntry = 0;` |
|     - |  233 | `		const char *zName;` |
|     - |  234 | `		int nLen;` |
|    85 |  235 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  236 | `		/* Extract given name */` |
|    85 |  237 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    85 |  238 | `		if( nArg >= 2 ){` |
|     6 |  239 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  240 | `		}` |
|    85 |  241 | `		if( nLen > 0 ){` |
|     - |  242 | `			/* Perform a hash lookup first */` |
|    85 |  243 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    40 |  244 | `		}` |
|    85 |  245 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  246 | `			/* Try autoload, then re-check */` |
|    25 |  247 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    25 |  248 | `			if( pClass ){` |
|     9 |  249 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     3 |  250 | `			}` |
|    10 |  251 | `		}` |
|    85 |  252 | `		if( pEntry ){` |
|     - |  253 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  254 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    69 |  255 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    69 |  256 | `			while( pClass ){` |
|    69 |  257 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    69 |  258 | `					res = 1;` |
|    69 |  259 | `					break;` |
|     - |  260 | `				}` |
|   ! 0 |  261 | `				pClass = pClass->pNextName;` |
|   ! 0 |  262 | `			}` |
|    32 |  263 | `		}` |
|    40 |  264 | `	}` |
|    85 |  265 | `	ph7_result_bool(pCtx,res);` |
|    85 |  266 | `	return PH7_OK;` |
|     5 |  267 | `}` |
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
|    28 |  280 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  281 | `{` |
|    29 |  282 | `	int res = 0; /* Assume interface does not exist */` |
|    29 |  283 | `	if( nArg > 0 ){` |
|    29 |  284 | `		SyHashEntry *pEntry = 0;` |
|     - |  285 | `		const char *zName;` |
|     - |  286 | `		int nLen;` |
|    29 |  287 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  288 | `		/* Extract given name */` |
|    29 |  289 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    29 |  290 | `		if( nArg >= 2 ){` |
|   ! 0 |  291 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  292 | `		}` |
|     - |  293 | `		/* Perform a hash lookup */` |
|    29 |  294 | `		if( nLen > 0 ){` |
|    29 |  295 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    14 |  296 | `		}` |
|    29 |  297 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  298 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  299 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  300 | `			if( pClass ){` |
|   ! 0 |  301 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  302 | `			}` |
|     1 |  303 | `		}` |
|    29 |  304 | `		if( pEntry ){` |
|    27 |  305 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    27 |  306 | `			while( pClass ){` |
|    27 |  307 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  308 | `					/* interface is available */` |
|    27 |  309 | `					res = 1;` |
|    27 |  310 | `					break;` |
|     - |  311 | `				}` |
|     - |  312 | `				/* Next with the same name */` |
|   ! 0 |  313 | `				pClass = pClass->pNextName;` |
|   ! 0 |  314 | `			}` |
|    13 |  315 | `		}` |
|    14 |  316 | `	}` |
|    29 |  317 | `	ph7_result_bool(pCtx,res);` |
|    29 |  318 | `	return PH7_OK;` |
|     1 |  319 | `}` |
|     - |  320 | `/*` |
|     - |  321 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  322 | ` *   Creates an alias for a class.` |
|     - |  323 | ` * Parameters` |
|     - |  324 | ` *  original` |
|     - |  325 | ` *    The original class.` |
|     - |  326 | ` *  alias` |
|     - |  327 | ` *   The alias name for the class.` |
|     - |  328 | ` * Return` |
|     - |  329 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  330 | ` */` |
|     2 |  331 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  332 | `{` |
|     - |  333 | `	const char *zOld,*zNew;` |
|     - |  334 | `	int nOldLen,nNewLen;` |
|     - |  335 | `	SyHashEntry *pEntry;` |
|     - |  336 | `	ph7_class *pClass;` |
|     - |  337 | `	char *zDup;` |
|     - |  338 | `	sxi32 rc;` |
|     3 |  339 | `	if( nArg < 2 ){` |
|     - |  340 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  341 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  342 | `		return PH7_OK;` |
|     - |  343 | `	}` |
|     - |  344 | `	/* Extract old class name */` |
|     3 |  345 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  346 | `	/* Extract alias name */` |
|     3 |  347 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  348 | `	if( nNewLen < 1 ){` |
|     - |  349 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  350 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  351 | `		return PH7_OK;` |
|     - |  352 | `	}` |
|     - |  353 | `	/* Perform a hash lookup */` |
|     3 |  354 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  355 | `	if( pEntry ==  0 ){` |
|     - |  356 | `		/* No such class,return FALSE */` |
|   ! 0 |  357 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  358 | `		return PH7_OK;` |
|     - |  359 | `	}` |
|     - |  360 | `	/* Point to the class */` |
|     3 |  361 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  362 | `	/* Duplicate alias name */` |
|     3 |  363 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  364 | `	if( zDup == 0 ){` |
|     - |  365 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  366 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  367 | `		return PH7_OK;` |
|     - |  368 | `	}` |
|     - |  369 | `	/* Create the alias */` |
|     3 |  370 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  371 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  372 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  373 | `	}` |
|     3 |  374 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  375 | `	return PH7_OK;` |
|     2 |  376 | `}` |
|     - |  377 | `/*` |
|     - |  378 | ` * array get_declared_classes(void)` |
|     - |  379 | ` *   Returns an array with the name of the defined classes` |
|     - |  380 | ` * Parameters` |
|     - |  381 | ` *  None` |
|     - |  382 | ` * Return` |
|     - |  383 | ` *   Returns an array of the names of the declared classes` |
|     - |  384 | ` *   in the current script.` |
|     - |  385 | ` * Note:` |
|     - |  386 | ` *   NULL is returned on failure.` |
|     - |  387 | ` */` |
|     2 |  388 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  389 | `{` |
|     - |  390 | `	ph7_value *pName,*pArray;` |
|     - |  391 | `	SyHashEntry *pEntry;` |
|     - |  392 | `	/* Create a new array first */` |
|     3 |  393 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  394 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  395 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  396 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  397 | `		SXUNUSED(apArg);` |
|     - |  398 | `		/* Out of memory,return NULL */` |
|   ! 0 |  399 | `		ph7_result_null(pCtx);` |
|   ! 0 |  400 | `		return PH7_OK;` |
|     - |  401 | `	}` |
|     - |  402 | `	/* Fill the array with the defined classes */` |
|     3 |  403 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   324 |  404 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   321 |  405 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  406 | `		/* Do not register classes defined as interfaces */` |
|   321 |  407 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   283 |  408 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  409 | `			/* insert class name */` |
|   283 |  410 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  411 | `			/* Reset the cursor */` |
|   283 |  412 | `			ph7_value_reset_string_cursor(pName);` |
|   141 |  413 | `		}` |
|     1 |  414 | `	}` |
|     - |  415 | `	/* Return the created array */` |
|     3 |  416 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  417 | `	return PH7_OK;` |
|     2 |  418 | `}` |
|     - |  419 | `/*` |
|     - |  420 | ` * array get_declared_interfaces(void)` |
|     - |  421 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  422 | ` * Parameters` |
|     - |  423 | ` *  None` |
|     - |  424 | ` * Return` |
|     - |  425 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  426 | ` *   in the current script.` |
|     - |  427 | ` * Note:` |
|     - |  428 | ` *   NULL is returned on failure.` |
|     - |  429 | ` */` |
|     2 |  430 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  431 | `{` |
|     - |  432 | `	ph7_value *pName,*pArray;` |
|     - |  433 | `	SyHashEntry *pEntry;` |
|     - |  434 | `	/* Create a new array first */` |
|     3 |  435 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  436 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  437 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  438 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  439 | `		SXUNUSED(apArg);` |
|     - |  440 | `		/* Out of memory,return NULL */` |
|   ! 0 |  441 | `		ph7_result_null(pCtx);` |
|   ! 0 |  442 | `		return PH7_OK;` |
|     - |  443 | `	}` |
|     - |  444 | `	/* Fill the array with the defined classes */` |
|     3 |  445 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   326 |  446 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   323 |  447 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  448 | `		/* Register classes defined as interfaces only */` |
|   323 |  449 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    41 |  450 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  451 | `			/* insert interface name */` |
|    41 |  452 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  453 | `			/* Reset the cursor */` |
|    41 |  454 | `			ph7_value_reset_string_cursor(pName);` |
|    20 |  455 | `		}` |
|     1 |  456 | `	}` |
|     - |  457 | `	/* Return the created array */` |
|     3 |  458 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  459 | `	return PH7_OK;` |
|     2 |  460 | `}` |
|     - |  461 | `/*` |
|     - |  462 | ` * array get_class_methods(string/object $class_name)` |
|     - |  463 | ` *   Returns an array with the name of the class methods` |
|     - |  464 | ` * Parameters` |
|     - |  465 | ` *  class_name` |
|     - |  466 | ` *  The class name or class instance` |
|     - |  467 | ` * Return` |
|     - |  468 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  469 | ` *  In case of an error, it returns NULL.` |
|     - |  470 | ` * Note:` |
|     - |  471 | ` *   NULL is returned on failure.` |
|     - |  472 | ` */` |
|     8 |  473 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  474 | `{` |
|     - |  475 | `	ph7_value *pName,*pArray;` |
|     - |  476 | `	SyHashEntry *pEntry;` |
|     - |  477 | `	ph7_class *pClass;` |
|     - |  478 | `	/* Extract the target class first */` |
|     9 |  479 | `	pClass = 0;` |
|     9 |  480 | `	if( nArg > 0 ){` |
|     9 |  481 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     4 |  482 | `	}` |
|     9 |  483 | `	if( pClass == 0 ){` |
|     - |  484 | `		/* No such class,return NULL */` |
|     3 |  485 | `		ph7_result_null(pCtx);` |
|     3 |  486 | `		return PH7_OK;` |
|     - |  487 | `	}` |
|     - |  488 | `	/* Create a new array  */` |
|     7 |  489 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  490 | `	pName = ph7_context_new_scalar(pCtx);` |
|     7 |  491 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  492 | `		/* Out of memory,return NULL */` |
|   ! 0 |  493 | `		ph7_result_null(pCtx);` |
|   ! 0 |  494 | `		return PH7_OK;` |
|     - |  495 | `	}` |
|     - |  496 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|     - |  497 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|     - |  498 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|     - |  499 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|     - |  500 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|     - |  501 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|     - |  502 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|     - |  503 | `	{` |
|     - |  504 | `		SySet aTmp;` |
|     - |  505 | `		SyHashEntry **apEntry;` |
|     - |  506 | `		ph7_class *pLevel;` |
|     - |  507 | `		sxu32 n;` |
|     7 |  508 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|     7 |  509 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    27 |  510 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    21 |  511 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|     1 |  512 | `		}` |
|     7 |  513 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|    15 |  514 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|     - |  515 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|     - |  516 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|     - |  517 | `			 * hash-order fallback covers line-less internal methods). */` |
|     - |  518 | `			SySet aLvl;` |
|     - |  519 | `			ph7_class_method **apLvl;` |
|     - |  520 | `			sxu32 i,j;` |
|     9 |  521 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     - |  522 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|     - |  523 | `			 * come out in declaration order when walked newest-first, while` |
|     - |  524 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|     - |  525 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    37 |  526 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    29 |  527 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    29 |  528 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|    29 |  529 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|    29 |  530 | `				if( pDecl != pLevel ){` |
|     - |  531 | `					/* A declarer outside the base chain (a used trait, or none)` |
|     - |  532 | `					 * counts as the class's own level, like php. */` |
|     - |  533 | `					ph7_class *pWalk;` |
|     9 |  534 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     7 |  535 | `						continue;` |
|     - |  536 | `					}` |
|     9 |  537 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     9 |  538 | `						if( pWalk == pDecl ){` |
|     5 |  539 | `							break;` |
|     - |  540 | `						}` |
|     3 |  541 | `					}` |
|     5 |  542 | `					if( pWalk != 0 ){` |
|     5 |  543 | `						continue; /* in-chain: its own level emits it */` |
|     - |  544 | `					}` |
|   ! 0 |  545 | `				}` |
|    21 |  546 | `				SySetPut(&aLvl,(const void *)&pMethod);` |
|    11 |  547 | `			}` |
|     9 |  548 | `			apLvl = (ph7_class_method **)SySetBasePtr(&aLvl);` |
|     - |  549 | `			/* Insertion sort by declaration line (stable) */` |
|    21 |  550 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    13 |  551 | `				ph7_class_method *pKey = apLvl[i];` |
|    13 |  552 | `				for( j = i; j > 0 && apLvl[j-1]->nLine > pKey->nLine; j-- ){` |
|   ! 0 |  553 | `					apLvl[j] = apLvl[j-1];` |
|   ! 0 |  554 | `				}` |
|    13 |  555 | `				apLvl[j] = pKey;` |
|     7 |  556 | `			}` |
|    29 |  557 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     - |  558 | `				/* Insert method name */` |
|    21 |  559 | `				ph7_value_string(pName,SyStringData(&apLvl[i]->sFunc.sName),(int)SyStringLength(&apLvl[i]->sFunc.sName));` |
|    21 |  560 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  561 | `				/* Reset the cursor */` |
|    21 |  562 | `				ph7_value_reset_string_cursor(pName);` |
|    11 |  563 | `			}` |
|     9 |  564 | `			SySetRelease(&aLvl);` |
|     5 |  565 | `		}` |
|     7 |  566 | `		SySetRelease(&aTmp);` |
|     - |  567 | `	}` |
|     - |  568 | `	/* Return the created array */` |
|     7 |  569 | `	ph7_result_value(pCtx,pArray);` |
|     - |  570 | `	/*` |
|     - |  571 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  572 | `	 * automatically as soon we return from this foreign function.` |
|     - |  573 | `	 */` |
|     7 |  574 | `	return PH7_OK;` |
|     5 |  575 | `}` |
|     - |  576 | `/*` |
|     - |  577 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  578 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  579 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  580 | ` */` |
| 31920 |  581 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  582 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  583 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  584 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  585 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  586 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  587 | `	)` |
|     5 |  588 | `{` |
| 31925 |  589 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 23843 |  590 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  591 | `		ph7_vm_func *pVmFunc;` |
|     - |  592 | `		ph7_class *pCallerScope;` |
| 23853 |  593 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  594 | `			/* Safely ignore the exception frame */` |
|    11 |  595 | `			pFrame = pFrame->pParent;` |
|     1 |  596 | `		}` |
| 23843 |  597 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  598 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  599 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 23843 |  600 | `		if( pFrame->pBoundScope ){` |
|    15 |  601 | `			pCallerScope = pFrame->pBoundScope;` |
| 23836 |  602 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 23745 |  603 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 11956 |  604 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  605 | `			/* A closure/arrow-fn defined inside a class carries its creation-site` |
|     - |  606 | `			 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|     - |  607 | ``			 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|     - |  608 | `			 * against inside the body). php binds that class as the closure's scope,` |
|     - |  609 | ``			 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|     - |  610 | `			 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|     - |  611 | `			 * pBoundScope. */` |
|    31 |  612 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|    71 |  613 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  614 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  615 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  616 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  617 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  618 | `		}else{` |
|    54 |  619 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  620 | `		}` |
| 23791 |  621 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  622 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  623 | `			 * class must declare a private attribute of this name (a base` |
|     - |  624 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  625 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  626 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  627 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  628 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  629 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  630 | `			 * match (dynamic props and other non-declared shapes). */` |
| 10475 |  631 | `			ph7_class *pCaller = pCallerScope;` |
| 15710 |  632 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
| 10470 |  633 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
| 10475 |  634 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
| 10475 |  635 | `			int bGranted = 0;` |
| 10475 |  636 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  8732 |  637 | `				if( pOwn->pDeclClass == 0` |
|  8732 |  638 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  5206 |  639 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  8733 |  640 | `					bGranted = 1;` |
|  4369 |  641 | `				}` |
|  6106 |  642 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   875 |  643 | `				bGranted = 1;` |
|   437 |  644 | `			}` |
| 10475 |  645 | `			if( !bGranted ){` |
|     - |  646 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  647 | `				ph7_class **apTrait;` |
|     - |  648 | `				sxu32 nTrait,k;` |
|   870 |  649 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   870 |  650 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   870 |  651 | `				for(k = 0; k < nTrait; k++){` |
|   ! 0 |  652 | `					if( apTrait[k] == pCaller ){` |
|   ! 0 |  653 | `						bGranted = 1;` |
|   ! 0 |  654 | `						break;` |
|     - |  655 | `					}` |
|   ! 0 |  656 | `				}` |
|   434 |  657 | `			}` |
| 10475 |  658 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  659 | `				/* The target "class" is itself a trait: a trait-copied private` |
|     - |  660 | `				 * member behaves as if declared in the adopting class, so a` |
|     - |  661 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|     - |  662 | `				 * from a using class's static method reaching a trait-private` |
|     - |  663 | `				 * static — the callee resolves via the shared trait VmFunc` |
|     - |  664 | `				 * whose owner is the trait, not the class). */` |
|     - |  665 | `				ph7_class **apTrait;` |
|     - |  666 | `				sxu32 nTrait,k;` |
|   862 |  667 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|   862 |  668 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|   862 |  669 | `				for(k = 0; k < nTrait; k++){` |
|   862 |  670 | `					if( apTrait[k] == pClass ){` |
|   862 |  671 | `						bGranted = 1;` |
|   862 |  672 | `						break;` |
|     - |  673 | `					}` |
|   ! 0 |  674 | `				}` |
|   430 |  675 | `			}` |
| 10475 |  676 | `			if( !bGranted ){` |
|    10 |  677 | `				goto dis; /* Access is forbidden */` |
|     - |  678 | `			}` |
|  5236 |  679 | `		}else{` |
|     - |  680 | `			/* Protected */` |
| 13321 |  681 | `			ph7_class *pBase = pCallerScope;` |
|     - |  682 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|     - |  683 | `			 * not the one that (re)declares the override we resolved. A protected` |
|     - |  684 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|     - |  685 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|     - |  686 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|     - |  687 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|     - |  688 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|     - |  689 | `			 * true declaring class) and test the hierarchy against that introducing` |
|     - |  690 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|     - |  691 | `			 * from a sibling, matching php. */` |
| 13321 |  692 | `			ph7_class *pIntro = pClass;` |
|     - |  693 | `			ph7_class *pAnc;` |
| 36589 |  694 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
| 23273 |  695 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
| 23273 |  696 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
| 23273 |  697 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
| 23273 |  698 | `				int bHere = 0;` |
| 23273 |  699 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|  3504 |  700 | `					bHere = 1;` |
|  1751 |  701 | `				}` |
| 23273 |  702 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
| 10351 |  703 | `					bHere = 1;` |
|  5173 |  704 | `				}` |
| 23273 |  705 | `				if( bHere ){` |
| 13853 |  706 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|  6924 |  707 | `				}` |
| 11639 |  708 | `			}` |
|     - |  709 | `			/* Must be in the same class hierarchy as the introducing class */` |
| 13321 |  710 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|    12 |  711 | `				int bTraitGrant = 0;` |
|    12 |  712 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  713 | `					/* Same trait-target rule as the private branch above */` |
|     - |  714 | `					ph7_class **apTrait;` |
|     - |  715 | `					sxu32 nTrait,k;` |
|     8 |  716 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     8 |  717 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     8 |  718 | `					for(k = 0; k < nTrait; k++){` |
|     6 |  719 | `						if( apTrait[k] == pClass ){` |
|     6 |  720 | `							bTraitGrant = 1;` |
|     6 |  721 | `							break;` |
|     - |  722 | `						}` |
|   ! 0 |  723 | `					}` |
|     3 |  724 | `				}` |
|    12 |  725 | `				if( !bTraitGrant ){` |
|     8 |  726 | `					goto dis; /* Access is forbidden */` |
|     - |  727 | `				}` |
|     2 |  728 | `			}` |
|     - |  729 | `		}` |
| 11886 |  730 | `	}` |
| 31859 |  731 | `	return 1; /* Access is granted */` |
|    33 |  732 | `dis:` |
|    69 |  733 | `	if( bLog ){` |
|   ! 0 |  734 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  735 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  736 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  737 | `	}` |
|    69 |  738 | `	return 0; /* Access is forbidden */` |
| 15965 |  739 | `}` |
|     - |  740 | `/*` |
|     - |  741 | ` * array get_class_vars(string/object $class_name)` |
|     - |  742 | ` *   Get the default properties of the class` |
|     - |  743 | ` * Parameters` |
|     - |  744 | ` *  class_name` |
|     - |  745 | ` *   The class name or class instance` |
|     - |  746 | ` * Return` |
|     - |  747 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  748 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  749 | ` *  of varname => value.` |
|     - |  750 | ` * Note:` |
|     - |  751 | ` *   NULL is returned on failure.` |
|     - |  752 | ` */` |
|     4 |  753 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  754 | `{` |
|     - |  755 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  756 | `	SyHashEntry *pEntry;` |
|     - |  757 | `	ph7_class *pClass;` |
|     - |  758 | `	/* Extract the target class first */` |
|     5 |  759 | `	pClass = 0;` |
|     5 |  760 | `	if( nArg > 0 ){` |
|     5 |  761 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     2 |  762 | `	}` |
|     5 |  763 | `	if( pClass == 0 ){` |
|     - |  764 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|     - |  765 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|     - |  766 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|     - |  767 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|   ! 0 |  768 | `		int nLen = 0;` |
|   ! 0 |  769 | `		const char *zVal = "";` |
|   ! 0 |  770 | `		if( nArg > 0 ){` |
|   ! 0 |  771 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|   ! 0 |  772 | `				zVal = "Array";` |
|   ! 0 |  773 | `				nLen = (int)sizeof("Array") - 1;` |
|   ! 0 |  774 | `			}else{` |
|   ! 0 |  775 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  776 | `			}` |
|   ! 0 |  777 | `		}` |
|   ! 0 |  778 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  779 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|   ! 0 |  780 | `			nLen,zVal);` |
|     - |  781 | `	}` |
|     - |  782 | `	/* Create a new array  */` |
|     5 |  783 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  784 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  785 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  786 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  787 | `		/* Out of memory,return NULL */` |
|   ! 0 |  788 | `		ph7_result_null(pCtx);` |
|   ! 0 |  789 | `		return PH7_OK;` |
|     - |  790 | `	}` |
|     - |  791 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  793 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  794 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  795 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  796 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  797 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  798 | `			continue;` |
|     - |  799 | `		}` |
|     - |  800 | `		/* Check if the access is allowed */` |
|     7 |  801 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  802 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  803 | `			ph7_value *pValue = 0;` |
|     7 |  804 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  805 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  806 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  807 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  808 | `			}else{` |
|     3 |  809 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  810 | `					PH7_MemObjRelease(&sValue);` |
|     - |  811 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  812 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  813 | `					pValue = &sValue;` |
|     1 |  814 | `				}` |
|     - |  815 | `			}` |
|     - |  816 | `			/* Fill in the array */` |
|     7 |  817 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  818 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  819 | `			/* Reset the cursor */` |
|     7 |  820 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  821 | `		}` |
|     1 |  822 | `	}` |
|     5 |  823 | `	PH7_MemObjRelease(&sValue);` |
|     - |  824 | `	/* Return the created array */` |
|     5 |  825 | `	ph7_result_value(pCtx,pArray);` |
|     - |  826 | `	/*` |
|     - |  827 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  828 | `	 * automatically as soon we return from this foreign function.` |
|     - |  829 | `	 */` |
|     5 |  830 | `	return PH7_OK;` |
|     3 |  831 | `}` |
|     - |  832 | `/*` |
|     - |  833 | ` * array get_object_vars(object $this)` |
|     - |  834 | ` *   Gets the properties of the given object` |
|     - |  835 | ` * Parameters` |
|     - |  836 | ` *  this` |
|     - |  837 | ` *   A class instance` |
|     - |  838 | ` * Return` |
|     - |  839 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  840 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  841 | ` *  it will be returned with a NULL value.` |
|     - |  842 | ` * Note:` |
|     - |  843 | ` *   NULL is returned on failure.` |
|     - |  844 | ` */` |
|    26 |  845 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  846 | `{` |
|    27 |  847 | `	ph7_class_instance *pThis = 0;` |
|     - |  848 | `	ph7_value *pName,*pArray;` |
|     - |  849 | `	SyHashEntry *pEntry;` |
|    27 |  850 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  851 | `		/* Extract the target instance */` |
|    27 |  852 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    13 |  853 | `	}` |
|    27 |  854 | `	if( pThis == 0 ){` |
|     - |  855 | `		/* No such instance,return NULL */` |
|   ! 0 |  856 | `		ph7_result_null(pCtx);` |
|   ! 0 |  857 | `		return PH7_OK;` |
|     - |  858 | `	}` |
|     - |  859 | `	/* Create a new array  */` |
|    27 |  860 | `	pArray = ph7_context_new_array(pCtx);` |
|    27 |  861 | `	pName = ph7_context_new_scalar(pCtx);` |
|    27 |  862 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  863 | `		/* Out of memory,return NULL */` |
|   ! 0 |  864 | `		ph7_result_null(pCtx);` |
|   ! 0 |  865 | `		return PH7_OK;` |
|     - |  866 | `	}` |
|     - |  867 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  868 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  869 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  870 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  871 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  872 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  873 | `	 * skipped instead of read after free. */` |
|     - |  874 | `	{` |
|     - |  875 | `		SySet sNames;` |
|     - |  876 | `		SyString *aName;` |
|     - |  877 | `		sxu32 iName,nName;` |
|    27 |  878 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    27 |  879 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|   103 |  880 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    77 |  881 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    77 |  882 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  883 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  884 | `				continue;` |
|     - |  885 | `			}` |
|    66 |  886 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    34 |  887 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  888 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  889 | `			}` |
|    65 |  890 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  891 | `		}` |
|    27 |  892 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    27 |  893 | `		nName = SySetUsed(&sNames);` |
|    91 |  894 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    65 |  895 | `			SyString *pAttrName = &aName[iName];` |
|     - |  896 | `			VmClassAttr *pVmAttr;` |
|    65 |  897 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    65 |  898 | `			if( pEntry == 0 ){` |
|   ! 0 |  899 | `				continue; /* unset by an earlier hook */` |
|     - |  900 | `			}` |
|    65 |  901 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  902 | `			/* Check if the access is allowed */` |
|    65 |  903 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    51 |  904 | `				ph7_value *pValue = 0;` |
|     - |  905 | `				ph7_value sHookVal;` |
|     - |  906 | `				sxi32 rcHk;` |
|     - |  907 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  908 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    51 |  909 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    51 |  910 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    51 |  911 | `				if( rcHk == SXRET_OK ){` |
|    15 |  912 | `					pValue = &sHookVal;` |
|    44 |  913 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  914 | `					/* Extract attribute */` |
|    37 |  915 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    19 |  916 | `				}else{` |
|     - |  917 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  918 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  919 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  920 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  921 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  922 | `					break;` |
|     - |  923 | `				}` |
|    51 |  924 | `				if( pValue ){` |
|     - |  925 | `					/* Insert attribute name in the array */` |
|    51 |  926 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    51 |  927 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    25 |  928 | `				}` |
|    51 |  929 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  930 | `				/* Reset the cursor */` |
|    51 |  931 | `				ph7_value_reset_string_cursor(pName);` |
|    25 |  932 | `			}` |
|    33 |  933 | `		}` |
|    27 |  934 | `		SySetRelease(&sNames);` |
|     - |  935 | `	}` |
|     - |  936 | `	/* Return the created array */` |
|    27 |  937 | `	ph7_result_value(pCtx,pArray);` |
|     - |  938 | `	/*` |
|     - |  939 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  940 | `	 * automatically as soon we return from this foreign function.` |
|     - |  941 | `	 */` |
|    27 |  942 | `	return PH7_OK;` |
|    14 |  943 | `}` |
|     - |  944 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  945 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  946 | ` * detection should reject them up front. */` |
|     - |  947 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  948 | `/*` |
|     - |  949 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|     - |  950 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|     - |  951 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|     - |  952 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|     - |  953 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|     - |  954 | ` */` |
| 13876 |  955 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|     5 |  956 | `{` |
| 25463 |  957 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|     - |  958 | `		ph7_class **apParent;` |
|     - |  959 | `		sxu32 n;` |
| 16317 |  960 | `		if( pIface == pTarget ){` |
|  4733 |  961 | `			return TRUE;` |
|     - |  962 | `		}` |
|     - |  963 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|     - |  964 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
| 11589 |  965 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
| 11593 |  966 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|     7 |  967 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|     3 |  968 | `				return TRUE;` |
|     - |  969 | `			}` |
|     3 |  970 | `		}` |
| 11587 |  971 | `		pIface = pIface->pBase;` |
| 11587 |  972 | `		iDepth++;` |
|     5 |  973 | `	}` |
|  9151 |  974 | `	return FALSE;` |
|  6943 |  975 | `}` |
|     - |  976 | `/*` |
|     - |  977 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  978 | ` * interface.Otherwise FALSE is returned.` |
|     - |  979 | ` */` |
| 16090 |  980 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  981 | `{` |
|     - |  982 | `	ph7_class **apInterface;` |
|     - |  983 | `	sxu32 n;` |
| 16095 |  984 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  985 | `		/* Empty interface container */` |
|  4105 |  986 | `		return FALSE;` |
|     - |  987 | `	}` |
|     - |  988 | `	/* Point to the set of implemented interfaces */` |
| 11995 |  989 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  990 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  991 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 21137 |  992 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 13875 |  993 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
|  4733 |  994 | `			return TRUE;` |
|     - |  995 | `		}` |
|  4576 |  996 | `	}` |
|  7267 |  997 | `	return FALSE;` |
|  8050 |  998 | `}` |
|     - |  999 | `/*` |
|     - | 1000 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1001 | ` * is an instance of the main class (second argument).` |
|     - | 1002 | ` * Otherwise FALSE is returned.` |
|     - | 1003 | ` */` |
| 26446 | 1004 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 | 1005 | `{` |
|     - | 1006 | `	ph7_class *pParent;` |
|     - | 1007 | `	sxi32 rc;` |
| 26451 | 1008 | `	if( pThis == pClass ){` |
|     - | 1009 | `		/* Instance of the same class */` |
| 14729 | 1010 | `		return TRUE;` |
|     - | 1011 | `	}` |
|     - | 1012 | `	/* Check implemented interfaces */` |
| 11727 | 1013 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 11727 | 1014 | `	if( rc ){` |
|  2007 | 1015 | `		return TRUE;` |
|     - | 1016 | `	}` |
|     - | 1017 | `	/* Check parent classes */` |
|  9725 | 1018 | `	pParent = pThis->pBase;` |
| 11357 | 1019 | `	while( pParent ){` |
|  5103 | 1020 | `		if( pParent == pClass ){` |
|     - | 1021 | `			/* Same instance */` |
|   745 | 1022 | `			return TRUE;` |
|     - | 1023 | `		}` |
|     - | 1024 | `		/* Check the implemented interfaces */` |
|  4363 | 1025 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4363 | 1026 | `		if( rc ){` |
|  2731 | 1027 | `			return TRUE;` |
|     - | 1028 | `		}` |
|     - | 1029 | `		/* Point to the parent class */` |
|  1637 | 1030 | `		pParent = pParent->pBase;` |
|     5 | 1031 | `	}` |
|     - | 1032 | `	/* Not an instance of the the given class */` |
|  6259 | 1033 | `	return FALSE;` |
| 13228 | 1034 | `}` |
|     - | 1035 | `/*` |
|     - | 1036 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1037 | ` * is a subclass of the main class (second argument).` |
|     - | 1038 | ` * Otherwise FALSE is returned.` |
|     - | 1039 | ` */` |
|    16 | 1040 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 | 1041 | `{` |
|    17 | 1042 | `	SySet *pInterface = &pClass->aInterface;` |
|     - | 1043 | `	SyHashEntry *pEntry;` |
|     - | 1044 | `	SyString *pName;` |
|     - | 1045 | `	sxi32 rc;` |
|    27 | 1046 | `	while( pClass ){` |
|    17 | 1047 | `		pName = &pClass->sName;` |
|     - | 1048 | `		/* Query the derived hashtable */` |
|    17 | 1049 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    17 | 1050 | `		if( pEntry ){` |
|     7 | 1051 | `			return TRUE;` |
|     - | 1052 | `		}` |
|    11 | 1053 | `		pClass = pClass->pBase;` |
|     1 | 1054 | `	}` |
|    11 | 1055 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|    11 | 1056 | `	if( rc ){` |
|   ! 0 | 1057 | `		return TRUE;` |
|     - | 1058 | `	}` |
|     - | 1059 | `	/* Not a subclass */` |
|    11 | 1060 | `	return FALSE;` |
|     9 | 1061 | `}` |
|     - | 1062 | `/*` |
|     - | 1063 | ` * bool is_a(object $object,string $class_name)` |
|     - | 1064 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - | 1065 | ` * Parameters` |
|     - | 1066 | ` *  object` |
|     - | 1067 | ` *   The tested object` |
|     - | 1068 | ` * class_name` |
|     - | 1069 | ` *  The class name` |
|     - | 1070 | ` * Return` |
|     - | 1071 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - | 1072 | ` *   parents, FALSE otherwise.` |
|     - | 1073 | ` */` |
|    18 | 1074 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1075 | `{` |
|    19 | 1076 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1077 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 | 1078 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - | 1079 | `		ph7_class *pClass;` |
|     - | 1080 | `		/* Extract the given class */` |
|    19 | 1081 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1082 | `		if( pClass ){` |
|     - | 1083 | `			/* Perform the query */` |
|    19 | 1084 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 | 1085 | `		}` |
|     9 | 1086 | `	}` |
|     - | 1087 | `	/* Query result */` |
|    19 | 1088 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1089 | `	return PH7_OK;` |
|     1 | 1090 | `}` |
|     - | 1091 | `/*` |
|     - | 1092 | ` * int spl_object_id(object $object)` |
|     - | 1093 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1094 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1095 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1096 | ` */` |
|    58 | 1097 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1098 | `{` |
|     - | 1099 | `	ph7_class_instance *pThis;` |
|    62 | 1100 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1101 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1102 | `		return PH7_OK;` |
|     - | 1103 | `	}` |
|    62 | 1104 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1105 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1106 | `	return PH7_OK;` |
|    33 | 1107 | `}` |
|     - | 1108 | `/*` |
|     - | 1109 | ` * string spl_object_hash(object $object)` |
|     - | 1110 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1111 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1112 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1113 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1114 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1115 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1116 | ` */` |
|    14 | 1117 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1118 | `{` |
|     - | 1119 | `	ph7_class_instance *pThis;` |
|    16 | 1120 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1121 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1122 | `		return PH7_OK;` |
|     - | 1123 | `	}` |
|    16 | 1124 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1125 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1126 | `	return PH7_OK;` |
|     9 | 1127 | `}` |
|     - | 1128 | `/*` |
|     - | 1129 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1130 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1131 | ` * Parameters` |
|     - | 1132 | ` *  object` |
|     - | 1133 | ` *   The tested object` |
|     - | 1134 | ` * class_name` |
|     - | 1135 | ` *  The class name` |
|     - | 1136 | ` * Return` |
|     - | 1137 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1138 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1139 | ` */` |
|    18 | 1140 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1141 | `{` |
|    19 | 1142 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1143 | `	if( nArg > 1 ){` |
|     - | 1144 | `		ph7_class *pClass,*pMain;` |
|     - | 1145 | `		/* Extract the given classes */` |
|    19 | 1146 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    19 | 1147 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1148 | `		if( pClass && pMain ){` |
|     - | 1149 | `			/* Perform the query */` |
|    17 | 1150 | `			res = VmSubclassOf(pClass,pMain);` |
|     8 | 1151 | `		}` |
|     9 | 1152 | `	}` |
|     - | 1153 | `	/* Query result */` |
|    19 | 1154 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1155 | `	return PH7_OK;` |
|     1 | 1156 | `}` |
|    80 | 1157 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1158 | `{` |
|     - | 1159 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1160 | `	sxi32 rc;` |
|    81 | 1161 | `	if( nArg < 1 ){` |
|     - | 1162 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1163 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1164 | `		return PH7_OK;` |
|     - | 1165 | `	}` |
|     - | 1166 | `	{` |
|     - | 1167 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|     - | 1168 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|    81 | 1169 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    81 | 1170 | `		if( rcCb != PH7_OK ){` |
|    11 | 1171 | `			return rcCb;` |
|     - | 1172 | `		}` |
|     - | 1173 | `	}` |
|    71 | 1174 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    71 | 1175 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1176 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|    71 | 1177 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|     - | 1178 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1179 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1180 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1181 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1182 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1183 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    80 | 1184 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1185 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1186 | `		VmCallArgMap sInner;` |
|    19 | 1187 | `		sInner.bHasNamed = 1;` |
|    19 | 1188 | `		sInner.bIsNamespaced = 0;` |
|     - | 1189 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1190 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1191 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1192 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1193 | `		sInner.bStrict = 0;` |
|    19 | 1194 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1195 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1196 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1197 | `	}else{` |
|    53 | 1198 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1199 | `	}` |
|    71 | 1200 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1201 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1202 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1203 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1204 | `		return PH7_EXCEPTION;` |
|     - | 1205 | `	}` |
|    65 | 1206 | `	if( rc != SXRET_OK ){` |
|     - | 1207 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1208 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1209 | `	}else{` |
|     - | 1210 | `		/* Callback result */` |
|    65 | 1211 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1212 | `	}` |
|    65 | 1213 | `	PH7_MemObjRelease(&sResult);` |
|    65 | 1214 | `	return PH7_OK;` |
|    41 | 1215 | `}` |
|     - | 1216 | `/*` |
|     - | 1217 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1218 | ` *  Call a callback with an array of parameters.` |
|     - | 1219 | ` * Parameter` |
|     - | 1220 | ` *  $callback` |
|     - | 1221 | ` *   The callable to be called.` |
|     - | 1222 | ` * $param_arr` |
|     - | 1223 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1224 | ` * Return` |
|     - | 1225 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1226 | ` */` |
|    36 | 1227 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1228 | `{` |
|     - | 1229 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1230 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1231 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1232 | `	SySet aArg;               /* Argument value pointers */` |
|    37 | 1233 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    37 | 1234 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1235 | `	sxi32 rc;` |
|     - | 1236 | `	sxu32 n;` |
|    37 | 1237 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1238 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1239 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1240 | `		return PH7_OK;` |
|     - | 1241 | `	}` |
|     - | 1242 | `	{` |
|    37 | 1243 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    37 | 1244 | `		if( rcCb != PH7_OK ){` |
|     3 | 1245 | `			return rcCb;` |
|     - | 1246 | `		}` |
|     - | 1247 | `	}` |
|    35 | 1248 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1249 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1250 | `	/* Initialize the arguments container */` |
|    35 | 1251 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1252 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1253 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1254 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1255 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1256 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1257 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1258 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1259 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1260 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1261 | `		/* Extract node value */` |
|   155 | 1262 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1263 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1264 | `				if( aNames == 0 ){` |
|     - | 1265 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1266 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1267 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1268 | `					if( aNames == 0 ){` |
|   ! 0 | 1269 | `						SySetRelease(&aArg);` |
|   ! 0 | 1270 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1271 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1272 | `					}` |
|    13 | 1273 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1274 | `				}` |
|    23 | 1275 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1276 | `			}` |
|   155 | 1277 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1278 | `			nSlot++;` |
|    77 | 1279 | `		}` |
|     - | 1280 | `		/* Point to the next entry */` |
|   155 | 1281 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1282 | `	}` |
|     - | 1283 | `	/* Try to invoke the callback */` |
|    35 | 1284 | `	if( aNames ){` |
|     - | 1285 | `		VmCallArgMap sMap;` |
|    13 | 1286 | `		sMap.bHasNamed = 1;` |
|    13 | 1287 | `		sMap.bIsNamespaced = 0;` |
|     - | 1288 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1289 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1290 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1291 | `		sMap.nTotal = nSlot;` |
|    13 | 1292 | `		sMap.aNames = aNames;` |
|    19 | 1293 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1294 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1295 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1296 | `	}else{` |
|    34 | 1297 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1298 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1299 | `	}` |
|    35 | 1300 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1301 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1302 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1303 | `		SySetRelease(&aArg);` |
|     5 | 1304 | `		return PH7_EXCEPTION;` |
|     - | 1305 | `	}` |
|    31 | 1306 | `	if( rc != SXRET_OK ){` |
|     - | 1307 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1308 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1309 | `	}else{` |
|     - | 1310 | `		/* Callback result */` |
|    31 | 1311 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1312 | `	}` |
|     - | 1313 | `	/* Cleanup the mess left behind */` |
|    31 | 1314 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1315 | `	SySetRelease(&aArg);` |
|    31 | 1316 | `	return PH7_OK;` |
|    19 | 1317 | `}` |
|     - | 1318 |  |
