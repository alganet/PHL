# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 376/446 lines (84.30%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|   18 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |    8 |  |
|    - |    9 | `	ph7_class *pClass;` |
|    - |   10 | `	SyString *pName;` |
|   20 |   11 | `	if( nArg < 1 ){` |
|    - |   12 | `		/* Check if we are inside a class */` |
|  ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|  ! 0 |   14 | `		if( pClass ){` |
|    - |   15 | `			/* Point to the class name */` |
|  ! 0 |   16 | `			pName = &pClass->sName;` |
|  ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|  ! 0 |   18 | `		}else{` |
|    - |   19 | `			/* Not inside class,return FALSE */` |
|  ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|    - |   21 | `		}` |
|  ! 0 |   22 | `	}else{` |
|    - |   23 | `		/* Extract the target class */` |
|   20 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   20 |   25 | `		if( pClass ){` |
|   18 |   26 | `			pName = &pClass->sName;` |
|    - |   27 | `			/* Return the class name */` |
|   18 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   10 |   29 | `		}else{` |
|    - |   30 | `			/* Not a class instance,return FALSE */` |
|    3 |   31 | `			ph7_result_bool(pCtx,0);` |
|    - |   32 | `		}` |
|    - |   33 | `	}` |
|   20 |   34 | `	return PH7_OK;` |
|    2 |   35 |  |
|    - |   36 | `/*` |
|    - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|    - |   38 | ` *   Returns the name of the parent class of an object` |
|    - |   39 | ` * Parameters` |
|    - |   40 | ` *  object` |
|    - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|    - |   42 | ` * Return` |
|    - |   43 | ` *  The name of the parent class of which object is an instance.` |
|    - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|    - |   45 | ` *  not have a parent.` |
|    - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|    - |   47 | ` */` |
|    8 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   49 |  |
|    - |   50 | `	ph7_class *pClass;` |
|    - |   51 | `	SyString *pName;` |
|    9 |   52 | `	if( nArg < 1 ){` |
|    - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|    3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|    3 |   55 | `		if( pClass && pClass->pBase ){` |
|    - |   56 | `			/* Point to the class name */` |
|    3 |   57 | `			pName = &pClass->pBase->sName;` |
|    3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    2 |   59 | `		}else{` |
|    - |   60 | `			/* Not inside class,return FALSE */` |
|  ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|    - |   62 | `		}` |
|    2 |   63 | `	}else{` |
|    - |   64 | `		/* Extract the target class */` |
|    7 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    7 |   66 | `		if( pClass ){` |
|    7 |   67 | `			if( pClass->pBase ){` |
|    5 |   68 | `				pName = &pClass->pBase->sName;` |
|    - |   69 | `				/* Return the parent class name */` |
|    5 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    3 |   71 | `			}else{` |
|    - |   72 | `				/* Object does not have a parent class */` |
|    3 |   73 | `				ph7_result_bool(pCtx,0);` |
|    - |   74 | `			}` |
|    4 |   75 | `		}else{` |
|    - |   76 | `			/* Not a class instance,return FALSE */` |
|  ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|    - |   78 | `		}` |
|    - |   79 | `	}` |
|    9 |   80 | `	return PH7_OK;` |
|    1 |   81 |  |
|    - |   82 | `/*` |
|    - |   83 | ` * string get_called_class(void)` |
|    - |   84 | ` *   Gets the name of the class the static method is called in.` |
|    - |   85 | ` * Parameters` |
|    - |   86 | ` *  None.` |
|    - |   87 | ` * Return` |
|    - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|    - |   89 | ` */` |
|    4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   91 |  |
|    - |   92 | `	ph7_class *pClass;` |
|    - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|    5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|    5 |   95 | `	if( pClass ){` |
|    - |   96 | `		SyString *pName;` |
|    - |   97 | `		/* Point to the class name */` |
|    5 |   98 | `		pName = &pClass->sName;` |
|    5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    3 |  100 | `	}else{` |
|  ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|  ! 0 |  102 | `		SXUNUSED(apArg);` |
|    - |  103 | `		/* Not inside class,return FALSE */` |
|  ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|    - |  105 | `	}` |
|    5 |  106 | `	return PH7_OK;` |
|    1 |  107 |  |
|    - |  108 | `/*` |
|    - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|    - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|    - |  111 | ` * string which hold the class name.` |
|    - |  112 | ` */` |
|   86 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|    2 |  114 |  |
|   88 |  115 | `	ph7_class *pClass = 0;` |
|   88 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|    - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   48 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|   65 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|    - |  120 | `		const char *zClass;` |
|    - |  121 | `		int nLen;` |
|    - |  122 | `		/* Extract class name */` |
|   40 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|   40 |  124 | `		if( nLen > 0 ){` |
|    - |  125 | `			SyHashEntry *pEntry;` |
|    - |  126 | `			/* Perform a lookup */` |
|   40 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|   40 |  128 | `			if( pEntry ){` |
|    - |  129 | `				/* Point to the desired class */` |
|   31 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   15 |  131 | `			}` |
|   19 |  132 | `		}` |
|   19 |  133 | `	}` |
|   88 |  134 | `	return pClass;` |
|    2 |  135 |  |
|    - |  136 | `/*` |
|    - |  137 | ` * bool property_exists(mixed $class,string $property)` |
|    - |  138 | ` *   Checks if the object or class has a property.` |
|    - |  139 | ` * Parameters` |
|    - |  140 | ` *  class` |
|    - |  141 | ` *   The class name or an object of the class to test for` |
|    - |  142 | ` * property` |
|    - |  143 | ` *  The name of the property` |
|    - |  144 | ` * Return` |
|    - |  145 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|    - |  146 | ` */` |
|   12 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  148 |  |
|   13 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|   13 |  150 | `	if( nArg > 1 ){` |
|    - |  151 | `		ph7_class *pClass;` |
|   13 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   13 |  153 | `		if( pClass ){` |
|    - |  154 | `			const char *zName;` |
|    - |  155 | `			int nLen;` |
|    - |  156 | `			/* Extract attribute name */` |
|   13 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|   13 |  158 | `			if( nLen > 0 ){` |
|    - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|   12 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    8 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|    - |  162 | `						/* property exists,flag that */` |
|   11 |  163 | `						res = 1;` |
|    5 |  164 | `				}` |
|    6 |  165 | `			}` |
|    6 |  166 | `		}` |
|    6 |  167 | `	}` |
|   13 |  168 | `	ph7_result_bool(pCtx,res);` |
|   13 |  169 | `	return PH7_OK;` |
|    1 |  170 |  |
|    - |  171 | `/*` |
|    - |  172 | ` * bool method_exists(mixed $class,string $method)` |
|    - |  173 | ` *   Checks if the given method is a class member.` |
|    - |  174 | ` * Parameters` |
|    - |  175 | ` *  class` |
|    - |  176 | ` *   The class name or an object of the class to test for` |
|    - |  177 | ` * property` |
|    - |  178 | ` *  The name of the method` |
|    - |  179 | ` * Return` |
|    - |  180 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|    - |  181 | ` */` |
|    4 |  182 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  183 |  |
|    5 |  184 | `	int res = 0; /* Assume method does not exists */` |
|    5 |  185 | `	if( nArg > 1 ){` |
|    - |  186 | `		ph7_class *pClass;` |
|    5 |  187 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    5 |  188 | `		if( pClass ){` |
|    - |  189 | `			const char *zName;` |
|    - |  190 | `			int nLen;` |
|    - |  191 | `			/* Extract method name */` |
|    5 |  192 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    5 |  193 | `			if( nLen > 0 ){` |
|    - |  194 | `				/* Perform the lookup in the method table */` |
|    5 |  195 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|    - |  196 | `					/* method exists,flag that */` |
|    3 |  197 | `					res = 1;` |
|    1 |  198 | `				}` |
|    2 |  199 | `			}` |
|    2 |  200 | `		}` |
|    2 |  201 | `	}` |
|    5 |  202 | `	ph7_result_bool(pCtx,res);` |
|    5 |  203 | `	return PH7_OK;` |
|    1 |  204 |  |
|    - |  205 | `/*` |
|    - |  206 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|    - |  207 | ` *   Checks if the class has been defined.` |
|    - |  208 | ` * Parameters` |
|    - |  209 | ` *  class_name` |
|    - |  210 | ` *   The class name. The name is matched in a case-sensitive manner` |
|    - |  211 | ` *   unlinke the standard PHP engine.` |
|    - |  212 | ` *  autoload` |
|    - |  213 | ` *   Whether or not to call __autoload by default.` |
|    - |  214 | ` * Return` |
|    - |  215 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|    - |  216 | ` */` |
|   16 |  217 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  218 |  |
|   18 |  219 | `	int res = 0; /* Assume class does not exists */` |
|   18 |  220 | `	if( nArg > 0 ){` |
|    - |  221 | `		const char *zName;` |
|    - |  222 | `		int nLen;` |
|    - |  223 | `		/* Extract given name */` |
|   18 |  224 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  225 | `		/* Perform a hashlookup */` |
|   18 |  226 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|    - |  227 | `			/* class is available */` |
|   12 |  228 | `			res = 1;` |
|    5 |  229 | `		}` |
|    8 |  230 | `	}` |
|   18 |  231 | `	ph7_result_bool(pCtx,res);` |
|   18 |  232 | `	return PH7_OK;` |
|    2 |  233 |  |
|    - |  234 | `/*` |
|    - |  235 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|    - |  236 | ` *   Checks if the interface has been defined.` |
|    - |  237 | ` * Parameters` |
|    - |  238 | ` *  class_name` |
|    - |  239 | ` *   The class name. The name is matched in a case-sensitive manner` |
|    - |  240 | ` *   unlinke the standard PHP engine.` |
|    - |  241 | ` *  autoload` |
|    - |  242 | ` *   Whether or not to call __autoload by default.` |
|    - |  243 | ` * Return` |
|    - |  244 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|    - |  245 | ` */` |
|    6 |  246 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  247 |  |
|    7 |  248 | `	int res = 0; /* Assume class does not exists */` |
|    7 |  249 | `	if( nArg > 0 ){` |
|    7 |  250 | `		SyHashEntry *pEntry = 0;` |
|    - |  251 | `		const char *zName;` |
|    - |  252 | `		int nLen;` |
|    - |  253 | `		/* Extract given name */` |
|    7 |  254 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  255 | `		/* Perform a hashlookup */` |
|    7 |  256 | `		if( nLen > 0 ){` |
|    7 |  257 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    3 |  258 | `		}` |
|    7 |  259 | `		if( pEntry ){` |
|    5 |  260 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    5 |  261 | `			while( pClass ){` |
|    5 |  262 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    - |  263 | `					/* interface is available */` |
|    5 |  264 | `					res = 1;` |
|    5 |  265 | `					break;` |
|    - |  266 | `				}` |
|    - |  267 | `				/* Next with the same name */` |
|  ! 0 |  268 | `				pClass = pClass->pNextName;` |
|  ! 0 |  269 | `			}` |
|    2 |  270 | `		}` |
|    3 |  271 | `	}` |
|    7 |  272 | `	ph7_result_bool(pCtx,res);` |
|    7 |  273 | `	return PH7_OK;` |
|    1 |  274 |  |
|    - |  275 | `/*` |
|    - |  276 | ` * bool class_alias([string $original[,string $alias ]])` |
|    - |  277 | ` *   Creates an alias for a class.` |
|    - |  278 | ` * Parameters` |
|    - |  279 | ` *  original` |
|    - |  280 | ` *    The original class.` |
|    - |  281 | ` *  alias` |
|    - |  282 | ` *   The alias name for the class.` |
|    - |  283 | ` * Return` |
|    - |  284 | ` *   Returns TRUE on success or FALSE on failure.` |
|    - |  285 | ` */` |
|    2 |  286 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  287 |  |
|    - |  288 | `	const char *zOld,*zNew;` |
|    - |  289 | `	int nOldLen,nNewLen;` |
|    - |  290 | `	SyHashEntry *pEntry;` |
|    - |  291 | `	ph7_class *pClass;` |
|    - |  292 | `	char *zDup;` |
|    - |  293 | `	sxi32 rc;` |
|    3 |  294 | `	if( nArg < 2 ){` |
|    - |  295 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  296 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  297 | `		return PH7_OK;` |
|    - |  298 | `	}` |
|    - |  299 | `	/* Extract old class name */` |
|    3 |  300 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|    - |  301 | `	/* Extract alias name */` |
|    3 |  302 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|    3 |  303 | `	if( nNewLen < 1 ){` |
|    - |  304 | `		/* Invalid alias name,return FALSE */` |
|  ! 0 |  305 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  306 | `		return PH7_OK;` |
|    - |  307 | `	}` |
|    - |  308 | `	/* Perform a hash lookup */` |
|    3 |  309 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|    3 |  310 | `	if( pEntry ==  0 ){` |
|    - |  311 | `		/* No such class,return FALSE */` |
|  ! 0 |  312 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  313 | `		return PH7_OK;` |
|    - |  314 | `	}` |
|    - |  315 | `	/* Point to the class */` |
|    3 |  316 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    - |  317 | `	/* Duplicate alias name */` |
|    3 |  318 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|    3 |  319 | `	if( zDup == 0 ){` |
|    - |  320 | `		/* Out of memory,return FALSE */` |
|  ! 0 |  321 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  322 | `		return PH7_OK;` |
|    - |  323 | `	}` |
|    - |  324 | `	/* Create the alias */` |
|    3 |  325 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|    3 |  326 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  327 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|  ! 0 |  328 | `	}` |
|    3 |  329 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|    3 |  330 | `	return PH7_OK;` |
|    2 |  331 |  |
|    - |  332 | `/*` |
|    - |  333 | ` * array get_declared_classes(void)` |
|    - |  334 | ` *   Returns an array with the name of the defined classes` |
|    - |  335 | ` * Parameters` |
|    - |  336 | ` *  None` |
|    - |  337 | ` * Return` |
|    - |  338 | ` *   Returns an array of the names of the declared classes` |
|    - |  339 | ` *   in the current script.` |
|    - |  340 | ` * Note:` |
|    - |  341 | ` *   NULL is returned on failure.` |
|    - |  342 | ` */` |
|    2 |  343 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  344 |  |
|    - |  345 | `	ph7_value *pName,*pArray;` |
|    - |  346 | `	SyHashEntry *pEntry;` |
|    - |  347 | `	/* Create a new array first */` |
|    3 |  348 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  349 | `	pName = ph7_context_new_scalar(pCtx);` |
|    3 |  350 | `	if( pArray == 0 \|\| pName == 0){` |
|  ! 0 |  351 | `		SXUNUSED(nArg); /* cc warning */` |
|  ! 0 |  352 | `		SXUNUSED(apArg);` |
|    - |  353 | `		/* Out of memory,return NULL */` |
|  ! 0 |  354 | `		ph7_result_null(pCtx);` |
|  ! 0 |  355 | `		return PH7_OK;` |
|    - |  356 | `	}` |
|    - |  357 | `	/* Fill the array with the defined classes */` |
|    3 |  358 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   54 |  359 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   51 |  360 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    - |  361 | `		/* Do not register classes defined as interfaces */` |
|   51 |  362 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   45 |  363 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|    - |  364 | `			/* insert class name */` |
|   45 |  365 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    - |  366 | `			/* Reset the cursor */` |
|   45 |  367 | `			ph7_value_reset_string_cursor(pName);` |
|   22 |  368 | `		}` |
|    1 |  369 | `	}` |
|    - |  370 | `	/* Return the created array */` |
|    3 |  371 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  372 | `	return PH7_OK;` |
|    2 |  373 |  |
|    - |  374 | `/*` |
|    - |  375 | ` * array get_declared_interfaces(void)` |
|    - |  376 | ` *   Returns an array with the name of the defined interfaces` |
|    - |  377 | ` * Parameters` |
|    - |  378 | ` *  None` |
|    - |  379 | ` * Return` |
|    - |  380 | ` *   Returns an array of the names of the declared interfaces` |
|    - |  381 | ` *   in the current script.` |
|    - |  382 | ` * Note:` |
|    - |  383 | ` *   NULL is returned on failure.` |
|    - |  384 | ` */` |
|    2 |  385 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  386 |  |
|    - |  387 | `	ph7_value *pName,*pArray;` |
|    - |  388 | `	SyHashEntry *pEntry;` |
|    - |  389 | `	/* Create a new array first */` |
|    3 |  390 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  391 | `	pName = ph7_context_new_scalar(pCtx);` |
|    3 |  392 | `	if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 |  393 | `		SXUNUSED(nArg); /* cc warning */` |
|  ! 0 |  394 | `		SXUNUSED(apArg);` |
|    - |  395 | `		/* Out of memory,return NULL */` |
|  ! 0 |  396 | `		ph7_result_null(pCtx);` |
|  ! 0 |  397 | `		return PH7_OK;` |
|    - |  398 | `	}` |
|    - |  399 | `	/* Fill the array with the defined classes */` |
|    3 |  400 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   56 |  401 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   53 |  402 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    - |  403 | `		/* Register classes defined as interfaces only */` |
|   53 |  404 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    9 |  405 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|    - |  406 | `			/* insert interface name */` |
|    9 |  407 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    - |  408 | `			/* Reset the cursor */` |
|    9 |  409 | `			ph7_value_reset_string_cursor(pName);` |
|    4 |  410 | `		}` |
|    1 |  411 | `	}` |
|    - |  412 | `	/* Return the created array */` |
|    3 |  413 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  414 | `	return PH7_OK;` |
|    2 |  415 |  |
|    - |  416 | `/*` |
|    - |  417 | ` * array get_class_methods(string/object $class_name)` |
|    - |  418 | ` *   Returns an array with the name of the class methods` |
|    - |  419 | ` * Parameters` |
|    - |  420 | ` *  class_name` |
|    - |  421 | ` *  The class name or class instance` |
|    - |  422 | ` * Return` |
|    - |  423 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|    - |  424 | ` *  In case of an error, it returns NULL.` |
|    - |  425 | ` * Note:` |
|    - |  426 | ` *   NULL is returned on failure.` |
|    - |  427 | ` */` |
|    6 |  428 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  429 |  |
|    - |  430 | `	ph7_value *pName,*pArray;` |
|    - |  431 | `	SyHashEntry *pEntry;` |
|    - |  432 | `	ph7_class *pClass;` |
|    - |  433 | `	/* Extract the target class first */` |
|    7 |  434 | `	pClass = 0;` |
|    7 |  435 | `	if( nArg > 0 ){` |
|    7 |  436 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    3 |  437 | `	}` |
|    7 |  438 | `	if( pClass == 0 ){` |
|    - |  439 | `		/* No such class,return NULL */` |
|    3 |  440 | `		ph7_result_null(pCtx);` |
|    3 |  441 | `		return PH7_OK;` |
|    - |  442 | `	}` |
|    - |  443 | `	/* Create a new array  */` |
|    5 |  444 | `	pArray = ph7_context_new_array(pCtx);` |
|    5 |  445 | `	pName = ph7_context_new_scalar(pCtx);` |
|    5 |  446 | `	if( pArray == 0 \|\| pName == 0){` |
|    - |  447 | `		/* Out of memory,return NULL */` |
|  ! 0 |  448 | `		ph7_result_null(pCtx);` |
|  ! 0 |  449 | `		return PH7_OK;` |
|    - |  450 | `	}` |
|    - |  451 | `	/* Fill the array with the defined methods */` |
|    5 |  452 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   17 |  453 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   13 |  454 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|    - |  455 | `		/* Insert method name */` |
|   13 |  456 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|   13 |  457 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|    - |  458 | `		/* Reset the cursor */` |
|   13 |  459 | `		ph7_value_reset_string_cursor(pName);` |
|    1 |  460 | `	}` |
|    - |  461 | `	/* Return the created array */` |
|    5 |  462 | `	ph7_result_value(pCtx,pArray);` |
|    - |  463 | `	/*` |
|    - |  464 | `	 * Don't worry about freeing memory here,everything will be relased` |
|    - |  465 | `	 * automatically as soon we return from this foreign function.` |
|    - |  466 | `	 */` |
|    5 |  467 | `	return PH7_OK;` |
|    4 |  468 |  |
|    - |  469 | `/*` |
|    - |  470 | ` * This function return TRUE(1) if the given class attribute stored` |
|    - |  471 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|    - |  472 | ` * from the current scope.Otherwise FALSE is returned.` |
|    - |  473 | ` */` |
| 3576 |  474 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|    - |  475 | `	ph7_vm *pVm,               /* Target VM */` |
|    - |  476 | `	ph7_class *pClass,         /* Target Class */` |
|    - |  477 | `	const SyString *pAttrName, /* Attribute name */` |
|    - |  478 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|    - |  479 | `	int bLog                   /* TRUE to log forbidden access. */` |
|    - |  480 | `	)` |
|    2 |  481 |  |
| 3578 |  482 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 2994 |  483 | `		VmFrame *pFrame = pVm->pFrame;` |
|    - |  484 | `		ph7_vm_func *pVmFunc;` |
| 2994 |  485 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|    - |  486 | `			/* Safely ignore the exception frame */` |
|  ! 0 |  487 | `			pFrame = pFrame->pParent;` |
|  ! 0 |  488 | `		}` |
| 2994 |  489 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
| 2994 |  490 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|    9 |  491 | `			goto dis; /* Access is forbidden */` |
|    - |  492 | `		}` |
| 2986 |  493 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    - |  494 | `			/* Must be the same instance or a trait used by the class */` |
|   26 |  495 | `			ph7_class *pCaller = (ph7_class *)pVmFunc->pUserData;` |
|   26 |  496 | `			if( pCaller != pClass ){` |
|    - |  497 | `				/* Check if the caller is a trait used by pClass */` |
|    - |  498 | `				ph7_class **apTrait;` |
|    - |  499 | `				sxu32 nTrait,k;` |
|    9 |  500 | `				int iFound = 0;` |
|    9 |  501 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    9 |  502 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   17 |  503 | `				for(k = 0; k < nTrait; k++){` |
|   17 |  504 | `					if( apTrait[k] == pCaller ){` |
|    9 |  505 | `						iFound = 1;` |
|    9 |  506 | `						break;` |
|    - |  507 | `					}` |
|    5 |  508 | `				}` |
|    9 |  509 | `				if( !iFound ){` |
|  ! 0 |  510 | `					goto dis; /* Access is forbidden */` |
|    - |  511 | `				}` |
|    4 |  512 | `			}` |
|   14 |  513 | `		}else{` |
|    - |  514 | `			/* Protected */` |
| 2962 |  515 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|    - |  516 | `			/* Must be a derived class */` |
| 2962 |  517 | `			if( !PH7_VmInstanceOf(pClass,pBase) ){` |
|  ! 0 |  518 | `				goto dis; /* Access is forbidden */` |
|    - |  519 | `			}` |
|    - |  520 | `		}` |
| 1492 |  521 | `	}` |
| 3570 |  522 | `	return 1; /* Access is granted */` |
|    4 |  523 | `dis:` |
|    9 |  524 | `	if( bLog ){` |
|  ! 0 |  525 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    - |  526 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|  ! 0 |  527 | `			&pClass->sName,pAttrName);` |
|  ! 0 |  528 | `	}` |
|    9 |  529 | `	return 0; /* Access is forbidden */` |
| 1790 |  530 |  |
|    - |  531 | `/*` |
|    - |  532 | ` * array get_class_vars(string/object $class_name)` |
|    - |  533 | ` *   Get the default properties of the class` |
|    - |  534 | ` * Parameters` |
|    - |  535 | ` *  class_name` |
|    - |  536 | ` *   The class name or class instance` |
|    - |  537 | ` * Return` |
|    - |  538 | ` *  Returns an associative array of declared properties visible from the current scope` |
|    - |  539 | ` *  with their default value. The resulting array elements are in the form` |
|    - |  540 | ` *  of varname => value.` |
|    - |  541 | ` * Note:` |
|    - |  542 | ` *   NULL is returned on failure.` |
|    - |  543 | ` */` |
|    2 |  544 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  545 |  |
|    - |  546 | `	ph7_value *pName,*pArray,sValue;` |
|    - |  547 | `	SyHashEntry *pEntry;` |
|    - |  548 | `	ph7_class *pClass;` |
|    - |  549 | `	/* Extract the target class first */` |
|    3 |  550 | `	pClass = 0;` |
|    3 |  551 | `	if( nArg > 0 ){` |
|    3 |  552 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    1 |  553 | `	}` |
|    3 |  554 | `	if( pClass == 0 ){` |
|    - |  555 | `		/* No such class,return NULL */` |
|  ! 0 |  556 | `		ph7_result_null(pCtx);` |
|  ! 0 |  557 | `		return PH7_OK;` |
|    - |  558 | `	}` |
|    - |  559 | `	/* Create a new array  */` |
|    3 |  560 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  561 | `	pName = ph7_context_new_scalar(pCtx);` |
|    3 |  562 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|    3 |  563 | `	if( pArray == 0 \|\| pName == 0){` |
|    - |  564 | `		/* Out of memory,return NULL */` |
|  ! 0 |  565 | `		ph7_result_null(pCtx);` |
|  ! 0 |  566 | `		return PH7_OK;` |
|    - |  567 | `	}` |
|    - |  568 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|    3 |  569 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    8 |  570 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    5 |  571 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    - |  572 | `		/* Check if the access is allowed */` |
|    5 |  573 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|    5 |  574 | `			SyString *pAttrName = &pAttr->sName;` |
|    5 |  575 | `			ph7_value *pValue = 0;` |
|    5 |  576 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|    - |  577 | `				/* Extract static attribute value which is always computed */` |
|    5 |  578 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|    3 |  579 | `			}else{` |
|  ! 0 |  580 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|  ! 0 |  581 | `					PH7_MemObjRelease(&sValue);` |
|    - |  582 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|  ! 0 |  583 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|  ! 0 |  584 | `					pValue = &sValue;` |
|  ! 0 |  585 | `				}` |
|    - |  586 | `			}` |
|    - |  587 | `			/* Fill in the array */` |
|    5 |  588 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    5 |  589 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    - |  590 | `			/* Reset the cursor */` |
|    5 |  591 | `			ph7_value_reset_string_cursor(pName);` |
|    2 |  592 | `		}` |
|    1 |  593 | `	}` |
|    3 |  594 | `	PH7_MemObjRelease(&sValue);` |
|    - |  595 | `	/* Return the created array */` |
|    3 |  596 | `	ph7_result_value(pCtx,pArray);` |
|    - |  597 | `	/*` |
|    - |  598 | `	 * Don't worry about freeing memory here,everything will be relased` |
|    - |  599 | `	 * automatically as soon we return from this foreign function.` |
|    - |  600 | `	 */` |
|    3 |  601 | `	return PH7_OK;` |
|    2 |  602 |  |
|    - |  603 | `/*` |
|    - |  604 | ` * array get_object_vars(object $this)` |
|    - |  605 | ` *   Gets the properties of the given object` |
|    - |  606 | ` * Parameters` |
|    - |  607 | ` *  this` |
|    - |  608 | ` *   A class instance` |
|    - |  609 | ` * Return` |
|    - |  610 | ` *  Returns an associative array of defined object accessible non-static properties` |
|    - |  611 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|    - |  612 | ` *  it will be returned with a NULL value.` |
|    - |  613 | ` * Note:` |
|    - |  614 | ` *   NULL is returned on failure.` |
|    - |  615 | ` */` |
|    2 |  616 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  617 |  |
|    3 |  618 | `	ph7_class_instance *pThis = 0;` |
|    - |  619 | `	ph7_value *pName,*pArray;` |
|    - |  620 | `	SyHashEntry *pEntry;` |
|    3 |  621 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|    - |  622 | `		/* Extract the target instance */` |
|    3 |  623 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    1 |  624 | `	}` |
|    3 |  625 | `	if( pThis == 0 ){` |
|    - |  626 | `		/* No such instance,return NULL */` |
|  ! 0 |  627 | `		ph7_result_null(pCtx);` |
|  ! 0 |  628 | `		return PH7_OK;` |
|    - |  629 | `	}` |
|    - |  630 | `	/* Create a new array  */` |
|    3 |  631 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  632 | `	pName = ph7_context_new_scalar(pCtx);` |
|    3 |  633 | `	if( pArray == 0 \|\| pName == 0){` |
|    - |  634 | `		/* Out of memory,return NULL */` |
|  ! 0 |  635 | `		ph7_result_null(pCtx);` |
|  ! 0 |  636 | `		return PH7_OK;` |
|    - |  637 | `	}` |
|    - |  638 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|    3 |  639 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    9 |  640 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    7 |  641 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    - |  642 | `		SyString *pAttrName;` |
|    7 |  643 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    - |  644 | `			/* Only non-static/constant attributes are extracted */` |
|  ! 0 |  645 | `			continue;` |
|    - |  646 | `		}` |
|    7 |  647 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|    - |  648 | `		/* Check if the access is allowed */` |
|    7 |  649 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    3 |  650 | `			ph7_value *pValue = 0;` |
|    - |  651 | `			/* Extract attribute */` |
|    3 |  652 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    3 |  653 | `			if( pValue ){` |
|    - |  654 | `				/* Insert attribute name in the array */` |
|    3 |  655 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    3 |  656 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    1 |  657 | `			}` |
|    - |  658 | `			/* Reset the cursor */` |
|    3 |  659 | `			ph7_value_reset_string_cursor(pName);` |
|    1 |  660 | `		}` |
|    1 |  661 | `	}` |
|    - |  662 | `	/* Return the created array */` |
|    3 |  663 | `	ph7_result_value(pCtx,pArray);` |
|    - |  664 | `	/*` |
|    - |  665 | `	 * Don't worry about freeing memory here,everything will be relased` |
|    - |  666 | `	 * automatically as soon we return from this foreign function.` |
|    - |  667 | `	 */` |
|    3 |  668 | `	return PH7_OK;` |
|    2 |  669 |  |
|    - |  670 | `/*` |
|    - |  671 | ` * This function returns TRUE if the given class is an implemented` |
|    - |  672 | ` * interface.Otherwise FALSE is returned.` |
|    - |  673 | ` */` |
| 6736 |  674 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|    2 |  675 |  |
|    - |  676 | `	ph7_class **apInterface;` |
|    - |  677 | `	sxu32 n;` |
| 6738 |  678 | `	if( SySetUsed(pSet) < 1 ){` |
|    - |  679 | `		/* Empty interface container */` |
| 6732 |  680 | `		return FALSE;` |
|    - |  681 | `	}` |
|    - |  682 | `	/* Point to the set of implemented interfaces */` |
|    8 |  683 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|    - |  684 | `	/* Perform the lookup */` |
|   10 |  685 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|    8 |  686 | `		if( apInterface[n] == pClass ){` |
|    6 |  687 | `			return TRUE;` |
|    - |  688 | `		}` |
|    2 |  689 | `	}` |
|    3 |  690 | `	return FALSE;` |
| 3370 |  691 |  |
|    - |  692 | `/*` |
|    - |  693 | ` * This function returns TRUE if the given class (first argument)` |
|    - |  694 | ` * is an instance of the main class (second argument).` |
|    - |  695 | ` * Otherwise FALSE is returned.` |
|    - |  696 | ` */` |
| 3046 |  697 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|    2 |  698 |  |
|    - |  699 | `	ph7_class *pParent;` |
|    - |  700 | `	sxi32 rc;` |
| 3048 |  701 | `	if( pThis == pClass ){` |
|    - |  702 | `		/* Instance of the same class */` |
|  158 |  703 | `		return TRUE;` |
|    - |  704 | `	}` |
|    - |  705 | `	/* Check implemented interfaces */` |
| 2892 |  706 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2892 |  707 | `	if( rc ){` |
|    6 |  708 | `		return TRUE;` |
|    - |  709 | `	}` |
|    - |  710 | `	/* Check parent classes */` |
| 2888 |  711 | `	pParent = pThis->pBase;` |
| 6734 |  712 | `	while( pParent ){` |
| 6732 |  713 | `		if( pParent == pClass ){` |
|    - |  714 | `			/* Same instance */` |
| 2886 |  715 | `			return TRUE;` |
|    - |  716 | `		}` |
|    - |  717 | `		/* Check the implemented interfaces */` |
| 3848 |  718 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
| 3848 |  719 | `		if( rc ){` |
|  ! 0 |  720 | `			return TRUE;` |
|    - |  721 | `		}` |
|    - |  722 | `		/* Point to the parent class */` |
| 3848 |  723 | `		pParent = pParent->pBase;` |
|    2 |  724 | `	}` |
|    - |  725 | `	/* Not an instance of the the given class */` |
|    3 |  726 | `	return FALSE;` |
| 1525 |  727 |  |
|    - |  728 | `/*` |
|    - |  729 | ` * This function returns TRUE if the given class (first argument)` |
|    - |  730 | ` * is a subclass of the main class (second argument).` |
|    - |  731 | ` * Otherwise FALSE is returned.` |
|    - |  732 | ` */` |
|    4 |  733 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|    1 |  734 |  |
|    5 |  735 | `	SySet *pInterface = &pClass->aInterface;` |
|    - |  736 | `	SyHashEntry *pEntry;` |
|    - |  737 | `	SyString *pName;` |
|    - |  738 | `	sxi32 rc;` |
|    5 |  739 | `	while( pClass ){` |
|    5 |  740 | `		pName = &pClass->sName;` |
|    - |  741 | `		/* Query the derived hashtable */` |
|    5 |  742 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    5 |  743 | `		if( pEntry ){` |
|    5 |  744 | `			return TRUE;` |
|    - |  745 | `		}` |
|  ! 0 |  746 | `		pClass = pClass->pBase;` |
|  ! 0 |  747 | `	}` |
|  ! 0 |  748 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|  ! 0 |  749 | `	if( rc ){` |
|  ! 0 |  750 | `		return TRUE;` |
|    - |  751 | `	}` |
|    - |  752 | `	/* Not a subclass */` |
|  ! 0 |  753 | `	return FALSE;` |
|    3 |  754 |  |
|    - |  755 | `/*` |
|    - |  756 | ` * bool is_a(object $object,string $class_name)` |
|    - |  757 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|    - |  758 | ` * Parameters` |
|    - |  759 | ` *  object` |
|    - |  760 | ` *   The tested object` |
|    - |  761 | ` * class_name` |
|    - |  762 | ` *  The class name` |
|    - |  763 | ` * Return` |
|    - |  764 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|    - |  765 | ` *   parents, FALSE otherwise.` |
|    - |  766 | ` */` |
|    2 |  767 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  768 |  |
|    3 |  769 | `	int res = 0; /* Assume FALSE by default */` |
|    3 |  770 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    3 |  771 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    - |  772 | `		ph7_class *pClass;` |
|    - |  773 | `		/* Extract the given class */` |
|    3 |  774 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    3 |  775 | `		if( pClass ){` |
|    - |  776 | `			/* Perform the query */` |
|    3 |  777 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|    1 |  778 | `		}` |
|    1 |  779 | `	}` |
|    - |  780 | `	/* Query result */` |
|    3 |  781 | `	ph7_result_bool(pCtx,res);` |
|    3 |  782 | `	return PH7_OK;` |
|    1 |  783 |  |
|    - |  784 | `/*` |
|    - |  785 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|    - |  786 | ` *   Checks if the object has this class as one of its parents.` |
|    - |  787 | ` * Parameters` |
|    - |  788 | ` *  object` |
|    - |  789 | ` *   The tested object` |
|    - |  790 | ` * class_name` |
|    - |  791 | ` *  The class name` |
|    - |  792 | ` * Return` |
|    - |  793 | ` *  This function returns TRUE if the object , belongs to a class` |
|    - |  794 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|    - |  795 | ` */` |
|    6 |  796 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  797 |  |
|    7 |  798 | `	int res = 0; /* Assume FALSE by default */` |
|    7 |  799 | `	if( nArg > 1 ){` |
|    - |  800 | `		ph7_class *pClass,*pMain;` |
|    - |  801 | `		/* Extract the given classes */` |
|    7 |  802 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    7 |  803 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    7 |  804 | `		if( pClass && pMain ){` |
|    - |  805 | `			/* Perform the query */` |
|    5 |  806 | `			res = VmSubclassOf(pClass,pMain);` |
|    2 |  807 | `		}` |
|    3 |  808 | `	}` |
|    - |  809 | `	/* Query result */` |
|    7 |  810 | `	ph7_result_bool(pCtx,res);` |
|    7 |  811 | `	return PH7_OK;` |
|    1 |  812 |  |
|   14 |  813 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  814 |  |
|    - |  815 | `	ph7_value sResult; /* Store callback return value here */` |
|    - |  816 | `	sxi32 rc;` |
|   15 |  817 | `	if( nArg < 1 ){` |
|    - |  818 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  819 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  820 | `		return PH7_OK;` |
|    - |  821 | `	}` |
|   15 |  822 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|   15 |  823 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    - |  824 | `	/* Try to invoke the callback */` |
|   15 |  825 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|   15 |  826 | `	if( rc != SXRET_OK ){` |
|    - |  827 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|  ! 0 |  828 | `		ph7_result_bool(pCtx,0); /* return false */` |
|  ! 0 |  829 | `	}else{` |
|    - |  830 | `		/* Callback result */` |
|   15 |  831 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|    - |  832 | `	}` |
|   15 |  833 | `	PH7_MemObjRelease(&sResult);` |
|   15 |  834 | `	return PH7_OK;` |
|    8 |  835 |  |
|    - |  836 | `/*` |
|    - |  837 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|    - |  838 | ` *  Call a callback with an array of parameters.` |
|    - |  839 | ` * Parameter` |
|    - |  840 | ` *  $callback` |
|    - |  841 | ` *   The callable to be called.` |
|    - |  842 | ` * $param_arr` |
|    - |  843 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|    - |  844 | ` * Return` |
|    - |  845 | ` *  Returns the return value of the callback, or FALSE on error.` |
|    - |  846 | ` */` |
|   10 |  847 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  848 |  |
|    - |  849 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|    - |  850 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|    - |  851 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|    - |  852 | `	SySet aArg;               /* Arguments containers */` |
|    - |  853 | `	sxi32 rc;` |
|    - |  854 | `	sxu32 n;` |
|   11 |  855 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|    - |  856 | `		/* Missing/Invalid arguments,return FALSE */` |
|  ! 0 |  857 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  858 | `		return PH7_OK;` |
|    - |  859 | `	}` |
|   11 |  860 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|   11 |  861 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    - |  862 | `	/* Initialize the arguments container */` |
|   11 |  863 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|    - |  864 | `	/* Turn hashmap entries into callback arguments */` |
|   11 |  865 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|   11 |  866 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   23 |  867 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    - |  868 | `		/* Extract node value */` |
|   13 |  869 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   13 |  870 | `			SySetPut(&aArg,(const void *)&pValue);` |
|    6 |  871 | `		}` |
|    - |  872 | `		/* Point to the next entry */` |
|   13 |  873 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    7 |  874 | `	}` |
|    - |  875 | `	/* Try to invoke the callback */` |
|   11 |  876 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|   11 |  877 | `	if( rc != SXRET_OK ){` |
|    - |  878 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|  ! 0 |  879 | `		ph7_result_bool(pCtx,0); /* return false */` |
|  ! 0 |  880 | `	}else{` |
|    - |  881 | `		/* Callback result */` |
|   11 |  882 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|    - |  883 | `	}` |
|    - |  884 | `	/* Cleanup the mess left behind */` |
|   11 |  885 | `	PH7_MemObjRelease(&sResult);` |
|   11 |  886 | `	SySetRelease(&aArg);` |
|   11 |  887 | `	return PH7_OK;` |
|    6 |  888 |  |
|    - |  889 |  |
