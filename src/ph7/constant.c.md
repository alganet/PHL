# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1007/1012 lines (99.51%)

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
|   1678 |   27 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   28 |  |
|      - |   29 | `#if defined(__WINNT__)` |
|      2 |   30 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   31 | `#elif defined(__UNIXES__)` |
|      - |   32 | `	struct utsname sInfo;` |
|   1678 |   33 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   34 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   35 | `	}else{` |
|   1678 |   36 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   37 | `	}` |
|      - |   38 | `#else` |
|      - |   39 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   40 | `#endif` |
|    839 |   41 | `	SXUNUSED(pUnused);` |
|   1680 |   42 |  |
|      - |   43 | `/*` |
|      - |   44 | ` * PHP_EOL` |
|      - |   45 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   46 | ` */` |
|    618 |   47 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   48 |  |
|    309 |   49 | `	SXUNUSED(pUnused);` |
|      - |   50 | `#ifdef __WINNT__` |
|      2 |   51 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   52 | `#else` |
|    618 |   53 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   54 | `#endif` |
|    620 |   55 |  |
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
|    154 |   79 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   80 |  |
|     77 |   81 | `	SXUNUSED(pUnused);` |
|      - |   82 | `#ifdef __WINNT__` |
|      2 |   83 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |   84 | `#else` |
|    154 |   85 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |   86 | `#endif` |
|    156 |   87 |  |
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
|      - |  101 |  |
|      - |  102 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  103 | `/*` |
|      - |  104 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  105 | ` */` |
|     36 |  106 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  107 |  |
|     18 |  108 | `	SXUNUSED(pUnused);` |
|     37 |  109 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     37 |  110 |  |
|      - |  111 |  |
|      - |  112 | `/*` |
|      - |  113 | ` * INF constant: positive infinity` |
|      - |  114 | ` */` |
|      4 |  115 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  116 |  |
|      2 |  117 | `	SXUNUSED(pUnused);` |
|      - |  118 | `	/* similarly avoid the INFINITY macro */` |
|      5 |  119 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|      5 |  120 |  |
|      - |  121 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  122 |  |
|      - |  123 | `#ifndef __WINNT__` |
|      - |  124 | `#include <time.h>` |
|      - |  125 | `#endif` |
|      - |  126 | `/*` |
|      - |  127 | ` * __TIME__` |
|      - |  128 | ` *  Expand the current time (GMT).` |
|      - |  129 | ` */` |
|      2 |  130 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
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
|      3 |  146 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  147 |  |
|      - |  148 | `/*` |
|      - |  149 | ` * __DATE__` |
|      - |  150 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  151 | ` */` |
|      2 |  152 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  153 |  |
|      - |  154 | `	Sytm sTm;` |
|      - |  155 | `#ifdef __WINNT__` |
|      - |  156 | `	SYSTEMTIME sOS;` |
|      1 |  157 | `	GetSystemTime(&sOS);` |
|      1 |  158 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  159 | `#else` |
|      - |  160 | `	struct tm *pTm;` |
|      - |  161 | `	time_t t;` |
|      2 |  162 | `	time(&t);` |
|      2 |  163 | `	pTm = gmtime(&t);` |
|      2 |  164 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  165 | `#endif` |
|      1 |  166 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  167 | `	/* Expand */` |
|      3 |  168 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  169 |  |
|      - |  170 | `/*` |
|      - |  171 | ` * __FILE__` |
|      - |  172 | ` *  Path of the processed script.` |
|      - |  173 | ` */` |
|    246 |  174 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  175 |  |
|    248 |  176 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  177 | `	SyString *pFile;` |
|      - |  178 | `	/* Peek the top entry */` |
|    248 |  179 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    248 |  180 | `	if( pFile == 0 ){` |
|      - |  181 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  182 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  183 | `	}else{` |
|    246 |  184 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  185 | `	}` |
|    248 |  186 |  |
|      - |  187 | `/*` |
|      - |  188 | ` * __DIR__` |
|      - |  189 | ` *  Directory holding the processed script.` |
|      - |  190 | ` */` |
|     20 |  191 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  192 |  |
|     22 |  193 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  194 | `	SyString *pFile;` |
|      - |  195 | `	/* Peek the top entry */` |
|     22 |  196 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     22 |  197 | `	if( pFile == 0 ){` |
|      - |  198 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  199 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  200 | `	}else{` |
|     19 |  201 | `		if( pFile->nByte > 0 ){` |
|      - |  202 | `			const char *zDir;` |
|      - |  203 | `			int nLen;` |
|     19 |  204 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     19 |  205 | `			ph7_value_string(pVal,zDir,nLen);` |
|     10 |  206 | `		}else{` |
|      - |  207 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  208 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  209 | `		}` |
|      - |  210 | `	}` |
|     22 |  211 |  |
|      - |  212 | `/*` |
|      - |  213 | ` * PHP_SHLIB_SUFFIX` |
|      - |  214 | ` *  Expand shared library suffix.` |
|      - |  215 | ` */` |
|      2 |  216 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  217 |  |
|      - |  218 | `#ifdef __WINNT__` |
|    ! 0 |  219 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  220 | `#else` |
|      2 |  221 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  222 | `#endif` |
|      1 |  223 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  224 |  |
|      - |  225 | `/*` |
|      - |  226 | ` * E_ERROR` |
|      - |  227 | ` *  Expands 1` |
|      - |  228 | ` */` |
|      2 |  229 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  230 |  |
|      3 |  231 | `	ph7_value_int(pVal,1);` |
|      1 |  232 | `	SXUNUSED(pUserData);` |
|      3 |  233 |  |
|      - |  234 | `/*` |
|      - |  235 | ` * E_WARNING` |
|      - |  236 | ` *  Expands 2` |
|      - |  237 | ` */` |
|      2 |  238 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  239 |  |
|      3 |  240 | `	ph7_value_int(pVal,2);` |
|      1 |  241 | `	SXUNUSED(pUserData);` |
|      3 |  242 |  |
|      - |  243 | `/*` |
|      - |  244 | ` * E_PARSE` |
|      - |  245 | ` *  Expands 4` |
|      - |  246 | ` */` |
|      2 |  247 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  248 |  |
|      3 |  249 | `	ph7_value_int(pVal,4);` |
|      1 |  250 | `	SXUNUSED(pUserData);` |
|      3 |  251 |  |
|      - |  252 | `/*` |
|      - |  253 | ` * E_NOTICE` |
|      - |  254 | ` * Expands 8` |
|      - |  255 | ` */` |
|      2 |  256 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  257 |  |
|      3 |  258 | `	ph7_value_int(pVal,8);` |
|      1 |  259 | `	SXUNUSED(pUserData);` |
|      3 |  260 |  |
|      - |  261 | `/*` |
|      - |  262 | ` * E_CORE_ERROR` |
|      - |  263 | ` * Expands 16` |
|      - |  264 | ` */` |
|      2 |  265 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  266 |  |
|      3 |  267 | `	ph7_value_int(pVal,16);` |
|      1 |  268 | `	SXUNUSED(pUserData);` |
|      3 |  269 |  |
|      - |  270 | `/*` |
|      - |  271 | ` * E_CORE_WARNING` |
|      - |  272 | ` * Expands 32` |
|      - |  273 | ` */` |
|      2 |  274 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  275 |  |
|      3 |  276 | `	ph7_value_int(pVal,32);` |
|      1 |  277 | `	SXUNUSED(pUserData);` |
|      3 |  278 |  |
|      - |  279 | `/*` |
|      - |  280 | ` * E_COMPILE_ERROR` |
|      - |  281 | ` * Expands 64` |
|      - |  282 | ` */` |
|      2 |  283 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  284 |  |
|      3 |  285 | `	ph7_value_int(pVal,64);` |
|      1 |  286 | `	SXUNUSED(pUserData);` |
|      3 |  287 |  |
|      - |  288 | `/*` |
|      - |  289 | ` * E_COMPILE_WARNING` |
|      - |  290 | ` * Expands 128` |
|      - |  291 | ` */` |
|      2 |  292 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  293 |  |
|      3 |  294 | `	ph7_value_int(pVal,128);` |
|      1 |  295 | `	SXUNUSED(pUserData);` |
|      3 |  296 |  |
|      - |  297 | `/*` |
|      - |  298 | ` * E_USER_ERROR` |
|      - |  299 | ` * Expands 256` |
|      - |  300 | ` */` |
|      4 |  301 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  302 |  |
|      6 |  303 | `	ph7_value_int(pVal,256);` |
|      2 |  304 | `	SXUNUSED(pUserData);` |
|      6 |  305 |  |
|      - |  306 | `/*` |
|      - |  307 | ` * E_USER_WARNING` |
|      - |  308 | ` * Expands 512` |
|      - |  309 | ` */` |
|      4 |  310 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  311 |  |
|      6 |  312 | `	ph7_value_int(pVal,512);` |
|      2 |  313 | `	SXUNUSED(pUserData);` |
|      6 |  314 |  |
|      - |  315 | `/*` |
|      - |  316 | ` * E_USER_NOTICE` |
|      - |  317 | ` * Expands 1024` |
|      - |  318 | ` */` |
|      6 |  319 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  320 |  |
|      8 |  321 | `	ph7_value_int(pVal,1024);` |
|      3 |  322 | `	SXUNUSED(pUserData);` |
|      8 |  323 |  |
|      - |  324 | `/*` |
|      - |  325 | ` * E_STRICT` |
|      - |  326 | ` * Expands 2048` |
|      - |  327 | ` */` |
|      2 |  328 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  329 |  |
|      3 |  330 | `	ph7_value_int(pVal,2048);` |
|      1 |  331 | `	SXUNUSED(pUserData);` |
|      3 |  332 |  |
|      - |  333 | `/*` |
|      - |  334 | ` * E_RECOVERABLE_ERROR` |
|      - |  335 | ` * Expands 4096` |
|      - |  336 | ` */` |
|      2 |  337 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  338 |  |
|      3 |  339 | `	ph7_value_int(pVal,4096);` |
|      1 |  340 | `	SXUNUSED(pUserData);` |
|      3 |  341 |  |
|      - |  342 | `/*` |
|      - |  343 | ` * E_DEPRECATED` |
|      - |  344 | ` * Expands 8192` |
|      - |  345 | ` */` |
|      4 |  346 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  347 |  |
|      5 |  348 | `	ph7_value_int(pVal,8192);` |
|      2 |  349 | `	SXUNUSED(pUserData);` |
|      5 |  350 |  |
|      - |  351 | `/*` |
|      - |  352 | ` * E_USER_DEPRECATED` |
|      - |  353 | ` *   Expands 16384.` |
|      - |  354 | ` */` |
|      2 |  355 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  356 |  |
|      3 |  357 | `	ph7_value_int(pVal,16384);` |
|      1 |  358 | `	SXUNUSED(pUserData);` |
|      3 |  359 |  |
|      - |  360 | `/*` |
|      - |  361 | ` * E_ALL` |
|      - |  362 | ` *  Expands 32767` |
|      - |  363 | ` */` |
|      2 |  364 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  365 |  |
|      3 |  366 | `	ph7_value_int(pVal,32767);` |
|      1 |  367 | `	SXUNUSED(pUserData);` |
|      3 |  368 |  |
|      - |  369 | `/*` |
|      - |  370 | ` * CASE_LOWER` |
|      - |  371 | ` *  Expands 0.` |
|      - |  372 | ` */` |
|      2 |  373 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  374 |  |
|      3 |  375 | `	ph7_value_int(pVal,0);` |
|      1 |  376 | `	SXUNUSED(pUserData);` |
|      3 |  377 |  |
|      - |  378 | `/*` |
|      - |  379 | ` * CASE_UPPER` |
|      - |  380 | ` *  Expands 1.` |
|      - |  381 | ` */` |
|      2 |  382 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  383 |  |
|      3 |  384 | `	ph7_value_int(pVal,1);` |
|      1 |  385 | `	SXUNUSED(pUserData);` |
|      3 |  386 |  |
|      - |  387 | `/*` |
|      - |  388 | ` * STR_PAD_LEFT` |
|      - |  389 | ` *  Expands 0.` |
|      - |  390 | ` */` |
|      4 |  391 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  392 |  |
|      5 |  393 | `	ph7_value_int(pVal,0);` |
|      2 |  394 | `	SXUNUSED(pUserData);` |
|      5 |  395 |  |
|      - |  396 | `/*` |
|      - |  397 | ` * STR_PAD_RIGHT` |
|      - |  398 | ` *  Expands 1.` |
|      - |  399 | ` */` |
|      4 |  400 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  401 |  |
|      5 |  402 | `	ph7_value_int(pVal,1);` |
|      2 |  403 | `	SXUNUSED(pUserData);` |
|      5 |  404 |  |
|      - |  405 | `/*` |
|      - |  406 | ` * STR_PAD_BOTH` |
|      - |  407 | ` *  Expands 2.` |
|      - |  408 | ` */` |
|      2 |  409 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  410 |  |
|      3 |  411 | `	ph7_value_int(pVal,2);` |
|      1 |  412 | `	SXUNUSED(pUserData);` |
|      3 |  413 |  |
|      - |  414 | `/*` |
|      - |  415 | ` * COUNT_NORMAL` |
|      - |  416 | ` *  Expands 0` |
|      - |  417 | ` */` |
|      2 |  418 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  419 |  |
|      3 |  420 | `	ph7_value_int(pVal,0);` |
|      1 |  421 | `	SXUNUSED(pUserData);` |
|      3 |  422 |  |
|      - |  423 | `/*` |
|      - |  424 | ` * COUNT_RECURSIVE` |
|      - |  425 | ` *  Expands 1.` |
|      - |  426 | ` */` |
|     22 |  427 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  428 |  |
|     23 |  429 | `	ph7_value_int(pVal,1);` |
|     11 |  430 | `	SXUNUSED(pUserData);` |
|     23 |  431 |  |
|      - |  432 | `/*` |
|      - |  433 | ` * SORT_ASC` |
|      - |  434 | ` *  Expands 1.` |
|      - |  435 | ` */` |
|      2 |  436 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  437 |  |
|      3 |  438 | `	ph7_value_int(pVal,1);` |
|      1 |  439 | `	SXUNUSED(pUserData);` |
|      3 |  440 |  |
|      - |  441 | `/*` |
|      - |  442 | ` * SORT_DESC` |
|      - |  443 | ` *  Expands 2.` |
|      - |  444 | ` */` |
|      2 |  445 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  446 |  |
|      3 |  447 | `	ph7_value_int(pVal,2);` |
|      1 |  448 | `	SXUNUSED(pUserData);` |
|      3 |  449 |  |
|      - |  450 | `/*` |
|      - |  451 | ` * SORT_REGULAR` |
|      - |  452 | ` *  Expands 3.` |
|      - |  453 | ` */` |
|      2 |  454 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  455 |  |
|      3 |  456 | `	ph7_value_int(pVal,3);` |
|      1 |  457 | `	SXUNUSED(pUserData);` |
|      3 |  458 |  |
|      - |  459 | `/*` |
|      - |  460 | ` * SORT_NUMERIC` |
|      - |  461 | ` *  Expands 4.` |
|      - |  462 | ` */` |
|      2 |  463 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  464 |  |
|      3 |  465 | `	ph7_value_int(pVal,4);` |
|      1 |  466 | `	SXUNUSED(pUserData);` |
|      3 |  467 |  |
|      - |  468 | `/*` |
|      - |  469 | ` * SORT_STRING` |
|      - |  470 | ` *  Expands 5.` |
|      - |  471 | ` */` |
|      4 |  472 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  473 |  |
|      5 |  474 | `	ph7_value_int(pVal,5);` |
|      2 |  475 | `	SXUNUSED(pUserData);` |
|      5 |  476 |  |
|      - |  477 | `/*` |
|      - |  478 | ` * PHP_ROUND_HALF_UP` |
|      - |  479 | ` *  Expands 1.` |
|      - |  480 | ` */` |
|      2 |  481 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  482 |  |
|      3 |  483 | `	ph7_value_int(pVal,1);` |
|      1 |  484 | `	SXUNUSED(pUserData);` |
|      3 |  485 |  |
|      - |  486 | `/*` |
|      - |  487 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  488 | ` *  Expands 2.` |
|      - |  489 | ` */` |
|      2 |  490 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  491 |  |
|      3 |  492 | `	ph7_value_int(pVal,2);` |
|      1 |  493 | `	SXUNUSED(pUserData);` |
|      3 |  494 |  |
|      - |  495 | `/*` |
|      - |  496 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  497 | ` *  Expands 3.` |
|      - |  498 | ` */` |
|      2 |  499 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  500 |  |
|      3 |  501 | `	ph7_value_int(pVal,3);` |
|      1 |  502 | `	SXUNUSED(pUserData);` |
|      3 |  503 |  |
|      - |  504 | `/*` |
|      - |  505 | ` * PHP_ROUND_HALF_ODD` |
|      - |  506 | ` *  Expands 4.` |
|      - |  507 | ` */` |
|      2 |  508 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  509 |  |
|      3 |  510 | `	ph7_value_int(pVal,4);` |
|      1 |  511 | `	SXUNUSED(pUserData);` |
|      3 |  512 |  |
|      - |  513 | `/*` |
|      - |  514 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  515 | ` *  Expand 0x01` |
|      - |  516 | ` * NOTE:` |
|      - |  517 | ` *  The expanded value must be a power of two.` |
|      - |  518 | ` */` |
|      2 |  519 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  520 |  |
|      3 |  521 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  522 | `	SXUNUSED(pUserData);` |
|      3 |  523 |  |
|      - |  524 | `/*` |
|      - |  525 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  526 | ` *  Expand 0x02` |
|      - |  527 | ` * NOTE:` |
|      - |  528 | ` *  The expanded value must be a power of two.` |
|      - |  529 | ` */` |
|      2 |  530 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  531 |  |
|      3 |  532 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  533 | `	SXUNUSED(pUserData);` |
|      3 |  534 |  |
|      - |  535 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  536 | `/*` |
|      - |  537 | ` * M_PI` |
|      - |  538 | ` *  Expand the value of pi.` |
|      - |  539 | ` */` |
|      2 |  540 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  541 |  |
|      1 |  542 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  543 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  544 |  |
|      - |  545 | `/*` |
|      - |  546 | ` * M_E` |
|      - |  547 | ` *  Expand 2.7182818284590452354` |
|      - |  548 | ` */` |
|      2 |  549 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  550 |  |
|      1 |  551 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  552 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  553 |  |
|      - |  554 | `/*` |
|      - |  555 | ` * M_LOG2E` |
|      - |  556 | ` *  Expand 2.7182818284590452354` |
|      - |  557 | ` */` |
|      2 |  558 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  559 |  |
|      1 |  560 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  561 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  562 |  |
|      - |  563 | `/*` |
|      - |  564 | ` * M_LOG10E` |
|      - |  565 | ` *  Expand 0.4342944819032518276` |
|      - |  566 | ` */` |
|      2 |  567 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  568 |  |
|      1 |  569 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  570 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  571 |  |
|      - |  572 | `/*` |
|      - |  573 | ` * M_LN2` |
|      - |  574 | ` *  Expand 	0.69314718055994530942` |
|      - |  575 | ` */` |
|      2 |  576 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  577 |  |
|      1 |  578 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  579 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  580 |  |
|      - |  581 | `/*` |
|      - |  582 | ` * M_LN10` |
|      - |  583 | ` *  Expand 	2.30258509299404568402` |
|      - |  584 | ` */` |
|      2 |  585 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  586 |  |
|      1 |  587 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  588 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  589 |  |
|      - |  590 | `/*` |
|      - |  591 | ` * M_PI_2` |
|      - |  592 | ` *  Expand 	1.57079632679489661923` |
|      - |  593 | ` */` |
|      2 |  594 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  595 |  |
|      1 |  596 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  597 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  598 |  |
|      - |  599 | `/*` |
|      - |  600 | ` * M_PI_4` |
|      - |  601 | ` *  Expand 	0.78539816339744830962` |
|      - |  602 | ` */` |
|      2 |  603 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  604 |  |
|      1 |  605 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  606 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  607 |  |
|      - |  608 | `/*` |
|      - |  609 | ` * M_1_PI` |
|      - |  610 | ` *  Expand 	0.31830988618379067154` |
|      - |  611 | ` */` |
|      2 |  612 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  613 |  |
|      1 |  614 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  615 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  616 |  |
|      - |  617 | `/*` |
|      - |  618 | ` * M_2_PI` |
|      - |  619 | ` *  Expand 0.63661977236758134308` |
|      - |  620 | ` */` |
|      4 |  621 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  622 |  |
|      2 |  623 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  624 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  625 |  |
|      - |  626 | `/*` |
|      - |  627 | ` * M_SQRTPI` |
|      - |  628 | ` *  Expand 1.77245385090551602729` |
|      - |  629 | ` */` |
|      2 |  630 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  631 |  |
|      1 |  632 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  633 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  634 |  |
|      - |  635 | `/*` |
|      - |  636 | ` * M_2_SQRTPI` |
|      - |  637 | ` *  Expand 	1.12837916709551257390` |
|      - |  638 | ` */` |
|      2 |  639 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  640 |  |
|      1 |  641 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  642 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  643 |  |
|      - |  644 | `/*` |
|      - |  645 | ` * M_SQRT2` |
|      - |  646 | ` *  Expand 	1.41421356237309504880` |
|      - |  647 | ` */` |
|      2 |  648 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  649 |  |
|      1 |  650 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  651 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  652 |  |
|      - |  653 | `/*` |
|      - |  654 | ` * M_SQRT3` |
|      - |  655 | ` *  Expand 	1.73205080756887729352` |
|      - |  656 | ` */` |
|      2 |  657 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  658 |  |
|      1 |  659 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  660 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  661 |  |
|      - |  662 | `/*` |
|      - |  663 | ` * M_SQRT1_2` |
|      - |  664 | ` *  Expand 	0.70710678118654752440` |
|      - |  665 | ` */` |
|      2 |  666 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  667 |  |
|      1 |  668 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  669 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  670 |  |
|      - |  671 | `/*` |
|      - |  672 | ` * M_LNPI` |
|      - |  673 | ` *  Expand 	1.14472988584940017414` |
|      - |  674 | ` */` |
|      2 |  675 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  676 |  |
|      1 |  677 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  678 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  679 |  |
|      - |  680 | `/*` |
|      - |  681 | ` * M_EULER` |
|      - |  682 | ` *  Expand  0.57721566490153286061` |
|      - |  683 | ` */` |
|      2 |  684 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  685 |  |
|      1 |  686 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  687 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  688 |  |
|      - |  689 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  690 | `/*` |
|      - |  691 | ` * DATE_ATOM` |
|      - |  692 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  693 | ` */` |
|      2 |  694 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  695 |  |
|      1 |  696 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  697 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  698 |  |
|      - |  699 | `/*` |
|      - |  700 | ` * DATE_COOKIE` |
|      - |  701 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  702 | ` */` |
|      2 |  703 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  704 |  |
|      1 |  705 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  706 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  707 |  |
|      - |  708 | `/*` |
|      - |  709 | ` * DATE_ISO8601` |
|      - |  710 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  711 | ` */` |
|      2 |  712 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  713 |  |
|      1 |  714 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  715 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  716 |  |
|      - |  717 | `/*` |
|      - |  718 | ` * DATE_RFC822` |
|      - |  719 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  720 | ` */` |
|      2 |  721 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  722 |  |
|      1 |  723 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  724 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  725 |  |
|      - |  726 | `/*` |
|      - |  727 | ` * DATE_RFC850` |
|      - |  728 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  729 | ` */` |
|      2 |  730 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  731 |  |
|      1 |  732 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  733 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  734 |  |
|      - |  735 | `/*` |
|      - |  736 | ` * DATE_RFC1036` |
|      - |  737 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  738 | ` */` |
|      2 |  739 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  740 |  |
|      1 |  741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  742 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  743 |  |
|      - |  744 | `/*` |
|      - |  745 | ` * DATE_RFC1123` |
|      - |  746 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  747 | ` */` |
|      2 |  748 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  749 |  |
|      1 |  750 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  751 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  752 |  |
|      - |  753 | `/*` |
|      - |  754 | ` * DATE_RFC2822` |
|      - |  755 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  756 | ` */` |
|      2 |  757 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  758 |  |
|      1 |  759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  760 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  761 |  |
|      - |  762 | `/*` |
|      - |  763 | ` * DATE_RSS` |
|      - |  764 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  765 | ` */` |
|      2 |  766 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  767 |  |
|      1 |  768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  769 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  770 |  |
|      - |  771 | `/*` |
|      - |  772 | ` * DATE_W3C` |
|      - |  773 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  774 | ` */` |
|      2 |  775 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  776 |  |
|      1 |  777 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  778 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  779 |  |
|      - |  780 | `/*` |
|      - |  781 | ` * ENT_COMPAT` |
|      - |  782 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  783 | ` */` |
|      2 |  784 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  785 |  |
|      1 |  786 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  787 | `	ph7_value_int(pVal,0x01);` |
|      3 |  788 |  |
|      - |  789 | `/*` |
|      - |  790 | ` * ENT_QUOTES` |
|      - |  791 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  792 | ` */` |
|     16 |  793 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  794 |  |
|      8 |  795 | `	SXUNUSED(pUserData); /* cc warning */` |
|     17 |  796 | `	ph7_value_int(pVal,0x02);` |
|     17 |  797 |  |
|      - |  798 | `/*` |
|      - |  799 | ` * ENT_NOQUOTES` |
|      - |  800 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  801 | ` */` |
|     12 |  802 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  803 |  |
|      6 |  804 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  805 | `	ph7_value_int(pVal,0x04);` |
|     13 |  806 |  |
|      - |  807 | `/*` |
|      - |  808 | ` * ENT_IGNORE` |
|      - |  809 | ` *  Expand 0x08 (Must be a power of two)` |
|      - |  810 | ` */` |
|      2 |  811 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  812 |  |
|      1 |  813 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  814 | `	ph7_value_int(pVal,0x08);` |
|      3 |  815 |  |
|      - |  816 | `/*` |
|      - |  817 | ` * ENT_SUBSTITUTE` |
|      - |  818 | ` *  Expand 0x10 (Must be a power of two)` |
|      - |  819 | ` */` |
|      2 |  820 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  821 |  |
|      1 |  822 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  823 | `	ph7_value_int(pVal,0x10);` |
|      3 |  824 |  |
|      - |  825 | `/*` |
|      - |  826 | ` * ENT_DISALLOWED` |
|      - |  827 | ` *  Expand 0x20 (Must be a power of two)` |
|      - |  828 | ` */` |
|      2 |  829 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  830 |  |
|      1 |  831 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  832 | `	ph7_value_int(pVal,0x20);` |
|      3 |  833 |  |
|      - |  834 | `/*` |
|      - |  835 | ` * ENT_HTML401` |
|      - |  836 | ` *  Expand 0x40 (Must be a power of two)` |
|      - |  837 | ` */` |
|      2 |  838 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  839 |  |
|      1 |  840 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  841 | `	ph7_value_int(pVal,0x40);` |
|      3 |  842 |  |
|      - |  843 | `/*` |
|      - |  844 | ` * ENT_XML1` |
|      - |  845 | ` *  Expand 0x80 (Must be a power of two)` |
|      - |  846 | ` */` |
|      2 |  847 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  848 |  |
|      1 |  849 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  850 | `	ph7_value_int(pVal,0x80);` |
|      3 |  851 |  |
|      - |  852 | `/*` |
|      - |  853 | ` * ENT_XHTML` |
|      - |  854 | ` *  Expand 0x100 (Must be a power of two)` |
|      - |  855 | ` */` |
|      2 |  856 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  857 |  |
|      1 |  858 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  859 | `	ph7_value_int(pVal,0x100);` |
|      3 |  860 |  |
|      - |  861 | `/*` |
|      - |  862 | ` * ENT_HTML5` |
|      - |  863 | ` *  Expand 0x200 (Must be a power of two)` |
|      - |  864 | ` */` |
|      2 |  865 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  866 |  |
|      1 |  867 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  868 | `	ph7_value_int(pVal,0x200);` |
|      3 |  869 |  |
|      - |  870 | `/*` |
|      - |  871 | ` * ISO-8859-1` |
|      - |  872 | ` * ISO_8859_1` |
|      - |  873 | ` *   Expand 1` |
|      - |  874 | ` */` |
|      2 |  875 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  876 |  |
|      1 |  877 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  878 | `	ph7_value_int(pVal,1);` |
|      3 |  879 |  |
|      - |  880 | `/*` |
|      - |  881 | ` * UTF-8` |
|      - |  882 | ` * UTF8` |
|      - |  883 | ` *  Expand 2` |
|      - |  884 | ` */` |
|      2 |  885 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  886 |  |
|      1 |  887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  888 | `	ph7_value_int(pVal,1);` |
|      3 |  889 |  |
|      - |  890 | `/*` |
|      - |  891 | ` * HTML_ENTITIES` |
|      - |  892 | ` *  Expand 1` |
|      - |  893 | ` */` |
|      2 |  894 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  895 |  |
|      1 |  896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  897 | `	ph7_value_int(pVal,1);` |
|      3 |  898 |  |
|      - |  899 | `/*` |
|      - |  900 | ` * HTML_SPECIALCHARS` |
|      - |  901 | ` *  Expand 2` |
|      - |  902 | ` */` |
|      2 |  903 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  904 |  |
|      1 |  905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  906 | `	ph7_value_int(pVal,2);` |
|      3 |  907 |  |
|      - |  908 | `/*` |
|      - |  909 | ` * PHP_URL_SCHEME.` |
|      - |  910 | ` * Expand 1` |
|      - |  911 | ` */` |
|      2 |  912 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  913 |  |
|      1 |  914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  915 | `	ph7_value_int(pVal,1);` |
|      3 |  916 |  |
|      - |  917 | `/*` |
|      - |  918 | ` * PHP_URL_HOST.` |
|      - |  919 | ` * Expand 2` |
|      - |  920 | ` */` |
|      2 |  921 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  922 |  |
|      1 |  923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  924 | `	ph7_value_int(pVal,2);` |
|      3 |  925 |  |
|      - |  926 | `/*` |
|      - |  927 | ` * PHP_URL_PORT.` |
|      - |  928 | ` * Expand 3` |
|      - |  929 | ` */` |
|      2 |  930 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  931 |  |
|      1 |  932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  933 | `	ph7_value_int(pVal,3);` |
|      3 |  934 |  |
|      - |  935 | `/*` |
|      - |  936 | ` * PHP_URL_USER.` |
|      - |  937 | ` * Expand 4` |
|      - |  938 | ` */` |
|      2 |  939 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  940 |  |
|      1 |  941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  942 | `	ph7_value_int(pVal,4);` |
|      3 |  943 |  |
|      - |  944 | `/*` |
|      - |  945 | ` * PHP_URL_PASS.` |
|      - |  946 | ` * Expand 5` |
|      - |  947 | ` */` |
|      2 |  948 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  949 |  |
|      1 |  950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  951 | `	ph7_value_int(pVal,5);` |
|      3 |  952 |  |
|      - |  953 | `/*` |
|      - |  954 | ` * PHP_URL_PATH.` |
|      - |  955 | ` * Expand 6` |
|      - |  956 | ` */` |
|      2 |  957 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  958 |  |
|      1 |  959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  960 | `	ph7_value_int(pVal,6);` |
|      3 |  961 |  |
|      - |  962 | `/*` |
|      - |  963 | ` * PHP_URL_QUERY.` |
|      - |  964 | ` * Expand 7` |
|      - |  965 | ` */` |
|      2 |  966 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  967 |  |
|      1 |  968 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  969 | `	ph7_value_int(pVal,7);` |
|      3 |  970 |  |
|      - |  971 | `/*` |
|      - |  972 | ` * PHP_URL_FRAGMENT.` |
|      - |  973 | ` * Expand 8` |
|      - |  974 | ` */` |
|      2 |  975 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  976 |  |
|      1 |  977 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  978 | `	ph7_value_int(pVal,8);` |
|      3 |  979 |  |
|      - |  980 | `/*` |
|      - |  981 | ` * PHP_QUERY_RFC1738` |
|      - |  982 | ` * Expand 1` |
|      - |  983 | ` */` |
|      2 |  984 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  985 |  |
|      1 |  986 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  987 | `	ph7_value_int(pVal,1);` |
|      3 |  988 |  |
|      - |  989 | `/*` |
|      - |  990 | ` * PHP_QUERY_RFC3986` |
|      - |  991 | ` * Expand 1` |
|      - |  992 | ` */` |
|      2 |  993 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  994 |  |
|      1 |  995 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  996 | `	ph7_value_int(pVal,2);` |
|      3 |  997 |  |
|      - |  998 | `/*` |
|      - |  999 | ` * FNM_NOESCAPE` |
|      - | 1000 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1001 | ` */` |
|      2 | 1002 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1003 |  |
|      1 | 1004 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1005 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1006 |  |
|      - | 1007 | `/*` |
|      - | 1008 | ` * FNM_PATHNAME` |
|      - | 1009 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1010 | ` */` |
|      2 | 1011 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1012 |  |
|      1 | 1013 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1014 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1015 |  |
|      - | 1016 | `/*` |
|      - | 1017 | ` * FNM_PERIOD` |
|      - | 1018 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1019 | ` */` |
|      6 | 1020 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1021 |  |
|      3 | 1022 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1023 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1024 |  |
|      - | 1025 | `/*` |
|      - | 1026 | ` * FNM_CASEFOLD` |
|      - | 1027 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1028 | ` */` |
|      4 | 1029 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1030 |  |
|      2 | 1031 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1032 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1033 |  |
|      - | 1034 | `/*` |
|      - | 1035 | ` * PATHINFO_DIRNAME` |
|      - | 1036 | ` *  Expand 1.` |
|      - | 1037 | ` */` |
|      4 | 1038 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1039 |  |
|      2 | 1040 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1041 | `	ph7_value_int(pVal,1);` |
|      5 | 1042 |  |
|      - | 1043 | `/*` |
|      - | 1044 | ` * PATHINFO_BASENAME` |
|      - | 1045 | ` *  Expand 2.` |
|      - | 1046 | ` */` |
|      4 | 1047 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1048 |  |
|      2 | 1049 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1050 | `	ph7_value_int(pVal,2);` |
|      5 | 1051 |  |
|      - | 1052 | `/*` |
|      - | 1053 | ` * PATHINFO_EXTENSION` |
|      - | 1054 | ` *  Expand 3.` |
|      - | 1055 | ` */` |
|   3522 | 1056 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1057 |  |
|   1761 | 1058 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3524 | 1059 | `	ph7_value_int(pVal,3);` |
|   3524 | 1060 |  |
|      - | 1061 | `/*` |
|      - | 1062 | ` * PATHINFO_FILENAME` |
|      - | 1063 | ` *  Expand 4.` |
|      - | 1064 | ` */` |
|   3518 | 1065 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1066 |  |
|   1759 | 1067 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3520 | 1068 | `	ph7_value_int(pVal,4);` |
|   3520 | 1069 |  |
|      - | 1070 | `/*` |
|      - | 1071 | ` * ASSERT_ACTIVE.` |
|      - | 1072 | ` *  Expand the value of PH7_ASSERT_ACTIVE defined in ph7Int.h` |
|      - | 1073 | ` */` |
|      2 | 1074 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1075 |  |
|      1 | 1076 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1077 | `	ph7_value_int(pVal,PH7_ASSERT_DISABLE);` |
|      3 | 1078 |  |
|      - | 1079 | `/*` |
|      - | 1080 | ` * ASSERT_WARNING.` |
|      - | 1081 | ` *  Expand the value of PH7_ASSERT_WARNING defined in ph7Int.h` |
|      - | 1082 | ` */` |
|      2 | 1083 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1084 |  |
|      1 | 1085 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1086 | `	ph7_value_int(pVal,PH7_ASSERT_WARNING);` |
|      3 | 1087 |  |
|      - | 1088 | `/*` |
|      - | 1089 | ` * ASSERT_BAIL.` |
|      - | 1090 | ` *  Expand the value of PH7_ASSERT_BAIL defined in ph7Int.h` |
|      - | 1091 | ` */` |
|      2 | 1092 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1093 |  |
|      1 | 1094 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1095 | `	ph7_value_int(pVal,PH7_ASSERT_BAIL);` |
|      3 | 1096 |  |
|      - | 1097 | `/*` |
|      - | 1098 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1099 | ` *  Expand the value of PH7_ASSERT_QUIET_EVAL defined in ph7Int.h` |
|      - | 1100 | ` */` |
|      2 | 1101 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1102 |  |
|      1 | 1103 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1104 | `	ph7_value_int(pVal,PH7_ASSERT_QUIET_EVAL);` |
|      3 | 1105 |  |
|      - | 1106 | `/*` |
|      - | 1107 | ` * ASSERT_CALLBACK.` |
|      - | 1108 | ` *  Expand the value of PH7_ASSERT_CALLBACK defined in ph7Int.h` |
|      - | 1109 | ` */` |
|      2 | 1110 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1111 |  |
|      1 | 1112 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1113 | `	ph7_value_int(pVal,PH7_ASSERT_CALLBACK);` |
|      3 | 1114 |  |
|      - | 1115 | `/*` |
|      - | 1116 | ` * SEEK_SET.` |
|      - | 1117 | ` *  Expand 0` |
|      - | 1118 | ` */` |
|      2 | 1119 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1120 |  |
|      1 | 1121 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1122 | `	ph7_value_int(pVal,0);` |
|      3 | 1123 |  |
|      - | 1124 | `/*` |
|      - | 1125 | ` * SEEK_CUR.` |
|      - | 1126 | ` *  Expand 1` |
|      - | 1127 | ` */` |
|      2 | 1128 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1129 |  |
|      1 | 1130 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1131 | `	ph7_value_int(pVal,1);` |
|      3 | 1132 |  |
|      - | 1133 | `/*` |
|      - | 1134 | ` * SEEK_END.` |
|      - | 1135 | ` *  Expand 2` |
|      - | 1136 | ` */` |
|      2 | 1137 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1138 |  |
|      1 | 1139 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1140 | `	ph7_value_int(pVal,2);` |
|      3 | 1141 |  |
|      - | 1142 | `/*` |
|      - | 1143 | ` * LOCK_SH.` |
|      - | 1144 | ` *  Expand 2` |
|      - | 1145 | ` */` |
|      2 | 1146 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1147 |  |
|      1 | 1148 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1149 | `	ph7_value_int(pVal,1);` |
|      3 | 1150 |  |
|      - | 1151 | `/*` |
|      - | 1152 | ` * LOCK_NB.` |
|      - | 1153 | ` *  Expand 5` |
|      - | 1154 | ` */` |
|      2 | 1155 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1156 |  |
|      1 | 1157 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1158 | `	ph7_value_int(pVal,5);` |
|      3 | 1159 |  |
|      - | 1160 | `/*` |
|      - | 1161 | ` * LOCK_EX.` |
|      - | 1162 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1163 | ` */` |
|      4 | 1164 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1165 |  |
|      2 | 1166 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1167 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1168 |  |
|      - | 1169 | `/*` |
|      - | 1170 | ` * LOCK_UN.` |
|      - | 1171 | ` *  Expand 0` |
|      - | 1172 | ` */` |
|      4 | 1173 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1174 |  |
|      2 | 1175 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1176 | `	ph7_value_int(pVal,0);` |
|      5 | 1177 |  |
|      - | 1178 | `/*` |
|      - | 1179 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1180 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1181 | ` */` |
|      2 | 1182 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1183 |  |
|      1 | 1184 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1185 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1186 |  |
|      - | 1187 | `/*` |
|      - | 1188 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1189 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1190 | ` */` |
|      2 | 1191 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1192 |  |
|      1 | 1193 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1194 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1195 |  |
|      - | 1196 | `/*` |
|      - | 1197 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1198 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1199 | ` */` |
|      2 | 1200 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1201 |  |
|      1 | 1202 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1203 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1204 |  |
|      - | 1205 | `/*` |
|      - | 1206 | ` * FILE_APPEND` |
|      - | 1207 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1208 | ` */` |
|      2 | 1209 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1210 |  |
|      1 | 1211 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1212 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1213 |  |
|      - | 1214 | `/*` |
|      - | 1215 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1216 | ` *  Expand 0` |
|      - | 1217 | ` */` |
|   1598 | 1218 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1219 |  |
|    799 | 1220 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1600 | 1221 | `	ph7_value_int(pVal,0);` |
|   1600 | 1222 |  |
|      - | 1223 | `/*` |
|      - | 1224 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1225 | ` *  Expand 1` |
|      - | 1226 | ` */` |
|    800 | 1227 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1228 |  |
|    400 | 1229 | `	SXUNUSED(pUserData); /* cc warning */` |
|    802 | 1230 | `	ph7_value_int(pVal,1);` |
|    802 | 1231 |  |
|      - | 1232 | `/*` |
|      - | 1233 | ` * SCANDIR_SORT_NONE` |
|      - | 1234 | ` *  Expand 2` |
|      - | 1235 | ` */` |
|      2 | 1236 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1237 |  |
|      1 | 1238 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1239 | `	ph7_value_int(pVal,2);` |
|      3 | 1240 |  |
|      - | 1241 | `/*` |
|      - | 1242 | ` * GLOB_MARK` |
|      - | 1243 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1244 | ` */` |
|      2 | 1245 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1246 |  |
|      1 | 1247 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1248 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1249 |  |
|      - | 1250 | `/*` |
|      - | 1251 | ` * GLOB_NOSORT` |
|      - | 1252 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1253 | ` */` |
|      2 | 1254 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1255 |  |
|      1 | 1256 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1257 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1258 |  |
|      - | 1259 | `/*` |
|      - | 1260 | ` * GLOB_NOCHECK` |
|      - | 1261 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1262 | ` */` |
|      2 | 1263 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1264 |  |
|      1 | 1265 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1266 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1267 |  |
|      - | 1268 | `/*` |
|      - | 1269 | ` * GLOB_NOESCAPE` |
|      - | 1270 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1271 | ` */` |
|      2 | 1272 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1273 |  |
|      1 | 1274 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1275 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1276 |  |
|      - | 1277 | `/*` |
|      - | 1278 | ` * GLOB_BRACE` |
|      - | 1279 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1280 | ` */` |
|      2 | 1281 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1282 |  |
|      1 | 1283 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1284 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1285 |  |
|      - | 1286 | `/*` |
|      - | 1287 | ` * GLOB_ONLYDIR` |
|      - | 1288 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1289 | ` */` |
|      2 | 1290 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1291 |  |
|      1 | 1292 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1293 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1294 |  |
|      - | 1295 | `/*` |
|      - | 1296 | ` * GLOB_ERR` |
|      - | 1297 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1298 | ` */` |
|      2 | 1299 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1300 |  |
|      1 | 1301 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1302 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1303 |  |
|      - | 1304 | `/*` |
|      - | 1305 | ` * STDIN` |
|      - | 1306 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1307 | ` */` |
|      2 | 1308 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1309 |  |
|      3 | 1310 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1311 | `	void *pResource;` |
|      3 | 1312 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1313 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1314 |  |
|      - | 1315 | `/*` |
|      - | 1316 | ` * STDOUT` |
|      - | 1317 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1318 | ` */` |
|      2 | 1319 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1320 |  |
|      3 | 1321 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1322 | `	void *pResource;` |
|      3 | 1323 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1324 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1325 |  |
|      - | 1326 | `/*` |
|      - | 1327 | ` * STDERR` |
|      - | 1328 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1329 | ` */` |
|      2 | 1330 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1331 |  |
|      3 | 1332 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1333 | `	void *pResource;` |
|      3 | 1334 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1335 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1336 |  |
|      - | 1337 | `/*` |
|      - | 1338 | ` * INI_SCANNER_NORMAL` |
|      - | 1339 | ` *   Expand 1` |
|      - | 1340 | ` */` |
|      2 | 1341 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1342 |  |
|      1 | 1343 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1344 | `	ph7_value_int(pVal,1);` |
|      3 | 1345 |  |
|      - | 1346 | `/*` |
|      - | 1347 | ` * INI_SCANNER_RAW` |
|      - | 1348 | ` *   Expand 2` |
|      - | 1349 | ` */` |
|      2 | 1350 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1351 |  |
|      1 | 1352 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1353 | `	ph7_value_int(pVal,2);` |
|      3 | 1354 |  |
|      - | 1355 | `/*` |
|      - | 1356 | ` * EXTR_OVERWRITE` |
|      - | 1357 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1358 | ` */` |
|      2 | 1359 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1360 |  |
|      1 | 1361 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1362 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1363 |  |
|      - | 1364 | `/*` |
|      - | 1365 | ` * EXTR_SKIP` |
|      - | 1366 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1367 | ` */` |
|      2 | 1368 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1369 |  |
|      1 | 1370 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1371 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1372 |  |
|      - | 1373 | `/*` |
|      - | 1374 | ` * EXTR_PREFIX_SAME` |
|      - | 1375 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1376 | ` */` |
|      2 | 1377 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1378 |  |
|      1 | 1379 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1380 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1381 |  |
|      - | 1382 | `/*` |
|      - | 1383 | ` * EXTR_PREFIX_ALL` |
|      - | 1384 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1385 | ` */` |
|      2 | 1386 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1387 |  |
|      1 | 1388 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1389 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1390 |  |
|      - | 1391 | `/*` |
|      - | 1392 | ` * EXTR_PREFIX_INVALID` |
|      - | 1393 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1394 | ` */` |
|      2 | 1395 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1396 |  |
|      1 | 1397 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1398 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1399 |  |
|      - | 1400 | `/*` |
|      - | 1401 | ` * EXTR_IF_EXISTS` |
|      - | 1402 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1403 | ` */` |
|      2 | 1404 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1405 |  |
|      1 | 1406 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1407 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1408 |  |
|      - | 1409 | `/*` |
|      - | 1410 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1411 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1412 | ` */` |
|      2 | 1413 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1414 |  |
|      1 | 1415 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1416 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1417 |  |
|      - | 1418 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1419 | `/*` |
|      - | 1420 | ` * XML_ERROR_NONE` |
|      - | 1421 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1422 | ` */` |
|      2 | 1423 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1424 |  |
|      1 | 1425 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1426 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1427 |  |
|      - | 1428 | `/*` |
|      - | 1429 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1430 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1431 | ` */` |
|      2 | 1432 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1433 |  |
|      1 | 1434 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1435 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1436 |  |
|      - | 1437 | `/*` |
|      - | 1438 | ` * XML_ERROR_SYNTAX` |
|      - | 1439 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1440 | ` */` |
|      2 | 1441 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1442 |  |
|      1 | 1443 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1444 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1445 |  |
|      - | 1446 | `/*` |
|      - | 1447 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1448 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1449 | ` */` |
|      2 | 1450 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1451 |  |
|      1 | 1452 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1453 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1454 |  |
|      - | 1455 | `/*` |
|      - | 1456 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1457 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1458 | ` */` |
|      2 | 1459 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1460 |  |
|      1 | 1461 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1462 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1463 |  |
|      - | 1464 | `/*` |
|      - | 1465 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1466 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1467 | ` */` |
|      2 | 1468 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1469 |  |
|      1 | 1470 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1471 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1472 |  |
|      - | 1473 | `/*` |
|      - | 1474 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1475 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1476 | ` */` |
|      2 | 1477 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1478 |  |
|      1 | 1479 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1480 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1481 |  |
|      - | 1482 | `/*` |
|      - | 1483 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1484 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1485 | ` */` |
|      2 | 1486 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1487 |  |
|      1 | 1488 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1489 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1490 |  |
|      - | 1491 | `/*` |
|      - | 1492 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1493 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1494 | ` */` |
|      2 | 1495 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1496 |  |
|      1 | 1497 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1498 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1499 |  |
|      - | 1500 | `/*` |
|      - | 1501 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1502 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1503 | ` */` |
|      2 | 1504 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1505 |  |
|      1 | 1506 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1507 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1508 |  |
|      - | 1509 | `/*` |
|      - | 1510 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1511 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1512 | ` */` |
|      2 | 1513 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1514 |  |
|      1 | 1515 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1516 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1517 |  |
|      - | 1518 | `/*` |
|      - | 1519 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1520 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1521 | ` */` |
|      2 | 1522 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1523 |  |
|      1 | 1524 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1525 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1526 |  |
|      - | 1527 | `/*` |
|      - | 1528 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1529 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1530 | ` */` |
|      2 | 1531 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1532 |  |
|      1 | 1533 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1534 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1535 |  |
|      - | 1536 | `/*` |
|      - | 1537 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1538 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1539 | ` */` |
|      2 | 1540 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1541 |  |
|      1 | 1542 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1543 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1544 |  |
|      - | 1545 | `/*` |
|      - | 1546 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1547 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1548 | ` */` |
|      2 | 1549 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1550 |  |
|      1 | 1551 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1552 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1553 |  |
|      - | 1554 | `/*` |
|      - | 1555 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1556 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1557 | ` */` |
|      2 | 1558 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1559 |  |
|      1 | 1560 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1561 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1562 |  |
|      - | 1563 | `/*` |
|      - | 1564 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1565 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1566 | ` */` |
|      2 | 1567 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1568 |  |
|      1 | 1569 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1570 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1571 |  |
|      - | 1572 | `/*` |
|      - | 1573 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1574 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1575 | ` */` |
|      2 | 1576 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1577 |  |
|      1 | 1578 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1579 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1580 |  |
|      - | 1581 | `/*` |
|      - | 1582 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1583 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1584 | ` */` |
|      2 | 1585 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1586 |  |
|      1 | 1587 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1588 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1589 |  |
|      - | 1590 | `/*` |
|      - | 1591 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1592 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1593 | ` */` |
|      2 | 1594 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1595 |  |
|      1 | 1596 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1597 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1598 |  |
|      - | 1599 | `/*` |
|      - | 1600 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1601 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1602 | ` */` |
|      2 | 1603 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1604 |  |
|      1 | 1605 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1606 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1607 |  |
|      - | 1608 | `/*` |
|      - | 1609 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1610 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1611 | ` */` |
|      2 | 1612 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1613 |  |
|      1 | 1614 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1615 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1616 |  |
|      - | 1617 | `/*` |
|      - | 1618 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1619 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1620 | ` */` |
|      2 | 1621 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1622 |  |
|      1 | 1623 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1624 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1625 |  |
|      - | 1626 | `/*` |
|      - | 1627 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1628 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1629 | ` */` |
|      4 | 1630 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1631 |  |
|      2 | 1632 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1633 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1634 |  |
|      - | 1635 | `/*` |
|      - | 1636 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1637 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1638 | ` */` |
|      2 | 1639 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1640 |  |
|      1 | 1641 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1642 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1643 |  |
|      - | 1644 | `/*` |
|      - | 1645 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1646 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1647 | ` */` |
|      4 | 1648 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1649 |  |
|      2 | 1650 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1651 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1652 |  |
|      - | 1653 | `/*` |
|      - | 1654 | ` * XML_SAX_IMPL.` |
|      - | 1655 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1656 | ` */` |
|      2 | 1657 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1658 |  |
|      1 | 1659 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1660 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1661 |  |
|      - | 1662 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1663 | `/*` |
|      - | 1664 | ` * JSON_HEX_TAG.` |
|      - | 1665 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1666 | ` */` |
|      2 | 1667 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1668 |  |
|      1 | 1669 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1670 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1671 |  |
|      - | 1672 | `/*` |
|      - | 1673 | ` * JSON_HEX_AMP.` |
|      - | 1674 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1675 | ` */` |
|      2 | 1676 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1677 |  |
|      1 | 1678 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1679 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1680 |  |
|      - | 1681 | `/*` |
|      - | 1682 | ` * JSON_HEX_APOS.` |
|      - | 1683 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1684 | ` */` |
|      2 | 1685 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1686 |  |
|      1 | 1687 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1688 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1689 |  |
|      - | 1690 | `/*` |
|      - | 1691 | ` * JSON_HEX_QUOT.` |
|      - | 1692 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1693 | ` */` |
|      2 | 1694 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1695 |  |
|      1 | 1696 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1697 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1698 |  |
|      - | 1699 | `/*` |
|      - | 1700 | ` * JSON_FORCE_OBJECT.` |
|      - | 1701 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1702 | ` */` |
|      2 | 1703 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1704 |  |
|      1 | 1705 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1706 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      3 | 1707 |  |
|      - | 1708 | `/*` |
|      - | 1709 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1710 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1711 | ` */` |
|      2 | 1712 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1713 |  |
|      1 | 1714 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1715 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1716 |  |
|      - | 1717 | `/*` |
|      - | 1718 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1719 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1720 | ` */` |
|      2 | 1721 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1722 |  |
|      1 | 1723 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1724 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1725 |  |
|      - | 1726 | `/*` |
|      - | 1727 | ` * JSON_PRETTY_PRINT.` |
|      - | 1728 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1729 | ` */` |
|      2 | 1730 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1731 |  |
|      1 | 1732 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1733 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1734 |  |
|      - | 1735 | `/*` |
|      - | 1736 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1737 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1738 | ` */` |
|      2 | 1739 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1740 |  |
|      1 | 1741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1742 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1743 |  |
|      - | 1744 | `/*` |
|      - | 1745 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1746 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1747 | ` */` |
|      2 | 1748 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1749 |  |
|      1 | 1750 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1751 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1752 |  |
|      - | 1753 | `/*` |
|      - | 1754 | ` * JSON_ERROR_NONE.` |
|      - | 1755 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1756 | ` */` |
|      4 | 1757 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1758 |  |
|      2 | 1759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1760 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1761 |  |
|      - | 1762 | `/*` |
|      - | 1763 | ` * JSON_ERROR_DEPTH.` |
|      - | 1764 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1765 | ` */` |
|      2 | 1766 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1767 |  |
|      1 | 1768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1769 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1770 |  |
|      - | 1771 | `/*` |
|      - | 1772 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1773 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1774 | ` */` |
|      2 | 1775 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1776 |  |
|      1 | 1777 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1778 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1779 |  |
|      - | 1780 | `/*` |
|      - | 1781 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1782 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1783 | ` */` |
|      2 | 1784 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1785 |  |
|      1 | 1786 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1787 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1788 |  |
|      - | 1789 | `/*` |
|      - | 1790 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1791 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1792 | ` */` |
|      2 | 1793 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1794 |  |
|      1 | 1795 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1796 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      3 | 1797 |  |
|      - | 1798 | `/*` |
|      - | 1799 | ` * JSON_ERROR_UTF8.` |
|      - | 1800 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1801 | ` */` |
|      2 | 1802 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1803 |  |
|      1 | 1804 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1805 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1806 |  |
|      - | 1807 | `/*` |
|      - | 1808 | ` * static` |
|      - | 1809 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1810 | ` */` |
|     12 | 1811 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1812 |  |
|     13 | 1813 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1814 | `	ph7_class *pClass;` |
|      - | 1815 | `	/* Extract the target class if available */` |
|     13 | 1816 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     13 | 1817 | `	if( pClass ){` |
|      9 | 1818 | `		SyString *pName = &pClass->sName;` |
|      - | 1819 | `		/* Expand class name */` |
|      9 | 1820 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1821 | `	}else{` |
|      - | 1822 | `		/* Expand 'static' */` |
|      5 | 1823 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1824 | `	}` |
|     13 | 1825 |  |
|      - | 1826 | `/*` |
|      - | 1827 | ` * self` |
|      - | 1828 | ` * __CLASS__` |
|      - | 1829 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1830 | ` */` |
|      8 | 1831 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1832 |  |
|      9 | 1833 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1834 | `	ph7_class *pClass;` |
|      - | 1835 |  |
|      - | 1836 | `	/* Get the declaring class of the current method */` |
|      9 | 1837 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      9 | 1838 | `	if( pClass == 0 ){` |
|      - | 1839 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1840 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1841 | `	}` |
|      - | 1842 |  |
|      9 | 1843 | `	if( pClass ){` |
|      7 | 1844 | `		SyString *pName = &pClass->sName;` |
|      - | 1845 | `		/* Expand class name */` |
|      7 | 1846 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      4 | 1847 | `	}else{` |
|      - | 1848 | `		/* Expand null */` |
|      3 | 1849 | `		ph7_value_null(pVal);` |
|      - | 1850 | `	}` |
|      9 | 1851 |  |
|      - | 1852 | `/* parent` |
|      - | 1853 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1854 | ` */` |
|     10 | 1855 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1856 |  |
|     11 | 1857 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1858 | `	ph7_class *pClass;` |
|      - | 1859 |  |
|      - | 1860 | `	/* Get the declaring class, then its parent */` |
|     11 | 1861 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|     15 | 1862 | `	if( pClass && pClass->pBase ){` |
|      9 | 1863 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1864 | `		/* Expand parent class name */` |
|      9 | 1865 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1866 | `	}else{` |
|      - | 1867 | `		/* Expand null */` |
|      3 | 1868 | `		ph7_value_null(pVal);` |
|      - | 1869 | `	}` |
|     11 | 1870 |  |
|      - | 1871 |  |
|      - | 1872 | `/*` |
|      - | 1873 | ` * Table of built-in constants.` |
|      - | 1874 | ` */` |
|      - | 1875 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1876 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1877 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1878 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1879 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1880 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1881 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 1882 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 1883 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 1884 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 1885 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 1886 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 1887 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 1888 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 1889 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 1890 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 1891 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 1892 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 1893 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 1894 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 1895 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 1896 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 1897 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 1898 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 1899 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 1900 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 1901 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 1902 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 1903 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 1904 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 1905 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 1906 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 1907 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 1908 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 1909 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 1910 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 1911 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 1912 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 1913 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 1914 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 1915 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 1916 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 1917 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 1918 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 1919 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 1920 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 1921 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 1922 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 1923 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 1924 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 1925 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 1926 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 1927 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 1928 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 1929 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 1930 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 1931 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 1932 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 1933 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 1934 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 1935 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 1936 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 1937 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 1938 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 1939 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 1940 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 1941 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 1942 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 1943 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 1944 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 1945 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 1946 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 1947 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 1948 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 1949 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 1950 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 1951 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 1952 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 1953 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 1954 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 1955 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 1956 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 1957 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 1958 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 1959 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 1960 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 1961 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 1962 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 1963 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 1964 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 1965 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 1966 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 1967 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 1968 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 1969 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 1970 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 1971 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 1972 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 1973 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 1974 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 1975 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 1976 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 1977 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 1978 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 1979 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 1980 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 1981 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 1982 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 1983 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 1984 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 1985 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 1986 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 1987 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 1988 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 1989 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 1990 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 1991 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 1992 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 1993 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 1994 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 1995 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 1996 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 1997 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 1998 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 1999 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2000 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2001 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2002 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2003 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2004 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2005 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2006 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2007 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2008 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2009 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2010 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2011 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2012 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2013 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2014 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2015 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2016 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2017 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2018 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2019 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2020 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2021 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2022 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2023 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2024 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2025 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2026 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2027 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2028 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2029 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2030 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2031 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2032 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2033 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2034 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2035 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2036 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2037 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2038 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2039 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2040 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2041 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2042 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2043 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2044 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2045 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2046 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2047 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2048 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2049 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2050 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2051 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2052 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2053 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2054 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2055 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2056 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2057 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2058 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2059 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2060 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2061 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2062 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2063 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2064 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2065 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2066 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2067 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2068 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2069 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2070 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2071 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2072 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2073 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2074 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2075 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2076 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2077 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2078 | `	{"static",               PH7_static_Const       },` |
|      - | 2079 | `	{"self",                 PH7_self_Const         },` |
|      - | 2080 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2081 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2082 | `};` |
|      - | 2083 | `/*` |
|      - | 2084 | ` * Register the built-in constants defined above.` |
|      - | 2085 | ` */` |
|   1360 | 2086 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      2 | 2087 |  |
|      - | 2088 | `	sxu32 n;` |
|      - | 2089 | `	/*` |
|      - | 2090 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2091 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2092 | `	 */` |
| 276082 | 2093 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 274722 | 2094 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 137362 | 2095 | `	}` |
|   1362 | 2096 |  |
