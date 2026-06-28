# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 429/507 lines (84.62%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   144 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 |  |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   149 |   11 | `	if( nArg < 1 ){` |
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
|   149 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   149 |   25 | `		if( pClass ){` |
|   147 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   147 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    76 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   149 |   34 | `	return PH7_OK;` |
|     5 |   35 |  |
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
|    34 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   49 |  |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    35 |   52 | `	if( nArg < 1 ){` |
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
|    33 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    33 |   66 | `		if( pClass ){` |
|    33 |   67 | `			if( pClass->pBase ){` |
|    31 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    31 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    16 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    17 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    35 |   80 | `	return PH7_OK;` |
|     1 |   81 |  |
|     - |   82 | `/*` |
|     - |   83 | ` * string get_called_class(void)` |
|     - |   84 | ` *   Gets the name of the class the static method is called in.` |
|     - |   85 | ` * Parameters` |
|     - |   86 | ` *  None.` |
|     - |   87 | ` * Return` |
|     - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|     - |   89 | ` */` |
|     4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   91 |  |
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
|     1 |  107 |  |
|     - |  108 | `/*` |
|     - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|     - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|     - |  111 | ` * string which hold the class name.` |
|     - |  112 | ` */` |
|   264 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 |  |
|   269 |  115 | `	ph7_class *pClass = 0;` |
|   269 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   191 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|   174 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|    78 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|    78 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|    78 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|    78 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|    69 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|    34 |  131 | `			}` |
|    38 |  132 | `		}` |
|    38 |  133 | `	}` |
|   269 |  134 | `	return pClass;` |
|     5 |  135 |  |
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
|    12 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  148 |  |
|    13 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|    13 |  150 | `	if( nArg > 1 ){` |
|     - |  151 | `		ph7_class *pClass;` |
|    13 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    13 |  153 | `		if( pClass ){` |
|     - |  154 | `			const char *zName;` |
|     - |  155 | `			int nLen;` |
|     - |  156 | `			/* Extract attribute name */` |
|    13 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    13 |  158 | `			if( nLen > 0 ){` |
|     - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|    12 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|     8 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  162 | `						/* property exists,flag that */` |
|    11 |  163 | `						res = 1;` |
|     5 |  164 | `				}` |
|     6 |  165 | `			}` |
|     6 |  166 | `		}` |
|     6 |  167 | `	}` |
|    13 |  168 | `	ph7_result_bool(pCtx,res);` |
|    13 |  169 | `	return PH7_OK;` |
|     1 |  170 |  |
|     - |  171 | `/*` |
|     - |  172 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  173 | ` *   Checks if the given method is a class member.` |
|     - |  174 | ` * Parameters` |
|     - |  175 | ` *  class` |
|     - |  176 | ` *   The class name or an object of the class to test for` |
|     - |  177 | ` * property` |
|     - |  178 | ` *  The name of the method` |
|     - |  179 | ` * Return` |
|     - |  180 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  181 | ` */` |
|     4 |  182 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  183 |  |
|     5 |  184 | `	int res = 0; /* Assume method does not exists */` |
|     5 |  185 | `	if( nArg > 1 ){` |
|     - |  186 | `		ph7_class *pClass;` |
|     5 |  187 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     5 |  188 | `		if( pClass ){` |
|     - |  189 | `			const char *zName;` |
|     - |  190 | `			int nLen;` |
|     - |  191 | `			/* Extract method name */` |
|     5 |  192 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     5 |  193 | `			if( nLen > 0 ){` |
|     - |  194 | `				/* Perform the lookup in the method table */` |
|     5 |  195 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  196 | `					/* method exists,flag that */` |
|     3 |  197 | `					res = 1;` |
|     1 |  198 | `				}` |
|     2 |  199 | `			}` |
|     2 |  200 | `		}` |
|     2 |  201 | `	}` |
|     5 |  202 | `	ph7_result_bool(pCtx,res);` |
|     5 |  203 | `	return PH7_OK;` |
|     1 |  204 |  |
|     - |  205 | `/*` |
|     - |  206 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  207 | ` *   Checks if the class has been defined.` |
|     - |  208 | ` * Parameters` |
|     - |  209 | ` *  class_name` |
|     - |  210 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  211 | ` *   unlinke the standard PHP engine.` |
|     - |  212 | ` *  autoload` |
|     - |  213 | ` *   Whether or not to call __autoload by default.` |
|     - |  214 | ` * Return` |
|     - |  215 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  216 | ` */` |
|    64 |  217 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  218 |  |
|    68 |  219 | `	int res = 0; /* Assume class does not exist */` |
|    68 |  220 | `	if( nArg > 0 ){` |
|    68 |  221 | `		SyHashEntry *pEntry = 0;` |
|     - |  222 | `		const char *zName;` |
|     - |  223 | `		int nLen;` |
|    68 |  224 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  225 | `		/* Extract given name */` |
|    68 |  226 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    68 |  227 | `		if( nArg >= 2 ){` |
|     6 |  228 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  229 | `		}` |
|    68 |  230 | `		if( nLen > 0 ){` |
|     - |  231 | `			/* Perform a hash lookup first */` |
|    68 |  232 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    32 |  233 | `		}` |
|    68 |  234 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  235 | `			/* Try autoload, then re-check */` |
|    20 |  236 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    20 |  237 | `			if( pClass ){` |
|     6 |  238 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  239 | `			}` |
|     8 |  240 | `		}` |
|    68 |  241 | `		if( pEntry ){` |
|     - |  242 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  243 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    54 |  244 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    54 |  245 | `			while( pClass ){` |
|    54 |  246 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    54 |  247 | `					res = 1;` |
|    54 |  248 | `					break;` |
|     - |  249 | `				}` |
|   ! 0 |  250 | `				pClass = pClass->pNextName;` |
|   ! 0 |  251 | `			}` |
|    25 |  252 | `		}` |
|    32 |  253 | `	}` |
|    68 |  254 | `	ph7_result_bool(pCtx,res);` |
|    68 |  255 | `	return PH7_OK;` |
|     4 |  256 |  |
|     - |  257 | `/*` |
|     - |  258 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  259 | ` *   Checks if the interface has been defined.` |
|     - |  260 | ` * Parameters` |
|     - |  261 | ` *  class_name` |
|     - |  262 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  263 | ` *   unlinke the standard PHP engine.` |
|     - |  264 | ` *  autoload` |
|     - |  265 | ` *   Whether or not to call __autoload by default.` |
|     - |  266 | ` * Return` |
|     - |  267 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  268 | ` */` |
|    24 |  269 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  270 |  |
|    25 |  271 | `	int res = 0; /* Assume interface does not exist */` |
|    25 |  272 | `	if( nArg > 0 ){` |
|    25 |  273 | `		SyHashEntry *pEntry = 0;` |
|     - |  274 | `		const char *zName;` |
|     - |  275 | `		int nLen;` |
|    25 |  276 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  277 | `		/* Extract given name */` |
|    25 |  278 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    25 |  279 | `		if( nArg >= 2 ){` |
|   ! 0 |  280 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  281 | `		}` |
|     - |  282 | `		/* Perform a hash lookup */` |
|    25 |  283 | `		if( nLen > 0 ){` |
|    25 |  284 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    12 |  285 | `		}` |
|    25 |  286 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  287 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  288 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  289 | `			if( pClass ){` |
|   ! 0 |  290 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  291 | `			}` |
|     1 |  292 | `		}` |
|    25 |  293 | `		if( pEntry ){` |
|    23 |  294 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    23 |  295 | `			while( pClass ){` |
|    23 |  296 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  297 | `					/* interface is available */` |
|    23 |  298 | `					res = 1;` |
|    23 |  299 | `					break;` |
|     - |  300 | `				}` |
|     - |  301 | `				/* Next with the same name */` |
|   ! 0 |  302 | `				pClass = pClass->pNextName;` |
|   ! 0 |  303 | `			}` |
|    11 |  304 | `		}` |
|    12 |  305 | `	}` |
|    25 |  306 | `	ph7_result_bool(pCtx,res);` |
|    25 |  307 | `	return PH7_OK;` |
|     1 |  308 |  |
|     - |  309 | `/*` |
|     - |  310 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  311 | ` *   Creates an alias for a class.` |
|     - |  312 | ` * Parameters` |
|     - |  313 | ` *  original` |
|     - |  314 | ` *    The original class.` |
|     - |  315 | ` *  alias` |
|     - |  316 | ` *   The alias name for the class.` |
|     - |  317 | ` * Return` |
|     - |  318 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  319 | ` */` |
|     2 |  320 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  321 |  |
|     - |  322 | `	const char *zOld,*zNew;` |
|     - |  323 | `	int nOldLen,nNewLen;` |
|     - |  324 | `	SyHashEntry *pEntry;` |
|     - |  325 | `	ph7_class *pClass;` |
|     - |  326 | `	char *zDup;` |
|     - |  327 | `	sxi32 rc;` |
|     3 |  328 | `	if( nArg < 2 ){` |
|     - |  329 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  330 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  331 | `		return PH7_OK;` |
|     - |  332 | `	}` |
|     - |  333 | `	/* Extract old class name */` |
|     3 |  334 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  335 | `	/* Extract alias name */` |
|     3 |  336 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  337 | `	if( nNewLen < 1 ){` |
|     - |  338 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  340 | `		return PH7_OK;` |
|     - |  341 | `	}` |
|     - |  342 | `	/* Perform a hash lookup */` |
|     3 |  343 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  344 | `	if( pEntry ==  0 ){` |
|     - |  345 | `		/* No such class,return FALSE */` |
|   ! 0 |  346 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  347 | `		return PH7_OK;` |
|     - |  348 | `	}` |
|     - |  349 | `	/* Point to the class */` |
|     3 |  350 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  351 | `	/* Duplicate alias name */` |
|     3 |  352 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  353 | `	if( zDup == 0 ){` |
|     - |  354 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  356 | `		return PH7_OK;` |
|     - |  357 | `	}` |
|     - |  358 | `	/* Create the alias */` |
|     3 |  359 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  360 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  361 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  362 | `	}` |
|     3 |  363 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  364 | `	return PH7_OK;` |
|     2 |  365 |  |
|     - |  366 | `/*` |
|     - |  367 | ` * array get_declared_classes(void)` |
|     - |  368 | ` *   Returns an array with the name of the defined classes` |
|     - |  369 | ` * Parameters` |
|     - |  370 | ` *  None` |
|     - |  371 | ` * Return` |
|     - |  372 | ` *   Returns an array of the names of the declared classes` |
|     - |  373 | ` *   in the current script.` |
|     - |  374 | ` * Note:` |
|     - |  375 | ` *   NULL is returned on failure.` |
|     - |  376 | ` */` |
|     2 |  377 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  378 |  |
|     - |  379 | `	ph7_value *pName,*pArray;` |
|     - |  380 | `	SyHashEntry *pEntry;` |
|     - |  381 | `	/* Create a new array first */` |
|     3 |  382 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  383 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  384 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  385 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  386 | `		SXUNUSED(apArg);` |
|     - |  387 | `		/* Out of memory,return NULL */` |
|   ! 0 |  388 | `		ph7_result_null(pCtx);` |
|   ! 0 |  389 | `		return PH7_OK;` |
|     - |  390 | `	}` |
|     - |  391 | `	/* Fill the array with the defined classes */` |
|     3 |  392 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   110 |  393 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   107 |  394 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  395 | `		/* Do not register classes defined as interfaces */` |
|   107 |  396 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|    85 |  397 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  398 | `			/* insert class name */` |
|    85 |  399 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  400 | `			/* Reset the cursor */` |
|    85 |  401 | `			ph7_value_reset_string_cursor(pName);` |
|    42 |  402 | `		}` |
|     1 |  403 | `	}` |
|     - |  404 | `	/* Return the created array */` |
|     3 |  405 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  406 | `	return PH7_OK;` |
|     2 |  407 |  |
|     - |  408 | `/*` |
|     - |  409 | ` * array get_declared_interfaces(void)` |
|     - |  410 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  411 | ` * Parameters` |
|     - |  412 | ` *  None` |
|     - |  413 | ` * Return` |
|     - |  414 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  415 | ` *   in the current script.` |
|     - |  416 | ` * Note:` |
|     - |  417 | ` *   NULL is returned on failure.` |
|     - |  418 | ` */` |
|     2 |  419 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  420 |  |
|     - |  421 | `	ph7_value *pName,*pArray;` |
|     - |  422 | `	SyHashEntry *pEntry;` |
|     - |  423 | `	/* Create a new array first */` |
|     3 |  424 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  425 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  426 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  427 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  428 | `		SXUNUSED(apArg);` |
|     - |  429 | `		/* Out of memory,return NULL */` |
|   ! 0 |  430 | `		ph7_result_null(pCtx);` |
|   ! 0 |  431 | `		return PH7_OK;` |
|     - |  432 | `	}` |
|     - |  433 | `	/* Fill the array with the defined classes */` |
|     3 |  434 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   112 |  435 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   109 |  436 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  437 | `		/* Register classes defined as interfaces only */` |
|   109 |  438 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    25 |  439 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  440 | `			/* insert interface name */` |
|    25 |  441 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  442 | `			/* Reset the cursor */` |
|    25 |  443 | `			ph7_value_reset_string_cursor(pName);` |
|    12 |  444 | `		}` |
|     1 |  445 | `	}` |
|     - |  446 | `	/* Return the created array */` |
|     3 |  447 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  448 | `	return PH7_OK;` |
|     2 |  449 |  |
|     - |  450 | `/*` |
|     - |  451 | ` * array get_class_methods(string/object $class_name)` |
|     - |  452 | ` *   Returns an array with the name of the class methods` |
|     - |  453 | ` * Parameters` |
|     - |  454 | ` *  class_name` |
|     - |  455 | ` *  The class name or class instance` |
|     - |  456 | ` * Return` |
|     - |  457 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  458 | ` *  In case of an error, it returns NULL.` |
|     - |  459 | ` * Note:` |
|     - |  460 | ` *   NULL is returned on failure.` |
|     - |  461 | ` */` |
|     6 |  462 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  463 |  |
|     - |  464 | `	ph7_value *pName,*pArray;` |
|     - |  465 | `	SyHashEntry *pEntry;` |
|     - |  466 | `	ph7_class *pClass;` |
|     - |  467 | `	/* Extract the target class first */` |
|     7 |  468 | `	pClass = 0;` |
|     7 |  469 | `	if( nArg > 0 ){` |
|     7 |  470 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     3 |  471 | `	}` |
|     7 |  472 | `	if( pClass == 0 ){` |
|     - |  473 | `		/* No such class,return NULL */` |
|     3 |  474 | `		ph7_result_null(pCtx);` |
|     3 |  475 | `		return PH7_OK;` |
|     - |  476 | `	}` |
|     - |  477 | `	/* Create a new array  */` |
|     5 |  478 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  479 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  480 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  481 | `		/* Out of memory,return NULL */` |
|   ! 0 |  482 | `		ph7_result_null(pCtx);` |
|   ! 0 |  483 | `		return PH7_OK;` |
|     - |  484 | `	}` |
|     - |  485 | `	/* Fill the array with the defined methods */` |
|     5 |  486 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    17 |  487 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    13 |  488 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|     - |  489 | `		/* Insert method name */` |
|    13 |  490 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|    13 |  491 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  492 | `		/* Reset the cursor */` |
|    13 |  493 | `		ph7_value_reset_string_cursor(pName);` |
|     1 |  494 | `	}` |
|     - |  495 | `	/* Return the created array */` |
|     5 |  496 | `	ph7_result_value(pCtx,pArray);` |
|     - |  497 | `	/*` |
|     - |  498 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  499 | `	 * automatically as soon we return from this foreign function.` |
|     - |  500 | `	 */` |
|     5 |  501 | `	return PH7_OK;` |
|     4 |  502 |  |
|     - |  503 | `/*` |
|     - |  504 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  505 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  506 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  507 | ` */` |
|  9168 |  508 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  509 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  510 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  511 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  512 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  513 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  514 | `	)` |
|     5 |  515 |  |
|  9173 |  516 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  7431 |  517 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  518 | `		ph7_vm_func *pVmFunc;` |
|  7431 |  519 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  520 | `			/* Safely ignore the exception frame */` |
|   ! 0 |  521 | `			pFrame = pFrame->pParent;` |
|   ! 0 |  522 | `		}` |
|  7431 |  523 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|  7431 |  524 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|    12 |  525 | `			goto dis; /* Access is forbidden */` |
|     - |  526 | `		}` |
|  7421 |  527 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  528 | `			/* Must be the same instance or a trait used by the class */` |
|   477 |  529 | `			ph7_class *pCaller = (ph7_class *)pVmFunc->pUserData;` |
|   477 |  530 | `			if( pCaller != pClass ){` |
|     - |  531 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  532 | `				ph7_class **apTrait;` |
|     - |  533 | `				sxu32 nTrait,k;` |
|    12 |  534 | `				int iFound = 0;` |
|    12 |  535 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    12 |  536 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    20 |  537 | `				for(k = 0; k < nTrait; k++){` |
|    17 |  538 | `					if( apTrait[k] == pCaller ){` |
|     9 |  539 | `						iFound = 1;` |
|     9 |  540 | `						break;` |
|     - |  541 | `					}` |
|     5 |  542 | `				}` |
|    12 |  543 | `				if( !iFound ){` |
|     3 |  544 | `					goto dis; /* Access is forbidden */` |
|     - |  545 | `				}` |
|     4 |  546 | `			}` |
|   240 |  547 | `		}else{` |
|     - |  548 | `			/* Protected */` |
|  6949 |  549 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|     - |  550 | `			/* Must be in the same class hierarchy */` |
|  6949 |  551 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|   ! 0 |  552 | `				goto dis; /* Access is forbidden */` |
|     - |  553 | `			}` |
|     - |  554 | `		}` |
|  3707 |  555 | `	}` |
|  9161 |  556 | `	return 1; /* Access is granted */` |
|     6 |  557 | `dis:` |
|    14 |  558 | `	if( bLog ){` |
|   ! 0 |  559 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  560 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  561 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  562 | `	}` |
|    14 |  563 | `	return 0; /* Access is forbidden */` |
|  4589 |  564 |  |
|     - |  565 | `/*` |
|     - |  566 | ` * array get_class_vars(string/object $class_name)` |
|     - |  567 | ` *   Get the default properties of the class` |
|     - |  568 | ` * Parameters` |
|     - |  569 | ` *  class_name` |
|     - |  570 | ` *   The class name or class instance` |
|     - |  571 | ` * Return` |
|     - |  572 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  573 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  574 | ` *  of varname => value.` |
|     - |  575 | ` * Note:` |
|     - |  576 | ` *   NULL is returned on failure.` |
|     - |  577 | ` */` |
|     2 |  578 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  579 |  |
|     - |  580 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  581 | `	SyHashEntry *pEntry;` |
|     - |  582 | `	ph7_class *pClass;` |
|     - |  583 | `	/* Extract the target class first */` |
|     3 |  584 | `	pClass = 0;` |
|     3 |  585 | `	if( nArg > 0 ){` |
|     3 |  586 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     1 |  587 | `	}` |
|     3 |  588 | `	if( pClass == 0 ){` |
|     - |  589 | `		/* No such class,return NULL */` |
|   ! 0 |  590 | `		ph7_result_null(pCtx);` |
|   ! 0 |  591 | `		return PH7_OK;` |
|     - |  592 | `	}` |
|     - |  593 | `	/* Create a new array  */` |
|     3 |  594 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  595 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  596 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     3 |  597 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  598 | `		/* Out of memory,return NULL */` |
|   ! 0 |  599 | `		ph7_result_null(pCtx);` |
|   ! 0 |  600 | `		return PH7_OK;` |
|     - |  601 | `	}` |
|     - |  602 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     3 |  603 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8 |  604 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     5 |  605 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     - |  606 | `		/* Check if the access is allowed */` |
|     5 |  607 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     5 |  608 | `			SyString *pAttrName = &pAttr->sName;` |
|     5 |  609 | `			ph7_value *pValue = 0;` |
|     5 |  610 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  611 | `				/* Extract static attribute value which is always computed */` |
|     5 |  612 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  613 | `			}else{` |
|   ! 0 |  614 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|   ! 0 |  615 | `					PH7_MemObjRelease(&sValue);` |
|     - |  616 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|   ! 0 |  617 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|   ! 0 |  618 | `					pValue = &sValue;` |
|   ! 0 |  619 | `				}` |
|     - |  620 | `			}` |
|     - |  621 | `			/* Fill in the array */` |
|     5 |  622 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     5 |  623 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  624 | `			/* Reset the cursor */` |
|     5 |  625 | `			ph7_value_reset_string_cursor(pName);` |
|     2 |  626 | `		}` |
|     1 |  627 | `	}` |
|     3 |  628 | `	PH7_MemObjRelease(&sValue);` |
|     - |  629 | `	/* Return the created array */` |
|     3 |  630 | `	ph7_result_value(pCtx,pArray);` |
|     - |  631 | `	/*` |
|     - |  632 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  633 | `	 * automatically as soon we return from this foreign function.` |
|     - |  634 | `	 */` |
|     3 |  635 | `	return PH7_OK;` |
|     2 |  636 |  |
|     - |  637 | `/*` |
|     - |  638 | ` * array get_object_vars(object $this)` |
|     - |  639 | ` *   Gets the properties of the given object` |
|     - |  640 | ` * Parameters` |
|     - |  641 | ` *  this` |
|     - |  642 | ` *   A class instance` |
|     - |  643 | ` * Return` |
|     - |  644 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  645 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  646 | ` *  it will be returned with a NULL value.` |
|     - |  647 | ` * Note:` |
|     - |  648 | ` *   NULL is returned on failure.` |
|     - |  649 | ` */` |
|     4 |  650 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  651 |  |
|     5 |  652 | `	ph7_class_instance *pThis = 0;` |
|     - |  653 | `	ph7_value *pName,*pArray;` |
|     - |  654 | `	SyHashEntry *pEntry;` |
|     5 |  655 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  656 | `		/* Extract the target instance */` |
|     5 |  657 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     2 |  658 | `	}` |
|     5 |  659 | `	if( pThis == 0 ){` |
|     - |  660 | `		/* No such instance,return NULL */` |
|   ! 0 |  661 | `		ph7_result_null(pCtx);` |
|   ! 0 |  662 | `		return PH7_OK;` |
|     - |  663 | `	}` |
|     - |  664 | `	/* Create a new array  */` |
|     5 |  665 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  666 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  667 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  668 | `		/* Out of memory,return NULL */` |
|   ! 0 |  669 | `		ph7_result_null(pCtx);` |
|   ! 0 |  670 | `		return PH7_OK;` |
|     - |  671 | `	}` |
|     - |  672 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  673 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    11 |  674 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     7 |  675 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  676 | `		SyString *pAttrName;` |
|     7 |  677 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  678 | `			/* Only non-static/constant attributes are extracted */` |
|   ! 0 |  679 | `			continue;` |
|     - |  680 | `		}` |
|     7 |  681 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|     - |  682 | `		/* Check if the access is allowed */` |
|     7 |  683 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     3 |  684 | `			ph7_value *pValue = 0;` |
|     - |  685 | `			/* Extract attribute */` |
|     3 |  686 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     3 |  687 | `			if( pValue ){` |
|     - |  688 | `				/* Insert attribute name in the array */` |
|     3 |  689 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     3 |  690 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     1 |  691 | `			}` |
|     - |  692 | `			/* Reset the cursor */` |
|     3 |  693 | `			ph7_value_reset_string_cursor(pName);` |
|     1 |  694 | `		}` |
|     1 |  695 | `	}` |
|     - |  696 | `	/* Return the created array */` |
|     5 |  697 | `	ph7_result_value(pCtx,pArray);` |
|     - |  698 | `	/*` |
|     - |  699 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  700 | `	 * automatically as soon we return from this foreign function.` |
|     - |  701 | `	 */` |
|     5 |  702 | `	return PH7_OK;` |
|     3 |  703 |  |
|     - |  704 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  705 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  706 | ` * detection should reject them up front. */` |
|     - |  707 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  708 | `/*` |
|     - |  709 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  710 | ` * interface.Otherwise FALSE is returned.` |
|     - |  711 | ` */` |
|  7624 |  712 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  713 |  |
|     - |  714 | `	ph7_class **apInterface;` |
|     - |  715 | `	sxu32 n;` |
|  7629 |  716 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  717 | `		/* Empty interface container */` |
|   165 |  718 | `		return FALSE;` |
|     - |  719 | `	}` |
|     - |  720 | `	/* Point to the set of implemented interfaces */` |
|  7469 |  721 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  722 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  723 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 14243 |  724 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|  7571 |  725 | `		ph7_class *pIface = apInterface[n];` |
|  7571 |  726 | `		int iDepth = 0;` |
| 14417 |  727 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|  7643 |  728 | `			if( pIface == pClass ){` |
|   797 |  729 | `				return TRUE;` |
|     - |  730 | `			}` |
|  6851 |  731 | `			pIface = pIface->pBase;` |
|  6851 |  732 | `			iDepth++;` |
|     5 |  733 | `		}` |
|  3392 |  734 | `	}` |
|  6677 |  735 | `	return FALSE;` |
|  3817 |  736 |  |
|     - |  737 | `/*` |
|     - |  738 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  739 | ` * is an instance of the main class (second argument).` |
|     - |  740 | ` * Otherwise FALSE is returned.` |
|     - |  741 | ` */` |
|  8604 |  742 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  743 |  |
|     - |  744 | `	ph7_class *pParent;` |
|     - |  745 | `	sxi32 rc;` |
|  8609 |  746 | `	if( pThis == pClass ){` |
|     - |  747 | `		/* Instance of the same class */` |
|  2735 |  748 | `		return TRUE;` |
|     - |  749 | `	}` |
|     - |  750 | `	/* Check implemented interfaces */` |
|  5879 |  751 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|  5879 |  752 | `	if( rc ){` |
|   685 |  753 | `		return TRUE;` |
|     - |  754 | `	}` |
|     - |  755 | `	/* Check parent classes */` |
|  5199 |  756 | `	pParent = pThis->pBase;` |
|  6837 |  757 | `	while( pParent ){` |
|  6691 |  758 | `		if( pParent == pClass ){` |
|     - |  759 | `			/* Same instance */` |
|  4941 |  760 | `			return TRUE;` |
|     - |  761 | `		}` |
|     - |  762 | `		/* Check the implemented interfaces */` |
|  1755 |  763 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  1755 |  764 | `		if( rc ){` |
|   117 |  765 | `			return TRUE;` |
|     - |  766 | `		}` |
|     - |  767 | `		/* Point to the parent class */` |
|  1643 |  768 | `		pParent = pParent->pBase;` |
|     5 |  769 | `	}` |
|     - |  770 | `	/* Not an instance of the the given class */` |
|   151 |  771 | `	return FALSE;` |
|  4307 |  772 |  |
|     - |  773 | `/*` |
|     - |  774 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  775 | ` * is a subclass of the main class (second argument).` |
|     - |  776 | ` * Otherwise FALSE is returned.` |
|     - |  777 | ` */` |
|     4 |  778 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  779 |  |
|     5 |  780 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  781 | `	SyHashEntry *pEntry;` |
|     - |  782 | `	SyString *pName;` |
|     - |  783 | `	sxi32 rc;` |
|     5 |  784 | `	while( pClass ){` |
|     5 |  785 | `		pName = &pClass->sName;` |
|     - |  786 | `		/* Query the derived hashtable */` |
|     5 |  787 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|     5 |  788 | `		if( pEntry ){` |
|     5 |  789 | `			return TRUE;` |
|     - |  790 | `		}` |
|   ! 0 |  791 | `		pClass = pClass->pBase;` |
|   ! 0 |  792 | `	}` |
|   ! 0 |  793 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|   ! 0 |  794 | `	if( rc ){` |
|   ! 0 |  795 | `		return TRUE;` |
|     - |  796 | `	}` |
|     - |  797 | `	/* Not a subclass */` |
|   ! 0 |  798 | `	return FALSE;` |
|     3 |  799 |  |
|     - |  800 | `/*` |
|     - |  801 | ` * bool is_a(object $object,string $class_name)` |
|     - |  802 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  803 | ` * Parameters` |
|     - |  804 | ` *  object` |
|     - |  805 | ` *   The tested object` |
|     - |  806 | ` * class_name` |
|     - |  807 | ` *  The class name` |
|     - |  808 | ` * Return` |
|     - |  809 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  810 | ` *   parents, FALSE otherwise.` |
|     - |  811 | ` */` |
|     2 |  812 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  813 |  |
|     3 |  814 | `	int res = 0; /* Assume FALSE by default */` |
|     3 |  815 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|     3 |  816 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - |  817 | `		ph7_class *pClass;` |
|     - |  818 | `		/* Extract the given class */` |
|     3 |  819 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|     3 |  820 | `		if( pClass ){` |
|     - |  821 | `			/* Perform the query */` |
|     3 |  822 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     1 |  823 | `		}` |
|     1 |  824 | `	}` |
|     - |  825 | `	/* Query result */` |
|     3 |  826 | `	ph7_result_bool(pCtx,res);` |
|     3 |  827 | `	return PH7_OK;` |
|     1 |  828 |  |
|     - |  829 | `/*` |
|     - |  830 | ` * int spl_object_id(object $object)` |
|     - |  831 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - |  832 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - |  833 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - |  834 | ` */` |
|    18 |  835 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  836 |  |
|     - |  837 | `	ph7_class_instance *pThis;` |
|    21 |  838 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  839 | `		ph7_result_null(pCtx);` |
|   ! 0 |  840 | `		return PH7_OK;` |
|     - |  841 | `	}` |
|    21 |  842 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  843 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    21 |  844 | `	return PH7_OK;` |
|    12 |  845 |  |
|     - |  846 | `/*` |
|     - |  847 | ` * string spl_object_hash(object $object)` |
|     - |  848 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - |  849 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - |  850 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - |  851 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - |  852 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - |  853 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - |  854 | ` */` |
|    10 |  855 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  856 |  |
|     - |  857 | `	ph7_class_instance *pThis;` |
|    11 |  858 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  859 | `		ph7_result_null(pCtx);` |
|   ! 0 |  860 | `		return PH7_OK;` |
|     - |  861 | `	}` |
|    11 |  862 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 |  863 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    11 |  864 | `	return PH7_OK;` |
|     6 |  865 |  |
|     - |  866 | `/*` |
|     - |  867 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - |  868 | ` *   Checks if the object has this class as one of its parents.` |
|     - |  869 | ` * Parameters` |
|     - |  870 | ` *  object` |
|     - |  871 | ` *   The tested object` |
|     - |  872 | ` * class_name` |
|     - |  873 | ` *  The class name` |
|     - |  874 | ` * Return` |
|     - |  875 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - |  876 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - |  877 | ` */` |
|     6 |  878 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  879 |  |
|     7 |  880 | `	int res = 0; /* Assume FALSE by default */` |
|     7 |  881 | `	if( nArg > 1 ){` |
|     - |  882 | `		ph7_class *pClass,*pMain;` |
|     - |  883 | `		/* Extract the given classes */` |
|     7 |  884 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     7 |  885 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|     7 |  886 | `		if( pClass && pMain ){` |
|     - |  887 | `			/* Perform the query */` |
|     5 |  888 | `			res = VmSubclassOf(pClass,pMain);` |
|     2 |  889 | `		}` |
|     3 |  890 | `	}` |
|     - |  891 | `	/* Query result */` |
|     7 |  892 | `	ph7_result_bool(pCtx,res);` |
|     7 |  893 | `	return PH7_OK;` |
|     1 |  894 |  |
|    26 |  895 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  896 |  |
|     - |  897 | `	ph7_value sResult; /* Store callback return value here */` |
|     - |  898 | `	sxi32 rc;` |
|    27 |  899 | `	if( nArg < 1 ){` |
|     - |  900 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  901 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  902 | `		return PH7_OK;` |
|     - |  903 | `	}` |
|    27 |  904 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    27 |  905 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  906 | `	/* Try to invoke the callback */` |
|    27 |  907 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|    27 |  908 | `	if( rc == PH7_EXCEPTION ){` |
|     - |  909 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - |  910 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     5 |  911 | `		PH7_MemObjRelease(&sResult);` |
|     5 |  912 | `		return PH7_EXCEPTION;` |
|     - |  913 | `	}` |
|    23 |  914 | `	if( rc != SXRET_OK ){` |
|     - |  915 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 |  916 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 |  917 | `	}else{` |
|     - |  918 | `		/* Callback result */` |
|    23 |  919 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - |  920 | `	}` |
|    23 |  921 | `	PH7_MemObjRelease(&sResult);` |
|    23 |  922 | `	return PH7_OK;` |
|    14 |  923 |  |
|     - |  924 | `/*` |
|     - |  925 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - |  926 | ` *  Call a callback with an array of parameters.` |
|     - |  927 | ` * Parameter` |
|     - |  928 | ` *  $callback` |
|     - |  929 | ` *   The callable to be called.` |
|     - |  930 | ` * $param_arr` |
|     - |  931 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - |  932 | ` * Return` |
|     - |  933 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - |  934 | ` */` |
|    14 |  935 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  936 |  |
|     - |  937 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - |  938 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - |  939 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - |  940 | `	SySet aArg;               /* Arguments containers */` |
|     - |  941 | `	sxi32 rc;` |
|     - |  942 | `	sxu32 n;` |
|    15 |  943 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - |  944 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 |  945 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  946 | `		return PH7_OK;` |
|     - |  947 | `	}` |
|    15 |  948 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    15 |  949 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  950 | `	/* Initialize the arguments container */` |
|    15 |  951 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - |  952 | `	/* Turn hashmap entries into callback arguments */` |
|    15 |  953 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    15 |  954 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|    35 |  955 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - |  956 | `		/* Extract node value */` |
|    21 |  957 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|    21 |  958 | `			SySetPut(&aArg,(const void *)&pValue);` |
|    10 |  959 | `		}` |
|     - |  960 | `		/* Point to the next entry */` |
|    21 |  961 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    11 |  962 | `	}` |
|     - |  963 | `	/* Try to invoke the callback */` |
|    15 |  964 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|    15 |  965 | `	if( rc == PH7_EXCEPTION ){` |
|     - |  966 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     3 |  967 | `		PH7_MemObjRelease(&sResult);` |
|     3 |  968 | `		SySetRelease(&aArg);` |
|     3 |  969 | `		return PH7_EXCEPTION;` |
|     - |  970 | `	}` |
|    13 |  971 | `	if( rc != SXRET_OK ){` |
|     - |  972 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 |  973 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 |  974 | `	}else{` |
|     - |  975 | `		/* Callback result */` |
|    13 |  976 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - |  977 | `	}` |
|     - |  978 | `	/* Cleanup the mess left behind */` |
|    13 |  979 | `	PH7_MemObjRelease(&sResult);` |
|    13 |  980 | `	SySetRelease(&aArg);` |
|    13 |  981 | `	return PH7_OK;` |
|     8 |  982 |  |
|     - |  983 |  |
