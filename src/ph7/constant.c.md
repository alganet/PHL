# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1070/1082 lines (98.89%)

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
|   3676 |   62 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   63 | `{` |
|      - |   64 | `#if defined(__WINNT__)` |
|      5 |   65 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   66 | `#elif defined(__UNIXES__)` |
|      - |   67 | `	struct utsname sInfo;` |
|   3676 |   68 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   69 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   70 | `	}else{` |
|   3676 |   71 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   72 | `	}` |
|      - |   73 | `#else` |
|      - |   74 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   75 | `#endif` |
|   1838 |   76 | `	SXUNUSED(pUnused);` |
|   3681 |   77 | `}` |
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
|   1322 |  209 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  210 | `{` |
|   1327 |  211 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  212 | `	SyString *pFile;` |
|      - |  213 | `	/* Peek the top entry */` |
|   1327 |  214 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   1327 |  215 | `	if( pFile == 0 ){` |
|      - |  216 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  217 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  218 | `	}else{` |
|   1325 |  219 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  220 | `	}` |
|   1327 |  221 | `}` |
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
|     40 |  236 | `		if( pFile->nByte > 0 ){` |
|      - |  237 | `			const char *zDir;` |
|      - |  238 | `			int nLen;` |
|     40 |  239 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     40 |  240 | `			ph7_value_string(pVal,zDir,nLen);` |
|     22 |  241 | `		}else{` |
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
|      - |  816 | ` * ENT_COMPAT` |
|      - |  817 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  818 | ` */` |
|      2 |  819 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  820 | `{` |
|      1 |  821 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  822 | `	ph7_value_int(pVal,0x01);` |
|      3 |  823 | `}` |
|      - |  824 | `/*` |
|      - |  825 | ` * ENT_QUOTES` |
|      - |  826 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  827 | ` */` |
|     16 |  828 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  829 | `{` |
|      8 |  830 | `	SXUNUSED(pUserData); /* cc warning */` |
|     17 |  831 | `	ph7_value_int(pVal,0x02);` |
|     17 |  832 | `}` |
|      - |  833 | `/*` |
|      - |  834 | ` * ENT_NOQUOTES` |
|      - |  835 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  836 | ` */` |
|     12 |  837 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  838 | `{` |
|      6 |  839 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  840 | `	ph7_value_int(pVal,0x04);` |
|     13 |  841 | `}` |
|      - |  842 | `/*` |
|      - |  843 | ` * ENT_IGNORE` |
|      - |  844 | ` *  Expand 0x08 (Must be a power of two)` |
|      - |  845 | ` */` |
|      2 |  846 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  847 | `{` |
|      1 |  848 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  849 | `	ph7_value_int(pVal,0x08);` |
|      3 |  850 | `}` |
|      - |  851 | `/*` |
|      - |  852 | ` * ENT_SUBSTITUTE` |
|      - |  853 | ` *  Expand 0x10 (Must be a power of two)` |
|      - |  854 | ` */` |
|      2 |  855 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  856 | `{` |
|      1 |  857 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  858 | `	ph7_value_int(pVal,0x10);` |
|      3 |  859 | `}` |
|      - |  860 | `/*` |
|      - |  861 | ` * ENT_DISALLOWED` |
|      - |  862 | ` *  Expand 0x20 (Must be a power of two)` |
|      - |  863 | ` */` |
|      2 |  864 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  865 | `{` |
|      1 |  866 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  867 | `	ph7_value_int(pVal,0x20);` |
|      3 |  868 | `}` |
|      - |  869 | `/*` |
|      - |  870 | ` * ENT_HTML401` |
|      - |  871 | ` *  Expand 0x40 (Must be a power of two)` |
|      - |  872 | ` */` |
|      2 |  873 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  874 | `{` |
|      1 |  875 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  876 | `	ph7_value_int(pVal,0x40);` |
|      3 |  877 | `}` |
|      - |  878 | `/*` |
|      - |  879 | ` * ENT_XML1` |
|      - |  880 | ` *  Expand 0x80 (Must be a power of two)` |
|      - |  881 | ` */` |
|      2 |  882 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  883 | `{` |
|      1 |  884 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  885 | `	ph7_value_int(pVal,0x80);` |
|      3 |  886 | `}` |
|      - |  887 | `/*` |
|      - |  888 | ` * ENT_XHTML` |
|      - |  889 | ` *  Expand 0x100 (Must be a power of two)` |
|      - |  890 | ` */` |
|      2 |  891 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  892 | `{` |
|      1 |  893 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  894 | `	ph7_value_int(pVal,0x100);` |
|      3 |  895 | `}` |
|      - |  896 | `/*` |
|      - |  897 | ` * ENT_HTML5` |
|      - |  898 | ` *  Expand 0x200 (Must be a power of two)` |
|      - |  899 | ` */` |
|      2 |  900 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  901 | `{` |
|      1 |  902 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  903 | `	ph7_value_int(pVal,0x200);` |
|      3 |  904 | `}` |
|      - |  905 | `/*` |
|      - |  906 | ` * ISO-8859-1` |
|      - |  907 | ` * ISO_8859_1` |
|      - |  908 | ` *   Expand 1` |
|      - |  909 | ` */` |
|      2 |  910 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  911 | `{` |
|      1 |  912 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  913 | `	ph7_value_int(pVal,1);` |
|      3 |  914 | `}` |
|      - |  915 | `/*` |
|      - |  916 | ` * UTF-8` |
|      - |  917 | ` * UTF8` |
|      - |  918 | ` *  Expand 2` |
|      - |  919 | ` */` |
|      2 |  920 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  921 | `{` |
|      1 |  922 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  923 | `	ph7_value_int(pVal,1);` |
|      3 |  924 | `}` |
|      - |  925 | `/*` |
|      - |  926 | ` * HTML_ENTITIES` |
|      - |  927 | ` *  Expand 1` |
|      - |  928 | ` */` |
|      2 |  929 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  930 | `{` |
|      1 |  931 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  932 | `	ph7_value_int(pVal,1);` |
|      3 |  933 | `}` |
|      - |  934 | `/*` |
|      - |  935 | ` * HTML_SPECIALCHARS` |
|      - |  936 | ` *  Expand 2` |
|      - |  937 | ` */` |
|      2 |  938 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  939 | `{` |
|      1 |  940 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  941 | `	ph7_value_int(pVal,2);` |
|      3 |  942 | `}` |
|      - |  943 | `/*` |
|      - |  944 | ` * PHP_URL_SCHEME.` |
|      - |  945 | ` * Expand 1` |
|      - |  946 | ` */` |
|      2 |  947 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  948 | `{` |
|      1 |  949 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  950 | `	ph7_value_int(pVal,1);` |
|      3 |  951 | `}` |
|      - |  952 | `/*` |
|      - |  953 | ` * PHP_URL_HOST.` |
|      - |  954 | ` * Expand 2` |
|      - |  955 | ` */` |
|      2 |  956 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  957 | `{` |
|      1 |  958 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  959 | `	ph7_value_int(pVal,2);` |
|      3 |  960 | `}` |
|      - |  961 | `/*` |
|      - |  962 | ` * PHP_URL_PORT.` |
|      - |  963 | ` * Expand 3` |
|      - |  964 | ` */` |
|      2 |  965 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  966 | `{` |
|      1 |  967 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  968 | `	ph7_value_int(pVal,3);` |
|      3 |  969 | `}` |
|      - |  970 | `/*` |
|      - |  971 | ` * PHP_URL_USER.` |
|      - |  972 | ` * Expand 4` |
|      - |  973 | ` */` |
|      2 |  974 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  975 | `{` |
|      1 |  976 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  977 | `	ph7_value_int(pVal,4);` |
|      3 |  978 | `}` |
|      - |  979 | `/*` |
|      - |  980 | ` * PHP_URL_PASS.` |
|      - |  981 | ` * Expand 5` |
|      - |  982 | ` */` |
|      2 |  983 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  984 | `{` |
|      1 |  985 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  986 | `	ph7_value_int(pVal,5);` |
|      3 |  987 | `}` |
|      - |  988 | `/*` |
|      - |  989 | ` * PHP_URL_PATH.` |
|      - |  990 | ` * Expand 6` |
|      - |  991 | ` */` |
|      2 |  992 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  993 | `{` |
|      1 |  994 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  995 | `	ph7_value_int(pVal,6);` |
|      3 |  996 | `}` |
|      - |  997 | `/*` |
|      - |  998 | ` * PHP_URL_QUERY.` |
|      - |  999 | ` * Expand 7` |
|      - | 1000 | ` */` |
|      2 | 1001 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1002 | `{` |
|      1 | 1003 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1004 | `	ph7_value_int(pVal,7);` |
|      3 | 1005 | `}` |
|      - | 1006 | `/*` |
|      - | 1007 | ` * PHP_URL_FRAGMENT.` |
|      - | 1008 | ` * Expand 8` |
|      - | 1009 | ` */` |
|      2 | 1010 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1011 | `{` |
|      1 | 1012 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1013 | `	ph7_value_int(pVal,8);` |
|      3 | 1014 | `}` |
|      - | 1015 | `/*` |
|      - | 1016 | ` * PHP_QUERY_RFC1738` |
|      - | 1017 | ` * Expand 1` |
|      - | 1018 | ` */` |
|      2 | 1019 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1020 | `{` |
|      1 | 1021 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1022 | `	ph7_value_int(pVal,1);` |
|      3 | 1023 | `}` |
|      - | 1024 | `/*` |
|      - | 1025 | ` * PHP_QUERY_RFC3986` |
|      - | 1026 | ` * Expand 1` |
|      - | 1027 | ` */` |
|      2 | 1028 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1029 | `{` |
|      1 | 1030 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1031 | `	ph7_value_int(pVal,2);` |
|      3 | 1032 | `}` |
|      - | 1033 | `/*` |
|      - | 1034 | ` * FNM_NOESCAPE` |
|      - | 1035 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1036 | ` */` |
|      2 | 1037 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1038 | `{` |
|      1 | 1039 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1040 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1041 | `}` |
|      - | 1042 | `/*` |
|      - | 1043 | ` * FNM_PATHNAME` |
|      - | 1044 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1045 | ` */` |
|      2 | 1046 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1047 | `{` |
|      1 | 1048 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1049 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1050 | `}` |
|      - | 1051 | `/*` |
|      - | 1052 | ` * FNM_PERIOD` |
|      - | 1053 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1054 | ` */` |
|      6 | 1055 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1056 | `{` |
|      3 | 1057 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1058 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1059 | `}` |
|      - | 1060 | `/*` |
|      - | 1061 | ` * FNM_CASEFOLD` |
|      - | 1062 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1063 | ` */` |
|      4 | 1064 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1065 | `{` |
|      2 | 1066 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1067 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1068 | `}` |
|      - | 1069 | `/*` |
|      - | 1070 | ` * PATHINFO_DIRNAME` |
|      - | 1071 | ` *  Expand 1.` |
|      - | 1072 | ` */` |
|      4 | 1073 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1074 | `{` |
|      2 | 1075 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1076 | `	ph7_value_int(pVal,1);` |
|      5 | 1077 | `}` |
|      - | 1078 | `/*` |
|      - | 1079 | ` * PATHINFO_BASENAME` |
|      - | 1080 | ` *  Expand 2.` |
|      - | 1081 | ` */` |
|      4 | 1082 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1083 | `{` |
|      2 | 1084 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1085 | `	ph7_value_int(pVal,2);` |
|      5 | 1086 | `}` |
|      - | 1087 | `/*` |
|      - | 1088 | ` * PATHINFO_EXTENSION` |
|      - | 1089 | ` *  Expand 3.` |
|      - | 1090 | ` */` |
|   6094 | 1091 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1092 | `{` |
|   3047 | 1093 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6099 | 1094 | `	ph7_value_int(pVal,3);` |
|   6099 | 1095 | `}` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * PATHINFO_FILENAME` |
|      - | 1098 | ` *  Expand 4.` |
|      - | 1099 | ` */` |
|   6086 | 1100 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1101 | `{` |
|   3043 | 1102 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6091 | 1103 | `	ph7_value_int(pVal,4);` |
|   6091 | 1104 | `}` |
|      - | 1105 | `/*` |
|      - | 1106 | ` * ASSERT_ACTIVE.` |
|      - | 1107 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1108 | ` */` |
|     14 | 1109 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1110 | `{` |
|      7 | 1111 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1112 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1113 | `}` |
|      - | 1114 | `/*` |
|      - | 1115 | ` * ASSERT_CALLBACK.` |
|      - | 1116 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1117 | ` */` |
|      6 | 1118 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1119 | `{` |
|      3 | 1120 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1121 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1122 | `}` |
|      - | 1123 | `/*` |
|      - | 1124 | ` * ASSERT_BAIL.` |
|      - | 1125 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1126 | ` */` |
|     14 | 1127 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1128 | `{` |
|      7 | 1129 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1130 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1131 | `}` |
|      - | 1132 | `/*` |
|      - | 1133 | ` * ASSERT_WARNING.` |
|      - | 1134 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1135 | ` */` |
|      4 | 1136 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1137 | `{` |
|      2 | 1138 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1139 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1140 | `}` |
|      - | 1141 | `/*` |
|      - | 1142 | ` * ASSERT_EXCEPTION.` |
|      - | 1143 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1144 | ` */` |
|      4 | 1145 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1146 | `{` |
|      2 | 1147 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1148 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1149 | `}` |
|      - | 1150 | `/*` |
|      - | 1151 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1152 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1153 | ` */` |
|      2 | 1154 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1155 | `{` |
|      1 | 1156 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1157 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1158 | `}` |
|      - | 1159 | `/*` |
|      - | 1160 | ` * SEEK_SET.` |
|      - | 1161 | ` *  Expand 0` |
|      - | 1162 | ` */` |
|      2 | 1163 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1164 | `{` |
|      1 | 1165 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1166 | `	ph7_value_int(pVal,0);` |
|      3 | 1167 | `}` |
|      - | 1168 | `/*` |
|      - | 1169 | ` * SEEK_CUR.` |
|      - | 1170 | ` *  Expand 1` |
|      - | 1171 | ` */` |
|      2 | 1172 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1173 | `{` |
|      1 | 1174 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1175 | `	ph7_value_int(pVal,1);` |
|      3 | 1176 | `}` |
|      - | 1177 | `/*` |
|      - | 1178 | ` * SEEK_END.` |
|      - | 1179 | ` *  Expand 2` |
|      - | 1180 | ` */` |
|      2 | 1181 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1182 | `{` |
|      1 | 1183 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1184 | `	ph7_value_int(pVal,2);` |
|      3 | 1185 | `}` |
|      - | 1186 | `/*` |
|      - | 1187 | ` * LOCK_SH.` |
|      - | 1188 | ` *  Expand 2` |
|      - | 1189 | ` */` |
|      2 | 1190 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1191 | `{` |
|      1 | 1192 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1193 | `	ph7_value_int(pVal,1);` |
|      3 | 1194 | `}` |
|      - | 1195 | `/*` |
|      - | 1196 | ` * LOCK_NB.` |
|      - | 1197 | ` *  Expand 5` |
|      - | 1198 | ` */` |
|      2 | 1199 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1200 | `{` |
|      1 | 1201 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1202 | `	ph7_value_int(pVal,5);` |
|      3 | 1203 | `}` |
|      - | 1204 | `/*` |
|      - | 1205 | ` * LOCK_EX.` |
|      - | 1206 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1207 | ` */` |
|      4 | 1208 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1209 | `{` |
|      2 | 1210 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1211 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1212 | `}` |
|      - | 1213 | `/*` |
|      - | 1214 | ` * LOCK_UN.` |
|      - | 1215 | ` *  Expand 0` |
|      - | 1216 | ` */` |
|      4 | 1217 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1218 | `{` |
|      2 | 1219 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1220 | `	ph7_value_int(pVal,0);` |
|      5 | 1221 | `}` |
|      - | 1222 | `/*` |
|      - | 1223 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1224 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1225 | ` */` |
|      2 | 1226 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1227 | `{` |
|      1 | 1228 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1229 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1230 | `}` |
|      - | 1231 | `/*` |
|      - | 1232 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1233 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1234 | ` */` |
|      2 | 1235 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1236 | `{` |
|      1 | 1237 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1238 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1239 | `}` |
|      - | 1240 | `/*` |
|      - | 1241 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1242 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1243 | ` */` |
|      2 | 1244 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1245 | `{` |
|      1 | 1246 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1247 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1248 | `}` |
|      - | 1249 | `/*` |
|      - | 1250 | ` * FILE_APPEND` |
|      - | 1251 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1252 | ` */` |
|      2 | 1253 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1254 | `{` |
|      1 | 1255 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1256 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1257 | `}` |
|      - | 1258 | `/*` |
|      - | 1259 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1260 | ` *  Expand 0` |
|      - | 1261 | ` */` |
|   1934 | 1262 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1263 | `{` |
|    967 | 1264 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1939 | 1265 | `	ph7_value_int(pVal,0);` |
|   1939 | 1266 | `}` |
|      - | 1267 | `/*` |
|      - | 1268 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1269 | ` *  Expand 1` |
|      - | 1270 | ` */` |
|    968 | 1271 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1272 | `{` |
|    484 | 1273 | `	SXUNUSED(pUserData); /* cc warning */` |
|    973 | 1274 | `	ph7_value_int(pVal,1);` |
|    973 | 1275 | `}` |
|      - | 1276 | `/*` |
|      - | 1277 | ` * SCANDIR_SORT_NONE` |
|      - | 1278 | ` *  Expand 2` |
|      - | 1279 | ` */` |
|      2 | 1280 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1281 | `{` |
|      1 | 1282 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1283 | `	ph7_value_int(pVal,2);` |
|      3 | 1284 | `}` |
|      - | 1285 | `/*` |
|      - | 1286 | ` * GLOB_MARK` |
|      - | 1287 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1288 | ` */` |
|      2 | 1289 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1290 | `{` |
|      1 | 1291 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1292 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1293 | `}` |
|      - | 1294 | `/*` |
|      - | 1295 | ` * GLOB_NOSORT` |
|      - | 1296 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1297 | ` */` |
|      2 | 1298 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1299 | `{` |
|      1 | 1300 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1301 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1302 | `}` |
|      - | 1303 | `/*` |
|      - | 1304 | ` * GLOB_NOCHECK` |
|      - | 1305 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1306 | ` */` |
|      2 | 1307 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1308 | `{` |
|      1 | 1309 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1310 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1311 | `}` |
|      - | 1312 | `/*` |
|      - | 1313 | ` * GLOB_NOESCAPE` |
|      - | 1314 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1315 | ` */` |
|      2 | 1316 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1317 | `{` |
|      1 | 1318 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1319 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1320 | `}` |
|      - | 1321 | `/*` |
|      - | 1322 | ` * GLOB_BRACE` |
|      - | 1323 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1324 | ` */` |
|      2 | 1325 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1326 | `{` |
|      1 | 1327 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1328 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1329 | `}` |
|      - | 1330 | `/*` |
|      - | 1331 | ` * GLOB_ONLYDIR` |
|      - | 1332 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1333 | ` */` |
|      2 | 1334 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1335 | `{` |
|      1 | 1336 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1337 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1338 | `}` |
|      - | 1339 | `/*` |
|      - | 1340 | ` * GLOB_ERR` |
|      - | 1341 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1342 | ` */` |
|      2 | 1343 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1344 | `{` |
|      1 | 1345 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1346 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1347 | `}` |
|      - | 1348 | `/*` |
|      - | 1349 | ` * STDIN` |
|      - | 1350 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1351 | ` */` |
|      2 | 1352 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1353 | `{` |
|      3 | 1354 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1355 | `	void *pResource;` |
|      3 | 1356 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1357 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1358 | `}` |
|      - | 1359 | `/*` |
|      - | 1360 | ` * STDOUT` |
|      - | 1361 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1362 | ` */` |
|      2 | 1363 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1364 | `{` |
|      3 | 1365 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1366 | `	void *pResource;` |
|      3 | 1367 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1368 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1369 | `}` |
|      - | 1370 | `/*` |
|      - | 1371 | ` * STDERR` |
|      - | 1372 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1373 | ` */` |
|      2 | 1374 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1375 | `{` |
|      3 | 1376 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1377 | `	void *pResource;` |
|      3 | 1378 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1379 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1380 | `}` |
|      - | 1381 | `/*` |
|      - | 1382 | ` * INI_SCANNER_NORMAL` |
|      - | 1383 | ` *   Expand 1` |
|      - | 1384 | ` */` |
|      2 | 1385 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1386 | `{` |
|      1 | 1387 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1388 | `	ph7_value_int(pVal,1);` |
|      3 | 1389 | `}` |
|      - | 1390 | `/*` |
|      - | 1391 | ` * INI_SCANNER_RAW` |
|      - | 1392 | ` *   Expand 2` |
|      - | 1393 | ` */` |
|      2 | 1394 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1395 | `{` |
|      1 | 1396 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1397 | `	ph7_value_int(pVal,2);` |
|      3 | 1398 | `}` |
|      - | 1399 | `/*` |
|      - | 1400 | ` * EXTR_OVERWRITE` |
|      - | 1401 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1402 | ` */` |
|      2 | 1403 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1404 | `{` |
|      1 | 1405 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1406 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1407 | `}` |
|      - | 1408 | `/*` |
|      - | 1409 | ` * EXTR_SKIP` |
|      - | 1410 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1411 | ` */` |
|      2 | 1412 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1413 | `{` |
|      1 | 1414 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1415 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1416 | `}` |
|      - | 1417 | `/*` |
|      - | 1418 | ` * EXTR_PREFIX_SAME` |
|      - | 1419 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1420 | ` */` |
|      2 | 1421 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1422 | `{` |
|      1 | 1423 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1424 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1425 | `}` |
|      - | 1426 | `/*` |
|      - | 1427 | ` * EXTR_PREFIX_ALL` |
|      - | 1428 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1429 | ` */` |
|      2 | 1430 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1431 | `{` |
|      1 | 1432 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1433 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1434 | `}` |
|      - | 1435 | `/*` |
|      - | 1436 | ` * EXTR_PREFIX_INVALID` |
|      - | 1437 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1438 | ` */` |
|      2 | 1439 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1440 | `{` |
|      1 | 1441 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1442 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1443 | `}` |
|      - | 1444 | `/*` |
|      - | 1445 | ` * EXTR_IF_EXISTS` |
|      - | 1446 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1447 | ` */` |
|      2 | 1448 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1449 | `{` |
|      1 | 1450 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1451 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1452 | `}` |
|      - | 1453 | `/*` |
|      - | 1454 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1455 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1456 | ` */` |
|      2 | 1457 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1458 | `{` |
|      1 | 1459 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1460 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1461 | `}` |
|      - | 1462 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1463 | `/*` |
|      - | 1464 | ` * XML_ERROR_NONE` |
|      - | 1465 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1466 | ` */` |
|      2 | 1467 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1468 | `{` |
|      1 | 1469 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1470 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1471 | `}` |
|      - | 1472 | `/*` |
|      - | 1473 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1474 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1475 | ` */` |
|      2 | 1476 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1477 | `{` |
|      1 | 1478 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1479 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1480 | `}` |
|      - | 1481 | `/*` |
|      - | 1482 | ` * XML_ERROR_SYNTAX` |
|      - | 1483 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1484 | ` */` |
|      2 | 1485 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1486 | `{` |
|      1 | 1487 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1488 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1489 | `}` |
|      - | 1490 | `/*` |
|      - | 1491 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1492 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1493 | ` */` |
|      2 | 1494 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1495 | `{` |
|      1 | 1496 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1497 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1498 | `}` |
|      - | 1499 | `/*` |
|      - | 1500 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1501 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1502 | ` */` |
|      2 | 1503 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1504 | `{` |
|      1 | 1505 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1506 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1507 | `}` |
|      - | 1508 | `/*` |
|      - | 1509 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1510 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1511 | ` */` |
|      2 | 1512 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1513 | `{` |
|      1 | 1514 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1515 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1516 | `}` |
|      - | 1517 | `/*` |
|      - | 1518 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1519 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1520 | ` */` |
|      2 | 1521 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1522 | `{` |
|      1 | 1523 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1524 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1525 | `}` |
|      - | 1526 | `/*` |
|      - | 1527 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1528 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1529 | ` */` |
|      2 | 1530 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1531 | `{` |
|      1 | 1532 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1533 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1534 | `}` |
|      - | 1535 | `/*` |
|      - | 1536 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1537 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1538 | ` */` |
|      2 | 1539 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1540 | `{` |
|      1 | 1541 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1542 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1543 | `}` |
|      - | 1544 | `/*` |
|      - | 1545 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1546 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1547 | ` */` |
|      2 | 1548 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1549 | `{` |
|      1 | 1550 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1551 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1552 | `}` |
|      - | 1553 | `/*` |
|      - | 1554 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1555 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1556 | ` */` |
|      2 | 1557 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1558 | `{` |
|      1 | 1559 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1560 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1561 | `}` |
|      - | 1562 | `/*` |
|      - | 1563 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1564 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1565 | ` */` |
|      2 | 1566 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1567 | `{` |
|      1 | 1568 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1569 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1570 | `}` |
|      - | 1571 | `/*` |
|      - | 1572 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1573 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1574 | ` */` |
|      2 | 1575 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1576 | `{` |
|      1 | 1577 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1578 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1579 | `}` |
|      - | 1580 | `/*` |
|      - | 1581 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1582 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1583 | ` */` |
|      2 | 1584 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1585 | `{` |
|      1 | 1586 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1587 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1588 | `}` |
|      - | 1589 | `/*` |
|      - | 1590 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1591 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1592 | ` */` |
|      2 | 1593 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1594 | `{` |
|      1 | 1595 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1596 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1597 | `}` |
|      - | 1598 | `/*` |
|      - | 1599 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1600 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1601 | ` */` |
|      2 | 1602 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1603 | `{` |
|      1 | 1604 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1605 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1606 | `}` |
|      - | 1607 | `/*` |
|      - | 1608 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1609 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1610 | ` */` |
|      2 | 1611 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1612 | `{` |
|      1 | 1613 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1614 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1615 | `}` |
|      - | 1616 | `/*` |
|      - | 1617 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1618 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1619 | ` */` |
|      2 | 1620 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1621 | `{` |
|      1 | 1622 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1623 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1624 | `}` |
|      - | 1625 | `/*` |
|      - | 1626 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1627 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1628 | ` */` |
|      2 | 1629 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1630 | `{` |
|      1 | 1631 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1632 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1633 | `}` |
|      - | 1634 | `/*` |
|      - | 1635 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1636 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1637 | ` */` |
|      2 | 1638 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1639 | `{` |
|      1 | 1640 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1641 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1642 | `}` |
|      - | 1643 | `/*` |
|      - | 1644 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1645 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1646 | ` */` |
|      2 | 1647 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1648 | `{` |
|      1 | 1649 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1650 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1651 | `}` |
|      - | 1652 | `/*` |
|      - | 1653 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1654 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1655 | ` */` |
|      2 | 1656 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1657 | `{` |
|      1 | 1658 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1659 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1660 | `}` |
|      - | 1661 | `/*` |
|      - | 1662 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1663 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1664 | ` */` |
|      2 | 1665 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1666 | `{` |
|      1 | 1667 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1668 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1669 | `}` |
|      - | 1670 | `/*` |
|      - | 1671 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1672 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1673 | ` */` |
|      4 | 1674 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1675 | `{` |
|      2 | 1676 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1677 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1678 | `}` |
|      - | 1679 | `/*` |
|      - | 1680 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1681 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1682 | ` */` |
|      2 | 1683 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1684 | `{` |
|      1 | 1685 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1686 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1687 | `}` |
|      - | 1688 | `/*` |
|      - | 1689 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1690 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1691 | ` */` |
|      4 | 1692 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1693 | `{` |
|      2 | 1694 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1695 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1696 | `}` |
|      - | 1697 | `/*` |
|      - | 1698 | ` * XML_SAX_IMPL.` |
|      - | 1699 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1700 | ` */` |
|      2 | 1701 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1702 | `{` |
|      1 | 1703 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1704 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1705 | `}` |
|      - | 1706 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1707 | `/*` |
|      - | 1708 | ` * JSON_HEX_TAG.` |
|      - | 1709 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1710 | ` */` |
|      2 | 1711 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1712 | `{` |
|      1 | 1713 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1714 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1715 | `}` |
|      - | 1716 | `/*` |
|      - | 1717 | ` * JSON_HEX_AMP.` |
|      - | 1718 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1719 | ` */` |
|      2 | 1720 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1721 | `{` |
|      1 | 1722 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1723 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1724 | `}` |
|      - | 1725 | `/*` |
|      - | 1726 | ` * JSON_HEX_APOS.` |
|      - | 1727 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1728 | ` */` |
|      2 | 1729 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1730 | `{` |
|      1 | 1731 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1732 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1733 | `}` |
|      - | 1734 | `/*` |
|      - | 1735 | ` * JSON_HEX_QUOT.` |
|      - | 1736 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1737 | ` */` |
|      2 | 1738 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1739 | `{` |
|      1 | 1740 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1741 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1742 | `}` |
|      - | 1743 | `/*` |
|      - | 1744 | ` * JSON_FORCE_OBJECT.` |
|      - | 1745 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1746 | ` */` |
|      4 | 1747 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1748 | `{` |
|      2 | 1749 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1750 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1751 | `}` |
|      - | 1752 | `/*` |
|      - | 1753 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1754 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1755 | ` */` |
|      2 | 1756 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1757 | `{` |
|      1 | 1758 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1759 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1760 | `}` |
|      - | 1761 | `/*` |
|      - | 1762 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1763 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1764 | ` */` |
|      2 | 1765 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1766 | `{` |
|      1 | 1767 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1768 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1769 | `}` |
|      - | 1770 | `/*` |
|      - | 1771 | ` * JSON_PRETTY_PRINT.` |
|      - | 1772 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1773 | ` */` |
|      2 | 1774 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1775 | `{` |
|      1 | 1776 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1777 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1778 | `}` |
|      - | 1779 | `/*` |
|      - | 1780 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1781 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1782 | ` */` |
|      2 | 1783 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1784 | `{` |
|      1 | 1785 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1786 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1787 | `}` |
|      - | 1788 | `/*` |
|      - | 1789 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1790 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1791 | ` */` |
|      2 | 1792 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1793 | `{` |
|      1 | 1794 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1795 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1796 | `}` |
|      - | 1797 | `/*` |
|      - | 1798 | ` * JSON_ERROR_NONE.` |
|      - | 1799 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1800 | ` */` |
|      4 | 1801 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1802 | `{` |
|      2 | 1803 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1804 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1805 | `}` |
|      - | 1806 | `/*` |
|      - | 1807 | ` * JSON_ERROR_DEPTH.` |
|      - | 1808 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1809 | ` */` |
|      2 | 1810 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1811 | `{` |
|      1 | 1812 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1813 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1814 | `}` |
|      - | 1815 | `/*` |
|      - | 1816 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1817 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1818 | ` */` |
|      2 | 1819 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1820 | `{` |
|      1 | 1821 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1822 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1823 | `}` |
|      - | 1824 | `/*` |
|      - | 1825 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1826 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1827 | ` */` |
|      2 | 1828 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1829 | `{` |
|      1 | 1830 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1831 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1832 | `}` |
|      - | 1833 | `/*` |
|      - | 1834 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1835 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1836 | ` */` |
|      4 | 1837 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1838 | `{` |
|      2 | 1839 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1840 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1841 | `}` |
|      - | 1842 | `/*` |
|      - | 1843 | ` * JSON_ERROR_UTF8.` |
|      - | 1844 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1845 | ` */` |
|      2 | 1846 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1847 | `{` |
|      1 | 1848 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1849 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1850 | `}` |
|      - | 1851 | `/*` |
|      - | 1852 | ` * static` |
|      - | 1853 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1854 | ` */` |
|      6 | 1855 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1856 | `{` |
|      7 | 1857 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1858 | `	ph7_class *pClass;` |
|      - | 1859 | `	/* Extract the target class if available */` |
|      7 | 1860 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1861 | `	if( pClass ){` |
|      3 | 1862 | `		SyString *pName = &pClass->sName;` |
|      - | 1863 | `		/* Expand class name */` |
|      3 | 1864 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1865 | `	}else{` |
|      - | 1866 | `		/* Expand 'static' */` |
|      5 | 1867 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1868 | `	}` |
|      7 | 1869 | `}` |
|      - | 1870 | `/*` |
|      - | 1871 | ` * self` |
|      - | 1872 | ` * __CLASS__` |
|      - | 1873 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1874 | ` */` |
|      2 | 1875 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1876 | `{` |
|      3 | 1877 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1878 | `	ph7_class *pClass;` |
|      - | 1879 |  |
|      - | 1880 | `	/* Get the declaring class of the current method */` |
|      3 | 1881 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1882 | `	if( pClass == 0 ){` |
|      - | 1883 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1884 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1885 | `	}` |
|      - | 1886 |  |
|      3 | 1887 | `	if( pClass ){` |
|    ! 0 | 1888 | `		SyString *pName = &pClass->sName;` |
|      - | 1889 | `		/* Expand class name */` |
|    ! 0 | 1890 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1891 | `	}else{` |
|      - | 1892 | `		/* Expand null */` |
|      3 | 1893 | `		ph7_value_null(pVal);` |
|      - | 1894 | `	}` |
|      3 | 1895 | `}` |
|      - | 1896 | `/* parent` |
|      - | 1897 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1898 | ` */` |
|      2 | 1899 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1900 | `{` |
|      3 | 1901 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1902 | `	ph7_class *pClass;` |
|      - | 1903 |  |
|      - | 1904 | `	/* Get the declaring class, then its parent */` |
|      3 | 1905 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1906 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 1907 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1908 | `		/* Expand parent class name */` |
|    ! 0 | 1909 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1910 | `	}else{` |
|      - | 1911 | `		/* Expand null */` |
|      3 | 1912 | `		ph7_value_null(pVal);` |
|      - | 1913 | `	}` |
|      3 | 1914 | `}` |
|      - | 1915 |  |
|      - | 1916 | `/*` |
|      - | 1917 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1918 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1919 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1920 | ` */` |
|     20 | 1921 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1922 | `{` |
|     10 | 1923 | `	SXUNUSED(pUnused);` |
|     22 | 1924 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1928 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1929 | ` */` |
|      2 | 1930 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 1931 | `{` |
|      1 | 1932 | `	SXUNUSED(pUnused);` |
|      3 | 1933 | `	ph7_value_int(pVal,12);` |
|      3 | 1934 | `}` |
|      - | 1935 | `/*` |
|      - | 1936 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 1937 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 1938 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 1939 | ` */` |
|      - | 1940 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 1941 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 1942 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 1943 | `	}` |
|      3 | 1944 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|    ! 0 | 1945 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     59 | 1946 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 1947 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     41 | 1948 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 1949 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 1950 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 1951 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     21 | 1952 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 1953 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 1954 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 1955 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 1956 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      5 | 1957 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      3 | 1958 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 1959 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 1960 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 1961 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 1962 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      3 | 1963 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     23 | 1964 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 1965 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 1966 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 1967 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|      7 | 1968 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 1969 | `/*` |
|      - | 1970 | ` * Table of built-in constants.` |
|      - | 1971 | ` */` |
|      - | 1972 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1973 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1974 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1975 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1976 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 1977 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 1978 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 1979 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 1980 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 1981 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 1982 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1983 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1984 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 1985 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 1986 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 1987 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 1988 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 1989 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 1990 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 1991 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 1992 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 1993 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 1994 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 1995 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 1996 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 1997 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 1998 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 1999 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2000 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2001 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2002 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2003 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2004 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2005 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2006 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2007 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2008 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2009 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2010 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2011 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2012 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2013 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2014 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2015 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2016 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2017 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2018 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2019 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2020 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2021 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2022 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2023 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2024 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2025 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2026 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2027 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2028 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2029 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2030 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2031 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2032 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2033 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2034 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2035 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2036 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2037 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2038 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2039 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2040 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2041 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2042 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2043 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2044 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2045 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2046 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2047 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2048 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2049 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2050 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2051 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2052 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2053 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2054 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2055 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2056 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2057 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2058 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2059 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2060 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2061 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2062 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2063 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2064 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2065 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2066 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2067 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2068 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2069 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2070 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2071 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2072 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2073 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2074 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2075 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2076 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2077 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2078 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2079 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2080 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2081 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2082 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2083 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2084 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2085 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2086 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2087 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2088 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2089 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2090 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2091 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2092 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2093 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2094 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2095 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2096 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2097 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2098 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2099 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2100 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2101 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2102 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2103 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2104 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2105 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2106 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2107 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2108 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2109 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2110 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2111 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2112 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2113 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2114 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2115 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2116 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2117 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2118 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2119 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2120 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2121 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2122 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2123 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2124 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2125 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2126 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2127 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2128 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2129 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2130 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2131 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2132 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2133 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2134 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2135 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2136 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2137 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2138 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2139 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2140 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2141 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2142 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2143 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2144 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2145 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2146 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2147 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2148 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2149 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2150 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2151 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2152 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2153 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2154 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2155 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2156 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2157 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2158 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2159 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2160 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2161 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2162 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2163 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2164 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2165 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2166 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2167 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2168 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2169 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2170 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2171 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2172 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2173 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2174 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2175 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2176 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2177 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2178 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2179 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2180 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2181 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2182 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2183 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2184 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2185 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2186 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2187 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2188 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2189 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2190 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2191 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2192 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2193 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2194 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2195 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2196 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2197 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2198 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2199 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2200 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2201 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2202 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2203 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2204 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2205 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2206 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2207 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2208 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2209 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2210 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2211 | `	{"static",               PH7_static_Const       },` |
|      - | 2212 | `	{"self",                 PH7_self_Const         },` |
|      - | 2213 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2214 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2215 | `};` |
|      - | 2216 | `/*` |
|      - | 2217 | ` * Register the built-in constants defined above.` |
|      - | 2218 | ` */` |
|   3268 | 2219 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2220 | `{` |
|      - | 2221 | `	sxu32 n;` |
|      - | 2222 | `	/*` |
|      - | 2223 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2224 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2225 | `	 */` |
| 781057 | 2226 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 777789 | 2227 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 388897 | 2228 | `	}` |
|   3273 | 2229 | `}` |
