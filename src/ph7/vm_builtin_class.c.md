# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 564/645 lines (87.44%)

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
|    23 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    23 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     9 |  249 | `		}` |
|    77 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    60 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    60 |  254 | `			while( pClass ){` |
|    60 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    60 |  256 | `					res = 1;` |
|    60 |  257 | `					break;` |
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
| 36742 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 36747 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 28927 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 28947 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    21 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 28927 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 28927 |  598 | `		if( pFrame->pBoundScope ){` |
|    15 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 28920 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 28871 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 14477 |  602 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  603 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  604 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  605 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  606 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  607 | `		}else{` |
|    42 |  608 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  609 | `		}` |
| 28887 |  610 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  611 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  612 | `			 * class must declare a private attribute of this name (a base` |
|     - |  613 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  614 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  615 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  616 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  617 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  618 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  619 | `			 * match (dynamic props and other non-declared shapes). */` |
|  9365 |  620 | `			ph7_class *pCaller = pCallerScope;` |
| 14045 |  621 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|  9360 |  622 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|  9365 |  623 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|  9365 |  624 | `			int bGranted = 0;` |
|  9365 |  625 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  7808 |  626 | `				if( pOwn->pDeclClass == 0` |
|  7808 |  627 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  4729 |  628 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  7809 |  629 | `					bGranted = 1;` |
|  3907 |  630 | `				}` |
|  5458 |  631 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   807 |  632 | `				bGranted = 1;` |
|   403 |  633 | `			}` |
|  9365 |  634 | `			if( !bGranted ){` |
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
|  9365 |  647 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
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
|  9365 |  665 | `			if( !bGranted ){` |
|    10 |  666 | `				goto dis; /* Access is forbidden */` |
|     - |  667 | `			}` |
|  4681 |  668 | `		}else{` |
|     - |  669 | `			/* Protected */` |
| 19527 |  670 | `			ph7_class *pBase = pCallerScope;` |
|     - |  671 | `			/* Must be in the same class hierarchy */` |
| 19527 |  672 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
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
| 14436 |  692 | `	}` |
| 36697 |  693 | `	return 1; /* Access is granted */` |
|    25 |  694 | `dis:` |
|    53 |  695 | `	if( bLog ){` |
|   ! 0 |  696 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  697 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  698 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  699 | `	}` |
|    53 |  700 | `	return 0; /* Access is forbidden */` |
| 18376 |  701 | `}` |
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
|     - |  726 | `		/* No such class,return NULL */` |
|   ! 0 |  727 | `		ph7_result_null(pCtx);` |
|   ! 0 |  728 | `		return PH7_OK;` |
|     - |  729 | `	}` |
|     - |  730 | `	/* Create a new array  */` |
|     5 |  731 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  732 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  733 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  734 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  735 | `		/* Out of memory,return NULL */` |
|   ! 0 |  736 | `		ph7_result_null(pCtx);` |
|   ! 0 |  737 | `		return PH7_OK;` |
|     - |  738 | `	}` |
|     - |  739 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  740 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  741 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  742 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  743 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  744 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  745 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  746 | `			continue;` |
|     - |  747 | `		}` |
|     - |  748 | `		/* Check if the access is allowed */` |
|     7 |  749 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  750 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  751 | `			ph7_value *pValue = 0;` |
|     7 |  752 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  753 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  754 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  755 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  756 | `			}else{` |
|     3 |  757 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  758 | `					PH7_MemObjRelease(&sValue);` |
|     - |  759 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  760 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  761 | `					pValue = &sValue;` |
|     1 |  762 | `				}` |
|     - |  763 | `			}` |
|     - |  764 | `			/* Fill in the array */` |
|     7 |  765 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  766 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  767 | `			/* Reset the cursor */` |
|     7 |  768 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  769 | `		}` |
|     1 |  770 | `	}` |
|     5 |  771 | `	PH7_MemObjRelease(&sValue);` |
|     - |  772 | `	/* Return the created array */` |
|     5 |  773 | `	ph7_result_value(pCtx,pArray);` |
|     - |  774 | `	/*` |
|     - |  775 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  776 | `	 * automatically as soon we return from this foreign function.` |
|     - |  777 | `	 */` |
|     5 |  778 | `	return PH7_OK;` |
|     3 |  779 | `}` |
|     - |  780 | `/*` |
|     - |  781 | ` * array get_object_vars(object $this)` |
|     - |  782 | ` *   Gets the properties of the given object` |
|     - |  783 | ` * Parameters` |
|     - |  784 | ` *  this` |
|     - |  785 | ` *   A class instance` |
|     - |  786 | ` * Return` |
|     - |  787 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  788 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  789 | ` *  it will be returned with a NULL value.` |
|     - |  790 | ` * Note:` |
|     - |  791 | ` *   NULL is returned on failure.` |
|     - |  792 | ` */` |
|    24 |  793 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  794 | `{` |
|    25 |  795 | `	ph7_class_instance *pThis = 0;` |
|     - |  796 | `	ph7_value *pName,*pArray;` |
|     - |  797 | `	SyHashEntry *pEntry;` |
|    25 |  798 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  799 | `		/* Extract the target instance */` |
|    25 |  800 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    12 |  801 | `	}` |
|    25 |  802 | `	if( pThis == 0 ){` |
|     - |  803 | `		/* No such instance,return NULL */` |
|   ! 0 |  804 | `		ph7_result_null(pCtx);` |
|   ! 0 |  805 | `		return PH7_OK;` |
|     - |  806 | `	}` |
|     - |  807 | `	/* Create a new array  */` |
|    25 |  808 | `	pArray = ph7_context_new_array(pCtx);` |
|    25 |  809 | `	pName = ph7_context_new_scalar(pCtx);` |
|    25 |  810 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  811 | `		/* Out of memory,return NULL */` |
|   ! 0 |  812 | `		ph7_result_null(pCtx);` |
|   ! 0 |  813 | `		return PH7_OK;` |
|     - |  814 | `	}` |
|     - |  815 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  816 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  817 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  818 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  819 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  820 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  821 | `	 * skipped instead of read after free. */` |
|     - |  822 | `	{` |
|     - |  823 | `		SySet sNames;` |
|     - |  824 | `		SyString *aName;` |
|     - |  825 | `		sxu32 iName,nName;` |
|    25 |  826 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    25 |  827 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    95 |  828 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    71 |  829 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    71 |  830 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  831 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  832 | `				continue;` |
|     - |  833 | `			}` |
|    60 |  834 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    31 |  835 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  836 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  837 | `			}` |
|    59 |  838 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  839 | `		}` |
|    25 |  840 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    25 |  841 | `		nName = SySetUsed(&sNames);` |
|    83 |  842 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    59 |  843 | `			SyString *pAttrName = &aName[iName];` |
|     - |  844 | `			VmClassAttr *pVmAttr;` |
|    59 |  845 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    59 |  846 | `			if( pEntry == 0 ){` |
|   ! 0 |  847 | `				continue; /* unset by an earlier hook */` |
|     - |  848 | `			}` |
|    59 |  849 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  850 | `			/* Check if the access is allowed */` |
|    59 |  851 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    47 |  852 | `				ph7_value *pValue = 0;` |
|     - |  853 | `				ph7_value sHookVal;` |
|     - |  854 | `				sxi32 rcHk;` |
|     - |  855 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  856 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    47 |  857 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    47 |  858 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    47 |  859 | `				if( rcHk == SXRET_OK ){` |
|    15 |  860 | `					pValue = &sHookVal;` |
|    40 |  861 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  862 | `					/* Extract attribute */` |
|    33 |  863 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    17 |  864 | `				}else{` |
|     - |  865 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  866 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  867 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  868 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  869 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  870 | `					break;` |
|     - |  871 | `				}` |
|    47 |  872 | `				if( pValue ){` |
|     - |  873 | `					/* Insert attribute name in the array */` |
|    47 |  874 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    47 |  875 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    23 |  876 | `				}` |
|    47 |  877 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  878 | `				/* Reset the cursor */` |
|    47 |  879 | `				ph7_value_reset_string_cursor(pName);` |
|    23 |  880 | `			}` |
|    30 |  881 | `		}` |
|    25 |  882 | `		SySetRelease(&sNames);` |
|     - |  883 | `	}` |
|     - |  884 | `	/* Return the created array */` |
|    25 |  885 | `	ph7_result_value(pCtx,pArray);` |
|     - |  886 | `	/*` |
|     - |  887 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  888 | `	 * automatically as soon we return from this foreign function.` |
|     - |  889 | `	 */` |
|    25 |  890 | `	return PH7_OK;` |
|    13 |  891 | `}` |
|     - |  892 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  893 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  894 | ` * detection should reject them up front. */` |
|     - |  895 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  896 | `/*` |
|     - |  897 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  898 | ` * interface.Otherwise FALSE is returned.` |
|     - |  899 | ` */` |
| 19692 |  900 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  901 | `{` |
|     - |  902 | `	ph7_class **apInterface;` |
|     - |  903 | `	sxu32 n;` |
| 19697 |  904 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  905 | `		/* Empty interface container */` |
|   425 |  906 | `		return FALSE;` |
|     - |  907 | `	}` |
|     - |  908 | `	/* Point to the set of implemented interfaces */` |
| 19277 |  909 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  910 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  911 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 37145 |  912 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 19871 |  913 | `		ph7_class *pIface = apInterface[n];` |
| 19871 |  914 | `		int iDepth = 0;` |
| 38353 |  915 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 20485 |  916 | `			if( pIface == pClass ){` |
|  2003 |  917 | `				return TRUE;` |
|     - |  918 | `			}` |
| 18487 |  919 | `			pIface = pIface->pBase;` |
| 18487 |  920 | `			iDepth++;` |
|     5 |  921 | `		}` |
|  8939 |  922 | `	}` |
| 17279 |  923 | `	return FALSE;` |
|  9851 |  924 | `}` |
|     - |  925 | `/*` |
|     - |  926 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  927 | ` * is an instance of the main class (second argument).` |
|     - |  928 | ` * Otherwise FALSE is returned.` |
|     - |  929 | ` */` |
| 24072 |  930 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  931 | `{` |
|     - |  932 | `	ph7_class *pParent;` |
|     - |  933 | `	sxi32 rc;` |
| 24077 |  934 | `	if( pThis == pClass ){` |
|     - |  935 | `		/* Instance of the same class */` |
|  9615 |  936 | `		return TRUE;` |
|     - |  937 | `	}` |
|     - |  938 | `	/* Check implemented interfaces */` |
| 14467 |  939 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 14467 |  940 | `	if( rc ){` |
|  1321 |  941 | `		return TRUE;` |
|     - |  942 | `	}` |
|     - |  943 | `	/* Check parent classes */` |
| 13151 |  944 | `	pParent = pThis->pBase;` |
| 17689 |  945 | `	while( pParent ){` |
| 17147 |  946 | `		if( pParent == pClass ){` |
|     - |  947 | `			/* Same instance */` |
| 11927 |  948 | `			return TRUE;` |
|     - |  949 | `		}` |
|     - |  950 | `		/* Check the implemented interfaces */` |
|  5225 |  951 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  5225 |  952 | `		if( rc ){` |
|   687 |  953 | `			return TRUE;` |
|     - |  954 | `		}` |
|     - |  955 | `		/* Point to the parent class */` |
|  4543 |  956 | `		pParent = pParent->pBase;` |
|     5 |  957 | `	}` |
|     - |  958 | `	/* Not an instance of the the given class */` |
|   547 |  959 | `	return FALSE;` |
| 12041 |  960 | `}` |
|     - |  961 | `/*` |
|     - |  962 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  963 | ` * is a subclass of the main class (second argument).` |
|     - |  964 | ` * Otherwise FALSE is returned.` |
|     - |  965 | ` */` |
|    16 |  966 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  967 | `{` |
|    17 |  968 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  969 | `	SyHashEntry *pEntry;` |
|     - |  970 | `	SyString *pName;` |
|     - |  971 | `	sxi32 rc;` |
|    27 |  972 | `	while( pClass ){` |
|    17 |  973 | `		pName = &pClass->sName;` |
|     - |  974 | `		/* Query the derived hashtable */` |
|    17 |  975 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    17 |  976 | `		if( pEntry ){` |
|     7 |  977 | `			return TRUE;` |
|     - |  978 | `		}` |
|    11 |  979 | `		pClass = pClass->pBase;` |
|     1 |  980 | `	}` |
|    11 |  981 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|    11 |  982 | `	if( rc ){` |
|   ! 0 |  983 | `		return TRUE;` |
|     - |  984 | `	}` |
|     - |  985 | `	/* Not a subclass */` |
|    11 |  986 | `	return FALSE;` |
|     9 |  987 | `}` |
|     - |  988 | `/*` |
|     - |  989 | ` * bool is_a(object $object,string $class_name)` |
|     - |  990 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  991 | ` * Parameters` |
|     - |  992 | ` *  object` |
|     - |  993 | ` *   The tested object` |
|     - |  994 | ` * class_name` |
|     - |  995 | ` *  The class name` |
|     - |  996 | ` * Return` |
|     - |  997 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  998 | ` *   parents, FALSE otherwise.` |
|     - |  999 | ` */` |
|    18 | 1000 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1001 | `{` |
|    19 | 1002 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1003 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 | 1004 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - | 1005 | `		ph7_class *pClass;` |
|     - | 1006 | `		/* Extract the given class */` |
|    19 | 1007 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1008 | `		if( pClass ){` |
|     - | 1009 | `			/* Perform the query */` |
|    19 | 1010 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 | 1011 | `		}` |
|     9 | 1012 | `	}` |
|     - | 1013 | `	/* Query result */` |
|    19 | 1014 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1015 | `	return PH7_OK;` |
|     1 | 1016 | `}` |
|     - | 1017 | `/*` |
|     - | 1018 | ` * int spl_object_id(object $object)` |
|     - | 1019 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1020 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1021 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1022 | ` */` |
|    58 | 1023 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1024 | `{` |
|     - | 1025 | `	ph7_class_instance *pThis;` |
|    62 | 1026 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1027 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1028 | `		return PH7_OK;` |
|     - | 1029 | `	}` |
|    62 | 1030 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1031 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1032 | `	return PH7_OK;` |
|    33 | 1033 | `}` |
|     - | 1034 | `/*` |
|     - | 1035 | ` * string spl_object_hash(object $object)` |
|     - | 1036 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1037 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1038 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1039 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1040 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1041 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1042 | ` */` |
|    14 | 1043 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1044 | `{` |
|     - | 1045 | `	ph7_class_instance *pThis;` |
|    16 | 1046 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1047 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1048 | `		return PH7_OK;` |
|     - | 1049 | `	}` |
|    16 | 1050 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1051 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1052 | `	return PH7_OK;` |
|     9 | 1053 | `}` |
|     - | 1054 | `/*` |
|     - | 1055 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1056 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1057 | ` * Parameters` |
|     - | 1058 | ` *  object` |
|     - | 1059 | ` *   The tested object` |
|     - | 1060 | ` * class_name` |
|     - | 1061 | ` *  The class name` |
|     - | 1062 | ` * Return` |
|     - | 1063 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1064 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1065 | ` */` |
|    18 | 1066 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1067 | `{` |
|    19 | 1068 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1069 | `	if( nArg > 1 ){` |
|     - | 1070 | `		ph7_class *pClass,*pMain;` |
|     - | 1071 | `		/* Extract the given classes */` |
|    19 | 1072 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    19 | 1073 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1074 | `		if( pClass && pMain ){` |
|     - | 1075 | `			/* Perform the query */` |
|    17 | 1076 | `			res = VmSubclassOf(pClass,pMain);` |
|     8 | 1077 | `		}` |
|     9 | 1078 | `	}` |
|     - | 1079 | `	/* Query result */` |
|    19 | 1080 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1081 | `	return PH7_OK;` |
|     1 | 1082 | `}` |
|    66 | 1083 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1084 | `{` |
|     - | 1085 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1086 | `	sxi32 rc;` |
|    67 | 1087 | `	if( nArg < 1 ){` |
|     - | 1088 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1089 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1090 | `		return PH7_OK;` |
|     - | 1091 | `	}` |
|    67 | 1092 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    67 | 1093 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1094 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1095 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1096 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1097 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1098 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1099 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    76 | 1100 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1101 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1102 | `		VmCallArgMap sInner;` |
|    19 | 1103 | `		sInner.bHasNamed = 1;` |
|    19 | 1104 | `		sInner.bIsNamespaced = 0;` |
|     - | 1105 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1106 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1107 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1108 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1109 | `		sInner.bStrict = 0;` |
|    19 | 1110 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1111 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1112 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1113 | `	}else{` |
|    49 | 1114 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1115 | `	}` |
|    67 | 1116 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1117 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1118 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1119 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1120 | `		return PH7_EXCEPTION;` |
|     - | 1121 | `	}` |
|    61 | 1122 | `	if( rc != SXRET_OK ){` |
|     - | 1123 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1124 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1125 | `	}else{` |
|     - | 1126 | `		/* Callback result */` |
|    61 | 1127 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1128 | `	}` |
|    61 | 1129 | `	PH7_MemObjRelease(&sResult);` |
|    61 | 1130 | `	return PH7_OK;` |
|    34 | 1131 | `}` |
|     - | 1132 | `/*` |
|     - | 1133 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1134 | ` *  Call a callback with an array of parameters.` |
|     - | 1135 | ` * Parameter` |
|     - | 1136 | ` *  $callback` |
|     - | 1137 | ` *   The callable to be called.` |
|     - | 1138 | ` * $param_arr` |
|     - | 1139 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1140 | ` * Return` |
|     - | 1141 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1142 | ` */` |
|    34 | 1143 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1144 | `{` |
|     - | 1145 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1146 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1147 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1148 | `	SySet aArg;               /* Argument value pointers */` |
|    35 | 1149 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    35 | 1150 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1151 | `	sxi32 rc;` |
|     - | 1152 | `	sxu32 n;` |
|    35 | 1153 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1154 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1155 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1156 | `		return PH7_OK;` |
|     - | 1157 | `	}` |
|    35 | 1158 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1159 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1160 | `	/* Initialize the arguments container */` |
|    35 | 1161 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1162 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1163 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1164 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1165 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1166 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1167 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1168 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1169 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1170 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1171 | `		/* Extract node value */` |
|   155 | 1172 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1173 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1174 | `				if( aNames == 0 ){` |
|     - | 1175 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1176 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1177 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1178 | `					if( aNames == 0 ){` |
|   ! 0 | 1179 | `						SySetRelease(&aArg);` |
|   ! 0 | 1180 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1181 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1182 | `					}` |
|    13 | 1183 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1184 | `				}` |
|    23 | 1185 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1186 | `			}` |
|   155 | 1187 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1188 | `			nSlot++;` |
|    77 | 1189 | `		}` |
|     - | 1190 | `		/* Point to the next entry */` |
|   155 | 1191 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1192 | `	}` |
|     - | 1193 | `	/* Try to invoke the callback */` |
|    35 | 1194 | `	if( aNames ){` |
|     - | 1195 | `		VmCallArgMap sMap;` |
|    13 | 1196 | `		sMap.bHasNamed = 1;` |
|    13 | 1197 | `		sMap.bIsNamespaced = 0;` |
|     - | 1198 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1199 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1200 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1201 | `		sMap.nTotal = nSlot;` |
|    13 | 1202 | `		sMap.aNames = aNames;` |
|    19 | 1203 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1204 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1205 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1206 | `	}else{` |
|    34 | 1207 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1208 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1209 | `	}` |
|    35 | 1210 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1211 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1212 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1213 | `		SySetRelease(&aArg);` |
|     5 | 1214 | `		return PH7_EXCEPTION;` |
|     - | 1215 | `	}` |
|    31 | 1216 | `	if( rc != SXRET_OK ){` |
|     - | 1217 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1218 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1219 | `	}else{` |
|     - | 1220 | `		/* Callback result */` |
|    31 | 1221 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1222 | `	}` |
|     - | 1223 | `	/* Cleanup the mess left behind */` |
|    31 | 1224 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1225 | `	SySetRelease(&aArg);` |
|    31 | 1226 | `	return PH7_OK;` |
|    18 | 1227 | `}` |
|     - | 1228 |  |
