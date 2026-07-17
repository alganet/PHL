# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 506/584 lines (86.64%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   520 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   525 |   11 | `	if( nArg < 1 ){` |
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
|   525 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   525 |   25 | `		if( pClass ){` |
|   523 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   523 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   264 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   525 |   34 | `	return PH7_OK;` |
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
|  2002 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  2007 |  115 | `	ph7_class *pClass = 0;` |
|  2007 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   627 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1694 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|  1380 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|  1380 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|  1380 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|  1380 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|  1360 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   679 |  131 | `			}` |
|   689 |  132 | `		}` |
|   689 |  133 | `	}` |
|  2007 |  134 | `	return pClass;` |
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
|    14 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  148 | `{` |
|    15 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|    15 |  150 | `	if( nArg > 1 ){` |
|     - |  151 | `		ph7_class *pClass;` |
|    15 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 |  153 | `		if( pClass ){` |
|     - |  154 | `			const char *zName;` |
|     - |  155 | `			int nLen;` |
|     - |  156 | `			/* Extract attribute name */` |
|    15 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    15 |  158 | `			if( nLen > 0 ){` |
|     - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|    14 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    10 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  162 | `						/* property exists,flag that */` |
|    11 |  163 | `						res = 1;` |
|     5 |  164 | `				}` |
|     - |  165 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  166 | `				 * attribute table, not the class's — php reports those too` |
|     - |  167 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    15 |  168 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  169 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  170 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  171 | `						res = 1;` |
|   ! 0 |  172 | `					}` |
|     1 |  173 | `				}` |
|     7 |  174 | `			}` |
|     7 |  175 | `		}` |
|     7 |  176 | `	}` |
|    15 |  177 | `	ph7_result_bool(pCtx,res);` |
|    15 |  178 | `	return PH7_OK;` |
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
|    66 |  226 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  227 | `{` |
|    70 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    70 |  229 | `	if( nArg > 0 ){` |
|    70 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    70 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    70 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    70 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    70 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    70 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    33 |  242 | `		}` |
|    70 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    20 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    20 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     8 |  249 | `		}` |
|    70 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    56 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    56 |  254 | `			while( pClass ){` |
|    56 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    56 |  256 | `					res = 1;` |
|    56 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    26 |  261 | `		}` |
|    33 |  262 | `	}` |
|    70 |  263 | `	ph7_result_bool(pCtx,res);` |
|    70 |  264 | `	return PH7_OK;` |
|     4 |  265 | `}` |
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
|    24 |  278 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  279 | `{` |
|    25 |  280 | `	int res = 0; /* Assume interface does not exist */` |
|    25 |  281 | `	if( nArg > 0 ){` |
|    25 |  282 | `		SyHashEntry *pEntry = 0;` |
|     - |  283 | `		const char *zName;` |
|     - |  284 | `		int nLen;` |
|    25 |  285 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  286 | `		/* Extract given name */` |
|    25 |  287 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    25 |  288 | `		if( nArg >= 2 ){` |
|   ! 0 |  289 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  290 | `		}` |
|     - |  291 | `		/* Perform a hash lookup */` |
|    25 |  292 | `		if( nLen > 0 ){` |
|    25 |  293 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    12 |  294 | `		}` |
|    25 |  295 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  296 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  297 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  298 | `			if( pClass ){` |
|   ! 0 |  299 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  300 | `			}` |
|     1 |  301 | `		}` |
|    25 |  302 | `		if( pEntry ){` |
|    23 |  303 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    23 |  304 | `			while( pClass ){` |
|    23 |  305 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  306 | `					/* interface is available */` |
|    23 |  307 | `					res = 1;` |
|    23 |  308 | `					break;` |
|     - |  309 | `				}` |
|     - |  310 | `				/* Next with the same name */` |
|   ! 0 |  311 | `				pClass = pClass->pNextName;` |
|   ! 0 |  312 | `			}` |
|    11 |  313 | `		}` |
|    12 |  314 | `	}` |
|    25 |  315 | `	ph7_result_bool(pCtx,res);` |
|    25 |  316 | `	return PH7_OK;` |
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
|   184 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   181 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   181 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   155 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   155 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   155 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|    77 |  411 | `		}` |
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
|   186 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   183 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   183 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    29 |  448 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  449 | `			/* insert interface name */` |
|    29 |  450 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  451 | `			/* Reset the cursor */` |
|    29 |  452 | `			ph7_value_reset_string_cursor(pName);` |
|    14 |  453 | `		}` |
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
| 22974 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 22979 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 18195 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 18207 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    13 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 18195 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 18195 |  598 | `		if( pFrame->pBoundScope ){` |
|    11 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 18190 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 18153 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|  9079 |  602 | `		}else{` |
|    34 |  603 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  604 | `		}` |
| 18163 |  605 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  606 | `			/* Must be the same instance or a trait used by the class */` |
|   591 |  607 | `			ph7_class *pCaller = pCallerScope;` |
|   591 |  608 | `			if( pCaller != pClass ){` |
|     - |  609 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  610 | `				ph7_class **apTrait;` |
|     - |  611 | `				sxu32 nTrait,k;` |
|    12 |  612 | `				int iFound = 0;` |
|    12 |  613 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    12 |  614 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    20 |  615 | `				for(k = 0; k < nTrait; k++){` |
|    17 |  616 | `					if( apTrait[k] == pCaller ){` |
|     9 |  617 | `						iFound = 1;` |
|     9 |  618 | `						break;` |
|     - |  619 | `					}` |
|     5 |  620 | `				}` |
|    12 |  621 | `				if( !iFound ){` |
|     3 |  622 | `					goto dis; /* Access is forbidden */` |
|     - |  623 | `				}` |
|     4 |  624 | `			}` |
|   297 |  625 | `		}else{` |
|     - |  626 | `			/* Protected */` |
| 17577 |  627 | `			ph7_class *pBase = pCallerScope;` |
|     - |  628 | `			/* Must be in the same class hierarchy */` |
| 17577 |  629 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|   ! 0 |  630 | `				goto dis; /* Access is forbidden */` |
|     - |  631 | `			}` |
|     - |  632 | `		}` |
|  9078 |  633 | `	}` |
| 22945 |  634 | `	return 1; /* Access is granted */` |
|    17 |  635 | `dis:` |
|    36 |  636 | `	if( bLog ){` |
|   ! 0 |  637 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  638 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  639 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  640 | `	}` |
|    36 |  641 | `	return 0; /* Access is forbidden */` |
| 11492 |  642 | `}` |
|     - |  643 | `/*` |
|     - |  644 | ` * array get_class_vars(string/object $class_name)` |
|     - |  645 | ` *   Get the default properties of the class` |
|     - |  646 | ` * Parameters` |
|     - |  647 | ` *  class_name` |
|     - |  648 | ` *   The class name or class instance` |
|     - |  649 | ` * Return` |
|     - |  650 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  651 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  652 | ` *  of varname => value.` |
|     - |  653 | ` * Note:` |
|     - |  654 | ` *   NULL is returned on failure.` |
|     - |  655 | ` */` |
|     2 |  656 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  657 | `{` |
|     - |  658 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  659 | `	SyHashEntry *pEntry;` |
|     - |  660 | `	ph7_class *pClass;` |
|     - |  661 | `	/* Extract the target class first */` |
|     3 |  662 | `	pClass = 0;` |
|     3 |  663 | `	if( nArg > 0 ){` |
|     3 |  664 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     1 |  665 | `	}` |
|     3 |  666 | `	if( pClass == 0 ){` |
|     - |  667 | `		/* No such class,return NULL */` |
|   ! 0 |  668 | `		ph7_result_null(pCtx);` |
|   ! 0 |  669 | `		return PH7_OK;` |
|     - |  670 | `	}` |
|     - |  671 | `	/* Create a new array  */` |
|     3 |  672 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  673 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  674 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     3 |  675 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  676 | `		/* Out of memory,return NULL */` |
|   ! 0 |  677 | `		ph7_result_null(pCtx);` |
|   ! 0 |  678 | `		return PH7_OK;` |
|     - |  679 | `	}` |
|     - |  680 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     3 |  681 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8 |  682 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     5 |  683 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     - |  684 | `		/* Check if the access is allowed */` |
|     5 |  685 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     5 |  686 | `			SyString *pAttrName = &pAttr->sName;` |
|     5 |  687 | `			ph7_value *pValue = 0;` |
|     5 |  688 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  689 | `				/* Extract static attribute value which is always computed */` |
|     5 |  690 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  691 | `			}else{` |
|   ! 0 |  692 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|   ! 0 |  693 | `					PH7_MemObjRelease(&sValue);` |
|     - |  694 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|   ! 0 |  695 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|   ! 0 |  696 | `					pValue = &sValue;` |
|   ! 0 |  697 | `				}` |
|     - |  698 | `			}` |
|     - |  699 | `			/* Fill in the array */` |
|     5 |  700 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     5 |  701 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  702 | `			/* Reset the cursor */` |
|     5 |  703 | `			ph7_value_reset_string_cursor(pName);` |
|     2 |  704 | `		}` |
|     1 |  705 | `	}` |
|     3 |  706 | `	PH7_MemObjRelease(&sValue);` |
|     - |  707 | `	/* Return the created array */` |
|     3 |  708 | `	ph7_result_value(pCtx,pArray);` |
|     - |  709 | `	/*` |
|     - |  710 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  711 | `	 * automatically as soon we return from this foreign function.` |
|     - |  712 | `	 */` |
|     3 |  713 | `	return PH7_OK;` |
|     2 |  714 | `}` |
|     - |  715 | `/*` |
|     - |  716 | ` * array get_object_vars(object $this)` |
|     - |  717 | ` *   Gets the properties of the given object` |
|     - |  718 | ` * Parameters` |
|     - |  719 | ` *  this` |
|     - |  720 | ` *   A class instance` |
|     - |  721 | ` * Return` |
|     - |  722 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  723 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  724 | ` *  it will be returned with a NULL value.` |
|     - |  725 | ` * Note:` |
|     - |  726 | ` *   NULL is returned on failure.` |
|     - |  727 | ` */` |
|    14 |  728 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  729 | `{` |
|    15 |  730 | `	ph7_class_instance *pThis = 0;` |
|     - |  731 | `	ph7_value *pName,*pArray;` |
|     - |  732 | `	SyHashEntry *pEntry;` |
|    15 |  733 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  734 | `		/* Extract the target instance */` |
|    15 |  735 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  736 | `	}` |
|    15 |  737 | `	if( pThis == 0 ){` |
|     - |  738 | `		/* No such instance,return NULL */` |
|   ! 0 |  739 | `		ph7_result_null(pCtx);` |
|   ! 0 |  740 | `		return PH7_OK;` |
|     - |  741 | `	}` |
|     - |  742 | `	/* Create a new array  */` |
|    15 |  743 | `	pArray = ph7_context_new_array(pCtx);` |
|    15 |  744 | `	pName = ph7_context_new_scalar(pCtx);` |
|    15 |  745 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  746 | `		/* Out of memory,return NULL */` |
|   ! 0 |  747 | `		ph7_result_null(pCtx);` |
|   ! 0 |  748 | `		return PH7_OK;` |
|     - |  749 | `	}` |
|     - |  750 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|    15 |  751 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    51 |  752 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    37 |  753 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  754 | `		SyString *pAttrName;` |
|    37 |  755 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  756 | `			/* Only non-static/constant attributes are extracted */` |
|     3 |  757 | `			continue;` |
|     - |  758 | `		}` |
|    35 |  759 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|     - |  760 | `		/* Check if the access is allowed */` |
|    35 |  761 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    27 |  762 | `			ph7_value *pValue = 0;` |
|     - |  763 | `			/* Extract attribute */` |
|    27 |  764 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    27 |  765 | `			if( pValue ){` |
|     - |  766 | `				/* Insert attribute name in the array */` |
|    27 |  767 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    27 |  768 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    13 |  769 | `			}` |
|     - |  770 | `			/* Reset the cursor */` |
|    27 |  771 | `			ph7_value_reset_string_cursor(pName);` |
|    13 |  772 | `		}` |
|     1 |  773 | `	}` |
|     - |  774 | `	/* Return the created array */` |
|    15 |  775 | `	ph7_result_value(pCtx,pArray);` |
|     - |  776 | `	/*` |
|     - |  777 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  778 | `	 * automatically as soon we return from this foreign function.` |
|     - |  779 | `	 */` |
|    15 |  780 | `	return PH7_OK;` |
|     8 |  781 | `}` |
|     - |  782 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  783 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  784 | ` * detection should reject them up front. */` |
|     - |  785 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  786 | `/*` |
|     - |  787 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  788 | ` * interface.Otherwise FALSE is returned.` |
|     - |  789 | ` */` |
| 17054 |  790 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  791 | `{` |
|     - |  792 | `	ph7_class **apInterface;` |
|     - |  793 | `	sxu32 n;` |
| 17059 |  794 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  795 | `		/* Empty interface container */` |
|   243 |  796 | `		return FALSE;` |
|     - |  797 | `	}` |
|     - |  798 | `	/* Point to the set of implemented interfaces */` |
| 16821 |  799 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  800 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  801 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 32303 |  802 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 16943 |  803 | `		ph7_class *pIface = apInterface[n];` |
| 16943 |  804 | `		int iDepth = 0;` |
| 32517 |  805 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 17035 |  806 | `			if( pIface == pClass ){` |
|  1461 |  807 | `				return TRUE;` |
|     - |  808 | `			}` |
| 15579 |  809 | `			pIface = pIface->pBase;` |
| 15579 |  810 | `			iDepth++;` |
|     5 |  811 | `		}` |
|  7746 |  812 | `	}` |
| 15365 |  813 | `	return FALSE;` |
|  8532 |  814 | `}` |
|     - |  815 | `/*` |
|     - |  816 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  817 | ` * is an instance of the main class (second argument).` |
|     - |  818 | ` * Otherwise FALSE is returned.` |
|     - |  819 | ` */` |
| 21052 |  820 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  821 | `{` |
|     - |  822 | `	ph7_class *pParent;` |
|     - |  823 | `	sxi32 rc;` |
| 21057 |  824 | `	if( pThis == pClass ){` |
|     - |  825 | `		/* Instance of the same class */` |
|  8353 |  826 | `		return TRUE;` |
|     - |  827 | `	}` |
|     - |  828 | `	/* Check implemented interfaces */` |
| 12709 |  829 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 12709 |  830 | `	if( rc ){` |
|   945 |  831 | `		return TRUE;` |
|     - |  832 | `	}` |
|     - |  833 | `	/* Check parent classes */` |
| 11769 |  834 | `	pParent = pThis->pBase;` |
| 15595 |  835 | `	while( pParent ){` |
| 15347 |  836 | `		if( pParent == pClass ){` |
|     - |  837 | `			/* Same instance */` |
| 11005 |  838 | `			return TRUE;` |
|     - |  839 | `		}` |
|     - |  840 | `		/* Check the implemented interfaces */` |
|  4347 |  841 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4347 |  842 | `		if( rc ){` |
|   521 |  843 | `			return TRUE;` |
|     - |  844 | `		}` |
|     - |  845 | `		/* Point to the parent class */` |
|  3831 |  846 | `		pParent = pParent->pBase;` |
|     5 |  847 | `	}` |
|     - |  848 | `	/* Not an instance of the the given class */` |
|   253 |  849 | `	return FALSE;` |
| 10531 |  850 | `}` |
|     - |  851 | `/*` |
|     - |  852 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  853 | ` * is a subclass of the main class (second argument).` |
|     - |  854 | ` * Otherwise FALSE is returned.` |
|     - |  855 | ` */` |
|    12 |  856 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  857 | `{` |
|    13 |  858 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  859 | `	SyHashEntry *pEntry;` |
|     - |  860 | `	SyString *pName;` |
|     - |  861 | `	sxi32 rc;` |
|    21 |  862 | `	while( pClass ){` |
|    13 |  863 | `		pName = &pClass->sName;` |
|     - |  864 | `		/* Query the derived hashtable */` |
|    13 |  865 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    13 |  866 | `		if( pEntry ){` |
|     5 |  867 | `			return TRUE;` |
|     - |  868 | `		}` |
|     9 |  869 | `		pClass = pClass->pBase;` |
|     1 |  870 | `	}` |
|     9 |  871 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|     9 |  872 | `	if( rc ){` |
|   ! 0 |  873 | `		return TRUE;` |
|     - |  874 | `	}` |
|     - |  875 | `	/* Not a subclass */` |
|     9 |  876 | `	return FALSE;` |
|     7 |  877 | `}` |
|     - |  878 | `/*` |
|     - |  879 | ` * bool is_a(object $object,string $class_name)` |
|     - |  880 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  881 | ` * Parameters` |
|     - |  882 | ` *  object` |
|     - |  883 | ` *   The tested object` |
|     - |  884 | ` * class_name` |
|     - |  885 | ` *  The class name` |
|     - |  886 | ` * Return` |
|     - |  887 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  888 | ` *   parents, FALSE otherwise.` |
|     - |  889 | ` */` |
|    18 |  890 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  891 | `{` |
|    19 |  892 | `	int res = 0; /* Assume FALSE by default */` |
|    19 |  893 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 |  894 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - |  895 | `		ph7_class *pClass;` |
|     - |  896 | `		/* Extract the given class */` |
|    19 |  897 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 |  898 | `		if( pClass ){` |
|     - |  899 | `			/* Perform the query */` |
|    19 |  900 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 |  901 | `		}` |
|     9 |  902 | `	}` |
|     - |  903 | `	/* Query result */` |
|    19 |  904 | `	ph7_result_bool(pCtx,res);` |
|    19 |  905 | `	return PH7_OK;` |
|     1 |  906 | `}` |
|     - |  907 | `/*` |
|     - |  908 | ` * int spl_object_id(object $object)` |
|     - |  909 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - |  910 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - |  911 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - |  912 | ` */` |
|    18 |  913 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  914 | `{` |
|     - |  915 | `	ph7_class_instance *pThis;` |
|    21 |  916 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  917 | `		ph7_result_null(pCtx);` |
|   ! 0 |  918 | `		return PH7_OK;` |
|     - |  919 | `	}` |
|    21 |  920 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  921 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    21 |  922 | `	return PH7_OK;` |
|    12 |  923 | `}` |
|     - |  924 | `/*` |
|     - |  925 | ` * string spl_object_hash(object $object)` |
|     - |  926 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - |  927 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - |  928 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - |  929 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - |  930 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - |  931 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - |  932 | ` */` |
|    10 |  933 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  934 | `{` |
|     - |  935 | `	ph7_class_instance *pThis;` |
|    11 |  936 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  937 | `		ph7_result_null(pCtx);` |
|   ! 0 |  938 | `		return PH7_OK;` |
|     - |  939 | `	}` |
|    11 |  940 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 |  941 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    11 |  942 | `	return PH7_OK;` |
|     6 |  943 | `}` |
|     - |  944 | `/*` |
|     - |  945 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - |  946 | ` *   Checks if the object has this class as one of its parents.` |
|     - |  947 | ` * Parameters` |
|     - |  948 | ` *  object` |
|     - |  949 | ` *   The tested object` |
|     - |  950 | ` * class_name` |
|     - |  951 | ` *  The class name` |
|     - |  952 | ` * Return` |
|     - |  953 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - |  954 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - |  955 | ` */` |
|    14 |  956 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  957 | `{` |
|    15 |  958 | `	int res = 0; /* Assume FALSE by default */` |
|    15 |  959 | `	if( nArg > 1 ){` |
|     - |  960 | `		ph7_class *pClass,*pMain;` |
|     - |  961 | `		/* Extract the given classes */` |
|    15 |  962 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 |  963 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    15 |  964 | `		if( pClass && pMain ){` |
|     - |  965 | `			/* Perform the query */` |
|    13 |  966 | `			res = VmSubclassOf(pClass,pMain);` |
|     6 |  967 | `		}` |
|     7 |  968 | `	}` |
|     - |  969 | `	/* Query result */` |
|    15 |  970 | `	ph7_result_bool(pCtx,res);` |
|    15 |  971 | `	return PH7_OK;` |
|     1 |  972 | `}` |
|    44 |  973 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  974 | `{` |
|     - |  975 | `	ph7_value sResult; /* Store callback return value here */` |
|     - |  976 | `	sxi32 rc;` |
|    45 |  977 | `	if( nArg < 1 ){` |
|     - |  978 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  979 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  980 | `		return PH7_OK;` |
|     - |  981 | `	}` |
|    45 |  982 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    45 |  983 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  984 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - |  985 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - |  986 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - |  987 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - |  988 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - |  989 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    53 |  990 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    17 |  991 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - |  992 | `		VmCallArgMap sInner;` |
|    17 |  993 | `		sInner.bHasNamed = 1;` |
|    17 |  994 | `		sInner.bIsNamespaced = 0;` |
|     - |  995 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - |  996 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - |  997 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - |  998 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    17 |  999 | `		sInner.bStrict = 0;` |
|    17 | 1000 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    17 | 1001 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    17 | 1002 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|     9 | 1003 | `	}else{` |
|    29 | 1004 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1005 | `	}` |
|    45 | 1006 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1007 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1008 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     5 | 1009 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1010 | `		return PH7_EXCEPTION;` |
|     - | 1011 | `	}` |
|    41 | 1012 | `	if( rc != SXRET_OK ){` |
|     - | 1013 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1014 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1015 | `	}else{` |
|     - | 1016 | `		/* Callback result */` |
|    41 | 1017 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1018 | `	}` |
|    41 | 1019 | `	PH7_MemObjRelease(&sResult);` |
|    41 | 1020 | `	return PH7_OK;` |
|    23 | 1021 | `}` |
|     - | 1022 | `/*` |
|     - | 1023 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1024 | ` *  Call a callback with an array of parameters.` |
|     - | 1025 | ` * Parameter` |
|     - | 1026 | ` *  $callback` |
|     - | 1027 | ` *   The callable to be called.` |
|     - | 1028 | ` * $param_arr` |
|     - | 1029 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1030 | ` * Return` |
|     - | 1031 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1032 | ` */` |
|    28 | 1033 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1034 | `{` |
|     - | 1035 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1036 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1037 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1038 | `	SySet aArg;               /* Argument value pointers */` |
|    29 | 1039 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    29 | 1040 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1041 | `	sxi32 rc;` |
|     - | 1042 | `	sxu32 n;` |
|    29 | 1043 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1044 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1045 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1046 | `		return PH7_OK;` |
|     - | 1047 | `	}` |
|    29 | 1048 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    29 | 1049 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1050 | `	/* Initialize the arguments container */` |
|    29 | 1051 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1052 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1053 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1054 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1055 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1056 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1057 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    29 | 1058 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    29 | 1059 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|    77 | 1060 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1061 | `		/* Extract node value */` |
|    49 | 1062 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|    49 | 1063 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1064 | `				if( aNames == 0 ){` |
|     - | 1065 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1066 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1067 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1068 | `					if( aNames == 0 ){` |
|   ! 0 | 1069 | `						SySetRelease(&aArg);` |
|   ! 0 | 1070 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1071 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1072 | `					}` |
|    13 | 1073 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1074 | `				}` |
|    23 | 1075 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1076 | `			}` |
|    49 | 1077 | `			SySetPut(&aArg,(const void *)&pValue);` |
|    49 | 1078 | `			nSlot++;` |
|    24 | 1079 | `		}` |
|     - | 1080 | `		/* Point to the next entry */` |
|    49 | 1081 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    25 | 1082 | `	}` |
|     - | 1083 | `	/* Try to invoke the callback */` |
|    29 | 1084 | `	if( aNames ){` |
|     - | 1085 | `		VmCallArgMap sMap;` |
|    13 | 1086 | `		sMap.bHasNamed = 1;` |
|    13 | 1087 | `		sMap.bIsNamespaced = 0;` |
|     - | 1088 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1089 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1090 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1091 | `		sMap.nTotal = nSlot;` |
|    13 | 1092 | `		sMap.aNames = aNames;` |
|    19 | 1093 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1094 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1095 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1096 | `	}else{` |
|    25 | 1097 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    16 | 1098 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1099 | `	}` |
|    29 | 1100 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1101 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1102 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1103 | `		SySetRelease(&aArg);` |
|     5 | 1104 | `		return PH7_EXCEPTION;` |
|     - | 1105 | `	}` |
|    25 | 1106 | `	if( rc != SXRET_OK ){` |
|     - | 1107 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1108 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1109 | `	}else{` |
|     - | 1110 | `		/* Callback result */` |
|    25 | 1111 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1112 | `	}` |
|     - | 1113 | `	/* Cleanup the mess left behind */` |
|    25 | 1114 | `	PH7_MemObjRelease(&sResult);` |
|    25 | 1115 | `	SySetRelease(&aArg);` |
|    25 | 1116 | `	return PH7_OK;` |
|    15 | 1117 | `}` |
|     - | 1118 |  |
