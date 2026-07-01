# src/ph7/vm_builtin_getopt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 42/170 lines (24.71%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    6 |    7 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|    1 |    8 | `{` |
|  409 |    9 | `	while( zIn < zEnd ){` |
|  403 |   10 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|    - |   11 | `			/* Got one */` |
|  ! 0 |   12 | `			return &zIn[1];` |
|    - |   13 | `		}` |
|    - |   14 | `		/* Advance the cursor */` |
|  403 |   15 | `		zIn++;` |
|    1 |   16 | `	}` |
|    - |   17 | `	/* No such option */` |
|    7 |   18 | `	return 0;` |
|    4 |   19 | `}` |
|    - |   20 | `/*` |
|    - |   21 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|    - |   22 | ` * line string. Return a pointer to the start of the stream on success.` |
|    - |   23 | ` * NULL otherwise.` |
|    - |   24 | ` */` |
|  ! 0 |   25 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|  ! 0 |   26 | `{` |
|    - |   27 | `	const char *zOpt;` |
|  ! 0 |   28 | `	while( zIn < zEnd ){` |
|  ! 0 |   29 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|  ! 0 |   30 | `			zIn += 2;` |
|  ! 0 |   31 | `			zOpt = zIn;` |
|  ! 0 |   32 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  ! 0 |   33 | `				if( zIn[0] == '=' /* --opt=val */){` |
|  ! 0 |   34 | `					break;` |
|    - |   35 | `				}` |
|  ! 0 |   36 | `				zIn++;` |
|  ! 0 |   37 | `			}` |
|    - |   38 | `			/* Test */` |
|  ! 0 |   39 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|    - |   40 | `				/* Got one,return it's value */` |
|  ! 0 |   41 | `				return zIn;` |
|    - |   42 | `			}` |
|    - |   43 |  |
|  ! 0 |   44 | `		}else{` |
|  ! 0 |   45 | `			zIn++;` |
|    - |   46 | `		}` |
|  ! 0 |   47 | `	}` |
|    - |   48 | `	/* No such option */` |
|  ! 0 |   49 | `	return 0;` |
|  ! 0 |   50 | `}` |
|    - |   51 | `/*` |
|    - |   52 | ` * Long option [i.e: --opt] arguments private data structure.` |
|    - |   53 | ` */` |
|    - |   54 | `struct getopt_long_opt` |
|    - |   55 | `{` |
|    - |   56 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|    - |   57 | `	ph7_value *pWorker;  /* Worker variable*/` |
|    - |   58 | `	ph7_value *pArray;   /* getopt() return value */` |
|    - |   59 | `	ph7_context *pCtx;   /* Call Context */` |
|    - |   60 | `};` |
|    - |   61 | `/* Forward declaration */` |
|    - |   62 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|    - |   63 | `/*` |
|    - |   64 | ` * Extract short or long argument option values.` |
|    - |   65 | ` */` |
|  ! 0 |   66 | `static void VmExtractOptArgValue(` |
|    - |   67 | `	ph7_value *pArray,  /* getopt() return value */` |
|    - |   68 | `	ph7_value *pWorker, /* Worker variable */` |
|    - |   69 | `	const char *zArg,   /* Argument stream */` |
|    - |   70 | `	const char *zArgEnd,/* End of the argument stream  */` |
|    - |   71 | `	int need_val,       /* TRUE to fetch option argument */` |
|    - |   72 | `	ph7_context *pCtx,  /* Call Context */` |
|    - |   73 | `	const char *zName   /* Option name */)` |
|  ! 0 |   74 | `{` |
|  ! 0 |   75 | `	ph7_value_bool(pWorker,0);` |
|  ! 0 |   76 | `	if( !need_val ){` |
|    - |   77 | `		/*` |
|    - |   78 | `		 * Option does not need arguments.` |
|    - |   79 | `		 * Insert the option name and a boolean FALSE.` |
|    - |   80 | `		 */` |
|  ! 0 |   81 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|  ! 0 |   82 | `	}else{` |
|    - |   83 | `		const char *zCur;` |
|    - |   84 | `		/* Extract option argument */` |
|  ! 0 |   85 | `		zArg++;` |
|  ! 0 |   86 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|  ! 0 |   87 | `			zArg++;` |
|  ! 0 |   88 | `		}` |
|  ! 0 |   89 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|  ! 0 |   90 | `			zArg++;` |
|  ! 0 |   91 | `		}` |
|  ! 0 |   92 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|    - |   93 | `			/*` |
|    - |   94 | `			 * Argument not found.` |
|    - |   95 | `			 * Insert the option name and a boolean FALSE.` |
|    - |   96 | `			 */` |
|  ! 0 |   97 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|  ! 0 |   98 | `			return;` |
|    - |   99 | `		}` |
|    - |  100 | `		/* Delimit the value */` |
|  ! 0 |  101 | `		zCur = zArg;` |
|  ! 0 |  102 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|  ! 0 |  103 | `			int d = zArg[0];` |
|    - |  104 | `			/* Delimt the argument */` |
|  ! 0 |  105 | `			zArg++;` |
|  ! 0 |  106 | `			zCur = zArg;` |
|  ! 0 |  107 | `			while( zArg < zArgEnd ){` |
|  ! 0 |  108 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|    - |  109 | `					/* Delimiter found,exit the loop  */` |
|  ! 0 |  110 | `					break;` |
|    - |  111 | `				}` |
|  ! 0 |  112 | `				zArg++;` |
|  ! 0 |  113 | `			}` |
|    - |  114 | `			/* Save the value */` |
|  ! 0 |  115 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|  ! 0 |  116 | `			if( zArg < zArgEnd ){ zArg++; }` |
|  ! 0 |  117 | `		}else{` |
|  ! 0 |  118 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|  ! 0 |  119 | `				zArg++;` |
|  ! 0 |  120 | `			}` |
|    - |  121 | `			/* Save the value */` |
|  ! 0 |  122 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|    - |  123 | `		}` |
|    - |  124 | `		/*` |
|    - |  125 | `		 * Check if we are dealing with multiple values.` |
|    - |  126 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|    - |  127 | `		 */` |
|  ! 0 |  128 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|  ! 0 |  129 | `			zArg++;` |
|  ! 0 |  130 | `		}` |
|  ! 0 |  131 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|    - |  132 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|  ! 0 |  133 | `			pOptArg = ph7_context_new_array(pCtx);` |
|  ! 0 |  134 | `			if( pOptArg == 0 ){` |
|  ! 0 |  135 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  136 | `			}else{` |
|    - |  137 | `				/* Insert the first value */` |
|  ! 0 |  138 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|  ! 0 |  139 | `				for(;;){` |
|  ! 0 |  140 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|    - |  141 | `						/* No more value */` |
|  ! 0 |  142 | `						break;` |
|    - |  143 | `					}` |
|    - |  144 | `					/* Delimit the value */` |
|  ! 0 |  145 | `					zCur = zArg;` |
|  ! 0 |  146 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|  ! 0 |  147 | `						zArg++;` |
|  ! 0 |  148 | `						zCur = zArg;` |
|  ! 0 |  149 | `					}` |
|  ! 0 |  150 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|  ! 0 |  151 | `						zArg++;` |
|  ! 0 |  152 | `					}` |
|    - |  153 | `					/* Reset the string cursor */` |
|  ! 0 |  154 | `					ph7_value_reset_string_cursor(pWorker);` |
|    - |  155 | `					/* Save the value */` |
|  ! 0 |  156 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|    - |  157 | `					/* Insert */` |
|  ! 0 |  158 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|    - |  159 | `					/* Jump trailing white spaces */` |
|  ! 0 |  160 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|  ! 0 |  161 | `						zArg++;` |
|  ! 0 |  162 | `					}` |
|  ! 0 |  163 | `				}` |
|    - |  164 | `				/* Insert the option arg array */` |
|  ! 0 |  165 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|    - |  166 | `				/* Safely release */` |
|  ! 0 |  167 | `				ph7_context_release_value(pCtx,pOptArg);` |
|    - |  168 | `			}` |
|  ! 0 |  169 | `		}else{` |
|    - |  170 | `			/* Single value */` |
|  ! 0 |  171 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|    - |  172 | `		}` |
|    - |  173 | `	}` |
|  ! 0 |  174 | `}` |
|    - |  175 | `/*` |
|    - |  176 | ` * array getopt(string $options[,array $longopts ])` |
|    - |  177 | ` *   Gets options from the command line argument list.` |
|    - |  178 | ` * Parameters` |
|    - |  179 | ` *  $options` |
|    - |  180 | ` *   Each character in this string will be used as option characters` |
|    - |  181 | ` *   and matched against options passed to the script starting with` |
|    - |  182 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|    - |  183 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|    - |  184 | ` *  $longopts` |
|    - |  185 | ` *   An array of options. Each element in this array will be used as option` |
|    - |  186 | ` *   strings and matched against options passed to the script starting with` |
|    - |  187 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|    - |  188 | ` *   option --opt.` |
|    - |  189 | ` * Return` |
|    - |  190 | ` *  This function will return an array of option / argument pairs or FALSE` |
|    - |  191 | ` *  on failure.` |
|    - |  192 | ` */` |
|    2 |  193 | `PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  194 | `{` |
|    - |  195 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|    - |  196 | `	struct getopt_long_opt sLong;` |
|    - |  197 | `	ph7_value *pArray,*pWorker;` |
|    - |  198 | `	SyBlob *pArg;` |
|    - |  199 | `	int nByte;` |
|    3 |  200 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |  201 | `		/* Missing/Invalid arguments,return FALSE */` |
|  ! 0 |  202 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|  ! 0 |  203 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  204 | `		return PH7_OK;` |
|    - |  205 | `	}` |
|    - |  206 | `	/* Extract option arguments */` |
|    3 |  207 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|    3 |  208 | `	zEnd = &zIn[nByte];` |
|    - |  209 | `	/* Point to the string representation of the $argv[] array */` |
|    3 |  210 | `	pArg = &pCtx->pVm->sArgv;` |
|    - |  211 | `	/* Create a new empty array and a worker variable */` |
|    3 |  212 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  213 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    3 |  214 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|  ! 0 |  215 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  216 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  217 | `		return PH7_OK;` |
|    - |  218 | `	}` |
|    3 |  219 | `	if( SyBlobLength(pArg) < 1 ){` |
|    - |  220 | `		/* Empty command line,return the empty array*/` |
|  ! 0 |  221 | `		ph7_result_value(pCtx,pArray);` |
|    - |  222 | `		/* Everything will be released automatically when we return` |
|    - |  223 | `		 * from this function.` |
|    - |  224 | `		 */` |
|  ! 0 |  225 | `		return PH7_OK;` |
|    - |  226 | `	}` |
|    3 |  227 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|    3 |  228 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|    - |  229 | `	/* Fill the long option structure */` |
|    3 |  230 | `	sLong.pArray = pArray;` |
|    3 |  231 | `	sLong.pWorker = pWorker;` |
|    3 |  232 | `	sLong.zArgIn =  zArgIn;` |
|    3 |  233 | `	sLong.zArgEnd = zArgEnd;` |
|    3 |  234 | `	sLong.pCtx = pCtx;` |
|    - |  235 | `	/* Start processing */` |
|    9 |  236 | `	while( zIn < zEnd ){` |
|    7 |  237 | `		int c = zIn[0];` |
|    7 |  238 | `		int need_val = 0;` |
|    - |  239 | `		/* Advance the stream cursor */` |
|    7 |  240 | `		zIn++;` |
|    - |  241 | `		/* Ignore non-alphanum characters */` |
|    7 |  242 | `		if( !SyisAlphaNum(c) ){` |
|  ! 0 |  243 | `			continue;` |
|    - |  244 | `		}` |
|    7 |  245 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|    5 |  246 | `			zIn++;` |
|    5 |  247 | `			need_val = 1;` |
|    5 |  248 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|  ! 0 |  249 | `				zIn++;` |
|  ! 0 |  250 | `			}` |
|    2 |  251 | `		}` |
|    - |  252 | `		/* Find option */` |
|    7 |  253 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|    7 |  254 | `		if( zArg == 0 ){` |
|    - |  255 | `			/* No such option */` |
|    7 |  256 | `			continue;` |
|    - |  257 | `		}` |
|    - |  258 | `		/* Extract option argument value */` |
|  ! 0 |  259 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|  ! 0 |  260 | `	}` |
|    3 |  261 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|    - |  262 | `		/* Process long options */` |
|  ! 0 |  263 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|  ! 0 |  264 | `	}` |
|    - |  265 | `	/* Return the option array */` |
|    3 |  266 | `	ph7_result_value(pCtx,pArray);` |
|    - |  267 | `	/*` |
|    - |  268 | `	 * Don't worry about freeing memory, everything will be released` |
|    - |  269 | `	 * automatically as soon we return from this foreign function.` |
|    - |  270 | `	 */` |
|    3 |  271 | `	return PH7_OK;` |
|    2 |  272 | `}` |
|    - |  273 | `/*` |
|    - |  274 | ` * Array walker callback used for processing long options values.` |
|    - |  275 | ` */` |
|  ! 0 |  276 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|  ! 0 |  277 | `{` |
|  ! 0 |  278 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|    - |  279 | `	const char *zArg,*zOpt,*zEnd;` |
|  ! 0 |  280 | `	int need_value = 0;` |
|    - |  281 | `	int nByte;` |
|    - |  282 | `	/* Value must be of type string */` |
|  ! 0 |  283 | `	if( !ph7_value_is_string(pValue) ){` |
|    - |  284 | `		/* Simply ignore */` |
|  ! 0 |  285 | `		return PH7_OK;` |
|    - |  286 | `	}` |
|  ! 0 |  287 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|  ! 0 |  288 | `	if( nByte < 1 ){` |
|    - |  289 | `		/* Empty string,ignore */` |
|  ! 0 |  290 | `		return PH7_OK;` |
|    - |  291 | `	}` |
|  ! 0 |  292 | `	zEnd = &zOpt[nByte - 1];` |
|  ! 0 |  293 | `	if( zEnd[0] == ':' ){` |
|    - |  294 | `		char *zTerm;` |
|    - |  295 | `		/* Try to extract a value */` |
|  ! 0 |  296 | `		need_value = 1;` |
|  ! 0 |  297 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|  ! 0 |  298 | `			zEnd--;` |
|  ! 0 |  299 | `		}` |
|  ! 0 |  300 | `		if( zOpt >= zEnd ){` |
|    - |  301 | `			/* Empty string,ignore */` |
|  ! 0 |  302 | `			SXUNUSED(pKey);` |
|  ! 0 |  303 | `			return PH7_OK;` |
|    - |  304 | `		}` |
|  ! 0 |  305 | `		zEnd++;` |
|  ! 0 |  306 | `		zTerm = (char *)zEnd;` |
|  ! 0 |  307 | `		zTerm[0] = 0;` |
|  ! 0 |  308 | `	}else{` |
|  ! 0 |  309 | `		zEnd = &zOpt[nByte];` |
|    - |  310 | `	}` |
|    - |  311 | `	/* Find the option */` |
|  ! 0 |  312 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|  ! 0 |  313 | `	if( zArg == 0 ){` |
|    - |  314 | `		/* No such option,return immediately */` |
|  ! 0 |  315 | `		return PH7_OK;` |
|    - |  316 | `	}` |
|    - |  317 | `	/* Try to extract a value */` |
|  ! 0 |  318 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|  ! 0 |  319 | `	return PH7_OK;` |
|  ! 0 |  320 | `}` |
|    - |  321 |  |
