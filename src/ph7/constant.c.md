# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1083/1094 lines (98.99%)

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
|      1 |   14 | `{` |
|      4 |   15 | `	SXUNUSED(pUnused);` |
|      9 |   16 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|      9 |   17 | `}` |
|      - |   18 | `/*` |
|      - |   19 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|      - |   20 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|      - |   21 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|      - |   22 | ` */` |
|      4 |   23 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|      1 |   24 | `{` |
|      2 |   25 | `	SXUNUSED(pUnused);` |
|      5 |   26 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|      5 |   27 | `}` |
|      4 |   28 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|      1 |   29 | `{` |
|      2 |   30 | `	SXUNUSED(pUnused);` |
|      5 |   31 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      5 |   32 | `}` |
|      4 |   33 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|      1 |   34 | `{` |
|      2 |   35 | `	SXUNUSED(pUnused);` |
|      5 |   36 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      5 |   37 | `}` |
|      4 |   38 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|      1 |   39 | `{` |
|      2 |   40 | `	SXUNUSED(pUnused);` |
|      5 |   41 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      5 |   42 | `}` |
|      2 |   43 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|      1 |   44 | `{` |
|      1 |   45 | `	SXUNUSED(pUnused);` |
|      3 |   46 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      3 |   47 | `}` |
|      8 |   48 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|      1 |   49 | `{` |
|      4 |   50 | `	SXUNUSED(pUnused);` |
|      9 |   51 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      9 |   52 | `}` |
|      - |   53 | `#ifdef __WINNT__` |
|      - |   54 | `#include <Windows.h>` |
|      - |   55 | `#elif defined(__UNIXES__)` |
|      - |   56 | `#include <sys/utsname.h>` |
|      - |   57 | `#endif` |
|      - |   58 | `/*` |
|      - |   59 | ` * PHP_OS` |
|      - |   60 | ` *  Expand the name of the host Operating System.` |
|      - |   61 | ` */` |
|   3818 |   62 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   63 | `{` |
|      - |   64 | `#if defined(__WINNT__)` |
|      5 |   65 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   66 | `#elif defined(__UNIXES__)` |
|      - |   67 | `	struct utsname sInfo;` |
|   3818 |   68 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   69 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   70 | `	}else{` |
|   3818 |   71 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   72 | `	}` |
|      - |   73 | `#else` |
|      - |   74 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   75 | `#endif` |
|   1909 |   76 | `	SXUNUSED(pUnused);` |
|   3823 |   77 | `}` |
|      - |   78 | `/*` |
|      - |   79 | ` * PHP_EOL` |
|      - |   80 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   81 | ` */` |
|    840 |   82 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      4 |   83 | `{` |
|    420 |   84 | `	SXUNUSED(pUnused);` |
|      - |   85 | `#ifdef __WINNT__` |
|      4 |   86 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   87 | `#else` |
|    840 |   88 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   89 | `#endif` |
|    844 |   90 | `}` |
|      - |   91 | `/*` |
|      - |   92 | ` * PHP_INT_MAX` |
|      - |   93 | ` * Expand the largest integer supported.` |
|      - |   94 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |   95 | ` */` |
|     14 |   96 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   97 | `{` |
|      7 |   98 | `	SXUNUSED(pUnused);` |
|     16 |   99 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|     16 |  100 | `}` |
|      - |  101 | `/*` |
|      - |  102 | ` * PHP_INT_SIZE` |
|      - |  103 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |  104 | ` */` |
|      4 |  105 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  106 | `{` |
|      2 |  107 | `	SXUNUSED(pUnused);` |
|      5 |  108 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      5 |  109 | `}` |
|      - |  110 | `/*` |
|      - |  111 | ` * DIRECTORY_SEPARATOR.` |
|      - |  112 | ` * Expand the directory separator character.` |
|      - |  113 | ` */` |
|    156 |  114 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  115 | `{` |
|     78 |  116 | `	SXUNUSED(pUnused);` |
|      - |  117 | `#ifdef __WINNT__` |
|      3 |  118 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  119 | `#else` |
|    156 |  120 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  121 | `#endif` |
|    159 |  122 | `}` |
|      - |  123 | `/*` |
|      - |  124 | ` * PATH_SEPARATOR.` |
|      - |  125 | ` * Expand the path separator character.` |
|      - |  126 | ` */` |
|      2 |  127 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  128 | `{` |
|      1 |  129 | `	SXUNUSED(pUnused);` |
|      - |  130 | `#ifdef __WINNT__` |
|      1 |  131 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |  132 | `#else` |
|      2 |  133 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |  134 | `#endif` |
|      3 |  135 | `}` |
|      - |  136 |  |
|      - |  137 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  138 | `/*` |
|      - |  139 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  140 | ` */` |
|     52 |  141 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  142 | `{` |
|     26 |  143 | `	SXUNUSED(pUnused);` |
|     53 |  144 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     53 |  145 | `}` |
|      - |  146 |  |
|      - |  147 | `/*` |
|      - |  148 | ` * INF constant: positive infinity` |
|      - |  149 | ` */` |
|     28 |  150 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  151 | `{` |
|     14 |  152 | `	SXUNUSED(pUnused);` |
|      - |  153 | `	/* similarly avoid the INFINITY macro */` |
|     29 |  154 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     29 |  155 | `}` |
|      - |  156 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  157 |  |
|      - |  158 | `#ifndef __WINNT__` |
|      - |  159 | `#include <time.h>` |
|      - |  160 | `#endif` |
|      - |  161 | `/*` |
|      - |  162 | ` * __TIME__` |
|      - |  163 | ` *  Expand the current time (GMT).` |
|      - |  164 | ` */` |
|      2 |  165 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  166 | `{` |
|      - |  167 | `	Sytm sTm;` |
|      - |  168 | `#ifdef __WINNT__` |
|      - |  169 | `	SYSTEMTIME sOS;` |
|      1 |  170 | `	GetSystemTime(&sOS);` |
|      1 |  171 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  172 | `#else` |
|      - |  173 | `	struct tm *pTm;` |
|      - |  174 | `	time_t t;` |
|      2 |  175 | `	time(&t);` |
|      2 |  176 | `	pTm = gmtime(&t);` |
|      2 |  177 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  178 | `#endif` |
|      1 |  179 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  180 | `	/* Expand */` |
|      3 |  181 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  182 | `}` |
|      - |  183 | `/*` |
|      - |  184 | ` * __DATE__` |
|      - |  185 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  186 | ` */` |
|      2 |  187 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  188 | `{` |
|      - |  189 | `	Sytm sTm;` |
|      - |  190 | `#ifdef __WINNT__` |
|      - |  191 | `	SYSTEMTIME sOS;` |
|      1 |  192 | `	GetSystemTime(&sOS);` |
|      1 |  193 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  194 | `#else` |
|      - |  195 | `	struct tm *pTm;` |
|      - |  196 | `	time_t t;` |
|      2 |  197 | `	time(&t);` |
|      2 |  198 | `	pTm = gmtime(&t);` |
|      2 |  199 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  200 | `#endif` |
|      1 |  201 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  202 | `	/* Expand */` |
|      3 |  203 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  204 | `}` |
|      - |  205 | `/*` |
|      - |  206 | ` * __FILE__` |
|      - |  207 | ` *  Path of the processed script.` |
|      - |  208 | ` */` |
|   1364 |  209 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  210 | `{` |
|   1369 |  211 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  212 | `	SyString *pFile;` |
|      - |  213 | `	/* Peek the top entry */` |
|   1369 |  214 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   1369 |  215 | `	if( pFile == 0 ){` |
|      - |  216 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  217 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  218 | `	}else{` |
|   1367 |  219 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  220 | `	}` |
|   1369 |  221 | `}` |
|      - |  222 | `/*` |
|      - |  223 | ` * __DIR__` |
|      - |  224 | ` *  Directory holding the processed script.` |
|      - |  225 | ` */` |
|     38 |  226 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      4 |  227 | `{` |
|     42 |  228 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  229 | `	SyString *pFile;` |
|      - |  230 | `	/* Peek the top entry */` |
|     42 |  231 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     42 |  232 | `	if( pFile == 0 ){` |
|      - |  233 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  234 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  235 | `	}else{` |
|     39 |  236 | `		if( pFile->nByte > 0 ){` |
|      - |  237 | `			const char *zDir;` |
|      - |  238 | `			int nLen;` |
|     39 |  239 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     39 |  240 | `			ph7_value_string(pVal,zDir,nLen);` |
|     21 |  241 | `		}else{` |
|      - |  242 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  243 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  244 | `		}` |
|      - |  245 | `	}` |
|     42 |  246 | `}` |
|      - |  247 | `/*` |
|      - |  248 | ` * PHP_SHLIB_SUFFIX` |
|      - |  249 | ` *  Expand shared library suffix.` |
|      - |  250 | ` */` |
|      2 |  251 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  252 | `{` |
|      - |  253 | `#ifdef __WINNT__` |
|    ! 0 |  254 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  255 | `#else` |
|      2 |  256 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  257 | `#endif` |
|      1 |  258 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  259 | `}` |
|      - |  260 | `/*` |
|      - |  261 | ` * E_ERROR` |
|      - |  262 | ` *  Expands 1` |
|      - |  263 | ` */` |
|      2 |  264 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  265 | `{` |
|      3 |  266 | `	ph7_value_int(pVal,1);` |
|      1 |  267 | `	SXUNUSED(pUserData);` |
|      3 |  268 | `}` |
|      - |  269 | `/*` |
|      - |  270 | ` * E_WARNING` |
|      - |  271 | ` *  Expands 2` |
|      - |  272 | ` */` |
|      2 |  273 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  274 | `{` |
|      3 |  275 | `	ph7_value_int(pVal,2);` |
|      1 |  276 | `	SXUNUSED(pUserData);` |
|      3 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * E_PARSE` |
|      - |  280 | ` *  Expands 4` |
|      - |  281 | ` */` |
|      2 |  282 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  283 | `{` |
|      3 |  284 | `	ph7_value_int(pVal,4);` |
|      1 |  285 | `	SXUNUSED(pUserData);` |
|      3 |  286 | `}` |
|      - |  287 | `/*` |
|      - |  288 | ` * E_NOTICE` |
|      - |  289 | ` * Expands 8` |
|      - |  290 | ` */` |
|      2 |  291 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  292 | `{` |
|      3 |  293 | `	ph7_value_int(pVal,8);` |
|      1 |  294 | `	SXUNUSED(pUserData);` |
|      3 |  295 | `}` |
|      - |  296 | `/*` |
|      - |  297 | ` * E_CORE_ERROR` |
|      - |  298 | ` * Expands 16` |
|      - |  299 | ` */` |
|      2 |  300 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  301 | `{` |
|      3 |  302 | `	ph7_value_int(pVal,16);` |
|      1 |  303 | `	SXUNUSED(pUserData);` |
|      3 |  304 | `}` |
|      - |  305 | `/*` |
|      - |  306 | ` * E_CORE_WARNING` |
|      - |  307 | ` * Expands 32` |
|      - |  308 | ` */` |
|      2 |  309 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  310 | `{` |
|      3 |  311 | `	ph7_value_int(pVal,32);` |
|      1 |  312 | `	SXUNUSED(pUserData);` |
|      3 |  313 | `}` |
|      - |  314 | `/*` |
|      - |  315 | ` * E_COMPILE_ERROR` |
|      - |  316 | ` * Expands 64` |
|      - |  317 | ` */` |
|      2 |  318 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  319 | `{` |
|      3 |  320 | `	ph7_value_int(pVal,64);` |
|      1 |  321 | `	SXUNUSED(pUserData);` |
|      3 |  322 | `}` |
|      - |  323 | `/*` |
|      - |  324 | ` * E_COMPILE_WARNING` |
|      - |  325 | ` * Expands 128` |
|      - |  326 | ` */` |
|      2 |  327 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  328 | `{` |
|      3 |  329 | `	ph7_value_int(pVal,128);` |
|      1 |  330 | `	SXUNUSED(pUserData);` |
|      3 |  331 | `}` |
|      - |  332 | `/*` |
|      - |  333 | ` * E_USER_ERROR` |
|      - |  334 | ` * Expands 256` |
|      - |  335 | ` */` |
|      4 |  336 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  337 | `{` |
|      6 |  338 | `	ph7_value_int(pVal,256);` |
|      2 |  339 | `	SXUNUSED(pUserData);` |
|      6 |  340 | `}` |
|      - |  341 | `/*` |
|      - |  342 | ` * E_USER_WARNING` |
|      - |  343 | ` * Expands 512` |
|      - |  344 | ` */` |
|      4 |  345 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  346 | `{` |
|      6 |  347 | `	ph7_value_int(pVal,512);` |
|      2 |  348 | `	SXUNUSED(pUserData);` |
|      6 |  349 | `}` |
|      - |  350 | `/*` |
|      - |  351 | ` * E_USER_NOTICE` |
|      - |  352 | ` * Expands 1024` |
|      - |  353 | ` */` |
|      6 |  354 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  355 | `{` |
|      9 |  356 | `	ph7_value_int(pVal,1024);` |
|      3 |  357 | `	SXUNUSED(pUserData);` |
|      9 |  358 | `}` |
|      - |  359 | `/*` |
|      - |  360 | ` * E_STRICT` |
|      - |  361 | ` * Expands 2048` |
|      - |  362 | ` */` |
|      2 |  363 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  364 | `{` |
|      3 |  365 | `	ph7_value_int(pVal,2048);` |
|      1 |  366 | `	SXUNUSED(pUserData);` |
|      3 |  367 | `}` |
|      - |  368 | `/*` |
|      - |  369 | ` * E_RECOVERABLE_ERROR` |
|      - |  370 | ` * Expands 4096` |
|      - |  371 | ` */` |
|      2 |  372 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  373 | `{` |
|      3 |  374 | `	ph7_value_int(pVal,4096);` |
|      1 |  375 | `	SXUNUSED(pUserData);` |
|      3 |  376 | `}` |
|      - |  377 | `/*` |
|      - |  378 | ` * E_DEPRECATED` |
|      - |  379 | ` * Expands 8192` |
|      - |  380 | ` */` |
|     22 |  381 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  382 | `{` |
|     27 |  383 | `	ph7_value_int(pVal,8192);` |
|     11 |  384 | `	SXUNUSED(pUserData);` |
|     27 |  385 | `}` |
|      - |  386 | `/*` |
|      - |  387 | ` * E_USER_DEPRECATED` |
|      - |  388 | ` *   Expands 16384.` |
|      - |  389 | ` */` |
|      2 |  390 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  391 | `{` |
|      3 |  392 | `	ph7_value_int(pVal,16384);` |
|      1 |  393 | `	SXUNUSED(pUserData);` |
|      3 |  394 | `}` |
|      - |  395 | `/*` |
|      - |  396 | ` * E_ALL` |
|      - |  397 | ` *  Expands 32767` |
|      - |  398 | ` */` |
|     22 |  399 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  400 | `{` |
|     27 |  401 | `	ph7_value_int(pVal,32767);` |
|     11 |  402 | `	SXUNUSED(pUserData);` |
|     27 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * CASE_LOWER` |
|      - |  406 | ` *  Expands 0.` |
|      - |  407 | ` */` |
|      2 |  408 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  409 | `{` |
|      3 |  410 | `	ph7_value_int(pVal,0);` |
|      1 |  411 | `	SXUNUSED(pUserData);` |
|      3 |  412 | `}` |
|      - |  413 | `/*` |
|      - |  414 | ` * CASE_UPPER` |
|      - |  415 | ` *  Expands 1.` |
|      - |  416 | ` */` |
|      2 |  417 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  418 | `{` |
|      3 |  419 | `	ph7_value_int(pVal,1);` |
|      1 |  420 | `	SXUNUSED(pUserData);` |
|      3 |  421 | `}` |
|      - |  422 | `/*` |
|      - |  423 | ` * STR_PAD_LEFT` |
|      - |  424 | ` *  Expands 0.` |
|      - |  425 | ` */` |
|      4 |  426 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  427 | `{` |
|      5 |  428 | `	ph7_value_int(pVal,0);` |
|      2 |  429 | `	SXUNUSED(pUserData);` |
|      5 |  430 | `}` |
|      - |  431 | `/*` |
|      - |  432 | ` * STR_PAD_RIGHT` |
|      - |  433 | ` *  Expands 1.` |
|      - |  434 | ` */` |
|      4 |  435 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  436 | `{` |
|      5 |  437 | `	ph7_value_int(pVal,1);` |
|      2 |  438 | `	SXUNUSED(pUserData);` |
|      5 |  439 | `}` |
|      - |  440 | `/*` |
|      - |  441 | ` * STR_PAD_BOTH` |
|      - |  442 | ` *  Expands 2.` |
|      - |  443 | ` */` |
|      2 |  444 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  445 | `{` |
|      3 |  446 | `	ph7_value_int(pVal,2);` |
|      1 |  447 | `	SXUNUSED(pUserData);` |
|      3 |  448 | `}` |
|      - |  449 | `/*` |
|      - |  450 | ` * COUNT_NORMAL` |
|      - |  451 | ` *  Expands 0` |
|      - |  452 | ` */` |
|      6 |  453 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  454 | `{` |
|      8 |  455 | `	ph7_value_int(pVal,0);` |
|      3 |  456 | `	SXUNUSED(pUserData);` |
|      8 |  457 | `}` |
|      - |  458 | `/*` |
|      - |  459 | ` * COUNT_RECURSIVE` |
|      - |  460 | ` *  Expands 1.` |
|      - |  461 | ` */` |
|     20 |  462 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  463 | `{` |
|     22 |  464 | `	ph7_value_int(pVal,1);` |
|     10 |  465 | `	SXUNUSED(pUserData);` |
|     22 |  466 | `}` |
|      - |  467 | `/*` |
|      - |  468 | ` * SORT_ASC` |
|      - |  469 | ` *  Expands 1.` |
|      - |  470 | ` */` |
|      2 |  471 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  472 | `{` |
|      3 |  473 | `	ph7_value_int(pVal,1);` |
|      1 |  474 | `	SXUNUSED(pUserData);` |
|      3 |  475 | `}` |
|      - |  476 | `/*` |
|      - |  477 | ` * SORT_DESC` |
|      - |  478 | ` *  Expands 2.` |
|      - |  479 | ` */` |
|      2 |  480 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  481 | `{` |
|      3 |  482 | `	ph7_value_int(pVal,2);` |
|      1 |  483 | `	SXUNUSED(pUserData);` |
|      3 |  484 | `}` |
|      - |  485 | `/*` |
|      - |  486 | ` * SORT_REGULAR` |
|      - |  487 | ` *  Expands 3.` |
|      - |  488 | ` */` |
|      4 |  489 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  490 | `{` |
|      5 |  491 | `	ph7_value_int(pVal,3);` |
|      2 |  492 | `	SXUNUSED(pUserData);` |
|      5 |  493 | `}` |
|      - |  494 | `/*` |
|      - |  495 | ` * SORT_NUMERIC` |
|      - |  496 | ` *  Expands 4.` |
|      - |  497 | ` */` |
|      6 |  498 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  499 | `{` |
|      7 |  500 | `	ph7_value_int(pVal,4);` |
|      3 |  501 | `	SXUNUSED(pUserData);` |
|      7 |  502 | `}` |
|      - |  503 | `/*` |
|      - |  504 | ` * SORT_STRING` |
|      - |  505 | ` *  Expands 5.` |
|      - |  506 | ` */` |
|     10 |  507 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  508 | `{` |
|     11 |  509 | `	ph7_value_int(pVal,5);` |
|      5 |  510 | `	SXUNUSED(pUserData);` |
|     11 |  511 | `}` |
|      - |  512 | `/*` |
|      - |  513 | ` * PHP_ROUND_HALF_UP` |
|      - |  514 | ` *  Expands 1.` |
|      - |  515 | ` */` |
|      4 |  516 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  517 | `{` |
|      5 |  518 | `	ph7_value_int(pVal,1);` |
|      2 |  519 | `	SXUNUSED(pUserData);` |
|      5 |  520 | `}` |
|      - |  521 | `/*` |
|      - |  522 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  523 | ` *  Expands 2.` |
|      - |  524 | ` */` |
|      4 |  525 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  526 | `{` |
|      5 |  527 | `	ph7_value_int(pVal,2);` |
|      2 |  528 | `	SXUNUSED(pUserData);` |
|      5 |  529 | `}` |
|      - |  530 | `/*` |
|      - |  531 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  532 | ` *  Expands 3.` |
|      - |  533 | ` */` |
|      8 |  534 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  535 | `{` |
|      9 |  536 | `	ph7_value_int(pVal,3);` |
|      4 |  537 | `	SXUNUSED(pUserData);` |
|      9 |  538 | `}` |
|      - |  539 | `/*` |
|      - |  540 | ` * PHP_ROUND_HALF_ODD` |
|      - |  541 | ` *  Expands 4.` |
|      - |  542 | ` */` |
|      4 |  543 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  544 | `{` |
|      5 |  545 | `	ph7_value_int(pVal,4);` |
|      2 |  546 | `	SXUNUSED(pUserData);` |
|      5 |  547 | `}` |
|      - |  548 | `/*` |
|      - |  549 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  550 | ` *  Expand 0x01` |
|      - |  551 | ` * NOTE:` |
|      - |  552 | ` *  The expanded value must be a power of two.` |
|      - |  553 | ` */` |
|      2 |  554 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  555 | `{` |
|      3 |  556 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  557 | `	SXUNUSED(pUserData);` |
|      3 |  558 | `}` |
|      - |  559 | `/*` |
|      - |  560 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  561 | ` *  Expand 0x02` |
|      - |  562 | ` * NOTE:` |
|      - |  563 | ` *  The expanded value must be a power of two.` |
|      - |  564 | ` */` |
|      2 |  565 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  566 | `{` |
|      3 |  567 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  568 | `	SXUNUSED(pUserData);` |
|      3 |  569 | `}` |
|      - |  570 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  571 | `/*` |
|      - |  572 | ` * M_PI` |
|      - |  573 | ` *  Expand the value of pi.` |
|      - |  574 | ` */` |
|      2 |  575 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  576 | `{` |
|      1 |  577 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  578 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  579 | `}` |
|      - |  580 | `/*` |
|      - |  581 | ` * M_E` |
|      - |  582 | ` *  Expand 2.7182818284590452354` |
|      - |  583 | ` */` |
|      2 |  584 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  585 | `{` |
|      1 |  586 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  587 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  588 | `}` |
|      - |  589 | `/*` |
|      - |  590 | ` * M_LOG2E` |
|      - |  591 | ` *  Expand 2.7182818284590452354` |
|      - |  592 | ` */` |
|      2 |  593 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  594 | `{` |
|      1 |  595 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  596 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  597 | `}` |
|      - |  598 | `/*` |
|      - |  599 | ` * M_LOG10E` |
|      - |  600 | ` *  Expand 0.4342944819032518276` |
|      - |  601 | ` */` |
|      2 |  602 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  603 | `{` |
|      1 |  604 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  605 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  606 | `}` |
|      - |  607 | `/*` |
|      - |  608 | ` * M_LN2` |
|      - |  609 | ` *  Expand 	0.69314718055994530942` |
|      - |  610 | ` */` |
|      2 |  611 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  612 | `{` |
|      1 |  613 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  614 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  615 | `}` |
|      - |  616 | `/*` |
|      - |  617 | ` * M_LN10` |
|      - |  618 | ` *  Expand 	2.30258509299404568402` |
|      - |  619 | ` */` |
|      2 |  620 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  621 | `{` |
|      1 |  622 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  623 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  624 | `}` |
|      - |  625 | `/*` |
|      - |  626 | ` * M_PI_2` |
|      - |  627 | ` *  Expand 	1.57079632679489661923` |
|      - |  628 | ` */` |
|      2 |  629 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  630 | `{` |
|      1 |  631 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  632 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  633 | `}` |
|      - |  634 | `/*` |
|      - |  635 | ` * M_PI_4` |
|      - |  636 | ` *  Expand 	0.78539816339744830962` |
|      - |  637 | ` */` |
|      2 |  638 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  639 | `{` |
|      1 |  640 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  641 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  642 | `}` |
|      - |  643 | `/*` |
|      - |  644 | ` * M_1_PI` |
|      - |  645 | ` *  Expand 	0.31830988618379067154` |
|      - |  646 | ` */` |
|      2 |  647 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  648 | `{` |
|      1 |  649 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  650 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  651 | `}` |
|      - |  652 | `/*` |
|      - |  653 | ` * M_2_PI` |
|      - |  654 | ` *  Expand 0.63661977236758134308` |
|      - |  655 | ` */` |
|      4 |  656 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  657 | `{` |
|      2 |  658 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  659 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  660 | `}` |
|      - |  661 | `/*` |
|      - |  662 | ` * M_SQRTPI` |
|      - |  663 | ` *  Expand 1.77245385090551602729` |
|      - |  664 | ` */` |
|      2 |  665 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  666 | `{` |
|      1 |  667 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  668 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  669 | `}` |
|      - |  670 | `/*` |
|      - |  671 | ` * M_2_SQRTPI` |
|      - |  672 | ` *  Expand 	1.12837916709551257390` |
|      - |  673 | ` */` |
|      2 |  674 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  675 | `{` |
|      1 |  676 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  677 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  678 | `}` |
|      - |  679 | `/*` |
|      - |  680 | ` * M_SQRT2` |
|      - |  681 | ` *  Expand 	1.41421356237309504880` |
|      - |  682 | ` */` |
|      2 |  683 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  684 | `{` |
|      1 |  685 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  686 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  687 | `}` |
|      - |  688 | `/*` |
|      - |  689 | ` * M_SQRT3` |
|      - |  690 | ` *  Expand 	1.73205080756887729352` |
|      - |  691 | ` */` |
|      2 |  692 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  693 | `{` |
|      1 |  694 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  695 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  696 | `}` |
|      - |  697 | `/*` |
|      - |  698 | ` * M_SQRT1_2` |
|      - |  699 | ` *  Expand 	0.70710678118654752440` |
|      - |  700 | ` */` |
|      2 |  701 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  702 | `{` |
|      1 |  703 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  704 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  705 | `}` |
|      - |  706 | `/*` |
|      - |  707 | ` * M_LNPI` |
|      - |  708 | ` *  Expand 	1.14472988584940017414` |
|      - |  709 | ` */` |
|      2 |  710 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  711 | `{` |
|      1 |  712 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  713 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  714 | `}` |
|      - |  715 | `/*` |
|      - |  716 | ` * M_EULER` |
|      - |  717 | ` *  Expand  0.57721566490153286061` |
|      - |  718 | ` */` |
|      2 |  719 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  720 | `{` |
|      1 |  721 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  722 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  723 | `}` |
|      - |  724 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  725 | `/*` |
|      - |  726 | ` * DATE_ATOM` |
|      - |  727 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  728 | ` */` |
|      2 |  729 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  730 | `{` |
|      1 |  731 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  732 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  733 | `}` |
|      - |  734 | `/*` |
|      - |  735 | ` * DATE_COOKIE` |
|      - |  736 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  737 | ` */` |
|      2 |  738 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  739 | `{` |
|      1 |  740 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  741 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  742 | `}` |
|      - |  743 | `/*` |
|      - |  744 | ` * DATE_ISO8601` |
|      - |  745 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  746 | ` */` |
|      2 |  747 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  748 | `{` |
|      1 |  749 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  750 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  751 | `}` |
|      - |  752 | `/*` |
|      - |  753 | ` * DATE_RFC822` |
|      - |  754 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  755 | ` */` |
|      2 |  756 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  757 | `{` |
|      1 |  758 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  759 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  760 | `}` |
|      - |  761 | `/*` |
|      - |  762 | ` * DATE_RFC850` |
|      - |  763 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  764 | ` */` |
|      2 |  765 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  766 | `{` |
|      1 |  767 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  768 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  769 | `}` |
|      - |  770 | `/*` |
|      - |  771 | ` * DATE_RFC1036` |
|      - |  772 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  773 | ` */` |
|      2 |  774 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  775 | `{` |
|      1 |  776 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  777 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  778 | `}` |
|      - |  779 | `/*` |
|      - |  780 | ` * DATE_RFC1123` |
|      - |  781 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  782 | ` */` |
|      2 |  783 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  784 | `{` |
|      1 |  785 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  786 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  787 | `}` |
|      - |  788 | `/*` |
|      - |  789 | ` * DATE_RFC2822` |
|      - |  790 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  791 | ` */` |
|      2 |  792 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  793 | `{` |
|      1 |  794 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  795 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  796 | `}` |
|      - |  797 | `/*` |
|      - |  798 | ` * DATE_RSS` |
|      - |  799 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  800 | ` */` |
|      2 |  801 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  802 | `{` |
|      1 |  803 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  804 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  805 | `}` |
|      - |  806 | `/*` |
|      - |  807 | ` * DATE_W3C` |
|      - |  808 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  809 | ` */` |
|      2 |  810 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  811 | `{` |
|      1 |  812 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  813 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  814 | `}` |
|      - |  815 | `/*` |
|      - |  816 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  817 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  818 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  819 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  820 | ` */` |
|      - |  821 | `/*` |
|      - |  822 | ` * ENT_COMPAT` |
|      - |  823 | ` *  Expand 2 (double-quote bit only)` |
|      - |  824 | ` */` |
|     12 |  825 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  826 | `{` |
|      6 |  827 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  828 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  829 | `}` |
|      - |  830 | `/*` |
|      - |  831 | ` * ENT_QUOTES` |
|      - |  832 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  833 | ` */` |
|     60 |  834 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  835 | `{` |
|     30 |  836 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  837 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * ENT_NOQUOTES` |
|      - |  841 | ` *  Expand 0 (no quote bits)` |
|      - |  842 | ` */` |
|     20 |  843 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  844 | `{` |
|     10 |  845 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  846 | `	ph7_value_int(pVal,0);` |
|     21 |  847 | `}` |
|      - |  848 | `/*` |
|      - |  849 | ` * ENT_IGNORE` |
|      - |  850 | ` *  Expand 4` |
|      - |  851 | ` */` |
|      6 |  852 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  853 | `{` |
|      3 |  854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  855 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  856 | `}` |
|      - |  857 | `/*` |
|      - |  858 | ` * ENT_SUBSTITUTE` |
|      - |  859 | ` *  Expand 8` |
|      - |  860 | ` */` |
|      2 |  861 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  862 | `{` |
|      1 |  863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  864 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  865 | `}` |
|      - |  866 | `/*` |
|      - |  867 | ` * ENT_DISALLOWED` |
|      - |  868 | ` *  Expand 128` |
|      - |  869 | ` */` |
|      2 |  870 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  871 | `{` |
|      1 |  872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  873 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  874 | `}` |
|      - |  875 | `/*` |
|      - |  876 | ` * ENT_HTML401` |
|      - |  877 | ` *  Expand 0 (the default doctype)` |
|      - |  878 | ` */` |
|      2 |  879 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  880 | `{` |
|      1 |  881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  882 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  883 | `}` |
|      - |  884 | `/*` |
|      - |  885 | ` * ENT_XML1` |
|      - |  886 | ` *  Expand 16` |
|      - |  887 | ` */` |
|      8 |  888 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  889 | `{` |
|      4 |  890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  891 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * ENT_XHTML` |
|      - |  895 | ` *  Expand 32` |
|      - |  896 | ` */` |
|      6 |  897 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  898 | `{` |
|      3 |  899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  900 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * ENT_HTML5` |
|      - |  904 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - |  905 | ` */` |
|      8 |  906 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  907 | `{` |
|      4 |  908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  909 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 |  910 | `}` |
|      - |  911 | `/*` |
|      - |  912 | ` * ISO-8859-1` |
|      - |  913 | ` * ISO_8859_1` |
|      - |  914 | ` *   Expand 1` |
|      - |  915 | ` */` |
|      2 |  916 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  917 | `{` |
|      1 |  918 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  919 | `	ph7_value_int(pVal,1);` |
|      3 |  920 | `}` |
|      - |  921 | `/*` |
|      - |  922 | ` * UTF-8` |
|      - |  923 | ` * UTF8` |
|      - |  924 | ` *  Expand 2` |
|      - |  925 | ` */` |
|      2 |  926 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  927 | `{` |
|      1 |  928 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  929 | `	ph7_value_int(pVal,1);` |
|      3 |  930 | `}` |
|      - |  931 | `/*` |
|      - |  932 | ` * HTML_ENTITIES` |
|      - |  933 | ` *  Expand 1` |
|      - |  934 | ` */` |
|      4 |  935 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  936 | `{` |
|      2 |  937 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  938 | `	ph7_value_int(pVal,1);` |
|      5 |  939 | `}` |
|      - |  940 | `/*` |
|      - |  941 | ` * HTML_SPECIALCHARS` |
|      - |  942 | ` *  Expand 0 (PHP-exact)` |
|      - |  943 | ` */` |
|     10 |  944 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  945 | `{` |
|      5 |  946 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 |  947 | `	ph7_value_int(pVal,0);` |
|     11 |  948 | `}` |
|      - |  949 | `/*` |
|      - |  950 | ` * PHP_URL_SCHEME.` |
|      - |  951 | ` * Expand 1` |
|      - |  952 | ` */` |
|      2 |  953 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  954 | `{` |
|      1 |  955 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  956 | `	ph7_value_int(pVal,1);` |
|      3 |  957 | `}` |
|      - |  958 | `/*` |
|      - |  959 | ` * PHP_URL_HOST.` |
|      - |  960 | ` * Expand 2` |
|      - |  961 | ` */` |
|      2 |  962 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  963 | `{` |
|      1 |  964 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  965 | `	ph7_value_int(pVal,2);` |
|      3 |  966 | `}` |
|      - |  967 | `/*` |
|      - |  968 | ` * PHP_URL_PORT.` |
|      - |  969 | ` * Expand 3` |
|      - |  970 | ` */` |
|      2 |  971 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  972 | `{` |
|      1 |  973 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  974 | `	ph7_value_int(pVal,3);` |
|      3 |  975 | `}` |
|      - |  976 | `/*` |
|      - |  977 | ` * PHP_URL_USER.` |
|      - |  978 | ` * Expand 4` |
|      - |  979 | ` */` |
|      2 |  980 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  981 | `{` |
|      1 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  983 | `	ph7_value_int(pVal,4);` |
|      3 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * PHP_URL_PASS.` |
|      - |  987 | ` * Expand 5` |
|      - |  988 | ` */` |
|      2 |  989 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  990 | `{` |
|      1 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  992 | `	ph7_value_int(pVal,5);` |
|      3 |  993 | `}` |
|      - |  994 | `/*` |
|      - |  995 | ` * PHP_URL_PATH.` |
|      - |  996 | ` * Expand 6` |
|      - |  997 | ` */` |
|      2 |  998 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  999 | `{` |
|      1 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1001 | `	ph7_value_int(pVal,6);` |
|      3 | 1002 | `}` |
|      - | 1003 | `/*` |
|      - | 1004 | ` * PHP_URL_QUERY.` |
|      - | 1005 | ` * Expand 7` |
|      - | 1006 | ` */` |
|      2 | 1007 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1008 | `{` |
|      1 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1010 | `	ph7_value_int(pVal,7);` |
|      3 | 1011 | `}` |
|      - | 1012 | `/*` |
|      - | 1013 | ` * PHP_URL_FRAGMENT.` |
|      - | 1014 | ` * Expand 8` |
|      - | 1015 | ` */` |
|      2 | 1016 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1017 | `{` |
|      1 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1019 | `	ph7_value_int(pVal,8);` |
|      3 | 1020 | `}` |
|      - | 1021 | `/*` |
|      - | 1022 | ` * PHP_QUERY_RFC1738` |
|      - | 1023 | ` * Expand 1` |
|      - | 1024 | ` */` |
|      2 | 1025 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1026 | `{` |
|      1 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1028 | `	ph7_value_int(pVal,1);` |
|      3 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * PHP_QUERY_RFC3986` |
|      - | 1032 | ` * Expand 1` |
|      - | 1033 | ` */` |
|      2 | 1034 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1035 | `{` |
|      1 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1037 | `	ph7_value_int(pVal,2);` |
|      3 | 1038 | `}` |
|      - | 1039 | `/*` |
|      - | 1040 | ` * FNM_NOESCAPE` |
|      - | 1041 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1042 | ` */` |
|      2 | 1043 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1044 | `{` |
|      1 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1046 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1047 | `}` |
|      - | 1048 | `/*` |
|      - | 1049 | ` * FNM_PATHNAME` |
|      - | 1050 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1051 | ` */` |
|      2 | 1052 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1053 | `{` |
|      1 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1055 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * FNM_PERIOD` |
|      - | 1059 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1060 | ` */` |
|      6 | 1061 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1062 | `{` |
|      3 | 1063 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1064 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1065 | `}` |
|      - | 1066 | `/*` |
|      - | 1067 | ` * FNM_CASEFOLD` |
|      - | 1068 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1069 | ` */` |
|      4 | 1070 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1071 | `{` |
|      2 | 1072 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1073 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1074 | `}` |
|      - | 1075 | `/*` |
|      - | 1076 | ` * PATHINFO_DIRNAME` |
|      - | 1077 | ` *  Expand 1.` |
|      - | 1078 | ` */` |
|      4 | 1079 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1080 | `{` |
|      2 | 1081 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1082 | `	ph7_value_int(pVal,1);` |
|      5 | 1083 | `}` |
|      - | 1084 | `/*` |
|      - | 1085 | ` * PATHINFO_BASENAME` |
|      - | 1086 | ` *  Expand 2.` |
|      - | 1087 | ` */` |
|      4 | 1088 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1089 | `{` |
|      2 | 1090 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1091 | `	ph7_value_int(pVal,2);` |
|      5 | 1092 | `}` |
|      - | 1093 | `/*` |
|      - | 1094 | ` * PATHINFO_EXTENSION` |
|      - | 1095 | ` *  Expand 3.` |
|      - | 1096 | ` */` |
|   6152 | 1097 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1098 | `{` |
|   3076 | 1099 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6157 | 1100 | `	ph7_value_int(pVal,3);` |
|   6157 | 1101 | `}` |
|      - | 1102 | `/*` |
|      - | 1103 | ` * PATHINFO_FILENAME` |
|      - | 1104 | ` *  Expand 4.` |
|      - | 1105 | ` */` |
|   6144 | 1106 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1107 | `{` |
|   3072 | 1108 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6149 | 1109 | `	ph7_value_int(pVal,4);` |
|   6149 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * ASSERT_ACTIVE.` |
|      - | 1113 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1114 | ` */` |
|     14 | 1115 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1116 | `{` |
|      7 | 1117 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1118 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1119 | `}` |
|      - | 1120 | `/*` |
|      - | 1121 | ` * ASSERT_CALLBACK.` |
|      - | 1122 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1123 | ` */` |
|      6 | 1124 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1125 | `{` |
|      3 | 1126 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1127 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1128 | `}` |
|      - | 1129 | `/*` |
|      - | 1130 | ` * ASSERT_BAIL.` |
|      - | 1131 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1132 | ` */` |
|     14 | 1133 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1134 | `{` |
|      7 | 1135 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1136 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1137 | `}` |
|      - | 1138 | `/*` |
|      - | 1139 | ` * ASSERT_WARNING.` |
|      - | 1140 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1141 | ` */` |
|      4 | 1142 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1143 | `{` |
|      2 | 1144 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1145 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1146 | `}` |
|      - | 1147 | `/*` |
|      - | 1148 | ` * ASSERT_EXCEPTION.` |
|      - | 1149 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1150 | ` */` |
|      4 | 1151 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1152 | `{` |
|      2 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1154 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1155 | `}` |
|      - | 1156 | `/*` |
|      - | 1157 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1158 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1159 | ` */` |
|      2 | 1160 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1161 | `{` |
|      1 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1163 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * SEEK_SET.` |
|      - | 1167 | ` *  Expand 0` |
|      - | 1168 | ` */` |
|      2 | 1169 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1170 | `{` |
|      1 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1172 | `	ph7_value_int(pVal,0);` |
|      3 | 1173 | `}` |
|      - | 1174 | `/*` |
|      - | 1175 | ` * SEEK_CUR.` |
|      - | 1176 | ` *  Expand 1` |
|      - | 1177 | ` */` |
|      2 | 1178 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1179 | `{` |
|      1 | 1180 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1181 | `	ph7_value_int(pVal,1);` |
|      3 | 1182 | `}` |
|      - | 1183 | `/*` |
|      - | 1184 | ` * SEEK_END.` |
|      - | 1185 | ` *  Expand 2` |
|      - | 1186 | ` */` |
|      2 | 1187 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1188 | `{` |
|      1 | 1189 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1190 | `	ph7_value_int(pVal,2);` |
|      3 | 1191 | `}` |
|      - | 1192 | `/*` |
|      - | 1193 | ` * LOCK_SH.` |
|      - | 1194 | ` *  Expand 2` |
|      - | 1195 | ` */` |
|      2 | 1196 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1197 | `{` |
|      1 | 1198 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1199 | `	ph7_value_int(pVal,1);` |
|      3 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * LOCK_NB.` |
|      - | 1203 | ` *  Expand 5` |
|      - | 1204 | ` */` |
|      2 | 1205 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1206 | `{` |
|      1 | 1207 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1208 | `	ph7_value_int(pVal,5);` |
|      3 | 1209 | `}` |
|      - | 1210 | `/*` |
|      - | 1211 | ` * LOCK_EX.` |
|      - | 1212 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1213 | ` */` |
|      4 | 1214 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1215 | `{` |
|      2 | 1216 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1217 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * LOCK_UN.` |
|      - | 1221 | ` *  Expand 0` |
|      - | 1222 | ` */` |
|      4 | 1223 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1224 | `{` |
|      2 | 1225 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1226 | `	ph7_value_int(pVal,0);` |
|      5 | 1227 | `}` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1230 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1231 | ` */` |
|      2 | 1232 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1233 | `{` |
|      1 | 1234 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1235 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1236 | `}` |
|      - | 1237 | `/*` |
|      - | 1238 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1239 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1240 | ` */` |
|      2 | 1241 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1242 | `{` |
|      1 | 1243 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1244 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1245 | `}` |
|      - | 1246 | `/*` |
|      - | 1247 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1248 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1249 | ` */` |
|      2 | 1250 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1251 | `{` |
|      1 | 1252 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1253 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * FILE_APPEND` |
|      - | 1257 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1258 | ` */` |
|      2 | 1259 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1260 | `{` |
|      1 | 1261 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1262 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1263 | `}` |
|      - | 1264 | `/*` |
|      - | 1265 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1266 | ` *  Expand 0` |
|      - | 1267 | ` */` |
|   1938 | 1268 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1269 | `{` |
|    969 | 1270 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1943 | 1271 | `	ph7_value_int(pVal,0);` |
|   1943 | 1272 | `}` |
|      - | 1273 | `/*` |
|      - | 1274 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1275 | ` *  Expand 1` |
|      - | 1276 | ` */` |
|    970 | 1277 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1278 | `{` |
|    485 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|    975 | 1280 | `	ph7_value_int(pVal,1);` |
|    975 | 1281 | `}` |
|      - | 1282 | `/*` |
|      - | 1283 | ` * SCANDIR_SORT_NONE` |
|      - | 1284 | ` *  Expand 2` |
|      - | 1285 | ` */` |
|      2 | 1286 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1287 | `{` |
|      1 | 1288 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1289 | `	ph7_value_int(pVal,2);` |
|      3 | 1290 | `}` |
|      - | 1291 | `/*` |
|      - | 1292 | ` * GLOB_MARK` |
|      - | 1293 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1294 | ` */` |
|      2 | 1295 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1296 | `{` |
|      1 | 1297 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1298 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1299 | `}` |
|      - | 1300 | `/*` |
|      - | 1301 | ` * GLOB_NOSORT` |
|      - | 1302 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1303 | ` */` |
|      2 | 1304 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1305 | `{` |
|      1 | 1306 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1307 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1308 | `}` |
|      - | 1309 | `/*` |
|      - | 1310 | ` * GLOB_NOCHECK` |
|      - | 1311 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1312 | ` */` |
|      2 | 1313 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1314 | `{` |
|      1 | 1315 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1316 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1317 | `}` |
|      - | 1318 | `/*` |
|      - | 1319 | ` * GLOB_NOESCAPE` |
|      - | 1320 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1321 | ` */` |
|      2 | 1322 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1323 | `{` |
|      1 | 1324 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1325 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1326 | `}` |
|      - | 1327 | `/*` |
|      - | 1328 | ` * GLOB_BRACE` |
|      - | 1329 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1330 | ` */` |
|      2 | 1331 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1332 | `{` |
|      1 | 1333 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1334 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1335 | `}` |
|      - | 1336 | `/*` |
|      - | 1337 | ` * GLOB_ONLYDIR` |
|      - | 1338 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1339 | ` */` |
|      2 | 1340 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1341 | `{` |
|      1 | 1342 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1343 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1344 | `}` |
|      - | 1345 | `/*` |
|      - | 1346 | ` * GLOB_ERR` |
|      - | 1347 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1348 | ` */` |
|      2 | 1349 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1350 | `{` |
|      1 | 1351 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1352 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1353 | `}` |
|      - | 1354 | `/*` |
|      - | 1355 | ` * STDIN` |
|      - | 1356 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1357 | ` */` |
|      2 | 1358 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1359 | `{` |
|      3 | 1360 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1361 | `	void *pResource;` |
|      3 | 1362 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1363 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1364 | `}` |
|      - | 1365 | `/*` |
|      - | 1366 | ` * STDOUT` |
|      - | 1367 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1368 | ` */` |
|      2 | 1369 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1370 | `{` |
|      3 | 1371 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1372 | `	void *pResource;` |
|      3 | 1373 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1374 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1375 | `}` |
|      - | 1376 | `/*` |
|      - | 1377 | ` * STDERR` |
|      - | 1378 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1379 | ` */` |
|      2 | 1380 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1381 | `{` |
|      3 | 1382 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1383 | `	void *pResource;` |
|      3 | 1384 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1385 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * INI_SCANNER_NORMAL` |
|      - | 1389 | ` *   Expand 1` |
|      - | 1390 | ` */` |
|      2 | 1391 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1392 | `{` |
|      1 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1394 | `	ph7_value_int(pVal,1);` |
|      3 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * INI_SCANNER_RAW` |
|      - | 1398 | ` *   Expand 2` |
|      - | 1399 | ` */` |
|      2 | 1400 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1401 | `{` |
|      1 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1403 | `	ph7_value_int(pVal,2);` |
|      3 | 1404 | `}` |
|      - | 1405 | `/*` |
|      - | 1406 | ` * EXTR_OVERWRITE` |
|      - | 1407 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1408 | ` */` |
|      2 | 1409 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1410 | `{` |
|      1 | 1411 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1412 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * EXTR_SKIP` |
|      - | 1416 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1417 | ` */` |
|      2 | 1418 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1419 | `{` |
|      1 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1421 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * EXTR_PREFIX_SAME` |
|      - | 1425 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1426 | ` */` |
|      2 | 1427 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1428 | `{` |
|      1 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1430 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * EXTR_PREFIX_ALL` |
|      - | 1434 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1435 | ` */` |
|      2 | 1436 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1437 | `{` |
|      1 | 1438 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1439 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * EXTR_PREFIX_INVALID` |
|      - | 1443 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1444 | ` */` |
|      2 | 1445 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1446 | `{` |
|      1 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1448 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * EXTR_IF_EXISTS` |
|      - | 1452 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1453 | ` */` |
|      2 | 1454 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1455 | `{` |
|      1 | 1456 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1457 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1458 | `}` |
|      - | 1459 | `/*` |
|      - | 1460 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1461 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1462 | ` */` |
|      2 | 1463 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1464 | `{` |
|      1 | 1465 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1466 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1467 | `}` |
|      - | 1468 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1469 | `/*` |
|      - | 1470 | ` * XML_ERROR_NONE` |
|      - | 1471 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1472 | ` */` |
|      2 | 1473 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1474 | `{` |
|      1 | 1475 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1476 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1477 | `}` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1480 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1481 | ` */` |
|      2 | 1482 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1483 | `{` |
|      1 | 1484 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1485 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * XML_ERROR_SYNTAX` |
|      - | 1489 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1490 | ` */` |
|      2 | 1491 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1492 | `{` |
|      1 | 1493 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1494 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1498 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1499 | ` */` |
|      2 | 1500 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1501 | `{` |
|      1 | 1502 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1503 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1504 | `}` |
|      - | 1505 | `/*` |
|      - | 1506 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1507 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1508 | ` */` |
|      2 | 1509 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1510 | `{` |
|      1 | 1511 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1512 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1516 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1517 | ` */` |
|      2 | 1518 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1519 | `{` |
|      1 | 1520 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1521 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1522 | `}` |
|      - | 1523 | `/*` |
|      - | 1524 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1525 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1526 | ` */` |
|      2 | 1527 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1528 | `{` |
|      1 | 1529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1530 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1534 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1535 | ` */` |
|      2 | 1536 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1537 | `{` |
|      1 | 1538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1539 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1540 | `}` |
|      - | 1541 | `/*` |
|      - | 1542 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1543 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1544 | ` */` |
|      2 | 1545 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1546 | `{` |
|      1 | 1547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1548 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1552 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1553 | ` */` |
|      2 | 1554 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1555 | `{` |
|      1 | 1556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1557 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1558 | `}` |
|      - | 1559 | `/*` |
|      - | 1560 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1561 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1562 | ` */` |
|      2 | 1563 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1564 | `{` |
|      1 | 1565 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1566 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1567 | `}` |
|      - | 1568 | `/*` |
|      - | 1569 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1570 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1571 | ` */` |
|      2 | 1572 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1573 | `{` |
|      1 | 1574 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1575 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1576 | `}` |
|      - | 1577 | `/*` |
|      - | 1578 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1579 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1580 | ` */` |
|      2 | 1581 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1582 | `{` |
|      1 | 1583 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1584 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1585 | `}` |
|      - | 1586 | `/*` |
|      - | 1587 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1588 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1589 | ` */` |
|      2 | 1590 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1591 | `{` |
|      1 | 1592 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1593 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1594 | `}` |
|      - | 1595 | `/*` |
|      - | 1596 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1597 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1598 | ` */` |
|      2 | 1599 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1600 | `{` |
|      1 | 1601 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1602 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1603 | `}` |
|      - | 1604 | `/*` |
|      - | 1605 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1606 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1607 | ` */` |
|      2 | 1608 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1609 | `{` |
|      1 | 1610 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1611 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1612 | `}` |
|      - | 1613 | `/*` |
|      - | 1614 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1615 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1616 | ` */` |
|      2 | 1617 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1618 | `{` |
|      1 | 1619 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1620 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1624 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1625 | ` */` |
|      2 | 1626 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1627 | `{` |
|      1 | 1628 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1629 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1630 | `}` |
|      - | 1631 | `/*` |
|      - | 1632 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1633 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1634 | ` */` |
|      2 | 1635 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1636 | `{` |
|      1 | 1637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1638 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1639 | `}` |
|      - | 1640 | `/*` |
|      - | 1641 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1642 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1643 | ` */` |
|      2 | 1644 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1645 | `{` |
|      1 | 1646 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1647 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1648 | `}` |
|      - | 1649 | `/*` |
|      - | 1650 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1651 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1652 | ` */` |
|      2 | 1653 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1654 | `{` |
|      1 | 1655 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1656 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1657 | `}` |
|      - | 1658 | `/*` |
|      - | 1659 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1660 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1661 | ` */` |
|      2 | 1662 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1663 | `{` |
|      1 | 1664 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1665 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1666 | `}` |
|      - | 1667 | `/*` |
|      - | 1668 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1669 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1670 | ` */` |
|      2 | 1671 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1672 | `{` |
|      1 | 1673 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1674 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1675 | `}` |
|      - | 1676 | `/*` |
|      - | 1677 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1678 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1679 | ` */` |
|      4 | 1680 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1681 | `{` |
|      2 | 1682 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1683 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1684 | `}` |
|      - | 1685 | `/*` |
|      - | 1686 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1687 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1688 | ` */` |
|      2 | 1689 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1690 | `{` |
|      1 | 1691 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1692 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1693 | `}` |
|      - | 1694 | `/*` |
|      - | 1695 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1696 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1697 | ` */` |
|      4 | 1698 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1699 | `{` |
|      2 | 1700 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1701 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1702 | `}` |
|      - | 1703 | `/*` |
|      - | 1704 | ` * XML_SAX_IMPL.` |
|      - | 1705 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1706 | ` */` |
|      2 | 1707 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1708 | `{` |
|      1 | 1709 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1710 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1711 | `}` |
|      - | 1712 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1713 | `/*` |
|      - | 1714 | ` * JSON_HEX_TAG.` |
|      - | 1715 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1716 | ` */` |
|      2 | 1717 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1718 | `{` |
|      1 | 1719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1720 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1721 | `}` |
|      - | 1722 | `/*` |
|      - | 1723 | ` * JSON_HEX_AMP.` |
|      - | 1724 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1725 | ` */` |
|      2 | 1726 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1727 | `{` |
|      1 | 1728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1729 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1730 | `}` |
|      - | 1731 | `/*` |
|      - | 1732 | ` * JSON_HEX_APOS.` |
|      - | 1733 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1734 | ` */` |
|      2 | 1735 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1736 | `{` |
|      1 | 1737 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1738 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1739 | `}` |
|      - | 1740 | `/*` |
|      - | 1741 | ` * JSON_HEX_QUOT.` |
|      - | 1742 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1743 | ` */` |
|      2 | 1744 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1745 | `{` |
|      1 | 1746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1747 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1748 | `}` |
|      - | 1749 | `/*` |
|      - | 1750 | ` * JSON_FORCE_OBJECT.` |
|      - | 1751 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1752 | ` */` |
|      4 | 1753 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1754 | `{` |
|      2 | 1755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1756 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1757 | `}` |
|      - | 1758 | `/*` |
|      - | 1759 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1760 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1761 | ` */` |
|      2 | 1762 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1763 | `{` |
|      1 | 1764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1765 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1766 | `}` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1769 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1770 | ` */` |
|      2 | 1771 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1772 | `{` |
|      1 | 1773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1774 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1775 | `}` |
|      - | 1776 | `/*` |
|      - | 1777 | ` * JSON_PRETTY_PRINT.` |
|      - | 1778 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1779 | ` */` |
|      2 | 1780 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1781 | `{` |
|      1 | 1782 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1783 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1784 | `}` |
|      - | 1785 | `/*` |
|      - | 1786 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1787 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1788 | ` */` |
|      2 | 1789 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1790 | `{` |
|      1 | 1791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1792 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1793 | `}` |
|      - | 1794 | `/*` |
|      - | 1795 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1796 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1797 | ` */` |
|      2 | 1798 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1799 | `{` |
|      1 | 1800 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1801 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1802 | `}` |
|      - | 1803 | `/*` |
|      - | 1804 | ` * JSON_ERROR_NONE.` |
|      - | 1805 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1806 | ` */` |
|      4 | 1807 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1808 | `{` |
|      2 | 1809 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1810 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1811 | `}` |
|      - | 1812 | `/*` |
|      - | 1813 | ` * JSON_ERROR_DEPTH.` |
|      - | 1814 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1815 | ` */` |
|      2 | 1816 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1817 | `{` |
|      1 | 1818 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1819 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1820 | `}` |
|      - | 1821 | `/*` |
|      - | 1822 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1823 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1824 | ` */` |
|      2 | 1825 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1826 | `{` |
|      1 | 1827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1828 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1829 | `}` |
|      - | 1830 | `/*` |
|      - | 1831 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1832 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1833 | ` */` |
|      2 | 1834 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1835 | `{` |
|      1 | 1836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1837 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1838 | `}` |
|      - | 1839 | `/*` |
|      - | 1840 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1841 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1842 | ` */` |
|      4 | 1843 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1844 | `{` |
|      2 | 1845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1846 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1847 | `}` |
|      - | 1848 | `/*` |
|      - | 1849 | ` * JSON_ERROR_UTF8.` |
|      - | 1850 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1851 | ` */` |
|      2 | 1852 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1853 | `{` |
|      1 | 1854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1855 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1856 | `}` |
|      - | 1857 | `/*` |
|      - | 1858 | ` * static` |
|      - | 1859 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1860 | ` */` |
|      6 | 1861 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1862 | `{` |
|      7 | 1863 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1864 | `	ph7_class *pClass;` |
|      - | 1865 | `	/* Extract the target class if available */` |
|      7 | 1866 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1867 | `	if( pClass ){` |
|      3 | 1868 | `		SyString *pName = &pClass->sName;` |
|      - | 1869 | `		/* Expand class name */` |
|      3 | 1870 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1871 | `	}else{` |
|      - | 1872 | `		/* Expand 'static' */` |
|      5 | 1873 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1874 | `	}` |
|      7 | 1875 | `}` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * self` |
|      - | 1878 | ` * __CLASS__` |
|      - | 1879 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1880 | ` */` |
|      2 | 1881 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1882 | `{` |
|      3 | 1883 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1884 | `	ph7_class *pClass;` |
|      - | 1885 |  |
|      - | 1886 | `	/* Get the declaring class of the current method */` |
|      3 | 1887 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1888 | `	if( pClass == 0 ){` |
|      - | 1889 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1890 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1891 | `	}` |
|      - | 1892 |  |
|      3 | 1893 | `	if( pClass ){` |
|    ! 0 | 1894 | `		SyString *pName = &pClass->sName;` |
|      - | 1895 | `		/* Expand class name */` |
|    ! 0 | 1896 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1897 | `	}else{` |
|      - | 1898 | `		/* Expand null */` |
|      3 | 1899 | `		ph7_value_null(pVal);` |
|      - | 1900 | `	}` |
|      3 | 1901 | `}` |
|      - | 1902 | `/* parent` |
|      - | 1903 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1904 | ` */` |
|      2 | 1905 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1906 | `{` |
|      3 | 1907 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1908 | `	ph7_class *pClass;` |
|      - | 1909 |  |
|      - | 1910 | `	/* Get the declaring class, then its parent */` |
|      3 | 1911 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1912 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 1913 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1914 | `		/* Expand parent class name */` |
|    ! 0 | 1915 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1916 | `	}else{` |
|      - | 1917 | `		/* Expand null */` |
|      3 | 1918 | `		ph7_value_null(pVal);` |
|      - | 1919 | `	}` |
|      3 | 1920 | `}` |
|      - | 1921 |  |
|      - | 1922 | `/*` |
|      - | 1923 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1924 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1925 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1926 | ` */` |
|     20 | 1927 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1928 | `{` |
|     10 | 1929 | `	SXUNUSED(pUnused);` |
|     22 | 1930 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1931 | `}` |
|      - | 1932 | `/*` |
|      - | 1933 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1934 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1935 | ` */` |
|      2 | 1936 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 1937 | `{` |
|      1 | 1938 | `	SXUNUSED(pUnused);` |
|      3 | 1939 | `	ph7_value_int(pVal,12);` |
|      3 | 1940 | `}` |
|      - | 1941 | `/*` |
|      - | 1942 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 1943 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 1944 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 1945 | ` */` |
|      - | 1946 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 1947 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 1948 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 1949 | `	}` |
|     10 | 1950 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 1951 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 1952 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 1953 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 1954 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 1955 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 1956 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 1957 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 1958 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 1959 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 1960 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 1961 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 1962 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 1963 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 1964 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 1965 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 1966 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 1967 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 1968 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 1969 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 1970 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 1971 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 1972 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 1973 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 1974 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 1975 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 1976 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 1977 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 1978 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 1979 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 1980 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 1981 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 1982 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 1983 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 1984 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 1985 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 1986 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 1987 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 1988 | `/*` |
|      - | 1989 | ` * Table of built-in constants.` |
|      - | 1990 | ` */` |
|      - | 1991 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1992 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1993 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1994 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1995 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 1996 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 1997 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 1998 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 1999 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2000 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2001 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2002 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2003 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2004 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2005 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2006 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2007 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2008 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2009 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2010 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2011 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2012 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2013 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2014 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2015 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2016 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2017 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2018 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2019 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2020 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2021 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2022 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2023 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2024 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2025 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2026 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2027 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2028 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2029 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2030 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2031 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2032 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2033 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2034 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2035 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2036 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2037 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2038 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2039 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2040 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2041 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2042 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2043 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2044 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2045 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2046 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2047 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2048 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2049 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2050 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2051 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2052 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2053 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2054 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2055 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2056 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2057 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2058 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2059 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2060 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2061 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2062 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2063 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2064 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2065 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2066 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2067 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2068 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2069 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2070 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2071 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2072 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2073 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2074 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2075 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2076 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2077 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2078 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2079 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2080 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2081 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2082 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2083 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2084 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2085 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2086 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2087 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2088 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2089 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2090 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2091 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2092 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2093 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2094 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2095 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2096 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2097 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2098 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2099 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2100 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2101 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2102 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2103 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2104 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2105 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2106 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2107 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2108 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2109 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2110 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2111 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2112 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2113 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2114 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2115 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2116 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2117 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2118 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2119 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2120 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2121 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2122 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2123 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2124 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2125 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2126 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2127 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2128 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2129 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2130 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2131 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2132 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2133 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2134 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2135 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2136 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2137 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2138 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2139 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2140 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2141 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2142 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2143 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2144 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2145 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2146 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2147 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2148 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2149 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2150 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2151 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2152 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2153 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2154 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2155 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2156 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2157 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2158 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2159 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2160 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2161 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2162 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2163 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2164 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2165 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2166 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2167 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2168 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2169 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2170 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2171 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2172 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2173 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2174 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2175 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2176 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2177 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2178 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2179 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2180 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2181 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2182 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2183 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2184 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2185 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2186 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2187 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2188 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2189 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2190 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2191 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2192 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2193 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2194 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2195 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2196 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2197 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2198 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2199 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2200 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2201 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2202 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2203 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2204 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2205 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2206 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2207 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2208 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2209 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2210 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2211 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2212 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2213 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2214 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2215 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2216 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2217 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2218 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2219 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2220 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2221 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2222 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2223 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2224 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2225 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2226 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2227 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2228 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2229 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2230 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2231 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2232 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2233 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2234 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2235 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2236 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2237 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2238 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2239 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2240 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2241 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2242 | `	{"static",               PH7_static_Const       },` |
|      - | 2243 | `	{"self",                 PH7_self_Const         },` |
|      - | 2244 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2245 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2246 | `};` |
|      - | 2247 | `/*` |
|      - | 2248 | ` * Register the built-in constants defined above.` |
|      - | 2249 | ` */` |
|   3402 | 2250 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2251 | `{` |
|      - | 2252 | `	sxu32 n;` |
|      - | 2253 | `	/*` |
|      - | 2254 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2255 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2256 | `	 */` |
| 853907 | 2257 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 850505 | 2258 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 425255 | 2259 | `	}` |
|   3407 | 2260 | `}` |
