# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 580/670 lines (86.57%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   734 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   739 |   11 | `	if( nArg < 1 ){` |
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
|   739 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   739 |   25 | `		if( pClass ){` |
|   737 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   737 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   371 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   739 |   34 | `	return PH7_OK;` |
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
|  1730 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  1735 |  115 | `	ph7_class *pClass = 0;` |
|  1735 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   899 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1288 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|   838 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|   838 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|   838 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|   838 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|   811 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   404 |  131 | `			}` |
|   417 |  132 | `		}` |
|   417 |  133 | `	}` |
|  1735 |  134 | `	return pClass;` |
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
|    80 |  226 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  227 | `{` |
|    85 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    85 |  229 | `	if( nArg > 0 ){` |
|    85 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    85 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    85 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    85 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    85 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    85 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    40 |  242 | `		}` |
|    85 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    24 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    24 |  246 | `			if( pClass ){` |
|     9 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     3 |  248 | `			}` |
|    10 |  249 | `		}` |
|    85 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    68 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    68 |  254 | `			while( pClass ){` |
|    68 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    68 |  256 | `					res = 1;` |
|    68 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    32 |  261 | `		}` |
|    40 |  262 | `	}` |
|    85 |  263 | `	ph7_result_bool(pCtx,res);` |
|    85 |  264 | `	return PH7_OK;` |
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
|   324 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   321 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   321 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   283 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   283 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   283 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|   141 |  411 | `		}` |
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
|   326 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   323 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   323 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
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
| 31828 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 31833 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 23777 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 23801 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    25 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 23777 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 23777 |  598 | `		if( pFrame->pBoundScope ){` |
|    15 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 23770 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 23713 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 11906 |  602 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  603 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  604 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  605 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  606 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  607 | `		}else{` |
|    50 |  608 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  609 | `		}` |
| 23729 |  610 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  611 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  612 | `			 * class must declare a private attribute of this name (a base` |
|     - |  613 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  614 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  615 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  616 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  617 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  618 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  619 | `			 * match (dynamic props and other non-declared shapes). */` |
| 10441 |  620 | `			ph7_class *pCaller = pCallerScope;` |
| 15659 |  621 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
| 10436 |  622 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
| 10441 |  623 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
| 10441 |  624 | `			int bGranted = 0;` |
| 10441 |  625 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  8722 |  626 | `				if( pOwn->pDeclClass == 0` |
|  8722 |  627 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  5201 |  628 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  8723 |  629 | `					bGranted = 1;` |
|  4364 |  630 | `				}` |
|  6078 |  631 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   851 |  632 | `				bGranted = 1;` |
|   425 |  633 | `			}` |
| 10441 |  634 | `			if( !bGranted ){` |
|     - |  635 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  636 | `				ph7_class **apTrait;` |
|     - |  637 | `				sxu32 nTrait,k;` |
|   871 |  638 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   871 |  639 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   871 |  640 | `				for(k = 0; k < nTrait; k++){` |
|   ! 0 |  641 | `					if( apTrait[k] == pCaller ){` |
|   ! 0 |  642 | `						bGranted = 1;` |
|   ! 0 |  643 | `						break;` |
|     - |  644 | `					}` |
|   ! 0 |  645 | `				}` |
|   434 |  646 | `			}` |
| 10441 |  647 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  648 | `				/* The target "class" is itself a trait: a trait-copied private` |
|     - |  649 | `				 * member behaves as if declared in the adopting class, so a` |
|     - |  650 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|     - |  651 | `				 * from a using class's static method reaching a trait-private` |
|     - |  652 | `				 * static — the callee resolves via the shared trait VmFunc` |
|     - |  653 | `				 * whose owner is the trait, not the class). */` |
|     - |  654 | `				ph7_class **apTrait;` |
|     - |  655 | `				sxu32 nTrait,k;` |
|   862 |  656 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|   862 |  657 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|   862 |  658 | `				for(k = 0; k < nTrait; k++){` |
|   862 |  659 | `					if( apTrait[k] == pClass ){` |
|   862 |  660 | `						bGranted = 1;` |
|   862 |  661 | `						break;` |
|     - |  662 | `					}` |
|   ! 0 |  663 | `				}` |
|   430 |  664 | `			}` |
| 10441 |  665 | `			if( !bGranted ){` |
|    10 |  666 | `				goto dis; /* Access is forbidden */` |
|     - |  667 | `			}` |
|  5219 |  668 | `		}else{` |
|     - |  669 | `			/* Protected */` |
| 13293 |  670 | `			ph7_class *pBase = pCallerScope;` |
|     - |  671 | `			/* Must be in the same class hierarchy */` |
| 13293 |  672 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
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
| 11857 |  692 | `	}` |
| 31775 |  693 | `	return 1; /* Access is granted */` |
|    29 |  694 | `dis:` |
|    61 |  695 | `	if( bLog ){` |
|   ! 0 |  696 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  697 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  698 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  699 | `	}` |
|    61 |  700 | `	return 0; /* Access is forbidden */` |
| 15919 |  701 | `}` |
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
|    26 |  807 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  808 | `{` |
|    27 |  809 | `	ph7_class_instance *pThis = 0;` |
|     - |  810 | `	ph7_value *pName,*pArray;` |
|     - |  811 | `	SyHashEntry *pEntry;` |
|    27 |  812 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  813 | `		/* Extract the target instance */` |
|    27 |  814 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    13 |  815 | `	}` |
|    27 |  816 | `	if( pThis == 0 ){` |
|     - |  817 | `		/* No such instance,return NULL */` |
|   ! 0 |  818 | `		ph7_result_null(pCtx);` |
|   ! 0 |  819 | `		return PH7_OK;` |
|     - |  820 | `	}` |
|     - |  821 | `	/* Create a new array  */` |
|    27 |  822 | `	pArray = ph7_context_new_array(pCtx);` |
|    27 |  823 | `	pName = ph7_context_new_scalar(pCtx);` |
|    27 |  824 | `	if( pArray == 0 \|\| pName == 0){` |
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
|    27 |  840 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    27 |  841 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|   103 |  842 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    77 |  843 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    77 |  844 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  845 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  846 | `				continue;` |
|     - |  847 | `			}` |
|    66 |  848 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    34 |  849 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  850 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  851 | `			}` |
|    65 |  852 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  853 | `		}` |
|    27 |  854 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    27 |  855 | `		nName = SySetUsed(&sNames);` |
|    91 |  856 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    65 |  857 | `			SyString *pAttrName = &aName[iName];` |
|     - |  858 | `			VmClassAttr *pVmAttr;` |
|    65 |  859 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    65 |  860 | `			if( pEntry == 0 ){` |
|   ! 0 |  861 | `				continue; /* unset by an earlier hook */` |
|     - |  862 | `			}` |
|    65 |  863 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  864 | `			/* Check if the access is allowed */` |
|    65 |  865 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    51 |  866 | `				ph7_value *pValue = 0;` |
|     - |  867 | `				ph7_value sHookVal;` |
|     - |  868 | `				sxi32 rcHk;` |
|     - |  869 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  870 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    51 |  871 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    51 |  872 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    51 |  873 | `				if( rcHk == SXRET_OK ){` |
|    15 |  874 | `					pValue = &sHookVal;` |
|    44 |  875 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  876 | `					/* Extract attribute */` |
|    37 |  877 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    19 |  878 | `				}else{` |
|     - |  879 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  880 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  881 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  882 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  883 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  884 | `					break;` |
|     - |  885 | `				}` |
|    51 |  886 | `				if( pValue ){` |
|     - |  887 | `					/* Insert attribute name in the array */` |
|    51 |  888 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    51 |  889 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    25 |  890 | `				}` |
|    51 |  891 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  892 | `				/* Reset the cursor */` |
|    51 |  893 | `				ph7_value_reset_string_cursor(pName);` |
|    25 |  894 | `			}` |
|    33 |  895 | `		}` |
|    27 |  896 | `		SySetRelease(&sNames);` |
|     - |  897 | `	}` |
|     - |  898 | `	/* Return the created array */` |
|    27 |  899 | `	ph7_result_value(pCtx,pArray);` |
|     - |  900 | `	/*` |
|     - |  901 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  902 | `	 * automatically as soon we return from this foreign function.` |
|     - |  903 | `	 */` |
|    27 |  904 | `	return PH7_OK;` |
|    14 |  905 | `}` |
|     - |  906 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  907 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  908 | ` * detection should reject them up front. */` |
|     - |  909 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  910 | `/*` |
|     - |  911 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|     - |  912 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|     - |  913 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|     - |  914 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|     - |  915 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|     - |  916 | ` */` |
| 21870 |  917 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|     5 |  918 | `{` |
| 41197 |  919 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|     - |  920 | `		ph7_class **apParent;` |
|     - |  921 | `		sxu32 n;` |
| 24047 |  922 | `		if( pIface == pTarget ){` |
|  4723 |  923 | `			return TRUE;` |
|     - |  924 | `		}` |
|     - |  925 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|     - |  926 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
| 19329 |  927 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
| 19333 |  928 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|     7 |  929 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|     3 |  930 | `				return TRUE;` |
|     - |  931 | `			}` |
|     3 |  932 | `		}` |
| 19327 |  933 | `		pIface = pIface->pBase;` |
| 19327 |  934 | `		iDepth++;` |
|     5 |  935 | `	}` |
| 17155 |  936 | `	return FALSE;` |
| 10940 |  937 | `}` |
|     - |  938 | `/*` |
|     - |  939 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  940 | ` * interface.Otherwise FALSE is returned.` |
|     - |  941 | ` */` |
| 24330 |  942 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  943 | `{` |
|     - |  944 | `	ph7_class **apInterface;` |
|     - |  945 | `	sxu32 n;` |
| 24335 |  946 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  947 | `		/* Empty interface container */` |
|  4075 |  948 | `		return FALSE;` |
|     - |  949 | `	}` |
|     - |  950 | `	/* Point to the set of implemented interfaces */` |
| 20265 |  951 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  952 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  953 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 37411 |  954 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 21869 |  955 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
|  4723 |  956 | `			return TRUE;` |
|     - |  957 | `		}` |
|  8578 |  958 | `	}` |
| 15547 |  959 | `	return FALSE;` |
| 12170 |  960 | `}` |
|     - |  961 | `/*` |
|     - |  962 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  963 | ` * is an instance of the main class (second argument).` |
|     - |  964 | ` * Otherwise FALSE is returned.` |
|     - |  965 | ` */` |
| 25854 |  966 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  967 | `{` |
|     - |  968 | `	ph7_class *pParent;` |
|     - |  969 | `	sxi32 rc;` |
| 25859 |  970 | `	if( pThis == pClass ){` |
|     - |  971 | `		/* Instance of the same class */` |
|  8225 |  972 | `		return TRUE;` |
|     - |  973 | `	}` |
|     - |  974 | `	/* Check implemented interfaces */` |
| 17639 |  975 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 17639 |  976 | `	if( rc ){` |
|  2003 |  977 | `		return TRUE;` |
|     - |  978 | `	}` |
|     - |  979 | `	/* Check parent classes */` |
| 15641 |  980 | `	pParent = pThis->pBase;` |
| 19607 |  981 | `	while( pParent ){` |
| 13891 |  982 | `		if( pParent == pClass ){` |
|     - |  983 | `			/* Same instance */` |
|  7205 |  984 | `			return TRUE;` |
|     - |  985 | `		}` |
|     - |  986 | `		/* Check the implemented interfaces */` |
|  6691 |  987 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  6691 |  988 | `		if( rc ){` |
|  2725 |  989 | `			return TRUE;` |
|     - |  990 | `		}` |
|     - |  991 | `		/* Point to the parent class */` |
|  3971 |  992 | `		pParent = pParent->pBase;` |
|     5 |  993 | `	}` |
|     - |  994 | `	/* Not an instance of the the given class */` |
|  5721 |  995 | `	return FALSE;` |
| 12932 |  996 | `}` |
|     - |  997 | `/*` |
|     - |  998 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  999 | ` * is a subclass of the main class (second argument).` |
|     - | 1000 | ` * Otherwise FALSE is returned.` |
|     - | 1001 | ` */` |
|    16 | 1002 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 | 1003 | `{` |
|    17 | 1004 | `	SySet *pInterface = &pClass->aInterface;` |
|     - | 1005 | `	SyHashEntry *pEntry;` |
|     - | 1006 | `	SyString *pName;` |
|     - | 1007 | `	sxi32 rc;` |
|    27 | 1008 | `	while( pClass ){` |
|    17 | 1009 | `		pName = &pClass->sName;` |
|     - | 1010 | `		/* Query the derived hashtable */` |
|    17 | 1011 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    17 | 1012 | `		if( pEntry ){` |
|     7 | 1013 | `			return TRUE;` |
|     - | 1014 | `		}` |
|    11 | 1015 | `		pClass = pClass->pBase;` |
|     1 | 1016 | `	}` |
|    11 | 1017 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|    11 | 1018 | `	if( rc ){` |
|   ! 0 | 1019 | `		return TRUE;` |
|     - | 1020 | `	}` |
|     - | 1021 | `	/* Not a subclass */` |
|    11 | 1022 | `	return FALSE;` |
|     9 | 1023 | `}` |
|     - | 1024 | `/*` |
|     - | 1025 | ` * bool is_a(object $object,string $class_name)` |
|     - | 1026 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - | 1027 | ` * Parameters` |
|     - | 1028 | ` *  object` |
|     - | 1029 | ` *   The tested object` |
|     - | 1030 | ` * class_name` |
|     - | 1031 | ` *  The class name` |
|     - | 1032 | ` * Return` |
|     - | 1033 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - | 1034 | ` *   parents, FALSE otherwise.` |
|     - | 1035 | ` */` |
|    18 | 1036 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1037 | `{` |
|    19 | 1038 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1039 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 | 1040 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - | 1041 | `		ph7_class *pClass;` |
|     - | 1042 | `		/* Extract the given class */` |
|    19 | 1043 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1044 | `		if( pClass ){` |
|     - | 1045 | `			/* Perform the query */` |
|    19 | 1046 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 | 1047 | `		}` |
|     9 | 1048 | `	}` |
|     - | 1049 | `	/* Query result */` |
|    19 | 1050 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1051 | `	return PH7_OK;` |
|     1 | 1052 | `}` |
|     - | 1053 | `/*` |
|     - | 1054 | ` * int spl_object_id(object $object)` |
|     - | 1055 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1056 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1057 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1058 | ` */` |
|    58 | 1059 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1060 | `{` |
|     - | 1061 | `	ph7_class_instance *pThis;` |
|    62 | 1062 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1063 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1064 | `		return PH7_OK;` |
|     - | 1065 | `	}` |
|    62 | 1066 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1067 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1068 | `	return PH7_OK;` |
|    33 | 1069 | `}` |
|     - | 1070 | `/*` |
|     - | 1071 | ` * string spl_object_hash(object $object)` |
|     - | 1072 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1073 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1074 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1075 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1076 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1077 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1078 | ` */` |
|    14 | 1079 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1080 | `{` |
|     - | 1081 | `	ph7_class_instance *pThis;` |
|    16 | 1082 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1083 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1084 | `		return PH7_OK;` |
|     - | 1085 | `	}` |
|    16 | 1086 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1087 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1088 | `	return PH7_OK;` |
|     9 | 1089 | `}` |
|     - | 1090 | `/*` |
|     - | 1091 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1092 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1093 | ` * Parameters` |
|     - | 1094 | ` *  object` |
|     - | 1095 | ` *   The tested object` |
|     - | 1096 | ` * class_name` |
|     - | 1097 | ` *  The class name` |
|     - | 1098 | ` * Return` |
|     - | 1099 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1100 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1101 | ` */` |
|    18 | 1102 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1103 | `{` |
|    19 | 1104 | `	int res = 0; /* Assume FALSE by default */` |
|    19 | 1105 | `	if( nArg > 1 ){` |
|     - | 1106 | `		ph7_class *pClass,*pMain;` |
|     - | 1107 | `		/* Extract the given classes */` |
|    19 | 1108 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    19 | 1109 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 | 1110 | `		if( pClass && pMain ){` |
|     - | 1111 | `			/* Perform the query */` |
|    17 | 1112 | `			res = VmSubclassOf(pClass,pMain);` |
|     8 | 1113 | `		}` |
|     9 | 1114 | `	}` |
|     - | 1115 | `	/* Query result */` |
|    19 | 1116 | `	ph7_result_bool(pCtx,res);` |
|    19 | 1117 | `	return PH7_OK;` |
|     1 | 1118 | `}` |
|    80 | 1119 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1120 | `{` |
|     - | 1121 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1122 | `	sxi32 rc;` |
|    81 | 1123 | `	if( nArg < 1 ){` |
|     - | 1124 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1125 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1126 | `		return PH7_OK;` |
|     - | 1127 | `	}` |
|     - | 1128 | `	{` |
|     - | 1129 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|     - | 1130 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|    81 | 1131 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    81 | 1132 | `		if( rcCb != PH7_OK ){` |
|    11 | 1133 | `			return rcCb;` |
|     - | 1134 | `		}` |
|     - | 1135 | `	}` |
|    71 | 1136 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    71 | 1137 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1138 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|    71 | 1139 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|     - | 1140 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1141 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1142 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1143 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1144 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1145 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    80 | 1146 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1147 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1148 | `		VmCallArgMap sInner;` |
|    19 | 1149 | `		sInner.bHasNamed = 1;` |
|    19 | 1150 | `		sInner.bIsNamespaced = 0;` |
|     - | 1151 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1152 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1153 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1154 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1155 | `		sInner.bStrict = 0;` |
|    19 | 1156 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1157 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1158 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1159 | `	}else{` |
|    53 | 1160 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1161 | `	}` |
|    71 | 1162 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1163 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1164 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1165 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1166 | `		return PH7_EXCEPTION;` |
|     - | 1167 | `	}` |
|    65 | 1168 | `	if( rc != SXRET_OK ){` |
|     - | 1169 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1170 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1171 | `	}else{` |
|     - | 1172 | `		/* Callback result */` |
|    65 | 1173 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1174 | `	}` |
|    65 | 1175 | `	PH7_MemObjRelease(&sResult);` |
|    65 | 1176 | `	return PH7_OK;` |
|    41 | 1177 | `}` |
|     - | 1178 | `/*` |
|     - | 1179 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1180 | ` *  Call a callback with an array of parameters.` |
|     - | 1181 | ` * Parameter` |
|     - | 1182 | ` *  $callback` |
|     - | 1183 | ` *   The callable to be called.` |
|     - | 1184 | ` * $param_arr` |
|     - | 1185 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1186 | ` * Return` |
|     - | 1187 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1188 | ` */` |
|    36 | 1189 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1190 | `{` |
|     - | 1191 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1192 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1193 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1194 | `	SySet aArg;               /* Argument value pointers */` |
|    37 | 1195 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    37 | 1196 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1197 | `	sxi32 rc;` |
|     - | 1198 | `	sxu32 n;` |
|    37 | 1199 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1200 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1201 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1202 | `		return PH7_OK;` |
|     - | 1203 | `	}` |
|     - | 1204 | `	{` |
|    37 | 1205 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    37 | 1206 | `		if( rcCb != PH7_OK ){` |
|     3 | 1207 | `			return rcCb;` |
|     - | 1208 | `		}` |
|     - | 1209 | `	}` |
|    35 | 1210 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1211 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1212 | `	/* Initialize the arguments container */` |
|    35 | 1213 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1214 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1215 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1216 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1217 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1218 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1219 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1220 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1221 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1222 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1223 | `		/* Extract node value */` |
|   155 | 1224 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1225 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1226 | `				if( aNames == 0 ){` |
|     - | 1227 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1228 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1229 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1230 | `					if( aNames == 0 ){` |
|   ! 0 | 1231 | `						SySetRelease(&aArg);` |
|   ! 0 | 1232 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1233 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1234 | `					}` |
|    13 | 1235 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1236 | `				}` |
|    23 | 1237 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1238 | `			}` |
|   155 | 1239 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1240 | `			nSlot++;` |
|    77 | 1241 | `		}` |
|     - | 1242 | `		/* Point to the next entry */` |
|   155 | 1243 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1244 | `	}` |
|     - | 1245 | `	/* Try to invoke the callback */` |
|    35 | 1246 | `	if( aNames ){` |
|     - | 1247 | `		VmCallArgMap sMap;` |
|    13 | 1248 | `		sMap.bHasNamed = 1;` |
|    13 | 1249 | `		sMap.bIsNamespaced = 0;` |
|     - | 1250 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1251 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1252 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1253 | `		sMap.nTotal = nSlot;` |
|    13 | 1254 | `		sMap.aNames = aNames;` |
|    19 | 1255 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1256 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1257 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1258 | `	}else{` |
|    34 | 1259 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1260 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1261 | `	}` |
|    35 | 1262 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1263 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1264 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1265 | `		SySetRelease(&aArg);` |
|     5 | 1266 | `		return PH7_EXCEPTION;` |
|     - | 1267 | `	}` |
|    31 | 1268 | `	if( rc != SXRET_OK ){` |
|     - | 1269 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1270 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1271 | `	}else{` |
|     - | 1272 | `		/* Callback result */` |
|    31 | 1273 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1274 | `	}` |
|     - | 1275 | `	/* Cleanup the mess left behind */` |
|    31 | 1276 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1277 | `	SySetRelease(&aArg);` |
|    31 | 1278 | `	return PH7_OK;` |
|    19 | 1279 | `}` |
|     - | 1280 |  |
