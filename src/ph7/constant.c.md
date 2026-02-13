# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 997/1002 lines (99.50%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement built-in constants for the PH7 engine. */` |
|      - |    8 | `/*` |
|      - |    9 | ` * PH7_VERSION` |
|      - |   10 | ` * __PH7__` |
|      - |   11 | ` *   Expand the current version of the PH7 engine.` |
|      - |   12 | ` */` |
|      8 |   13 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   14 |  |
|      4 |   15 | `	SXUNUSED(pUnused);` |
|      9 |   16 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|      9 |   17 |  |
|      - |   18 | `#ifdef __WINNT__` |
|      - |   19 | `#include <Windows.h>` |
|      - |   20 | `#elif defined(__UNIXES__)` |
|      - |   21 | `#include <sys/utsname.h>` |
|      - |   22 | `#endif` |
|      - |   23 | `/*` |
|      - |   24 | ` * PHP_OS` |
|      - |   25 | ` *  Expand the name of the host Operating System.` |
|      - |   26 | ` */` |
|   1246 |   27 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   28 |  |
|      - |   29 | `#if defined(__WINNT__)` |
|      2 |   30 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   31 | `#elif defined(__UNIXES__)` |
|      - |   32 | `	struct utsname sInfo;` |
|   1246 |   33 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   34 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   35 | `	}else{` |
|   1246 |   36 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   37 | `	}` |
|      - |   38 | `#else` |
|      - |   39 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   40 | `#endif` |
|    623 |   41 | `	SXUNUSED(pUnused);` |
|   1248 |   42 |  |
|      - |   43 | `/*` |
|      - |   44 | ` * PHP_EOL` |
|      - |   45 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   46 | ` */` |
|    488 |   47 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   48 |  |
|    244 |   49 | `	SXUNUSED(pUnused);` |
|      - |   50 | `#ifdef __WINNT__` |
|      2 |   51 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   52 | `#else` |
|    488 |   53 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   54 | `#endif` |
|    490 |   55 |  |
|      - |   56 | `/*` |
|      - |   57 | ` * PHP_INT_MAX` |
|      - |   58 | ` * Expand the largest integer supported.` |
|      - |   59 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |   60 | ` */` |
|      2 |   61 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   62 |  |
|      1 |   63 | `	SXUNUSED(pUnused);` |
|      3 |   64 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|      3 |   65 |  |
|      - |   66 | `/*` |
|      - |   67 | ` * PHP_INT_SIZE` |
|      - |   68 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |   69 | ` */` |
|      2 |   70 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   71 |  |
|      1 |   72 | `	SXUNUSED(pUnused);` |
|      3 |   73 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      3 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * DIRECTORY_SEPARATOR.` |
|      - |   77 | ` * Expand the directory separator character.` |
|      - |   78 | ` */` |
|    156 |   79 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   80 |  |
|     78 |   81 | `	SXUNUSED(pUnused);` |
|      - |   82 | `#ifdef __WINNT__` |
|      2 |   83 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |   84 | `#else` |
|    156 |   85 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |   86 | `#endif` |
|    158 |   87 |  |
|      - |   88 | `/*` |
|      - |   89 | ` * PATH_SEPARATOR.` |
|      - |   90 | ` * Expand the path separator character.` |
|      - |   91 | ` */` |
|      2 |   92 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   93 |  |
|      1 |   94 | `	SXUNUSED(pUnused);` |
|      - |   95 | `#ifdef __WINNT__` |
|      1 |   96 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |   97 | `#else` |
|      2 |   98 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |   99 | `#endif` |
|      3 |  100 |  |
|      - |  101 | `#ifndef __WINNT__` |
|      - |  102 | `#include <time.h>` |
|      - |  103 | `#endif` |
|      - |  104 | `/*` |
|      - |  105 | ` * __TIME__` |
|      - |  106 | ` *  Expand the current time (GMT).` |
|      - |  107 | ` */` |
|      2 |  108 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  109 |  |
|      - |  110 | `	Sytm sTm;` |
|      - |  111 | `#ifdef __WINNT__` |
|      - |  112 | `	SYSTEMTIME sOS;` |
|      1 |  113 | `	GetSystemTime(&sOS);` |
|      1 |  114 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  115 | `#else` |
|      - |  116 | `	struct tm *pTm;` |
|      - |  117 | `	time_t t;` |
|      2 |  118 | `	time(&t);` |
|      2 |  119 | `	pTm = gmtime(&t);` |
|      2 |  120 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  121 | `#endif` |
|      1 |  122 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  123 | `	/* Expand */` |
|      3 |  124 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  125 |  |
|      - |  126 | `/*` |
|      - |  127 | ` * __DATE__` |
|      - |  128 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  129 | ` */` |
|      2 |  130 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  131 |  |
|      - |  132 | `	Sytm sTm;` |
|      - |  133 | `#ifdef __WINNT__` |
|      - |  134 | `	SYSTEMTIME sOS;` |
|      1 |  135 | `	GetSystemTime(&sOS);` |
|      1 |  136 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  137 | `#else` |
|      - |  138 | `	struct tm *pTm;` |
|      - |  139 | `	time_t t;` |
|      2 |  140 | `	time(&t);` |
|      2 |  141 | `	pTm = gmtime(&t);` |
|      2 |  142 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  143 | `#endif` |
|      1 |  144 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  145 | `	/* Expand */` |
|      3 |  146 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  147 |  |
|      - |  148 | `/*` |
|      - |  149 | ` * __FILE__` |
|      - |  150 | ` *  Path of the processed script.` |
|      - |  151 | ` */` |
|     34 |  152 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  153 |  |
|     36 |  154 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  155 | `	SyString *pFile;` |
|      - |  156 | `	/* Peek the top entry */` |
|     36 |  157 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     36 |  158 | `	if( pFile == 0 ){` |
|      - |  159 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  160 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  161 | `	}else{` |
|     34 |  162 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  163 | `	}` |
|     36 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * __DIR__` |
|      - |  167 | ` *  Directory holding the processed script.` |
|      - |  168 | ` */` |
|     20 |  169 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  170 |  |
|     22 |  171 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  172 | `	SyString *pFile;` |
|      - |  173 | `	/* Peek the top entry */` |
|     22 |  174 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     22 |  175 | `	if( pFile == 0 ){` |
|      - |  176 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  177 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  178 | `	}else{` |
|     19 |  179 | `		if( pFile->nByte > 0 ){` |
|      - |  180 | `			const char *zDir;` |
|      - |  181 | `			int nLen;` |
|     19 |  182 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     19 |  183 | `			ph7_value_string(pVal,zDir,nLen);` |
|     10 |  184 | `		}else{` |
|      - |  185 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  186 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  187 | `		}` |
|      - |  188 | `	}` |
|     22 |  189 |  |
|      - |  190 | `/*` |
|      - |  191 | ` * PHP_SHLIB_SUFFIX` |
|      - |  192 | ` *  Expand shared library suffix.` |
|      - |  193 | ` */` |
|      2 |  194 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  195 |  |
|      - |  196 | `#ifdef __WINNT__` |
|    ! 0 |  197 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  198 | `#else` |
|      2 |  199 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  200 | `#endif` |
|      1 |  201 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  202 |  |
|      - |  203 | `/*` |
|      - |  204 | ` * E_ERROR` |
|      - |  205 | ` *  Expands 1` |
|      - |  206 | ` */` |
|      2 |  207 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  208 |  |
|      3 |  209 | `	ph7_value_int(pVal,1);` |
|      1 |  210 | `	SXUNUSED(pUserData);` |
|      3 |  211 |  |
|      - |  212 | `/*` |
|      - |  213 | ` * E_WARNING` |
|      - |  214 | ` *  Expands 2` |
|      - |  215 | ` */` |
|      2 |  216 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  217 |  |
|      3 |  218 | `	ph7_value_int(pVal,2);` |
|      1 |  219 | `	SXUNUSED(pUserData);` |
|      3 |  220 |  |
|      - |  221 | `/*` |
|      - |  222 | ` * E_PARSE` |
|      - |  223 | ` *  Expands 4` |
|      - |  224 | ` */` |
|      2 |  225 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  226 |  |
|      3 |  227 | `	ph7_value_int(pVal,4);` |
|      1 |  228 | `	SXUNUSED(pUserData);` |
|      3 |  229 |  |
|      - |  230 | `/*` |
|      - |  231 | ` * E_NOTICE` |
|      - |  232 | ` * Expands 8` |
|      - |  233 | ` */` |
|      2 |  234 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  235 |  |
|      3 |  236 | `	ph7_value_int(pVal,8);` |
|      1 |  237 | `	SXUNUSED(pUserData);` |
|      3 |  238 |  |
|      - |  239 | `/*` |
|      - |  240 | ` * E_CORE_ERROR` |
|      - |  241 | ` * Expands 16` |
|      - |  242 | ` */` |
|      2 |  243 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  244 |  |
|      3 |  245 | `	ph7_value_int(pVal,16);` |
|      1 |  246 | `	SXUNUSED(pUserData);` |
|      3 |  247 |  |
|      - |  248 | `/*` |
|      - |  249 | ` * E_CORE_WARNING` |
|      - |  250 | ` * Expands 32` |
|      - |  251 | ` */` |
|      2 |  252 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  253 |  |
|      3 |  254 | `	ph7_value_int(pVal,32);` |
|      1 |  255 | `	SXUNUSED(pUserData);` |
|      3 |  256 |  |
|      - |  257 | `/*` |
|      - |  258 | ` * E_COMPILE_ERROR` |
|      - |  259 | ` * Expands 64` |
|      - |  260 | ` */` |
|      2 |  261 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  262 |  |
|      3 |  263 | `	ph7_value_int(pVal,64);` |
|      1 |  264 | `	SXUNUSED(pUserData);` |
|      3 |  265 |  |
|      - |  266 | `/*` |
|      - |  267 | ` * E_COMPILE_WARNING` |
|      - |  268 | ` * Expands 128` |
|      - |  269 | ` */` |
|      2 |  270 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  271 |  |
|      3 |  272 | `	ph7_value_int(pVal,128);` |
|      1 |  273 | `	SXUNUSED(pUserData);` |
|      3 |  274 |  |
|      - |  275 | `/*` |
|      - |  276 | ` * E_USER_ERROR` |
|      - |  277 | ` * Expands 256` |
|      - |  278 | ` */` |
|      4 |  279 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  280 |  |
|      6 |  281 | `	ph7_value_int(pVal,256);` |
|      2 |  282 | `	SXUNUSED(pUserData);` |
|      6 |  283 |  |
|      - |  284 | `/*` |
|      - |  285 | ` * E_USER_WARNING` |
|      - |  286 | ` * Expands 512` |
|      - |  287 | ` */` |
|      4 |  288 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  289 |  |
|      6 |  290 | `	ph7_value_int(pVal,512);` |
|      2 |  291 | `	SXUNUSED(pUserData);` |
|      6 |  292 |  |
|      - |  293 | `/*` |
|      - |  294 | ` * E_USER_NOTICE` |
|      - |  295 | ` * Expands 1024` |
|      - |  296 | ` */` |
|      6 |  297 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  298 |  |
|      8 |  299 | `	ph7_value_int(pVal,1024);` |
|      3 |  300 | `	SXUNUSED(pUserData);` |
|      8 |  301 |  |
|      - |  302 | `/*` |
|      - |  303 | ` * E_STRICT` |
|      - |  304 | ` * Expands 2048` |
|      - |  305 | ` */` |
|      2 |  306 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  307 |  |
|      3 |  308 | `	ph7_value_int(pVal,2048);` |
|      1 |  309 | `	SXUNUSED(pUserData);` |
|      3 |  310 |  |
|      - |  311 | `/*` |
|      - |  312 | ` * E_RECOVERABLE_ERROR` |
|      - |  313 | ` * Expands 4096` |
|      - |  314 | ` */` |
|      2 |  315 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  316 |  |
|      3 |  317 | `	ph7_value_int(pVal,4096);` |
|      1 |  318 | `	SXUNUSED(pUserData);` |
|      3 |  319 |  |
|      - |  320 | `/*` |
|      - |  321 | ` * E_DEPRECATED` |
|      - |  322 | ` * Expands 8192` |
|      - |  323 | ` */` |
|      4 |  324 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  325 |  |
|      5 |  326 | `	ph7_value_int(pVal,8192);` |
|      2 |  327 | `	SXUNUSED(pUserData);` |
|      5 |  328 |  |
|      - |  329 | `/*` |
|      - |  330 | ` * E_USER_DEPRECATED` |
|      - |  331 | ` *   Expands 16384.` |
|      - |  332 | ` */` |
|      2 |  333 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  334 |  |
|      3 |  335 | `	ph7_value_int(pVal,16384);` |
|      1 |  336 | `	SXUNUSED(pUserData);` |
|      3 |  337 |  |
|      - |  338 | `/*` |
|      - |  339 | ` * E_ALL` |
|      - |  340 | ` *  Expands 32767` |
|      - |  341 | ` */` |
|      2 |  342 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  343 |  |
|      3 |  344 | `	ph7_value_int(pVal,32767);` |
|      1 |  345 | `	SXUNUSED(pUserData);` |
|      3 |  346 |  |
|      - |  347 | `/*` |
|      - |  348 | ` * CASE_LOWER` |
|      - |  349 | ` *  Expands 0.` |
|      - |  350 | ` */` |
|      2 |  351 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  352 |  |
|      3 |  353 | `	ph7_value_int(pVal,0);` |
|      1 |  354 | `	SXUNUSED(pUserData);` |
|      3 |  355 |  |
|      - |  356 | `/*` |
|      - |  357 | ` * CASE_UPPER` |
|      - |  358 | ` *  Expands 1.` |
|      - |  359 | ` */` |
|      2 |  360 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  361 |  |
|      3 |  362 | `	ph7_value_int(pVal,1);` |
|      1 |  363 | `	SXUNUSED(pUserData);` |
|      3 |  364 |  |
|      - |  365 | `/*` |
|      - |  366 | ` * STR_PAD_LEFT` |
|      - |  367 | ` *  Expands 0.` |
|      - |  368 | ` */` |
|      4 |  369 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  370 |  |
|      5 |  371 | `	ph7_value_int(pVal,0);` |
|      2 |  372 | `	SXUNUSED(pUserData);` |
|      5 |  373 |  |
|      - |  374 | `/*` |
|      - |  375 | ` * STR_PAD_RIGHT` |
|      - |  376 | ` *  Expands 1.` |
|      - |  377 | ` */` |
|      4 |  378 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  379 |  |
|      5 |  380 | `	ph7_value_int(pVal,1);` |
|      2 |  381 | `	SXUNUSED(pUserData);` |
|      5 |  382 |  |
|      - |  383 | `/*` |
|      - |  384 | ` * STR_PAD_BOTH` |
|      - |  385 | ` *  Expands 2.` |
|      - |  386 | ` */` |
|      2 |  387 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  388 |  |
|      3 |  389 | `	ph7_value_int(pVal,2);` |
|      1 |  390 | `	SXUNUSED(pUserData);` |
|      3 |  391 |  |
|      - |  392 | `/*` |
|      - |  393 | ` * COUNT_NORMAL` |
|      - |  394 | ` *  Expands 0` |
|      - |  395 | ` */` |
|      2 |  396 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  397 |  |
|      3 |  398 | `	ph7_value_int(pVal,0);` |
|      1 |  399 | `	SXUNUSED(pUserData);` |
|      3 |  400 |  |
|      - |  401 | `/*` |
|      - |  402 | ` * COUNT_RECURSIVE` |
|      - |  403 | ` *  Expands 1.` |
|      - |  404 | ` */` |
|     22 |  405 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  406 |  |
|     23 |  407 | `	ph7_value_int(pVal,1);` |
|     11 |  408 | `	SXUNUSED(pUserData);` |
|     23 |  409 |  |
|      - |  410 | `/*` |
|      - |  411 | ` * SORT_ASC` |
|      - |  412 | ` *  Expands 1.` |
|      - |  413 | ` */` |
|      2 |  414 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  415 |  |
|      3 |  416 | `	ph7_value_int(pVal,1);` |
|      1 |  417 | `	SXUNUSED(pUserData);` |
|      3 |  418 |  |
|      - |  419 | `/*` |
|      - |  420 | ` * SORT_DESC` |
|      - |  421 | ` *  Expands 2.` |
|      - |  422 | ` */` |
|      2 |  423 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  424 |  |
|      3 |  425 | `	ph7_value_int(pVal,2);` |
|      1 |  426 | `	SXUNUSED(pUserData);` |
|      3 |  427 |  |
|      - |  428 | `/*` |
|      - |  429 | ` * SORT_REGULAR` |
|      - |  430 | ` *  Expands 3.` |
|      - |  431 | ` */` |
|      2 |  432 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  433 |  |
|      3 |  434 | `	ph7_value_int(pVal,3);` |
|      1 |  435 | `	SXUNUSED(pUserData);` |
|      3 |  436 |  |
|      - |  437 | `/*` |
|      - |  438 | ` * SORT_NUMERIC` |
|      - |  439 | ` *  Expands 4.` |
|      - |  440 | ` */` |
|      2 |  441 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  442 |  |
|      3 |  443 | `	ph7_value_int(pVal,4);` |
|      1 |  444 | `	SXUNUSED(pUserData);` |
|      3 |  445 |  |
|      - |  446 | `/*` |
|      - |  447 | ` * SORT_STRING` |
|      - |  448 | ` *  Expands 5.` |
|      - |  449 | ` */` |
|      4 |  450 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  451 |  |
|      5 |  452 | `	ph7_value_int(pVal,5);` |
|      2 |  453 | `	SXUNUSED(pUserData);` |
|      5 |  454 |  |
|      - |  455 | `/*` |
|      - |  456 | ` * PHP_ROUND_HALF_UP` |
|      - |  457 | ` *  Expands 1.` |
|      - |  458 | ` */` |
|      2 |  459 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  460 |  |
|      3 |  461 | `	ph7_value_int(pVal,1);` |
|      1 |  462 | `	SXUNUSED(pUserData);` |
|      3 |  463 |  |
|      - |  464 | `/*` |
|      - |  465 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  466 | ` *  Expands 2.` |
|      - |  467 | ` */` |
|      2 |  468 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  469 |  |
|      3 |  470 | `	ph7_value_int(pVal,2);` |
|      1 |  471 | `	SXUNUSED(pUserData);` |
|      3 |  472 |  |
|      - |  473 | `/*` |
|      - |  474 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  475 | ` *  Expands 3.` |
|      - |  476 | ` */` |
|      2 |  477 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  478 |  |
|      3 |  479 | `	ph7_value_int(pVal,3);` |
|      1 |  480 | `	SXUNUSED(pUserData);` |
|      3 |  481 |  |
|      - |  482 | `/*` |
|      - |  483 | ` * PHP_ROUND_HALF_ODD` |
|      - |  484 | ` *  Expands 4.` |
|      - |  485 | ` */` |
|      2 |  486 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  487 |  |
|      3 |  488 | `	ph7_value_int(pVal,4);` |
|      1 |  489 | `	SXUNUSED(pUserData);` |
|      3 |  490 |  |
|      - |  491 | `/*` |
|      - |  492 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  493 | ` *  Expand 0x01` |
|      - |  494 | ` * NOTE:` |
|      - |  495 | ` *  The expanded value must be a power of two.` |
|      - |  496 | ` */` |
|      2 |  497 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  498 |  |
|      3 |  499 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  500 | `	SXUNUSED(pUserData);` |
|      3 |  501 |  |
|      - |  502 | `/*` |
|      - |  503 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  504 | ` *  Expand 0x02` |
|      - |  505 | ` * NOTE:` |
|      - |  506 | ` *  The expanded value must be a power of two.` |
|      - |  507 | ` */` |
|      2 |  508 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  509 |  |
|      3 |  510 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  511 | `	SXUNUSED(pUserData);` |
|      3 |  512 |  |
|      - |  513 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  514 | `/*` |
|      - |  515 | ` * M_PI` |
|      - |  516 | ` *  Expand the value of pi.` |
|      - |  517 | ` */` |
|      2 |  518 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  519 |  |
|      1 |  520 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  521 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  522 |  |
|      - |  523 | `/*` |
|      - |  524 | ` * M_E` |
|      - |  525 | ` *  Expand 2.7182818284590452354` |
|      - |  526 | ` */` |
|      2 |  527 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  528 |  |
|      1 |  529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  530 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  531 |  |
|      - |  532 | `/*` |
|      - |  533 | ` * M_LOG2E` |
|      - |  534 | ` *  Expand 2.7182818284590452354` |
|      - |  535 | ` */` |
|      2 |  536 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  537 |  |
|      1 |  538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  539 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  540 |  |
|      - |  541 | `/*` |
|      - |  542 | ` * M_LOG10E` |
|      - |  543 | ` *  Expand 0.4342944819032518276` |
|      - |  544 | ` */` |
|      2 |  545 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  546 |  |
|      1 |  547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  548 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  549 |  |
|      - |  550 | `/*` |
|      - |  551 | ` * M_LN2` |
|      - |  552 | ` *  Expand 	0.69314718055994530942` |
|      - |  553 | ` */` |
|      2 |  554 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  555 |  |
|      1 |  556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  557 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  558 |  |
|      - |  559 | `/*` |
|      - |  560 | ` * M_LN10` |
|      - |  561 | ` *  Expand 	2.30258509299404568402` |
|      - |  562 | ` */` |
|      2 |  563 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  564 |  |
|      1 |  565 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  566 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  567 |  |
|      - |  568 | `/*` |
|      - |  569 | ` * M_PI_2` |
|      - |  570 | ` *  Expand 	1.57079632679489661923` |
|      - |  571 | ` */` |
|      2 |  572 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  573 |  |
|      1 |  574 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  575 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  576 |  |
|      - |  577 | `/*` |
|      - |  578 | ` * M_PI_4` |
|      - |  579 | ` *  Expand 	0.78539816339744830962` |
|      - |  580 | ` */` |
|      2 |  581 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  582 |  |
|      1 |  583 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  584 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  585 |  |
|      - |  586 | `/*` |
|      - |  587 | ` * M_1_PI` |
|      - |  588 | ` *  Expand 	0.31830988618379067154` |
|      - |  589 | ` */` |
|      2 |  590 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  591 |  |
|      1 |  592 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  593 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  594 |  |
|      - |  595 | `/*` |
|      - |  596 | ` * M_2_PI` |
|      - |  597 | ` *  Expand 0.63661977236758134308` |
|      - |  598 | ` */` |
|      4 |  599 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  600 |  |
|      2 |  601 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  602 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  603 |  |
|      - |  604 | `/*` |
|      - |  605 | ` * M_SQRTPI` |
|      - |  606 | ` *  Expand 1.77245385090551602729` |
|      - |  607 | ` */` |
|      2 |  608 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  609 |  |
|      1 |  610 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  611 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  612 |  |
|      - |  613 | `/*` |
|      - |  614 | ` * M_2_SQRTPI` |
|      - |  615 | ` *  Expand 	1.12837916709551257390` |
|      - |  616 | ` */` |
|      2 |  617 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  618 |  |
|      1 |  619 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  620 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  621 |  |
|      - |  622 | `/*` |
|      - |  623 | ` * M_SQRT2` |
|      - |  624 | ` *  Expand 	1.41421356237309504880` |
|      - |  625 | ` */` |
|      2 |  626 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  627 |  |
|      1 |  628 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  629 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  630 |  |
|      - |  631 | `/*` |
|      - |  632 | ` * M_SQRT3` |
|      - |  633 | ` *  Expand 	1.73205080756887729352` |
|      - |  634 | ` */` |
|      2 |  635 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  636 |  |
|      1 |  637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  638 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  639 |  |
|      - |  640 | `/*` |
|      - |  641 | ` * M_SQRT1_2` |
|      - |  642 | ` *  Expand 	0.70710678118654752440` |
|      - |  643 | ` */` |
|      2 |  644 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  645 |  |
|      1 |  646 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  647 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  648 |  |
|      - |  649 | `/*` |
|      - |  650 | ` * M_LNPI` |
|      - |  651 | ` *  Expand 	1.14472988584940017414` |
|      - |  652 | ` */` |
|      2 |  653 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  654 |  |
|      1 |  655 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  656 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  657 |  |
|      - |  658 | `/*` |
|      - |  659 | ` * M_EULER` |
|      - |  660 | ` *  Expand  0.57721566490153286061` |
|      - |  661 | ` */` |
|      2 |  662 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  663 |  |
|      1 |  664 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  665 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  666 |  |
|      - |  667 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  668 | `/*` |
|      - |  669 | ` * DATE_ATOM` |
|      - |  670 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  671 | ` */` |
|      2 |  672 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  673 |  |
|      1 |  674 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  675 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  676 |  |
|      - |  677 | `/*` |
|      - |  678 | ` * DATE_COOKIE` |
|      - |  679 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  680 | ` */` |
|      2 |  681 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  682 |  |
|      1 |  683 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  684 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  685 |  |
|      - |  686 | `/*` |
|      - |  687 | ` * DATE_ISO8601` |
|      - |  688 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  689 | ` */` |
|      2 |  690 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  691 |  |
|      1 |  692 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  693 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  694 |  |
|      - |  695 | `/*` |
|      - |  696 | ` * DATE_RFC822` |
|      - |  697 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  698 | ` */` |
|      2 |  699 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  700 |  |
|      1 |  701 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  702 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  703 |  |
|      - |  704 | `/*` |
|      - |  705 | ` * DATE_RFC850` |
|      - |  706 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  707 | ` */` |
|      2 |  708 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  709 |  |
|      1 |  710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  711 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  712 |  |
|      - |  713 | `/*` |
|      - |  714 | ` * DATE_RFC1036` |
|      - |  715 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  716 | ` */` |
|      2 |  717 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  718 |  |
|      1 |  719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  720 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  721 |  |
|      - |  722 | `/*` |
|      - |  723 | ` * DATE_RFC1123` |
|      - |  724 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  725 | ` */` |
|      2 |  726 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  727 |  |
|      1 |  728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  729 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  730 |  |
|      - |  731 | `/*` |
|      - |  732 | ` * DATE_RFC2822` |
|      - |  733 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  734 | ` */` |
|      2 |  735 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  736 |  |
|      1 |  737 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  738 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  739 |  |
|      - |  740 | `/*` |
|      - |  741 | ` * DATE_RSS` |
|      - |  742 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  743 | ` */` |
|      2 |  744 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  745 |  |
|      1 |  746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  747 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  748 |  |
|      - |  749 | `/*` |
|      - |  750 | ` * DATE_W3C` |
|      - |  751 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  752 | ` */` |
|      2 |  753 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  754 |  |
|      1 |  755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  756 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  757 |  |
|      - |  758 | `/*` |
|      - |  759 | ` * ENT_COMPAT` |
|      - |  760 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  761 | ` */` |
|      2 |  762 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  763 |  |
|      1 |  764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  765 | `	ph7_value_int(pVal,0x01);` |
|      3 |  766 |  |
|      - |  767 | `/*` |
|      - |  768 | ` * ENT_QUOTES` |
|      - |  769 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  770 | ` */` |
|     16 |  771 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  772 |  |
|      8 |  773 | `	SXUNUSED(pUserData); /* cc warning */` |
|     17 |  774 | `	ph7_value_int(pVal,0x02);` |
|     17 |  775 |  |
|      - |  776 | `/*` |
|      - |  777 | ` * ENT_NOQUOTES` |
|      - |  778 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  779 | ` */` |
|     12 |  780 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  781 |  |
|      6 |  782 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  783 | `	ph7_value_int(pVal,0x04);` |
|     13 |  784 |  |
|      - |  785 | `/*` |
|      - |  786 | ` * ENT_IGNORE` |
|      - |  787 | ` *  Expand 0x08 (Must be a power of two)` |
|      - |  788 | ` */` |
|      2 |  789 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  790 |  |
|      1 |  791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  792 | `	ph7_value_int(pVal,0x08);` |
|      3 |  793 |  |
|      - |  794 | `/*` |
|      - |  795 | ` * ENT_SUBSTITUTE` |
|      - |  796 | ` *  Expand 0x10 (Must be a power of two)` |
|      - |  797 | ` */` |
|      2 |  798 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  799 |  |
|      1 |  800 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  801 | `	ph7_value_int(pVal,0x10);` |
|      3 |  802 |  |
|      - |  803 | `/*` |
|      - |  804 | ` * ENT_DISALLOWED` |
|      - |  805 | ` *  Expand 0x20 (Must be a power of two)` |
|      - |  806 | ` */` |
|      2 |  807 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  808 |  |
|      1 |  809 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  810 | `	ph7_value_int(pVal,0x20);` |
|      3 |  811 |  |
|      - |  812 | `/*` |
|      - |  813 | ` * ENT_HTML401` |
|      - |  814 | ` *  Expand 0x40 (Must be a power of two)` |
|      - |  815 | ` */` |
|      2 |  816 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  817 |  |
|      1 |  818 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  819 | `	ph7_value_int(pVal,0x40);` |
|      3 |  820 |  |
|      - |  821 | `/*` |
|      - |  822 | ` * ENT_XML1` |
|      - |  823 | ` *  Expand 0x80 (Must be a power of two)` |
|      - |  824 | ` */` |
|      2 |  825 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  826 |  |
|      1 |  827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  828 | `	ph7_value_int(pVal,0x80);` |
|      3 |  829 |  |
|      - |  830 | `/*` |
|      - |  831 | ` * ENT_XHTML` |
|      - |  832 | ` *  Expand 0x100 (Must be a power of two)` |
|      - |  833 | ` */` |
|      2 |  834 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  835 |  |
|      1 |  836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  837 | `	ph7_value_int(pVal,0x100);` |
|      3 |  838 |  |
|      - |  839 | `/*` |
|      - |  840 | ` * ENT_HTML5` |
|      - |  841 | ` *  Expand 0x200 (Must be a power of two)` |
|      - |  842 | ` */` |
|      2 |  843 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  844 |  |
|      1 |  845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  846 | `	ph7_value_int(pVal,0x200);` |
|      3 |  847 |  |
|      - |  848 | `/*` |
|      - |  849 | ` * ISO-8859-1` |
|      - |  850 | ` * ISO_8859_1` |
|      - |  851 | ` *   Expand 1` |
|      - |  852 | ` */` |
|      2 |  853 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  854 |  |
|      1 |  855 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  856 | `	ph7_value_int(pVal,1);` |
|      3 |  857 |  |
|      - |  858 | `/*` |
|      - |  859 | ` * UTF-8` |
|      - |  860 | ` * UTF8` |
|      - |  861 | ` *  Expand 2` |
|      - |  862 | ` */` |
|      2 |  863 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  864 |  |
|      1 |  865 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  866 | `	ph7_value_int(pVal,1);` |
|      3 |  867 |  |
|      - |  868 | `/*` |
|      - |  869 | ` * HTML_ENTITIES` |
|      - |  870 | ` *  Expand 1` |
|      - |  871 | ` */` |
|      2 |  872 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  873 |  |
|      1 |  874 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  875 | `	ph7_value_int(pVal,1);` |
|      3 |  876 |  |
|      - |  877 | `/*` |
|      - |  878 | ` * HTML_SPECIALCHARS` |
|      - |  879 | ` *  Expand 2` |
|      - |  880 | ` */` |
|      2 |  881 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  882 |  |
|      1 |  883 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  884 | `	ph7_value_int(pVal,2);` |
|      3 |  885 |  |
|      - |  886 | `/*` |
|      - |  887 | ` * PHP_URL_SCHEME.` |
|      - |  888 | ` * Expand 1` |
|      - |  889 | ` */` |
|      2 |  890 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  891 |  |
|      1 |  892 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  893 | `	ph7_value_int(pVal,1);` |
|      3 |  894 |  |
|      - |  895 | `/*` |
|      - |  896 | ` * PHP_URL_HOST.` |
|      - |  897 | ` * Expand 2` |
|      - |  898 | ` */` |
|      2 |  899 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  900 |  |
|      1 |  901 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  902 | `	ph7_value_int(pVal,2);` |
|      3 |  903 |  |
|      - |  904 | `/*` |
|      - |  905 | ` * PHP_URL_PORT.` |
|      - |  906 | ` * Expand 3` |
|      - |  907 | ` */` |
|      2 |  908 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  909 |  |
|      1 |  910 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  911 | `	ph7_value_int(pVal,3);` |
|      3 |  912 |  |
|      - |  913 | `/*` |
|      - |  914 | ` * PHP_URL_USER.` |
|      - |  915 | ` * Expand 4` |
|      - |  916 | ` */` |
|      2 |  917 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  918 |  |
|      1 |  919 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  920 | `	ph7_value_int(pVal,4);` |
|      3 |  921 |  |
|      - |  922 | `/*` |
|      - |  923 | ` * PHP_URL_PASS.` |
|      - |  924 | ` * Expand 5` |
|      - |  925 | ` */` |
|      2 |  926 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  927 |  |
|      1 |  928 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  929 | `	ph7_value_int(pVal,5);` |
|      3 |  930 |  |
|      - |  931 | `/*` |
|      - |  932 | ` * PHP_URL_PATH.` |
|      - |  933 | ` * Expand 6` |
|      - |  934 | ` */` |
|      2 |  935 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  936 |  |
|      1 |  937 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  938 | `	ph7_value_int(pVal,6);` |
|      3 |  939 |  |
|      - |  940 | `/*` |
|      - |  941 | ` * PHP_URL_QUERY.` |
|      - |  942 | ` * Expand 7` |
|      - |  943 | ` */` |
|      2 |  944 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  945 |  |
|      1 |  946 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  947 | `	ph7_value_int(pVal,7);` |
|      3 |  948 |  |
|      - |  949 | `/*` |
|      - |  950 | ` * PHP_URL_FRAGMENT.` |
|      - |  951 | ` * Expand 8` |
|      - |  952 | ` */` |
|      2 |  953 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  954 |  |
|      1 |  955 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  956 | `	ph7_value_int(pVal,8);` |
|      3 |  957 |  |
|      - |  958 | `/*` |
|      - |  959 | ` * PHP_QUERY_RFC1738` |
|      - |  960 | ` * Expand 1` |
|      - |  961 | ` */` |
|      2 |  962 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  963 |  |
|      1 |  964 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  965 | `	ph7_value_int(pVal,1);` |
|      3 |  966 |  |
|      - |  967 | `/*` |
|      - |  968 | ` * PHP_QUERY_RFC3986` |
|      - |  969 | ` * Expand 1` |
|      - |  970 | ` */` |
|      2 |  971 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  972 |  |
|      1 |  973 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  974 | `	ph7_value_int(pVal,2);` |
|      3 |  975 |  |
|      - |  976 | `/*` |
|      - |  977 | ` * FNM_NOESCAPE` |
|      - |  978 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  979 | ` */` |
|      2 |  980 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  981 |  |
|      1 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  983 | `	ph7_value_int(pVal,0x01);` |
|      3 |  984 |  |
|      - |  985 | `/*` |
|      - |  986 | ` * FNM_PATHNAME` |
|      - |  987 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  988 | ` */` |
|      2 |  989 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  990 |  |
|      1 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  992 | `	ph7_value_int(pVal,0x02);` |
|      3 |  993 |  |
|      - |  994 | `/*` |
|      - |  995 | ` * FNM_PERIOD` |
|      - |  996 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  997 | ` */` |
|      6 |  998 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  999 |  |
|      3 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1001 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1002 |  |
|      - | 1003 | `/*` |
|      - | 1004 | ` * FNM_CASEFOLD` |
|      - | 1005 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1006 | ` */` |
|      4 | 1007 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1008 |  |
|      2 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1010 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1011 |  |
|      - | 1012 | `/*` |
|      - | 1013 | ` * PATHINFO_DIRNAME` |
|      - | 1014 | ` *  Expand 1.` |
|      - | 1015 | ` */` |
|      4 | 1016 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1017 |  |
|      2 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1019 | `	ph7_value_int(pVal,1);` |
|      5 | 1020 |  |
|      - | 1021 | `/*` |
|      - | 1022 | ` * PATHINFO_BASENAME` |
|      - | 1023 | ` *  Expand 2.` |
|      - | 1024 | ` */` |
|      4 | 1025 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1026 |  |
|      2 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1028 | `	ph7_value_int(pVal,2);` |
|      5 | 1029 |  |
|      - | 1030 | `/*` |
|      - | 1031 | ` * PATHINFO_EXTENSION` |
|      - | 1032 | ` *  Expand 3.` |
|      - | 1033 | ` */` |
|   3024 | 1034 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1035 |  |
|   1512 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3026 | 1037 | `	ph7_value_int(pVal,3);` |
|   3026 | 1038 |  |
|      - | 1039 | `/*` |
|      - | 1040 | ` * PATHINFO_FILENAME` |
|      - | 1041 | ` *  Expand 4.` |
|      - | 1042 | ` */` |
|   3020 | 1043 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1044 |  |
|   1510 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3022 | 1046 | `	ph7_value_int(pVal,4);` |
|   3022 | 1047 |  |
|      - | 1048 | `/*` |
|      - | 1049 | ` * ASSERT_ACTIVE.` |
|      - | 1050 | ` *  Expand the value of PH7_ASSERT_ACTIVE defined in ph7Int.h` |
|      - | 1051 | ` */` |
|      2 | 1052 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1053 |  |
|      1 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1055 | `	ph7_value_int(pVal,PH7_ASSERT_DISABLE);` |
|      3 | 1056 |  |
|      - | 1057 | `/*` |
|      - | 1058 | ` * ASSERT_WARNING.` |
|      - | 1059 | ` *  Expand the value of PH7_ASSERT_WARNING defined in ph7Int.h` |
|      - | 1060 | ` */` |
|      2 | 1061 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1062 |  |
|      1 | 1063 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1064 | `	ph7_value_int(pVal,PH7_ASSERT_WARNING);` |
|      3 | 1065 |  |
|      - | 1066 | `/*` |
|      - | 1067 | ` * ASSERT_BAIL.` |
|      - | 1068 | ` *  Expand the value of PH7_ASSERT_BAIL defined in ph7Int.h` |
|      - | 1069 | ` */` |
|      2 | 1070 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1071 |  |
|      1 | 1072 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1073 | `	ph7_value_int(pVal,PH7_ASSERT_BAIL);` |
|      3 | 1074 |  |
|      - | 1075 | `/*` |
|      - | 1076 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1077 | ` *  Expand the value of PH7_ASSERT_QUIET_EVAL defined in ph7Int.h` |
|      - | 1078 | ` */` |
|      2 | 1079 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1080 |  |
|      1 | 1081 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1082 | `	ph7_value_int(pVal,PH7_ASSERT_QUIET_EVAL);` |
|      3 | 1083 |  |
|      - | 1084 | `/*` |
|      - | 1085 | ` * ASSERT_CALLBACK.` |
|      - | 1086 | ` *  Expand the value of PH7_ASSERT_CALLBACK defined in ph7Int.h` |
|      - | 1087 | ` */` |
|      2 | 1088 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1089 |  |
|      1 | 1090 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1091 | `	ph7_value_int(pVal,PH7_ASSERT_CALLBACK);` |
|      3 | 1092 |  |
|      - | 1093 | `/*` |
|      - | 1094 | ` * SEEK_SET.` |
|      - | 1095 | ` *  Expand 0` |
|      - | 1096 | ` */` |
|      2 | 1097 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1098 |  |
|      1 | 1099 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1100 | `	ph7_value_int(pVal,0);` |
|      3 | 1101 |  |
|      - | 1102 | `/*` |
|      - | 1103 | ` * SEEK_CUR.` |
|      - | 1104 | ` *  Expand 1` |
|      - | 1105 | ` */` |
|      2 | 1106 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1107 |  |
|      1 | 1108 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1109 | `	ph7_value_int(pVal,1);` |
|      3 | 1110 |  |
|      - | 1111 | `/*` |
|      - | 1112 | ` * SEEK_END.` |
|      - | 1113 | ` *  Expand 2` |
|      - | 1114 | ` */` |
|      2 | 1115 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1116 |  |
|      1 | 1117 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1118 | `	ph7_value_int(pVal,2);` |
|      3 | 1119 |  |
|      - | 1120 | `/*` |
|      - | 1121 | ` * LOCK_SH.` |
|      - | 1122 | ` *  Expand 2` |
|      - | 1123 | ` */` |
|      2 | 1124 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1125 |  |
|      1 | 1126 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1127 | `	ph7_value_int(pVal,1);` |
|      3 | 1128 |  |
|      - | 1129 | `/*` |
|      - | 1130 | ` * LOCK_NB.` |
|      - | 1131 | ` *  Expand 5` |
|      - | 1132 | ` */` |
|      2 | 1133 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1134 |  |
|      1 | 1135 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1136 | `	ph7_value_int(pVal,5);` |
|      3 | 1137 |  |
|      - | 1138 | `/*` |
|      - | 1139 | ` * LOCK_EX.` |
|      - | 1140 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1141 | ` */` |
|      4 | 1142 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1143 |  |
|      2 | 1144 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1145 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1146 |  |
|      - | 1147 | `/*` |
|      - | 1148 | ` * LOCK_UN.` |
|      - | 1149 | ` *  Expand 0` |
|      - | 1150 | ` */` |
|      4 | 1151 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1152 |  |
|      2 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1154 | `	ph7_value_int(pVal,0);` |
|      5 | 1155 |  |
|      - | 1156 | `/*` |
|      - | 1157 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1158 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1159 | ` */` |
|      2 | 1160 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1161 |  |
|      1 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1163 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1164 |  |
|      - | 1165 | `/*` |
|      - | 1166 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1167 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1168 | ` */` |
|      2 | 1169 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1170 |  |
|      1 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1172 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1173 |  |
|      - | 1174 | `/*` |
|      - | 1175 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1176 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1177 | ` */` |
|      2 | 1178 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1179 |  |
|      1 | 1180 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1181 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1182 |  |
|      - | 1183 | `/*` |
|      - | 1184 | ` * FILE_APPEND` |
|      - | 1185 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1186 | ` */` |
|      2 | 1187 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1188 |  |
|      1 | 1189 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1190 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1191 |  |
|      - | 1192 | `/*` |
|      - | 1193 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1194 | ` *  Expand 0` |
|      - | 1195 | ` */` |
|   1486 | 1196 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1197 |  |
|    743 | 1198 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1488 | 1199 | `	ph7_value_int(pVal,0);` |
|   1488 | 1200 |  |
|      - | 1201 | `/*` |
|      - | 1202 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1203 | ` *  Expand 1` |
|      - | 1204 | ` */` |
|    744 | 1205 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1206 |  |
|    372 | 1207 | `	SXUNUSED(pUserData); /* cc warning */` |
|    746 | 1208 | `	ph7_value_int(pVal,1);` |
|    746 | 1209 |  |
|      - | 1210 | `/*` |
|      - | 1211 | ` * SCANDIR_SORT_NONE` |
|      - | 1212 | ` *  Expand 2` |
|      - | 1213 | ` */` |
|      2 | 1214 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1215 |  |
|      1 | 1216 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1217 | `	ph7_value_int(pVal,2);` |
|      3 | 1218 |  |
|      - | 1219 | `/*` |
|      - | 1220 | ` * GLOB_MARK` |
|      - | 1221 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1222 | ` */` |
|      2 | 1223 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1224 |  |
|      1 | 1225 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1226 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1227 |  |
|      - | 1228 | `/*` |
|      - | 1229 | ` * GLOB_NOSORT` |
|      - | 1230 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1231 | ` */` |
|      2 | 1232 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1233 |  |
|      1 | 1234 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1235 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1236 |  |
|      - | 1237 | `/*` |
|      - | 1238 | ` * GLOB_NOCHECK` |
|      - | 1239 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1240 | ` */` |
|      2 | 1241 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1242 |  |
|      1 | 1243 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1244 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1245 |  |
|      - | 1246 | `/*` |
|      - | 1247 | ` * GLOB_NOESCAPE` |
|      - | 1248 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1249 | ` */` |
|      2 | 1250 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1251 |  |
|      1 | 1252 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1253 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1254 |  |
|      - | 1255 | `/*` |
|      - | 1256 | ` * GLOB_BRACE` |
|      - | 1257 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1258 | ` */` |
|      2 | 1259 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1260 |  |
|      1 | 1261 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1262 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1263 |  |
|      - | 1264 | `/*` |
|      - | 1265 | ` * GLOB_ONLYDIR` |
|      - | 1266 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1267 | ` */` |
|      2 | 1268 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1269 |  |
|      1 | 1270 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1271 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1272 |  |
|      - | 1273 | `/*` |
|      - | 1274 | ` * GLOB_ERR` |
|      - | 1275 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1276 | ` */` |
|      2 | 1277 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1278 |  |
|      1 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1280 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1281 |  |
|      - | 1282 | `/*` |
|      - | 1283 | ` * STDIN` |
|      - | 1284 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1285 | ` */` |
|      2 | 1286 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1287 |  |
|      3 | 1288 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1289 | `	void *pResource;` |
|      3 | 1290 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1291 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1292 |  |
|      - | 1293 | `/*` |
|      - | 1294 | ` * STDOUT` |
|      - | 1295 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1296 | ` */` |
|      2 | 1297 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1298 |  |
|      3 | 1299 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1300 | `	void *pResource;` |
|      3 | 1301 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1302 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1303 |  |
|      - | 1304 | `/*` |
|      - | 1305 | ` * STDERR` |
|      - | 1306 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1307 | ` */` |
|      2 | 1308 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1309 |  |
|      3 | 1310 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1311 | `	void *pResource;` |
|      3 | 1312 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1313 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1314 |  |
|      - | 1315 | `/*` |
|      - | 1316 | ` * INI_SCANNER_NORMAL` |
|      - | 1317 | ` *   Expand 1` |
|      - | 1318 | ` */` |
|      2 | 1319 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1320 |  |
|      1 | 1321 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1322 | `	ph7_value_int(pVal,1);` |
|      3 | 1323 |  |
|      - | 1324 | `/*` |
|      - | 1325 | ` * INI_SCANNER_RAW` |
|      - | 1326 | ` *   Expand 2` |
|      - | 1327 | ` */` |
|      2 | 1328 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1329 |  |
|      1 | 1330 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1331 | `	ph7_value_int(pVal,2);` |
|      3 | 1332 |  |
|      - | 1333 | `/*` |
|      - | 1334 | ` * EXTR_OVERWRITE` |
|      - | 1335 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1336 | ` */` |
|      2 | 1337 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1338 |  |
|      1 | 1339 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1340 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1341 |  |
|      - | 1342 | `/*` |
|      - | 1343 | ` * EXTR_SKIP` |
|      - | 1344 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1345 | ` */` |
|      2 | 1346 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1347 |  |
|      1 | 1348 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1349 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1350 |  |
|      - | 1351 | `/*` |
|      - | 1352 | ` * EXTR_PREFIX_SAME` |
|      - | 1353 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1354 | ` */` |
|      2 | 1355 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1356 |  |
|      1 | 1357 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1358 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1359 |  |
|      - | 1360 | `/*` |
|      - | 1361 | ` * EXTR_PREFIX_ALL` |
|      - | 1362 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1363 | ` */` |
|      2 | 1364 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1365 |  |
|      1 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1367 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1368 |  |
|      - | 1369 | `/*` |
|      - | 1370 | ` * EXTR_PREFIX_INVALID` |
|      - | 1371 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1372 | ` */` |
|      2 | 1373 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1374 |  |
|      1 | 1375 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1376 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1377 |  |
|      - | 1378 | `/*` |
|      - | 1379 | ` * EXTR_IF_EXISTS` |
|      - | 1380 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1381 | ` */` |
|      2 | 1382 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1383 |  |
|      1 | 1384 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1385 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1386 |  |
|      - | 1387 | `/*` |
|      - | 1388 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1389 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1390 | ` */` |
|      2 | 1391 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1392 |  |
|      1 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1394 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1395 |  |
|      - | 1396 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1397 | `/*` |
|      - | 1398 | ` * XML_ERROR_NONE` |
|      - | 1399 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1400 | ` */` |
|      2 | 1401 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1402 |  |
|      1 | 1403 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1404 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1405 |  |
|      - | 1406 | `/*` |
|      - | 1407 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1408 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1409 | ` */` |
|      2 | 1410 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1411 |  |
|      1 | 1412 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1413 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1414 |  |
|      - | 1415 | `/*` |
|      - | 1416 | ` * XML_ERROR_SYNTAX` |
|      - | 1417 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1418 | ` */` |
|      2 | 1419 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1420 |  |
|      1 | 1421 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1422 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1423 |  |
|      - | 1424 | `/*` |
|      - | 1425 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1426 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1427 | ` */` |
|      2 | 1428 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1429 |  |
|      1 | 1430 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1431 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1432 |  |
|      - | 1433 | `/*` |
|      - | 1434 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1435 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1436 | ` */` |
|      2 | 1437 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1438 |  |
|      1 | 1439 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1440 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1441 |  |
|      - | 1442 | `/*` |
|      - | 1443 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1444 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1445 | ` */` |
|      2 | 1446 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1447 |  |
|      1 | 1448 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1449 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1450 |  |
|      - | 1451 | `/*` |
|      - | 1452 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1453 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1454 | ` */` |
|      2 | 1455 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1456 |  |
|      1 | 1457 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1458 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1459 |  |
|      - | 1460 | `/*` |
|      - | 1461 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1462 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1463 | ` */` |
|      2 | 1464 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1465 |  |
|      1 | 1466 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1467 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1468 |  |
|      - | 1469 | `/*` |
|      - | 1470 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1471 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1472 | ` */` |
|      2 | 1473 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1474 |  |
|      1 | 1475 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1476 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1477 |  |
|      - | 1478 | `/*` |
|      - | 1479 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1480 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1481 | ` */` |
|      2 | 1482 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1483 |  |
|      1 | 1484 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1485 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1486 |  |
|      - | 1487 | `/*` |
|      - | 1488 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1489 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1490 | ` */` |
|      2 | 1491 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1492 |  |
|      1 | 1493 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1494 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1495 |  |
|      - | 1496 | `/*` |
|      - | 1497 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1498 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1499 | ` */` |
|      2 | 1500 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1501 |  |
|      1 | 1502 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1503 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1504 |  |
|      - | 1505 | `/*` |
|      - | 1506 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1507 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1508 | ` */` |
|      2 | 1509 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1510 |  |
|      1 | 1511 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1512 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1513 |  |
|      - | 1514 | `/*` |
|      - | 1515 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1516 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1517 | ` */` |
|      2 | 1518 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1519 |  |
|      1 | 1520 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1521 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1522 |  |
|      - | 1523 | `/*` |
|      - | 1524 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1525 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1526 | ` */` |
|      2 | 1527 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1528 |  |
|      1 | 1529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1530 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1531 |  |
|      - | 1532 | `/*` |
|      - | 1533 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1534 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1535 | ` */` |
|      2 | 1536 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1537 |  |
|      1 | 1538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1539 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1540 |  |
|      - | 1541 | `/*` |
|      - | 1542 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1543 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1544 | ` */` |
|      2 | 1545 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1546 |  |
|      1 | 1547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1548 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1549 |  |
|      - | 1550 | `/*` |
|      - | 1551 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1552 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1553 | ` */` |
|      2 | 1554 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1555 |  |
|      1 | 1556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1557 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1558 |  |
|      - | 1559 | `/*` |
|      - | 1560 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1561 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1562 | ` */` |
|      2 | 1563 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1564 |  |
|      1 | 1565 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1566 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1567 |  |
|      - | 1568 | `/*` |
|      - | 1569 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1570 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1571 | ` */` |
|      2 | 1572 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1573 |  |
|      1 | 1574 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1575 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1576 |  |
|      - | 1577 | `/*` |
|      - | 1578 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1579 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1580 | ` */` |
|      2 | 1581 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1582 |  |
|      1 | 1583 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1584 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1585 |  |
|      - | 1586 | `/*` |
|      - | 1587 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1588 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1589 | ` */` |
|      2 | 1590 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1591 |  |
|      1 | 1592 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1593 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1594 |  |
|      - | 1595 | `/*` |
|      - | 1596 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1597 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1598 | ` */` |
|      2 | 1599 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1600 |  |
|      1 | 1601 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1602 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1603 |  |
|      - | 1604 | `/*` |
|      - | 1605 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1606 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1607 | ` */` |
|      4 | 1608 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1609 |  |
|      2 | 1610 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1611 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1612 |  |
|      - | 1613 | `/*` |
|      - | 1614 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1615 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1616 | ` */` |
|      2 | 1617 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1618 |  |
|      1 | 1619 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1620 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1621 |  |
|      - | 1622 | `/*` |
|      - | 1623 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1624 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1625 | ` */` |
|      4 | 1626 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1627 |  |
|      2 | 1628 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1629 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1630 |  |
|      - | 1631 | `/*` |
|      - | 1632 | ` * XML_SAX_IMPL.` |
|      - | 1633 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1634 | ` */` |
|      2 | 1635 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1636 |  |
|      1 | 1637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1638 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1639 |  |
|      - | 1640 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1641 | `/*` |
|      - | 1642 | ` * JSON_HEX_TAG.` |
|      - | 1643 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1644 | ` */` |
|      2 | 1645 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1646 |  |
|      1 | 1647 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1648 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1649 |  |
|      - | 1650 | `/*` |
|      - | 1651 | ` * JSON_HEX_AMP.` |
|      - | 1652 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1653 | ` */` |
|      2 | 1654 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1655 |  |
|      1 | 1656 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1657 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1658 |  |
|      - | 1659 | `/*` |
|      - | 1660 | ` * JSON_HEX_APOS.` |
|      - | 1661 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1662 | ` */` |
|      2 | 1663 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1664 |  |
|      1 | 1665 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1666 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1667 |  |
|      - | 1668 | `/*` |
|      - | 1669 | ` * JSON_HEX_QUOT.` |
|      - | 1670 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1671 | ` */` |
|      2 | 1672 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1673 |  |
|      1 | 1674 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1675 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1676 |  |
|      - | 1677 | `/*` |
|      - | 1678 | ` * JSON_FORCE_OBJECT.` |
|      - | 1679 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1680 | ` */` |
|      2 | 1681 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1682 |  |
|      1 | 1683 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1684 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      3 | 1685 |  |
|      - | 1686 | `/*` |
|      - | 1687 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1688 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1689 | ` */` |
|      2 | 1690 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1691 |  |
|      1 | 1692 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1693 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1694 |  |
|      - | 1695 | `/*` |
|      - | 1696 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1697 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1698 | ` */` |
|      2 | 1699 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1700 |  |
|      1 | 1701 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1702 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1703 |  |
|      - | 1704 | `/*` |
|      - | 1705 | ` * JSON_PRETTY_PRINT.` |
|      - | 1706 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1707 | ` */` |
|      2 | 1708 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1709 |  |
|      1 | 1710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1711 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1712 |  |
|      - | 1713 | `/*` |
|      - | 1714 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1715 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1716 | ` */` |
|      2 | 1717 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1718 |  |
|      1 | 1719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1720 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1721 |  |
|      - | 1722 | `/*` |
|      - | 1723 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1724 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1725 | ` */` |
|      2 | 1726 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1727 |  |
|      1 | 1728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1729 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1730 |  |
|      - | 1731 | `/*` |
|      - | 1732 | ` * JSON_ERROR_NONE.` |
|      - | 1733 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1734 | ` */` |
|      4 | 1735 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1736 |  |
|      2 | 1737 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1738 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1739 |  |
|      - | 1740 | `/*` |
|      - | 1741 | ` * JSON_ERROR_DEPTH.` |
|      - | 1742 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1743 | ` */` |
|      2 | 1744 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1745 |  |
|      1 | 1746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1747 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1748 |  |
|      - | 1749 | `/*` |
|      - | 1750 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1751 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1752 | ` */` |
|      2 | 1753 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1754 |  |
|      1 | 1755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1756 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1757 |  |
|      - | 1758 | `/*` |
|      - | 1759 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1760 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1761 | ` */` |
|      2 | 1762 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1763 |  |
|      1 | 1764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1765 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1766 |  |
|      - | 1767 | `/*` |
|      - | 1768 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1769 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1770 | ` */` |
|      2 | 1771 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1772 |  |
|      1 | 1773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1774 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      3 | 1775 |  |
|      - | 1776 | `/*` |
|      - | 1777 | ` * JSON_ERROR_UTF8.` |
|      - | 1778 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1779 | ` */` |
|      2 | 1780 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1781 |  |
|      1 | 1782 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1783 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1784 |  |
|      - | 1785 | `/*` |
|      - | 1786 | ` * static` |
|      - | 1787 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1788 | ` */` |
|     12 | 1789 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1790 |  |
|     13 | 1791 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1792 | `	ph7_class *pClass;` |
|      - | 1793 | `	/* Extract the target class if available */` |
|     13 | 1794 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     13 | 1795 | `	if( pClass ){` |
|      9 | 1796 | `		SyString *pName = &pClass->sName;` |
|      - | 1797 | `		/* Expand class name */` |
|      9 | 1798 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1799 | `	}else{` |
|      - | 1800 | `		/* Expand 'static' */` |
|      5 | 1801 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1802 | `	}` |
|     13 | 1803 |  |
|      - | 1804 | `/*` |
|      - | 1805 | ` * self` |
|      - | 1806 | ` * __CLASS__` |
|      - | 1807 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1808 | ` */` |
|      8 | 1809 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1810 |  |
|      9 | 1811 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1812 | `	ph7_class *pClass;` |
|      - | 1813 |  |
|      - | 1814 | `	/* Get the declaring class of the current method */` |
|      9 | 1815 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      9 | 1816 | `	if( pClass == 0 ){` |
|      - | 1817 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1818 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1819 | `	}` |
|      - | 1820 |  |
|      9 | 1821 | `	if( pClass ){` |
|      7 | 1822 | `		SyString *pName = &pClass->sName;` |
|      - | 1823 | `		/* Expand class name */` |
|      7 | 1824 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      4 | 1825 | `	}else{` |
|      - | 1826 | `		/* Expand null */` |
|      3 | 1827 | `		ph7_value_null(pVal);` |
|      - | 1828 | `	}` |
|      9 | 1829 |  |
|      - | 1830 | `/* parent` |
|      - | 1831 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1832 | ` */` |
|     10 | 1833 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1834 |  |
|     11 | 1835 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1836 | `	ph7_class *pClass;` |
|      - | 1837 |  |
|      - | 1838 | `	/* Get the declaring class, then its parent */` |
|     11 | 1839 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|     15 | 1840 | `	if( pClass && pClass->pBase ){` |
|      9 | 1841 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1842 | `		/* Expand parent class name */` |
|      9 | 1843 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1844 | `	}else{` |
|      - | 1845 | `		/* Expand null */` |
|      3 | 1846 | `		ph7_value_null(pVal);` |
|      - | 1847 | `	}` |
|     11 | 1848 |  |
|      - | 1849 |  |
|      - | 1850 | `/*` |
|      - | 1851 | ` * Table of built-in constants.` |
|      - | 1852 | ` */` |
|      - | 1853 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1854 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1855 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1856 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1857 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1858 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1859 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 1860 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 1861 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 1862 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 1863 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 1864 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 1865 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 1866 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 1867 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 1868 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 1869 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 1870 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 1871 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 1872 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 1873 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 1874 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 1875 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 1876 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 1877 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 1878 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 1879 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 1880 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 1881 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 1882 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 1883 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 1884 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 1885 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 1886 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 1887 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 1888 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 1889 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 1890 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 1891 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 1892 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 1893 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 1894 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 1895 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 1896 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 1897 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 1898 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 1899 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 1900 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 1901 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 1902 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 1903 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 1904 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 1905 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 1906 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 1907 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 1908 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 1909 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 1910 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 1911 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 1912 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 1913 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 1914 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 1915 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 1916 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 1917 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 1918 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 1919 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 1920 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 1921 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 1922 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 1923 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 1924 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 1925 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 1926 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 1927 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 1928 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 1929 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 1930 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 1931 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 1932 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 1933 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 1934 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 1935 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 1936 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 1937 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 1938 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 1939 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 1940 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 1941 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 1942 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 1943 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 1944 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 1945 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 1946 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 1947 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 1948 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 1949 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 1950 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 1951 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 1952 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 1953 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 1954 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 1955 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 1956 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 1957 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 1958 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 1959 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 1960 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 1961 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 1962 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 1963 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 1964 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 1965 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 1966 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 1967 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 1968 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 1969 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 1970 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 1971 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 1972 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 1973 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 1974 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 1975 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 1976 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 1977 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 1978 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 1979 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 1980 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 1981 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 1982 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 1983 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 1984 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 1985 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 1986 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 1987 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 1988 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 1989 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 1990 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 1991 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 1992 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 1993 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 1994 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 1995 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 1996 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 1997 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 1998 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 1999 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2000 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2001 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2002 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2003 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2004 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2005 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2006 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2007 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2008 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2009 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2010 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2011 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2012 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2013 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2014 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2015 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2016 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2017 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2018 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2019 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2020 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2021 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2022 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2023 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2024 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2025 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2026 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2027 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2028 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2029 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2030 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2031 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2032 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2033 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2034 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2035 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2036 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2037 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2038 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2039 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2040 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2041 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2042 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2043 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2044 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2045 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2046 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2047 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2048 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2049 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2050 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2051 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2052 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2053 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2054 | `	{"static",               PH7_static_Const       },` |
|      - | 2055 | `	{"self",                 PH7_self_Const         },` |
|      - | 2056 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2057 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2058 | `};` |
|      - | 2059 | `/*` |
|      - | 2060 | ` * Register the built-in constants defined above.` |
|      - | 2061 | ` */` |
|    926 | 2062 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      2 | 2063 |  |
|      - | 2064 | `	sxu32 n;` |
|      - | 2065 | `	/*` |
|      - | 2066 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2067 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2068 | `	 */` |
| 186128 | 2069 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 185202 | 2070 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  92602 | 2071 | `	}` |
|    928 | 2072 |  |
