# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 564/654 lines (86.24%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   644 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   649 |   11 | `	if( nArg < 1 ){` |
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
|   649 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   649 |   25 | `		if( pClass ){` |
|   647 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   647 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   326 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   649 |   34 | `	return PH7_OK;` |
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
|  2374 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  2379 |  115 | `	ph7_class *pClass = 0;` |
|  2379 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   757 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  2002 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|  1624 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|  1624 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|  1624 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|  1624 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|  1600 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   799 |  131 | `			}` |
|   810 |  132 | `		}` |
|   810 |  133 | `	}` |
|  2379 |  134 | `	return pClass;` |
|     5 |  135 | `}` |
|     - |  136 | `/*` |
|     - |  137 | ` * bool property_exists(mixed $class,string $property)` |
|     - |  138 | ` *   Checks if the object or class has a property.` |
|     - |  139 | ` * Parameters` |
|     - |  140 | ` *  class` |
|     - |  141 | ` *   The class name or an object of the class to test for` |
|     - |  142 | ` * property` |
|     - |  143 | ` *  The name of the property` |
|     - |  144 | ` * Return` |
|     - |  145 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|     - |  146 | ` */` |
|    16 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  148 | `{` |
|    17 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|    17 |  150 | `	if( nArg > 1 ){` |
|     - |  151 | `		ph7_class *pClass;` |
|    17 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    17 |  153 | `		if( pClass ){` |
|     - |  154 | `			const char *zName;` |
|     - |  155 | `			int nLen;` |
|     - |  156 | `			/* Extract attribute name */` |
|    17 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    17 |  158 | `			if( nLen > 0 ){` |
|     - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|    16 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    11 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  162 | `						/* property exists,flag that */` |
|    13 |  163 | `						res = 1;` |
|     6 |  164 | `				}` |
|     - |  165 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  166 | `				 * attribute table, not the class's — php reports those too` |
|     - |  167 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    17 |  168 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  169 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  170 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  171 | `						res = 1;` |
|   ! 0 |  172 | `					}` |
|     1 |  173 | `				}` |
|     8 |  174 | `			}` |
|     8 |  175 | `		}` |
|     8 |  176 | `	}` |
|    17 |  177 | `	ph7_result_bool(pCtx,res);` |
|    17 |  178 | `	return PH7_OK;` |
|     1 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  182 | ` *   Checks if the given method is a class member.` |
|     - |  183 | ` * Parameters` |
|     - |  184 | ` *  class` |
|     - |  185 | ` *   The class name or an object of the class to test for` |
|     - |  186 | ` * property` |
|     - |  187 | ` *  The name of the method` |
|     - |  188 | ` * Return` |
|     - |  189 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  190 | ` */` |
|     4 |  191 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  192 | `{` |
|     5 |  193 | `	int res = 0; /* Assume method does not exists */` |
|     5 |  194 | `	if( nArg > 1 ){` |
|     - |  195 | `		ph7_class *pClass;` |
|     5 |  196 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     5 |  197 | `		if( pClass ){` |
|     - |  198 | `			const char *zName;` |
|     - |  199 | `			int nLen;` |
|     - |  200 | `			/* Extract method name */` |
|     5 |  201 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     5 |  202 | `			if( nLen > 0 ){` |
|     - |  203 | `				/* Perform the lookup in the method table */` |
|     5 |  204 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  205 | `					/* method exists,flag that */` |
|     3 |  206 | `					res = 1;` |
|     1 |  207 | `				}` |
|     2 |  208 | `			}` |
|     2 |  209 | `		}` |
|     2 |  210 | `	}` |
|     5 |  211 | `	ph7_result_bool(pCtx,res);` |
|     5 |  212 | `	return PH7_OK;` |
|     1 |  213 | `}` |
|     - |  214 | `/*` |
|     - |  215 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  216 | ` *   Checks if the class has been defined.` |
|     - |  217 | ` * Parameters` |
|     - |  218 | ` *  class_name` |
|     - |  219 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  220 | ` *   unlinke the standard PHP engine.` |
|     - |  221 | ` *  autoload` |
|     - |  222 | ` *   Whether or not to call __autoload by default.` |
|     - |  223 | ` * Return` |
|     - |  224 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  225 | ` */` |
|    72 |  226 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  227 | `{` |
|    77 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    77 |  229 | `	if( nArg > 0 ){` |
|    77 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    77 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    77 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    77 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    77 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    77 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    36 |  242 | `		}` |
|    77 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    22 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    22 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     9 |  249 | `		}` |
|    77 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    61 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    61 |  254 | `			while( pClass ){` |
|    61 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    61 |  256 | `					res = 1;` |
|    61 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    28 |  261 | `		}` |
|    36 |  262 | `	}` |
|    77 |  263 | `	ph7_result_bool(pCtx,res);` |
|    77 |  264 | `	return PH7_OK;` |
|     5 |  265 | `}` |
|     - |  266 | `/*` |
|     - |  267 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  268 | ` *   Checks if the interface has been defined.` |
|     - |  269 | ` * Parameters` |
|     - |  270 | ` *  class_name` |
|     - |  271 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  272 | ` *   unlinke the standard PHP engine.` |
|     - |  273 | ` *  autoload` |
|     - |  274 | ` *   Whether or not to call __autoload by default.` |
|     - |  275 | ` * Return` |
|     - |  276 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  277 | ` */` |
|    28 |  278 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  279 | `{` |
|    29 |  280 | `	int res = 0; /* Assume interface does not exist */` |
|    29 |  281 | `	if( nArg > 0 ){` |
|    29 |  282 | `		SyHashEntry *pEntry = 0;` |
|     - |  283 | `		const char *zName;` |
|     - |  284 | `		int nLen;` |
|    29 |  285 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  286 | `		/* Extract given name */` |
|    29 |  287 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    29 |  288 | `		if( nArg >= 2 ){` |
|   ! 0 |  289 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  290 | `		}` |
|     - |  291 | `		/* Perform a hash lookup */` |
|    29 |  292 | `		if( nLen > 0 ){` |
|    29 |  293 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    14 |  294 | `		}` |
|    29 |  295 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  296 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  297 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  298 | `			if( pClass ){` |
|   ! 0 |  299 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  300 | `			}` |
|     1 |  301 | `		}` |
|    29 |  302 | `		if( pEntry ){` |
|    27 |  303 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    27 |  304 | `			while( pClass ){` |
|    27 |  305 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  306 | `					/* interface is available */` |
|    27 |  307 | `					res = 1;` |
|    27 |  308 | `					break;` |
|     - |  309 | `				}` |
|     - |  310 | `				/* Next with the same name */` |
|   ! 0 |  311 | `				pClass = pClass->pNextName;` |
|   ! 0 |  312 | `			}` |
|    13 |  313 | `		}` |
|    14 |  314 | `	}` |
|    29 |  315 | `	ph7_result_bool(pCtx,res);` |
|    29 |  316 | `	return PH7_OK;` |
|     1 |  317 | `}` |
|     - |  318 | `/*` |
|     - |  319 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  320 | ` *   Creates an alias for a class.` |
|     - |  321 | ` * Parameters` |
|     - |  322 | ` *  original` |
|     - |  323 | ` *    The original class.` |
|     - |  324 | ` *  alias` |
|     - |  325 | ` *   The alias name for the class.` |
|     - |  326 | ` * Return` |
|     - |  327 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  328 | ` */` |
|     2 |  329 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  330 | `{` |
|     - |  331 | `	const char *zOld,*zNew;` |
|     - |  332 | `	int nOldLen,nNewLen;` |
|     - |  333 | `	SyHashEntry *pEntry;` |
|     - |  334 | `	ph7_class *pClass;` |
|     - |  335 | `	char *zDup;` |
|     - |  336 | `	sxi32 rc;` |
|     3 |  337 | `	if( nArg < 2 ){` |
|     - |  338 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  340 | `		return PH7_OK;` |
|     - |  341 | `	}` |
|     - |  342 | `	/* Extract old class name */` |
|     3 |  343 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  344 | `	/* Extract alias name */` |
|     3 |  345 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  346 | `	if( nNewLen < 1 ){` |
|     - |  347 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  348 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* Perform a hash lookup */` |
|     3 |  352 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  353 | `	if( pEntry ==  0 ){` |
|     - |  354 | `		/* No such class,return FALSE */` |
|   ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  356 | `		return PH7_OK;` |
|     - |  357 | `	}` |
|     - |  358 | `	/* Point to the class */` |
|     3 |  359 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  360 | `	/* Duplicate alias name */` |
|     3 |  361 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  362 | `	if( zDup == 0 ){` |
|     - |  363 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  364 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  365 | `		return PH7_OK;` |
|     - |  366 | `	}` |
|     - |  367 | `	/* Create the alias */` |
|     3 |  368 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  369 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  370 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  371 | `	}` |
|     3 |  372 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  373 | `	return PH7_OK;` |
|     2 |  374 | `}` |
|     - |  375 | `/*` |
|     - |  376 | ` * array get_declared_classes(void)` |
|     - |  377 | ` *   Returns an array with the name of the defined classes` |
|     - |  378 | ` * Parameters` |
|     - |  379 | ` *  None` |
|     - |  380 | ` * Return` |
|     - |  381 | ` *   Returns an array of the names of the declared classes` |
|     - |  382 | ` *   in the current script.` |
|     - |  383 | ` * Note:` |
|     - |  384 | ` *   NULL is returned on failure.` |
|     - |  385 | ` */` |
|     2 |  386 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  387 | `{` |
|     - |  388 | `	ph7_value *pName,*pArray;` |
|     - |  389 | `	SyHashEntry *pEntry;` |
|     - |  390 | `	/* Create a new array first */` |
|     3 |  391 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  392 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  393 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  394 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  395 | `		SXUNUSED(apArg);` |
|     - |  396 | `		/* Out of memory,return NULL */` |
|   ! 0 |  397 | `		ph7_result_null(pCtx);` |
|   ! 0 |  398 | `		return PH7_OK;` |
|     - |  399 | `	}` |
|     - |  400 | `	/* Fill the array with the defined classes */` |
|     3 |  401 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   310 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   307 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   307 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   269 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   269 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   269 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|   134 |  411 | `		}` |
|     1 |  412 | `	}` |
|     - |  413 | `	/* Return the created array */` |
|     3 |  414 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  415 | `	return PH7_OK;` |
|     2 |  416 | `}` |
|     - |  417 | `/*` |
|     - |  418 | ` * array get_declared_interfaces(void)` |
|     - |  419 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  420 | ` * Parameters` |
|     - |  421 | ` *  None` |
|     - |  422 | ` * Return` |
|     - |  423 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  424 | ` *   in the current script.` |
|     - |  425 | ` * Note:` |
|     - |  426 | ` *   NULL is returned on failure.` |
|     - |  427 | ` */` |
|     2 |  428 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  429 | `{` |
|     - |  430 | `	ph7_value *pName,*pArray;` |
|     - |  431 | `	SyHashEntry *pEntry;` |
|     - |  432 | `	/* Create a new array first */` |
|     3 |  433 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  434 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  435 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  436 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  437 | `		SXUNUSED(apArg);` |
|     - |  438 | `		/* Out of memory,return NULL */` |
|   ! 0 |  439 | `		ph7_result_null(pCtx);` |
|   ! 0 |  440 | `		return PH7_OK;` |
|     - |  441 | `	}` |
|     - |  442 | `	/* Fill the array with the defined classes */` |
|     3 |  443 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   312 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   309 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   309 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    41 |  448 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  449 | `			/* insert interface name */` |
|    41 |  450 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  451 | `			/* Reset the cursor */` |
|    41 |  452 | `			ph7_value_reset_string_cursor(pName);` |
|    20 |  453 | `		}` |
|     1 |  454 | `	}` |
|     - |  455 | `	/* Return the created array */` |
|     3 |  456 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  457 | `	return PH7_OK;` |
|     2 |  458 | `}` |
|     - |  459 | `/*` |
|     - |  460 | ` * array get_class_methods(string/object $class_name)` |
|     - |  461 | ` *   Returns an array with the name of the class methods` |
|     - |  462 | ` * Parameters` |
|     - |  463 | ` *  class_name` |
|     - |  464 | ` *  The class name or class instance` |
|     - |  465 | ` * Return` |
|     - |  466 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  467 | ` *  In case of an error, it returns NULL.` |
|     - |  468 | ` * Note:` |
|     - |  469 | ` *   NULL is returned on failure.` |
|     - |  470 | ` */` |
|     8 |  471 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  472 | `{` |
|     - |  473 | `	ph7_value *pName,*pArray;` |
|     - |  474 | `	SyHashEntry *pEntry;` |
|     - |  475 | `	ph7_class *pClass;` |
|     - |  476 | `	/* Extract the target class first */` |
|     9 |  477 | `	pClass = 0;` |
|     9 |  478 | `	if( nArg > 0 ){` |
|     9 |  479 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     4 |  480 | `	}` |
|     9 |  481 | `	if( pClass == 0 ){` |
|     - |  482 | `		/* No such class,return NULL */` |
|     3 |  483 | `		ph7_result_null(pCtx);` |
|     3 |  484 | `		return PH7_OK;` |
|     - |  485 | `	}` |
|     - |  486 | `	/* Create a new array  */` |
|     7 |  487 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  488 | `	pName = ph7_context_new_scalar(pCtx);` |
|     7 |  489 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  490 | `		/* Out of memory,return NULL */` |
|   ! 0 |  491 | `		ph7_result_null(pCtx);` |
|   ! 0 |  492 | `		return PH7_OK;` |
|     - |  493 | `	}` |
|     - |  494 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|     - |  495 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|     - |  496 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|     - |  497 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|     - |  498 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|     - |  499 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|     - |  500 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|     - |  501 | `	{` |
|     - |  502 | `		SySet aTmp;` |
|     - |  503 | `		SyHashEntry **apEntry;` |
|     - |  504 | `		ph7_class *pLevel;` |
|     - |  505 | `		sxu32 n;` |
|     7 |  506 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|     7 |  507 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    27 |  508 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    21 |  509 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|     1 |  510 | `		}` |
|     7 |  511 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|    15 |  512 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|     - |  513 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|     - |  514 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|     - |  515 | `			 * hash-order fallback covers line-less internal methods). */` |
|     - |  516 | `			SySet aLvl;` |
|     - |  517 | `			ph7_class_method **apLvl;` |
|     - |  518 | `			sxu32 i,j;` |
|     9 |  519 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     - |  520 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|     - |  521 | `			 * come out in declaration order when walked newest-first, while` |
|     - |  522 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|     - |  523 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    37 |  524 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    29 |  525 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    29 |  526 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|    29 |  527 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|    29 |  528 | `				if( pDecl != pLevel ){` |
|     - |  529 | `					/* A declarer outside the base chain (a used trait, or none)` |
|     - |  530 | `					 * counts as the class's own level, like php. */` |
|     - |  531 | `					ph7_class *pWalk;` |
|     9 |  532 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     7 |  533 | `						continue;` |
|     - |  534 | `					}` |
|     9 |  535 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     9 |  536 | `						if( pWalk == pDecl ){` |
|     5 |  537 | `							break;` |
|     - |  538 | `						}` |
|     3 |  539 | `					}` |
|     5 |  540 | `					if( pWalk != 0 ){` |
|     5 |  541 | `						continue; /* in-chain: its own level emits it */` |
|     - |  542 | `					}` |
|   ! 0 |  543 | `				}` |
|    21 |  544 | `				SySetPut(&aLvl,(const void *)&pMethod);` |
|    11 |  545 | `			}` |
|     9 |  546 | `			apLvl = (ph7_class_method **)SySetBasePtr(&aLvl);` |
|     - |  547 | `			/* Insertion sort by declaration line (stable) */` |
|    21 |  548 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    13 |  549 | `				ph7_class_method *pKey = apLvl[i];` |
|    13 |  550 | `				for( j = i; j > 0 && apLvl[j-1]->nLine > pKey->nLine; j-- ){` |
|   ! 0 |  551 | `					apLvl[j] = apLvl[j-1];` |
|   ! 0 |  552 | `				}` |
|    13 |  553 | `				apLvl[j] = pKey;` |
|     7 |  554 | `			}` |
|    29 |  555 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     - |  556 | `				/* Insert method name */` |
|    21 |  557 | `				ph7_value_string(pName,SyStringData(&apLvl[i]->sFunc.sName),(int)SyStringLength(&apLvl[i]->sFunc.sName));` |
|    21 |  558 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  559 | `				/* Reset the cursor */` |
|    21 |  560 | `				ph7_value_reset_string_cursor(pName);` |
|    11 |  561 | `			}` |
|     9 |  562 | `			SySetRelease(&aLvl);` |
|     5 |  563 | `		}` |
|     7 |  564 | `		SySetRelease(&aTmp);` |
|     - |  565 | `	}` |
|     - |  566 | `	/* Return the created array */` |
|     7 |  567 | `	ph7_result_value(pCtx,pArray);` |
|     - |  568 | `	/*` |
|     - |  569 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  570 | `	 * automatically as soon we return from this foreign function.` |
|     - |  571 | `	 */` |
|     7 |  572 | `	return PH7_OK;` |
|     5 |  573 | `}` |
|     - |  574 | `/*` |
|     - |  575 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  576 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  577 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  578 | ` */` |
| 36998 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 37003 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 29165 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 29185 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    21 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 29165 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 29165 |  598 | `		if( pFrame->pBoundScope ){` |
|    15 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 29158 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 29109 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 14596 |  602 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  603 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  604 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  605 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  606 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  607 | `		}else{` |
|    42 |  608 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  609 | `		}` |
| 29125 |  610 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  611 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  612 | `			 * class must declare a private attribute of this name (a base` |
|     - |  613 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  614 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  615 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  616 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  617 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  618 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  619 | `			 * match (dynamic props and other non-declared shapes). */` |
|  9411 |  620 | `			ph7_class *pCaller = pCallerScope;` |
| 14114 |  621 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|  9406 |  622 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|  9411 |  623 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|  9411 |  624 | `			int bGranted = 0;` |
|  9411 |  625 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  7854 |  626 | `				if( pOwn->pDeclClass == 0` |
|  7854 |  627 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  4752 |  628 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  7855 |  629 | `					bGranted = 1;` |
|  3930 |  630 | `				}` |
|  5481 |  631 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   807 |  632 | `				bGranted = 1;` |
|   403 |  633 | `			}` |
|  9411 |  634 | `			if( !bGranted ){` |
|     - |  635 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  636 | `				ph7_class **apTrait;` |
|     - |  637 | `				sxu32 nTrait,k;` |
|   752 |  638 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   752 |  639 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   752 |  640 | `				for(k = 0; k < nTrait; k++){` |
|   ! 0 |  641 | `					if( apTrait[k] == pCaller ){` |
|   ! 0 |  642 | `						bGranted = 1;` |
|   ! 0 |  643 | `						break;` |
|     - |  644 | `					}` |
|   ! 0 |  645 | `				}` |
|   375 |  646 | `			}` |
|  9411 |  647 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  648 | `				/* The target "class" is itself a trait: a trait-copied private` |
|     - |  649 | `				 * member behaves as if declared in the adopting class, so a` |
|     - |  650 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|     - |  651 | `				 * from a using class's static method reaching a trait-private` |
|     - |  652 | `				 * static — the callee resolves via the shared trait VmFunc` |
|     - |  653 | `				 * whose owner is the trait, not the class). */` |
|     - |  654 | `				ph7_class **apTrait;` |
|     - |  655 | `				sxu32 nTrait,k;` |
|   743 |  656 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|   743 |  657 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|   743 |  658 | `				for(k = 0; k < nTrait; k++){` |
|   743 |  659 | `					if( apTrait[k] == pClass ){` |
|   743 |  660 | `						bGranted = 1;` |
|   743 |  661 | `						break;` |
|     - |  662 | `					}` |
|   ! 0 |  663 | `				}` |
|   371 |  664 | `			}` |
|  9411 |  665 | `			if( !bGranted ){` |
|    10 |  666 | `				goto dis; /* Access is forbidden */` |
|     - |  667 | `			}` |
|  4704 |  668 | `		}else{` |
|     - |  669 | `			/* Protected */` |
| 19719 |  670 | `			ph7_class *pBase = pCallerScope;` |
|     - |  671 | `			/* Must be in the same class hierarchy */` |
| 19719 |  672 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|     8 |  673 | `				int bTraitGrant = 0;` |
|     8 |  674 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  675 | `					/* Same trait-target rule as the private branch above */` |
|     - |  676 | `					ph7_class **apTrait;` |
|     - |  677 | `					sxu32 nTrait,k;` |
|     8 |  678 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     8 |  679 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     8 |  680 | `					for(k = 0; k < nTrait; k++){` |
|     6 |  681 | `						if( apTrait[k] == pClass ){` |
|     6 |  682 | `							bTraitGrant = 1;` |
|     6 |  683 | `							break;` |
|     - |  684 | `						}` |
|   ! 0 |  685 | `					}` |
|     3 |  686 | `				}` |
|     8 |  687 | `				if( !bTraitGrant ){` |
|     3 |  688 | `					goto dis; /* Access is forbidden */` |
|     - |  689 | `				}` |
|     2 |  690 | `			}` |
|     - |  691 | `		}` |
| 14555 |  692 | `	}` |
| 36953 |  693 | `	return 1; /* Access is granted */` |
|    25 |  694 | `dis:` |
|    53 |  695 | `	if( bLog ){` |
|   ! 0 |  696 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  697 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  698 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  699 | `	}` |
|    53 |  700 | `	return 0; /* Access is forbidden */` |
| 18504 |  701 | `}` |
|     - |  702 | `/*` |
|     - |  703 | ` * array get_class_vars(string/object $class_name)` |
|     - |  704 | ` *   Get the default properties of the class` |
|     - |  705 | ` * Parameters` |
|     - |  706 | ` *  class_name` |
|     - |  707 | ` *   The class name or class instance` |
|     - |  708 | ` * Return` |
|     - |  709 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  710 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  711 | ` *  of varname => value.` |
|     - |  712 | ` * Note:` |
|     - |  713 | ` *   NULL is returned on failure.` |
|     - |  714 | ` */` |
|     4 |  715 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  716 | `{` |
|     - |  717 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  718 | `	SyHashEntry *pEntry;` |
|     - |  719 | `	ph7_class *pClass;` |
|     - |  720 | `	/* Extract the target class first */` |
|     5 |  721 | `	pClass = 0;` |
|     5 |  722 | `	if( nArg > 0 ){` |
|     5 |  723 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     2 |  724 | `	}` |
|     5 |  725 | `	if( pClass == 0 ){` |
|     - |  726 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|     - |  727 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|     - |  728 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|     - |  729 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|   ! 0 |  730 | `		int nLen = 0;` |
|   ! 0 |  731 | `		const char *zVal = "";` |
|   ! 0 |  732 | `		if( nArg > 0 ){` |
|   ! 0 |  733 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|   ! 0 |  734 | `				zVal = "Array";` |
|   ! 0 |  735 | `				nLen = (int)sizeof("Array") - 1;` |
|   ! 0 |  736 | `			}else{` |
|   ! 0 |  737 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  738 | `			}` |
|   ! 0 |  739 | `		}` |
|   ! 0 |  740 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  741 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|   ! 0 |  742 | `			nLen,zVal);` |
|     - |  743 | `	}` |
|     - |  744 | `	/* Create a new array  */` |
|     5 |  745 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  746 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  747 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  748 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  749 | `		/* Out of memory,return NULL */` |
|   ! 0 |  750 | `		ph7_result_null(pCtx);` |
|   ! 0 |  751 | `		return PH7_OK;` |
|     - |  752 | `	}` |
|     - |  753 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  754 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  755 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  756 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  757 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  758 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  759 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  760 | `			continue;` |
|     - |  761 | `		}` |
|     - |  762 | `		/* Check if the access is allowed */` |
|     7 |  763 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  764 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  765 | `			ph7_value *pValue = 0;` |
|     7 |  766 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  767 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  768 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  769 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  770 | `			}else{` |
|     3 |  771 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  772 | `					PH7_MemObjRelease(&sValue);` |
|     - |  773 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  774 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  775 | `					pValue = &sValue;` |
|     1 |  776 | `				}` |
|     - |  777 | `			}` |
|     - |  778 | `			/* Fill in the array */` |
|     7 |  779 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  780 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  781 | `			/* Reset the cursor */` |
|     7 |  782 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  783 | `		}` |
|     1 |  784 | `	}` |
|     5 |  785 | `	PH7_MemObjRelease(&sValue);` |
|     - |  786 | `	/* Return the created array */` |
|     5 |  787 | `	ph7_result_value(pCtx,pArray);` |
|     - |  788 | `	/*` |
|     - |  789 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  790 | `	 * automatically as soon we return from this foreign function.` |
|     - |  791 | `	 */` |
|     5 |  792 | `	return PH7_OK;` |
|     3 |  793 | `}` |
|     - |  794 | `/*` |
|     - |  795 | ` * array get_object_vars(object $this)` |
|     - |  796 | ` *   Gets the properties of the given object` |
|     - |  797 | ` * Parameters` |
|     - |  798 | ` *  this` |
|     - |  799 | ` *   A class instance` |
|     - |  800 | ` * Return` |
|     - |  801 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  802 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  803 | ` *  it will be returned with a NULL value.` |
|     - |  804 | ` * Note:` |
|     - |  805 | ` *   NULL is returned on failure.` |
|     - |  806 | ` */` |
|    24 |  807 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  808 | `{` |
|    25 |  809 | `	ph7_class_instance *pThis = 0;` |
|     - |  810 | `	ph7_value *pName,*pArray;` |
|     - |  811 | `	SyHashEntry *pEntry;` |
|    25 |  812 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  813 | `		/* Extract the target instance */` |
|    25 |  814 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    12 |  815 | `	}` |
|    25 |  816 | `	if( pThis == 0 ){` |
|     - |  817 | `		/* No such instance,return NULL */` |
|   ! 0 |  818 | `		ph7_result_null(pCtx);` |
|   ! 0 |  819 | `		return PH7_OK;` |
|     - |  820 | `	}` |
|     - |  821 | `	/* Create a new array  */` |
|    25 |  822 | `	pArray = ph7_context_new_array(pCtx);` |
|    25 |  823 | `	pName = ph7_context_new_scalar(pCtx);` |
|    25 |  824 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  825 | `		/* Out of memory,return NULL */` |
|   ! 0 |  826 | `		ph7_result_null(pCtx);` |
|   ! 0 |  827 | `		return PH7_OK;` |
|     - |  828 | `	}` |
|     - |  829 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  830 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  831 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  832 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  833 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  834 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  835 | `	 * skipped instead of read after free. */` |
|     - |  836 | `	{` |
|     - |  837 | `		SySet sNames;` |
|     - |  838 | `		SyString *aName;` |
|     - |  839 | `		sxu32 iName,nName;` |
|    25 |  840 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    25 |  841 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    95 |  842 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    71 |  843 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    71 |  844 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  845 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  846 | `				continue;` |
|     - |  847 | `			}` |
|    60 |  848 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    31 |  849 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  850 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  851 | `			}` |
|    59 |  852 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  853 | `		}` |
|    25 |  854 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    25 |  855 | `		nName = SySetUsed(&sNames);` |
|    83 |  856 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    59 |  857 | `			SyString *pAttrName = &aName[iName];` |
|     - |  858 | `			VmClassAttr *pVmAttr;` |
|    59 |  859 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    59 |  860 | `			if( pEntry == 0 ){` |
|   ! 0 |  861 | `				continue; /* unset by an earlier hook */` |
|     - |  862 | `			}` |
|    59 |  863 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  864 | `			/* Check if the access is allowed */` |
|    59 |  865 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    47 |  866 | `				ph7_value *pValue = 0;` |
|     - |  867 | `				ph7_value sHookVal;` |
|     - |  868 | `				sxi32 rcHk;` |
|     - |  869 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  870 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    47 |  871 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    47 |  872 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    47 |  873 | `				if( rcHk == SXRET_OK ){` |
|    15 |  874 | `					pValue = &sHookVal;` |
|    40 |  875 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  876 | `					/* Extract attribute */` |
|    33 |  877 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    17 |  878 | `				}else{` |
|     - |  879 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  880 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  881 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  882 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  883 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  884 | `					break;` |
|     - |  885 | `				}` |
|    47 |  886 | `				if( pValue ){` |
|     - |  887 | `					/* Insert attribute name in the array */` |
|    47 |  888 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    47 |  889 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    23 |  890 | `				}` |
|    47 |  891 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  892 | `				/* Reset the cursor */` |
|    47 |  893 | `				ph7_value_reset_string_cursor(pName);` |
|    23 |  894 | `			}` |
|    30 |  895 | `		}` |
|    25 |  896 | `		SySetRelease(&sNames);` |
|     - |  897 | `	}` |
|     - |  898 | `	/* Return the created array */` |
|    25 |  899 | `	ph7_result_value(pCtx,pArray);` |
|     - |  900 | `	/*` |
|     - |  901 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  902 | `	 * automatically as soon we return from this foreign function.` |
|     - |  903 | `	 */` |
|    25 |  904 | `	return PH7_OK;` |
|    13 |  905 | `}` |
|     - |  906 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  907 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  908 | ` * detection should reject them up front. */` |
|     - |  909 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  910 | `/*` |
|     - |  911 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  912 | ` * interface.Otherwise FALSE is returned.` |
|     - |  913 | ` */` |
| 19896 |  914 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  915 | `{` |
|     - |  916 | `	ph7_class **apInterface;` |
|     - |  917 | `	sxu32 n;` |
| 19901 |  918 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  919 | `		/* Empty interface container */` |
|   425 |  920 | `		return FALSE;` |
|     - |  921 | `	}` |
|     - |  922 | `	/* Point to the set of implemented interfaces */` |
| 19481 |  923 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  924 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  925 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 37553 |  926 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 20075 |  927 | `		ph7_class *pIface = apInterface[n];` |
| 20075 |  928 | `		int iDepth = 0;` |
| 38761 |  929 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 20689 |  930 | `			if( pIface == pClass ){` |
|  2003 |  931 | `				return TRUE;` |
|     - |  932 | `			}` |
| 18691 |  933 | `			pIface = pIface->pBase;` |
| 18691 |  934 | `			iDepth++;` |
|     5 |  935 | `		}` |
|  9041 |  936 | `	}` |
| 17483 |  937 | `	return FALSE;` |
|  9953 |  938 | `}` |
|     - |  939 | `/*` |
|     - |  940 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  941 | ` * is an instance of the main class (second argument).` |
|     - |  942 | ` * Otherwise FALSE is returned.` |
|     - |  943 | ` */` |
| 24292 |  944 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  945 | `{` |
|     - |  946 | `	ph7_class *pParent;` |
|     - |  947 | `	sxi32 rc;` |
| 24297 |  948 | `	if( pThis == pClass ){` |
|     - |  949 | `		/* Instance of the same class */` |
|  9667 |  950 | `		return TRUE;` |
|     - |  951 | `	}` |
|     - |  952 | `	/* Check implemented interfaces */` |
| 14635 |  953 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 14635 |  954 | `	if( rc ){` |
|  1321 |  955 | `		return TRUE;` |
|     - |  956 | `	}` |
|     - |  957 | `	/* Check parent classes */` |
| 13319 |  958 | `	pParent = pThis->pBase;` |
| 17893 |  959 | `	while( pParent ){` |
| 17351 |  960 | `		if( pParent == pClass ){` |
|     - |  961 | `			/* Same instance */` |
| 12095 |  962 | `			return TRUE;` |
|     - |  963 | `		}` |
|     - |  964 | `		/* Check the implemented interfaces */` |
|  5261 |  965 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  5261 |  966 | `		if( rc ){` |
|   687 |  967 | `			return TRUE;` |
|     - |  968 | `		}` |
|     - |  969 | `		/* Point to the parent class */` |
|  4579 |  970 | `		pParent = pParent->pBase;` |
|     5 |  971 | `	}` |
|     - |  972 | `	/* Not an instance of the the given class */` |
|   547 |  973 | `	return FALSE;` |
| 12151 |  974 | `}` |
|     - |  975 | `/*` |
|     - |  976 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  977 | ` * is a subclass of the main class (second argument).` |
|     - |  978 | ` * Otherwise FALSE is returned.` |
|     - |  979 | ` */` |
|    16 |  980 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  981 | `{` |
|    17 |  982 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  983 | `	SyHashEntry *pEntry;` |
|     - |  984 | `	SyString *pName;` |
|     - |  985 | `	sxi32 rc;` |
|    27 |  986 | `	while( pClass ){` |
|    17 |  987 | `		pName = &pClass->sName;` |
|     - |  988 | `		/* Query the derived hashtable */` |
|    17 |  989 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    17 |  990 | `		if( pEntry ){` |
|     7 |  991 | `			return TRUE;` |
|     - |  992 | `		}` |
|    11 |  993 | `		pClass = pClass->pBase;` |
|     1 |  994 | `	}` |
|    11 |  995 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|    11 |  996 | `	if( rc ){` |
|   ! 0 |  997 | `		return TRUE;` |
|     - |  998 | `	}` |
|     - |  999 | `	/* Not a subclass */` |
|    11 | 1000 | `	return FALSE;` |
|     9 | 1001 | `}` |
|     - | 1002 | `/*` |
|     - | 1003 | ` * bool is_a(object $object,string $class_name)` |
|     - | 1004 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - | 1005 | ` * Parameters` |
|     - | 1006 | ` *  object` |
|     - | 1007 | ` *   The tested object` |
|     - | 1008 | ` * class_name` |
|     - | 1009 | ` *  The class name` |
|     - | 1010 | ` * Return` |
|     - | 1011 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - | 1012 | ` *   parents, FALSE otherwise.` |
|     - | 1013 | ` */` |
|    18 | 1014 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1015 | `{` |
|    19 | 1016 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1017 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 | 1018 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - | 1019 | `		ph7_class *pClass;` |
|     - | 1020 | `		/* Extract the given class */` |
|    19 | 1021 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1022 | `		if( pClass ){` |
|     - | 1023 | `			/* Perform the query */` |
|    19 | 1024 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 | 1025 | `		}` |
|     9 | 1026 | `	}` |
|     - | 1027 | `	/* Query result */` |
|    19 | 1028 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1029 | `	return PH7_OK;` |
|     1 | 1030 | `}` |
|     - | 1031 | `/*` |
|     - | 1032 | ` * int spl_object_id(object $object)` |
|     - | 1033 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1034 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1035 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1036 | ` */` |
|    58 | 1037 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1038 | `{` |
|     - | 1039 | `	ph7_class_instance *pThis;` |
|    62 | 1040 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1041 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1042 | `		return PH7_OK;` |
|     - | 1043 | `	}` |
|    62 | 1044 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1045 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1046 | `	return PH7_OK;` |
|    33 | 1047 | `}` |
|     - | 1048 | `/*` |
|     - | 1049 | ` * string spl_object_hash(object $object)` |
|     - | 1050 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1051 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1052 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1053 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1054 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1055 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1056 | ` */` |
|    14 | 1057 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1058 | `{` |
|     - | 1059 | `	ph7_class_instance *pThis;` |
|    16 | 1060 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1061 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1062 | `		return PH7_OK;` |
|     - | 1063 | `	}` |
|    16 | 1064 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1065 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1066 | `	return PH7_OK;` |
|     9 | 1067 | `}` |
|     - | 1068 | `/*` |
|     - | 1069 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1070 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1071 | ` * Parameters` |
|     - | 1072 | ` *  object` |
|     - | 1073 | ` *   The tested object` |
|     - | 1074 | ` * class_name` |
|     - | 1075 | ` *  The class name` |
|     - | 1076 | ` * Return` |
|     - | 1077 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1078 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1079 | ` */` |
|    18 | 1080 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1081 | `{` |
|    19 | 1082 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1083 | `	if( nArg > 1 ){` |
|     - | 1084 | `		ph7_class *pClass,*pMain;` |
|     - | 1085 | `		/* Extract the given classes */` |
|    19 | 1086 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    19 | 1087 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1088 | `		if( pClass && pMain ){` |
|     - | 1089 | `			/* Perform the query */` |
|    17 | 1090 | `			res = VmSubclassOf(pClass,pMain);` |
|     8 | 1091 | `		}` |
|     9 | 1092 | `	}` |
|     - | 1093 | `	/* Query result */` |
|    19 | 1094 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1095 | `	return PH7_OK;` |
|     1 | 1096 | `}` |
|    66 | 1097 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1098 | `{` |
|     - | 1099 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1100 | `	sxi32 rc;` |
|    67 | 1101 | `	if( nArg < 1 ){` |
|     - | 1102 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1103 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1104 | `		return PH7_OK;` |
|     - | 1105 | `	}` |
|    67 | 1106 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    67 | 1107 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1108 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1109 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1110 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1111 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1112 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1113 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    76 | 1114 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1115 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1116 | `		VmCallArgMap sInner;` |
|    19 | 1117 | `		sInner.bHasNamed = 1;` |
|    19 | 1118 | `		sInner.bIsNamespaced = 0;` |
|     - | 1119 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1120 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1121 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1122 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1123 | `		sInner.bStrict = 0;` |
|    19 | 1124 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1125 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1126 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1127 | `	}else{` |
|    49 | 1128 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1129 | `	}` |
|    67 | 1130 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1131 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1132 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1133 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1134 | `		return PH7_EXCEPTION;` |
|     - | 1135 | `	}` |
|    61 | 1136 | `	if( rc != SXRET_OK ){` |
|     - | 1137 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1138 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1139 | `	}else{` |
|     - | 1140 | `		/* Callback result */` |
|    61 | 1141 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1142 | `	}` |
|    61 | 1143 | `	PH7_MemObjRelease(&sResult);` |
|    61 | 1144 | `	return PH7_OK;` |
|    34 | 1145 | `}` |
|     - | 1146 | `/*` |
|     - | 1147 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1148 | ` *  Call a callback with an array of parameters.` |
|     - | 1149 | ` * Parameter` |
|     - | 1150 | ` *  $callback` |
|     - | 1151 | ` *   The callable to be called.` |
|     - | 1152 | ` * $param_arr` |
|     - | 1153 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1154 | ` * Return` |
|     - | 1155 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1156 | ` */` |
|    34 | 1157 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1158 | `{` |
|     - | 1159 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1160 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1161 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1162 | `	SySet aArg;               /* Argument value pointers */` |
|    35 | 1163 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    35 | 1164 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1165 | `	sxi32 rc;` |
|     - | 1166 | `	sxu32 n;` |
|    35 | 1167 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1168 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1169 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1170 | `		return PH7_OK;` |
|     - | 1171 | `	}` |
|    35 | 1172 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1173 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1174 | `	/* Initialize the arguments container */` |
|    35 | 1175 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1176 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1177 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1178 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1179 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1180 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1181 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1182 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1183 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1184 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1185 | `		/* Extract node value */` |
|   155 | 1186 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1187 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1188 | `				if( aNames == 0 ){` |
|     - | 1189 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1190 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1191 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1192 | `					if( aNames == 0 ){` |
|   ! 0 | 1193 | `						SySetRelease(&aArg);` |
|   ! 0 | 1194 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1195 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1196 | `					}` |
|    13 | 1197 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1198 | `				}` |
|    23 | 1199 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1200 | `			}` |
|   155 | 1201 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1202 | `			nSlot++;` |
|    77 | 1203 | `		}` |
|     - | 1204 | `		/* Point to the next entry */` |
|   155 | 1205 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1206 | `	}` |
|     - | 1207 | `	/* Try to invoke the callback */` |
|    35 | 1208 | `	if( aNames ){` |
|     - | 1209 | `		VmCallArgMap sMap;` |
|    13 | 1210 | `		sMap.bHasNamed = 1;` |
|    13 | 1211 | `		sMap.bIsNamespaced = 0;` |
|     - | 1212 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1213 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1214 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1215 | `		sMap.nTotal = nSlot;` |
|    13 | 1216 | `		sMap.aNames = aNames;` |
|    19 | 1217 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1218 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1219 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1220 | `	}else{` |
|    34 | 1221 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1222 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1223 | `	}` |
|    35 | 1224 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1225 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1226 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1227 | `		SySetRelease(&aArg);` |
|     5 | 1228 | `		return PH7_EXCEPTION;` |
|     - | 1229 | `	}` |
|    31 | 1230 | `	if( rc != SXRET_OK ){` |
|     - | 1231 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1232 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1233 | `	}else{` |
|     - | 1234 | `		/* Callback result */` |
|    31 | 1235 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1236 | `	}` |
|     - | 1237 | `	/* Cleanup the mess left behind */` |
|    31 | 1238 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1239 | `	SySetRelease(&aArg);` |
|    31 | 1240 | `	return PH7_OK;` |
|    18 | 1241 | `}` |
|     - | 1242 |  |
