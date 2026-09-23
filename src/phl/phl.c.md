# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 329/427 lines (77.05%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `/*` |
|     - |    7 | ` * The PHL interpreter is a simple stand-alone PHP interpreter that allows` |
|     - |    8 | ` * the user to enter and execute PHP files against a PH7 engine.` |
|     - |    9 | ` * To start the phl program, just type "phl" followed by the name of the PHP file` |
|     - |   10 | ` * to compile and execute. That is, the first argument is to the interpreter, the rest` |
|     - |   11 | ` * are scripts arguments, press "Enter" and the PHP code will be executed.` |
|     - |   12 | ` * If something goes wrong while processing the PHP script due to a compile-time error` |
|     - |   13 | ` * your error output (STDOUT) should display the compile-time error messages.` |
|     - |   14 | ` *` |
|     - |   15 | ` * Usage example of the phl interpreter:` |
|     - |   16 | ` *   phl hello_world.php` |
|     - |   17 | ` * Running the interpreter with script arguments` |
|     - |   18 | ` *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s` |
|     - |   19 | ` *` |
|     - |   20 | ` * Command line options:` |
|     - |   21 | ` *   -b: Dump PH7 byte-code instructions` |
|     - |   22 | ` *   -h: Display this help message` |
|     - |   23 | ` *` |
|     - |   24 | ` * The PHL interpreter package includes more than 70 PHP scripts to test ranging from` |
|     - |   25 | ` * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,` |
|     - |   26 | ` * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many` |
|     - |   27 | ` * more. These scripts are available in the scripts directory from the zip archive.` |
|     - |   28 | ` */` |
|     - |   29 | `#include <stdio.h>` |
|     - |   30 | `#include <stdlib.h>` |
|     - |   31 | `#include <string.h>` |
|     - |   32 | `#include <time.h>` |
|     - |   33 | `#include <errno.h>` |
|     - |   34 | `#ifdef __UNIXES__` |
|     - |   35 | `#include <unistd.h>` |
|     - |   36 | `#endif` |
|     - |   37 | `/* Make sure this header file is available.*/` |
|     - |   38 | `#include "ph7.h"` |
|     - |   39 | `#ifdef PHL_ENABLE_SERVER` |
|     - |   40 | `#include "server.h"` |
|     - |   41 | `#endif` |
|     - |   42 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |   43 | `#define MINIDUMP_IMPLEMENTATION` |
|     - |   44 | `#include "minidump.h"` |
|     - |   45 | `#endif` |
|     - |   46 | `/*` |
|     - |   47 | ` * Display an error message and exit.` |
|     - |   48 | ` */` |
|   ! 0 |   49 | `static void FatalCode(const char *zMsg,int iCode)` |
|   ! 0 |   50 | `{` |
|   ! 0 |   51 | `	puts(zMsg);` |
|     - |   52 | `	/* Shutdown the library */` |
|   ! 0 |   53 | `	ph7_lib_shutdown();` |
|     - |   54 | `	/* Exit immediately */` |
|   ! 0 |   55 | `	exit(iCode);` |
|   ! 0 |   56 | `}` |
|     - |   57 | `/*` |
|     - |   58 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|     - |   59 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|     - |   60 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|     - |   61 | ` */` |
|   ! 0 |   62 | `static void Fatal(const char *zMsg)` |
|   ! 0 |   63 | `{` |
|   ! 0 |   64 | `	FatalCode(zMsg,255);` |
|   ! 0 |   65 | `}` |
|     - |   66 | `/*` |
|     - |   67 | ` * Exit 255 without printing anything: used when the diagnostic has already been` |
|     - |   68 | ` * emitted by the error consumer (a compile/parse error), which is exactly what` |
|     - |   69 | ` * php does — it prints the parse error and nothing more.` |
|     - |   70 | ` */` |
|   596 |   71 | `static void FatalSilent(void)` |
|     4 |   72 | `{` |
|   600 |   73 | `	ph7_lib_shutdown();` |
|   600 |   74 | `	exit(255);` |
|   ! 0 |   75 | `}` |
|     - |   76 | `/*` |
|     - |   77 | ` * Display the banner,a help message and exit.` |
|     - |   78 | ` */` |
|     2 |   79 | `static void Help(void)` |
|     2 |   80 | `{` |
|     4 |   81 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|     - |   82 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |   83 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   84 | `#endif` |
|     4 |   85 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     4 |   86 | `	puts("\t-i: Display interpreter information and exit");` |
|     4 |   87 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     4 |   88 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   89 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |   90 | `	puts("\t-S host:port: Start the built-in development server");` |
|     4 |   91 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   92 | `#endif` |
|     4 |   93 | `	puts("\t-v, --version: Display version information and exit");` |
|     4 |   94 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   95 | `	/* Exit immediately */` |
|     4 |   96 | `	exit(0);` |
|   ! 0 |   97 | `}` |
|     - |   98 | `/*` |
|     - |   99 | ` * Display version information and exit.` |
|     - |  100 | ` */` |
|     6 |  101 | `static void Version(void)` |
|     1 |  102 | `{` |
|     7 |  103 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |  104 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |  105 | `	/* Exit immediately */` |
|     7 |  106 | `	exit(0);` |
|   ! 0 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |  110 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |  111 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |  112 | ` */` |
|     2 |  113 | `static void Info(void)` |
|   ! 0 |  114 | `{` |
|     2 |  115 | `	printf("phpinfo()\n");` |
|     2 |  116 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  117 | `	printf("System => %s\n",` |
|     - |  118 | `#ifdef __WINNT__` |
|     - |  119 | `		"Windows NT"` |
|     - |  120 | `#elif defined(__UNIXES__)` |
|     - |  121 | `		"UNIX-Like"` |
|     - |  122 | `#else` |
|     - |  123 | `		"Other OS"` |
|     - |  124 | `#endif` |
|     - |  125 | `	);` |
|     2 |  126 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  127 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  128 | `	printf("PHP SAPI => cli\n");` |
|     - |  129 | `	/* Exit immediately */` |
|     2 |  130 | `	exit(0);` |
|   ! 0 |  131 | `}` |
|     - |  132 | `#ifdef __WINNT__` |
|     - |  133 | `#include <Windows.h>` |
|     - |  134 | `#else` |
|     - |  135 | `/* Assume UNIX */` |
|     - |  136 | `#include <unistd.h>` |
|     - |  137 | `#include <limits.h>` |
|     - |  138 | `#endif` |
|     - |  139 | `/*` |
|     - |  140 | ` * The following define is used by the UNIX built and have` |
|     - |  141 | ` * no particular meaning on windows.` |
|     - |  142 | ` */` |
|     - |  143 | `#ifndef STDOUT_FILENO` |
|     - |  144 | `#define STDOUT_FILENO	1` |
|     - |  145 | `#endif` |
|     - |  146 | `#ifndef STDERR_FILENO` |
|     - |  147 | `#define STDERR_FILENO	2` |
|     - |  148 | `#endif` |
|     - |  149 | `#ifndef PATH_MAX` |
|     - |  150 | `#define PATH_MAX 4096` |
|     - |  151 | `#endif` |
|     - |  152 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  153 | `/*` |
|     - |  154 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  155 | ` * pUserData points to the resolved binary path.` |
|     - |  156 | ` */` |
|    12 |  157 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  158 | `{` |
|    13 |  159 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|    13 |  160 | `}` |
|     - |  161 | `/*` |
|     - |  162 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  163 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  164 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  165 | ` */` |
|  4072 |  166 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  167 | `{` |
|     - |  168 | `#ifdef __WINNT__` |
|     5 |  169 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  170 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  171 | `		return zPhlBinaryPath;` |
|     - |  172 | `	}` |
|     - |  173 | `#else` |
|  4072 |  174 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  4072 |  175 | `		return zPhlBinaryPath;` |
|     - |  176 | `	}` |
|     - |  177 | `#endif` |
|   ! 0 |  178 | `	return zArgv0;` |
|  2041 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * VM output consumer callback.` |
|     - |  182 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  183 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  184 | ` * the generated output.` |
|     - |  185 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  186 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  187 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  188 | ` */` |
| 27150 |  189 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  190 | `{` |
| 13575 |  191 | `	(void)pUserData;` |
|     - |  192 | `#ifdef __WINNT__` |
|     - |  193 | `	BOOL rc;` |
|     5 |  194 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  195 | `	if( !rc ){` |
|     - |  196 | `		/* Abort processing */` |
|   ! 0 |  197 | `		return PH7_ABORT;` |
|     - |  198 | `	}` |
|     - |  199 | `#else` |
|     - |  200 | `	ssize_t nWr;` |
| 27150 |  201 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 27150 |  202 | `	if( nWr < 0 ){` |
|     - |  203 | `		/* Abort processing */` |
|   ! 0 |  204 | `		return PH7_ABORT;` |
|     - |  205 | `	}` |
|     - |  206 | `#endif /* __WINT__ */` |
|     - |  207 | `	/* All done,VM output was redirected to STDOUT */` |
| 27155 |  208 | `	return PH7_OK;` |
| 13580 |  209 | `}` |
|     - |  210 | `/*` |
|     - |  211 | ` * VM diagnostics consumer (PH7_VM_CONFIG_ERR_STREAM): the log copy of a runtime` |
|     - |  212 | ` * warning/notice/deprecation and the uncaught-exception fatal go here — STDERR —` |
|     - |  213 | ` * so program STDOUT stays clean, matching stock CLI php.` |
|     - |  214 | ` */` |
|   756 |  215 | `static int Error_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  216 | `{` |
|   378 |  217 | `	(void)pUserData;` |
|     - |  218 | `#ifdef __WINNT__` |
|     - |  219 | `	BOOL rc;` |
|     5 |  220 | `	rc = WriteFile(GetStdHandle(STD_ERROR_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  221 | `	if( !rc ){` |
|     - |  222 | `		/* Abort processing */` |
|   ! 0 |  223 | `		return PH7_ABORT;` |
|     - |  224 | `	}` |
|     - |  225 | `#else` |
|     - |  226 | `	ssize_t nWr;` |
|   756 |  227 | `	nWr = write(STDERR_FILENO,pOutput,nOutputLen);` |
|   756 |  228 | `	if( nWr < 0 ){` |
|     - |  229 | `		/* Abort processing */` |
|   ! 0 |  230 | `		return PH7_ABORT;` |
|     - |  231 | `	}` |
|     - |  232 | `#endif /* __WINNT__ */` |
|     - |  233 | `	/* All done, VM diagnostics were redirected to STDERR */` |
|   761 |  234 | `	return PH7_OK;` |
|   383 |  235 | `}` |
|     - |  236 | `/*` |
|     - |  237 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  238 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  239 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  240 | ` * [uFloor, uCeil]; returns` |
|     - |  241 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  242 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  243 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  244 | ` */` |
| 17384 |  245 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  246 | `{` |
| 17389 |  247 | `	const char *zVal = getenv(zName);` |
| 17389 |  248 | `	char *zEnd = 0;` |
|     - |  249 | `	unsigned long uMax;` |
| 17389 |  250 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 17375 |  251 | `		return 0;` |
|     - |  252 | `	}` |
|     - |  253 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  254 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    17 |  255 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  256 | `		return 0;` |
|     - |  257 | `	}` |
|    17 |  258 | `	errno = 0;` |
|    17 |  259 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    17 |  260 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  261 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  262 | `	}` |
|    17 |  263 | `	if( uMax < uFloor ){` |
|   ! 0 |  264 | `		uMax = uFloor;` |
|   ! 0 |  265 | `	}` |
|    17 |  266 | `	if( uMax > uCeil ){` |
|   ! 0 |  267 | `		uMax = uCeil;` |
|   ! 0 |  268 | `	}` |
|    17 |  269 | `	*pOut = uMax;` |
|    17 |  270 | `	return 1;` |
|  8697 |  271 | `}` |
|     - |  272 | `/*` |
|     - |  273 | ` * Apply one "name=value" php.ini directive to the VM (used by -d and each` |
|     - |  274 | ` * -c file line). Trims surrounding whitespace and one layer of quotes off` |
|     - |  275 | ` * the value, php.ini style.` |
|     - |  276 | ` */` |
|    92 |  277 | `static void PHL_ApplyIniPair(ph7_vm *pVm,const char *zPair)` |
|     4 |  278 | `{` |
|     - |  279 | `	char zName[128];` |
|     - |  280 | `	char zValue[512];` |
|    96 |  281 | `	const char *zEq = strchr(zPair,'=');` |
|     - |  282 | `	const char *zEnd;` |
|     - |  283 | `	size_t n;` |
|    96 |  284 | `	if( zEq == 0 ){` |
|     - |  285 | `		/* php: a bare -d name defines the entry with value "1" */` |
|     2 |  286 | `		zEq = zPair + strlen(zPair);` |
|     1 |  287 | `	}` |
|     - |  288 | `	/* name: trim */` |
|    96 |  289 | `	while( *zPair == ' ' \|\| *zPair == '\t' ){ zPair++; }` |
|    96 |  290 | `	zEnd = zEq;` |
|   150 |  291 | `	while( zEnd > zPair && (zEnd[-1] == ' ' \|\| zEnd[-1] == '\t') ){ zEnd--; }` |
|    96 |  292 | `	n = (size_t)(zEnd - zPair);` |
|    96 |  293 | `	if( n == 0 \|\| n >= sizeof(zName) ){` |
|   ! 0 |  294 | `		return;` |
|     - |  295 | `	}` |
|    96 |  296 | `	memcpy(zName,zPair,n);` |
|    96 |  297 | `	zName[n] = 0;` |
|     - |  298 | `	/* value: trim + unquote */` |
|    96 |  299 | `	if( *zEq == '=' ){` |
|    94 |  300 | `		const char *zV = zEq + 1;` |
|     - |  301 | `		const char *zVEnd;` |
|   102 |  302 | `		while( *zV == ' ' \|\| *zV == '\t' ){ zV++; }` |
|    94 |  303 | `		zVEnd = zV + strlen(zV);` |
|   147 |  304 | `		while( zVEnd > zV && (zVEnd[-1] == ' ' \|\| zVEnd[-1] == '\t'` |
|   102 |  305 | `		    \|\| zVEnd[-1] == '\r' \|\| zVEnd[-1] == '\n') ){ zVEnd--; }` |
|    94 |  306 | `		if( zVEnd - zV >= 2 && (zV[0] == '"' \|\| zV[0] == '\'') && zVEnd[-1] == zV[0] ){` |
|     4 |  307 | `			zV++;` |
|     4 |  308 | `			zVEnd--;` |
|     2 |  309 | `		}` |
|    94 |  310 | `		n = (size_t)(zVEnd - zV);` |
|    94 |  311 | `		if( n >= sizeof(zValue) ){` |
|   ! 0 |  312 | `			n = sizeof(zValue) - 1;` |
|   ! 0 |  313 | `		}` |
|    94 |  314 | `		memcpy(zValue,zV,n);` |
|    94 |  315 | `		zValue[n] = 0;` |
|    49 |  316 | `	}else{` |
|     - |  317 | `		/* default "on" flag; avoid strcpy (MSVC C4996 under /WX) */` |
|     2 |  318 | `		zValue[0] = '1';` |
|     2 |  319 | `		zValue[1] = 0;` |
|     - |  320 | `	}` |
|    96 |  321 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_INI_ENTRY,zName,zValue);` |
|    50 |  322 | `}` |
|     - |  323 | `/*` |
|     - |  324 | ` * Load php.ini directives from a -c file: name=value lines; [sections],` |
|     - |  325 | ` * empty lines and ;/# comments are ignored (enough of php's ini grammar` |
|     - |  326 | ` * for CLI configuration).` |
|     - |  327 | ` */` |
|     4 |  328 | `static void PHL_LoadIniFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 |  329 | `{` |
|     - |  330 | `	char zLine[768];` |
|     4 |  331 | `	FILE *pFile = fopen(zPath,"r");` |
|     4 |  332 | `	if( pFile == 0 ){` |
|   ! 0 |  333 | `		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);` |
|   ! 0 |  334 | `		return;` |
|     - |  335 | `	}` |
|    20 |  336 | `	while( fgets(zLine,sizeof(zLine),pFile) ){` |
|    16 |  337 | `		const char *z = zLine;` |
|    16 |  338 | `		while( *z == ' ' \|\| *z == '\t' ){ z++; }` |
|    16 |  339 | `		if( *z == 0 \|\| *z == ';' \|\| *z == '#' \|\| *z == '[' \|\| *z == '\n' \|\| *z == '\r' ){` |
|     8 |  340 | `			continue;` |
|     - |  341 | `		}` |
|     8 |  342 | `		PHL_ApplyIniPair(pVm,z);` |
|   ! 0 |  343 | `	}` |
|     4 |  344 | `	fclose(pFile);` |
|     2 |  345 | `}` |
|     - |  346 | `/*` |
|     - |  347 | ` * Return TRUE when standard input is a pipe/redirect rather than an interactive` |
|     - |  348 | ` * terminal. php reads the script from stdin in exactly that case; a terminal` |
|     - |  349 | ` * would block waiting for input, so there we keep the usage error instead.` |
|     - |  350 | ` */` |
|    10 |  351 | `static int PHL_StdinIsPipe(void)` |
|     1 |  352 | `{` |
|     - |  353 | `#ifdef __UNIXES__` |
|    10 |  354 | `	return !isatty(0);` |
|     - |  355 | `#else` |
|     1 |  356 | `	return 0;` |
|     - |  357 | `#endif` |
|     1 |  358 | `}` |
|     - |  359 | `/*` |
|     - |  360 | ` * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the` |
|     - |  361 | ` * buffer (caller owns it, though the process exits shortly after) and writes the` |
|     - |  362 | ` * byte length to *pnLen, or NULL on allocation failure.` |
|     - |  363 | ` */` |
|    12 |  364 | `static char * PHL_SlurpStdin(int *pnLen)` |
|   ! 0 |  365 | `{` |
|    12 |  366 | `	size_t nCap = 8192, nUsed = 0;` |
|    12 |  367 | `	char *zBuf = (char *)malloc(nCap);` |
|    12 |  368 | `	if( zBuf == 0 ){ return 0; }` |
|     6 |  369 | `	for(;;){` |
|     - |  370 | `		size_t nRd;` |
|    12 |  371 | `		if( nUsed + 4096 + 1 > nCap ){` |
|     - |  372 | `			char *zNew;` |
|   ! 0 |  373 | `			nCap *= 2;` |
|   ! 0 |  374 | `			zNew = (char *)realloc(zBuf, nCap);` |
|   ! 0 |  375 | `			if( zNew == 0 ){ free(zBuf); return 0; }` |
|   ! 0 |  376 | `			zBuf = zNew;` |
|   ! 0 |  377 | `		}` |
|    12 |  378 | `		nRd = fread(zBuf + nUsed, 1, 4096, stdin);` |
|    12 |  379 | `		nUsed += nRd;` |
|    12 |  380 | `		if( nRd < 4096 ){ break; }` |
|   ! 0 |  381 | `	}` |
|    12 |  382 | `	zBuf[nUsed] = 0;` |
|    12 |  383 | `	*pnLen = (int)nUsed;` |
|    12 |  384 | `	return zBuf;` |
|     6 |  385 | `}` |
|     - |  386 | `/*` |
|     - |  387 | ` * Main program: Compile and execute the PHP file.` |
|     - |  388 | ` */` |
|  4943 |  389 | `int main(int argc,char **argv)` |
|     5 |  390 | `{` |
|     - |  391 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  392 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  4948 |  393 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  4948 |  394 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  4948 |  395 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  4948 |  396 | `	const char *zRunCode = 0; /* Inline code string */` |
|  4948 |  397 | `	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */` |
|  4948 |  398 | `	char *zStdinCode = 0;     /* Script slurped from stdin */` |
|  4948 |  399 | `	int nStdinCode = 0;       /* Length of the stdin script */` |
|  4948 |  400 | ``	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */`` |
|     - |  401 | `#ifdef PHL_ENABLE_SERVER` |
|  4948 |  402 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  4948 |  403 | `	const char *zServerAddr = 0; /* host:port string */` |
|  4948 |  404 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  405 | `#endif` |
|     - |  406 | `	int n;              /* Script arguments */` |
|     - |  407 | `	int rc;` |
|     - |  408 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|  4948 |  409 | `	int nIniDefine = 0;` |
|  4948 |  410 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|     - |  411 | `	/* Process interpreter arguments first*/` |
|  5123 |  412 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  413 | `		int c;` |
|  4799 |  414 | `		if( argv[n][0] != '-' ){` |
|     - |  415 | `			/* No more interpreter arguments */` |
|  4617 |  416 | `			break;` |
|     - |  417 | `		}` |
|     - |  418 | `		/* Check for long options */` |
|   186 |  419 | `		if( argv[n][1] == '-' ){` |
|    18 |  420 | `			if( argv[n][2] == 0 ){` |
|     - |  421 | ``				/* php CLI parity: a bare `--` ends interpreter options; the`` |
|     - |  422 | ``				 * script is read from stdin and everything after `--` becomes`` |
|     - |  423 | `				 * the script's own arguments ($argv[1..]). */` |
|     2 |  424 | `				dash_dash = 1;` |
|     2 |  425 | `				n++;` |
|     2 |  426 | `				break;` |
|     - |  427 | `			}` |
|    16 |  428 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  429 | `				Version();` |
|    12 |  430 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  431 | `				Help();` |
|     8 |  432 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|     - |  433 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|     - |  434 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|     - |  435 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|     - |  436 | `				 * does not exist. Implemented as an inline snippet riding the` |
|     - |  437 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|     - |  438 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|     - |  439 | `				static char zReflCode[768];` |
|     7 |  440 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|     - |  441 | `				const char *zName;` |
|     - |  442 | `				const char *zChk;` |
|     7 |  443 | `				if( n + 1 >= argc ){` |
|   ! 0 |  444 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|   ! 0 |  445 | `				}` |
|     7 |  446 | `				zName = argv[++n];` |
|    71 |  447 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  448 | `					char ch = *zChk;` |
|    65 |  449 | `					if( !(ch == '_' \|\| ch == '\\'` |
|    58 |  450 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|     4 |  451 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  452 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|   ! 0 |  453 | `					}` |
|    33 |  454 | `				}` |
|     7 |  455 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  456 | `					FatalCode("Name too long for --rf/--rc",1);` |
|   ! 0 |  457 | `				}` |
|     - |  458 | `				{` |
|     - |  459 | `					/* Double the namespace separators: inside the snippet's` |
|     - |  460 | `					 * double-quoted string a lone backslash could form an` |
|     - |  461 | `					 * escape sequence ("App\name" -> newline). */` |
|     - |  462 | `					static char zEsc[512];` |
|     7 |  463 | `					char *pOut = zEsc;` |
|    71 |  464 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  465 | `						if( *zChk == '\\' ){` |
|   ! 0 |  466 | `							*pOut++ = '\\';` |
|   ! 0 |  467 | `						}` |
|    65 |  468 | `						*pOut++ = *zChk;` |
|    33 |  469 | `					}` |
|     7 |  470 | `					*pOut = 0;` |
|     7 |  471 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|     - |  472 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|     - |  473 | `						zWhat,zEsc);` |
|     - |  474 | `				}` |
|     7 |  475 | `				zRunCode = zReflCode;` |
|     7 |  476 | `				run_code = 1;` |
|     4 |  477 | `			}else{` |
|     - |  478 | `				/* Unknown long option */` |
|   ! 0 |  479 | `				Help();` |
|     - |  480 | `			}` |
|    11 |  481 | `			continue;` |
|     - |  482 | `		}` |
|   170 |  483 | `		c = argv[n][1];` |
|   170 |  484 | `		if( c == 'b' ){` |
|     - |  485 | `			/* Dump byte-code instructions */` |
|     3 |  486 | `			dump_vm = 1;` |
|   169 |  487 | `		}else if( c == 'l' ){` |
|     - |  488 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  489 | `			lint_mode = 1;` |
|   166 |  490 | `		}else if( c == 'i' ){` |
|     - |  491 | `			/* Display interpreter information and exit */` |
|     2 |  492 | `			Info();` |
|   163 |  493 | `		}else if( c == 'f' ){` |
|     - |  494 | ``			/* php CLI parity: `-f <file>` explicitly names the script to run.`` |
|     - |  495 | `			 * The path follows as the next argument, which the positional` |
|     - |  496 | `			 * file handling below already consumes, so treat -f as a no-op. */` |
|   ! 0 |  497 | `			continue;` |
|   162 |  498 | `		}else if( c == 'r' ){` |
|     - |  499 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    20 |  500 | `			if( n + 1 >= argc ){` |
|     - |  501 | `				/* Missing code argument */` |
|   ! 0 |  502 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  503 | `			}` |
|    20 |  504 | `			zRunCode = argv[++n];` |
|    20 |  505 | `			run_code = 1;` |
|   153 |  506 | `		}else if( c == 'S' ){` |
|     - |  507 | `			/* Start built-in development server */` |
|     - |  508 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  509 | `			if( n + 1 >= argc ){` |
|   ! 0 |  510 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  511 | `			}` |
|    26 |  512 | `			zServerAddr = argv[++n];` |
|    26 |  513 | `			server_mode = 1;` |
|     - |  514 | `#else` |
|     - |  515 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  516 | `#endif` |
|   131 |  517 | `		}else if( c == 't' ){` |
|     - |  518 | `			/* Set document root for the server */` |
|     - |  519 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  520 | `			if( n + 1 >= argc ){` |
|   ! 0 |  521 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  522 | `			}` |
|    26 |  523 | `			zDocRoot = argv[++n];` |
|     - |  524 | `#else` |
|     - |  525 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  526 | `#endif` |
|   105 |  527 | `		}else if( c == 'v' ){` |
|     - |  528 | `			/* Display version */` |
|   ! 0 |  529 | `			Version();` |
|    92 |  530 | `		}else if( c == 'd' ){` |
|     - |  531 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|     - |  532 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|    88 |  533 | `			if( n + 1 >= argc ){` |
|   ! 0 |  534 | `				FatalCode("Missing name=value argument for -d",1);` |
|   ! 0 |  535 | `			}` |
|    88 |  536 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|    88 |  537 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|    46 |  538 | `			}else{` |
|   ! 0 |  539 | `				FatalCode("Too many -d directives",1);` |
|     4 |  540 | `			}` |
|    46 |  541 | `		}else if( c == 'c' ){` |
|     - |  542 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|     - |  543 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|     4 |  544 | `			if( n + 1 >= argc ){` |
|   ! 0 |  545 | `				FatalCode("Missing file argument for -c",1);` |
|   ! 0 |  546 | `			}` |
|     4 |  547 | `			zIniFile = argv[++n];` |
|     2 |  548 | `		}else{` |
|     - |  549 | `			/* Display a help message and exit */` |
|   ! 0 |  550 | `			Help();` |
|     - |  551 | `		}` |
|    87 |  552 | `	}` |
|     - |  553 | `#ifdef PHL_ENABLE_SERVER` |
|  4943 |  554 | `	if( server_mode ){` |
|     - |  555 | `		/* Parse host:port from zServerAddr */` |
|     - |  556 | `		char zHost[256];` |
|    26 |  557 | `		int iPort = 0;` |
|     - |  558 | `		const char *zColon;` |
|    26 |  559 | `		const char *zRouter = 0;` |
|    26 |  560 | `		zColon = strrchr(zServerAddr, ':');` |
|    26 |  561 | `		if( zColon == 0 ){` |
|   ! 0 |  562 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  563 | `		}` |
|     - |  564 | `		{` |
|    26 |  565 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    26 |  566 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    26 |  567 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    26 |  568 | `			zHost[nHostLen] = 0;` |
|     - |  569 | `		}` |
|    26 |  570 | `		iPort = atoi(zColon + 1);` |
|    26 |  571 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  572 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  573 | `		}` |
|     - |  574 | `		/* Check for optional router script */` |
|    26 |  575 | `		if( n < argc ){` |
|   ! 0 |  576 | `			zRouter = argv[n];` |
|   ! 0 |  577 | `		}` |
|    26 |  578 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  579 | `	}` |
|     - |  580 | `#endif` |
|  4917 |  581 | `	if( (n >= argc \|\| dash_dash) && !run_code ){` |
|     - |  582 | ``		/* No file and no -r: php reads the script from stdin. `--` forces this`` |
|     - |  583 | `		 * (rest are script args); otherwise only when stdin is a pipe/redirect` |
|     - |  584 | `		 * (an interactive terminal would just block). */` |
|    13 |  585 | `		if( dash_dash \|\| PHL_StdinIsPipe() ){` |
|    12 |  586 | `			zStdinCode = PHL_SlurpStdin(&nStdinCode);` |
|    12 |  587 | `			if( zStdinCode == 0 ){` |
|   ! 0 |  588 | `				FatalCode("IO error while reading standard input",1);` |
|   ! 0 |  589 | `			}` |
|    12 |  590 | `			stdin_code = 1;` |
|     6 |  591 | `		}else{` |
|     1 |  592 | `			puts("Missing PHP file to compile");` |
|     1 |  593 | `			Help();` |
|     - |  594 | `		}` |
|     6 |  595 | `	}` |
|     - |  596 |  |
|     - |  597 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  598 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  599 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  600 | `#endif` |
|     - |  601 | `	/* Allocate a new PH7 engine instance */` |
|  4353 |  602 | `	rc = ph7_init(&pEngine);` |
|  4353 |  603 | `	if( rc != PH7_OK ){` |
|     - |  604 | `		/*` |
|     - |  605 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  606 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  607 | `		 */` |
|   ! 0 |  608 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  609 | `	}` |
|     - |  610 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  611 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  612 | `	 */` |
|  4353 |  613 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  614 | `		Output_Consumer, /* Error log consumer */` |
|     - |  615 | `		0 /* NULL: Callback Private data */` |
|     - |  616 | `		);` |
|     - |  617 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  618 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  619 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  620 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  621 | `	{` |
|     - |  622 | `		unsigned long uMax;` |
|     - |  623 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  4353 |  624 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  625 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  626 | `		}` |
|     - |  627 | `	}` |
|     - |  628 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  629 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  630 | `	{` |
|     - |  631 | `		unsigned long uMax;` |
|  4353 |  632 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  633 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  634 | `		}` |
|     - |  635 | `	}` |
|     - |  636 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  637 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  638 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  639 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  4353 |  640 | `	if( lint_mode ){` |
|     - |  641 | `		const char *zFile;` |
|     4 |  642 | `		if( n >= argc ){` |
|     - |  643 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  644 | `			ph7_release(pEngine);` |
|   ! 0 |  645 | `			puts("No input file specified");` |
|   ! 0 |  646 | `			return 255;` |
|     - |  647 | `		}` |
|     4 |  648 | `		zFile = argv[n];` |
|     4 |  649 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  650 | `		if( rc == PH7_OK ){` |
|     2 |  651 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  652 | `			ph7_vm_release(pVm);` |
|     3 |  653 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  654 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  655 | `		}else{` |
|     2 |  656 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  657 | `		}` |
|     4 |  658 | `		ph7_release(pEngine);` |
|     4 |  659 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  660 | `	}` |
|     - |  661 | `	/* Now,it's time to compile our PHP file */` |
|  4349 |  662 | `	if( run_code ){` |
|     - |  663 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    26 |  664 | `		rc = ph7_compile_v2(` |
|    12 |  665 | `			pEngine, /* PH7 Engine */` |
|    12 |  666 | `			zRunCode, /* Source code */` |
|     - |  667 | `			-1,       /* Let API compute length */` |
|     - |  668 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  669 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  670 | `			);` |
|    26 |  671 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  672 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  673 | `				Fatal("VM initialization error");` |
|   ! 0 |  674 | `			}else{` |
|     - |  675 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  676 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|   ! 0 |  677 | `				FatalSilent();` |
|     - |  678 | `			}` |
|     2 |  679 | `		}` |
|  4337 |  680 | `	}else if( stdin_code ){` |
|     - |  681 | `		/* Script read from stdin: compile it like a file (PHP tags expected). */` |
|    12 |  682 | `		rc = ph7_compile_v2(` |
|     6 |  683 | `			pEngine,     /* PH7 Engine */` |
|     6 |  684 | `			zStdinCode,  /* Source code slurped from stdin */` |
|     6 |  685 | `			nStdinCode,  /* Its byte length */` |
|     - |  686 | `			&pVm,        /* OUT: Compiled PHP program */` |
|     - |  687 | `			0            /* IN: tag mode, like a file */` |
|     - |  688 | `			);` |
|    12 |  689 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  690 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  691 | `				Fatal("VM initialization error");` |
|   ! 0 |  692 | `			}else{` |
|   ! 0 |  693 | `				FatalSilent();` |
|     - |  694 | `			}` |
|   ! 0 |  695 | `		}` |
|     6 |  696 | `	}else{` |
|  4313 |  697 | `		rc = ph7_compile_file(` |
|  2005 |  698 | `			pEngine, /* PH7 Engine */` |
|  4308 |  699 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  700 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  701 | `			0        /* IN: Compile flags */` |
|     - |  702 | `			);` |
|  4313 |  703 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   600 |  704 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  705 | `				FatalCode("IO error while opening the target file",1);` |
|   600 |  706 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  707 | `				Fatal("VM initialization error");` |
|   ! 0 |  708 | `			}else{` |
|     - |  709 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  710 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   600 |  711 | `				FatalSilent();` |
|     - |  712 | `			}` |
|   298 |  713 | `		}` |
|     - |  714 | `	}` |
|     - |  715 | `	/*` |
|     - |  716 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  717 | `	 * We will install the VM output consumer callback defined above` |
|     - |  718 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  719 | `	 */` |
|  4051 |  720 | `	rc = ph7_vm_config(pVm,` |
|     - |  721 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  722 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  723 | `		0                   /* Callback private data */` |
|     - |  724 | `		);` |
|  4051 |  725 | `	if( rc != PH7_OK ){` |
|   ! 0 |  726 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  727 | `	}` |
|     - |  728 | `	/* Diagnostics stream: route the log copy of runtime warnings/notices and the` |
|     - |  729 | `	 * uncaught-exception fatal to STDERR (gated by log_errors), so program STDOUT` |
|     - |  730 | `	 * stays clean like stock CLI php. */` |
|  4051 |  731 | `	rc = ph7_vm_config(pVm,` |
|     - |  732 | `		PH7_VM_CONFIG_ERR_STREAM,` |
|     - |  733 | `		Error_Consumer,     /* Diagnostics (STDERR) consumer callback */` |
|     - |  734 | `		0                   /* Callback private data */` |
|     - |  735 | `		);` |
|  4051 |  736 | `	if( rc != PH7_OK ){` |
|   ! 0 |  737 | `		Fatal("Error while installing the VM diagnostics consumer callback");` |
|   ! 0 |  738 | `	}` |
|     - |  739 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  740 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  741 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  742 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  743 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  744 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  745 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  746 | `	 *                          floor 2) */` |
|     - |  747 | `	{` |
|     - |  748 | `		unsigned long uMax;` |
|  4051 |  749 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  750 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  751 | `		}` |
|  4051 |  752 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  753 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  754 | `		}` |
|     - |  755 | `	}` |
|     - |  756 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  6074 |  757 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  4046 |  758 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  759 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  760 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  761 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  762 | `	 * script's own arguments.` |
|     - |  763 | `	 */` |
|     - |  764 | `	{` |
|  4051 |  765 | `		const char *zScriptName = (run_code \|\| stdin_code) ? "Standard input code" : argv[n];` |
|  4051 |  766 | `		int argv_count = 0;` |
|     - |  767 | `		ph7_value *pArgc;` |
|     - |  768 | `		/* Count only the entries actually inserted, so $argc can never disagree` |
|     - |  769 | `		 * with count($argv) if a registration fails. */` |
|  4051 |  770 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  4051 |  771 | `			argv_count++;` |
|  2023 |  772 | `		}` |
|     - |  773 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  774 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  4127 |  775 | `		for( n = (run_code \|\| stdin_code) ? n : n + 1; n < argc ; ++n ){` |
|    81 |  776 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    81 |  777 | `				argv_count++;` |
|    38 |  778 | `			}` |
|    43 |  779 | `		}` |
|     - |  780 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  4051 |  781 | `		pArgc = ph7_new_scalar(pVm);` |
|  4051 |  782 | `		if( pArgc ){` |
|  4051 |  783 | `			ph7_value_int(pArgc,argv_count);` |
|  4051 |  784 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  4051 |  785 | `			ph7_release_value(pVm,pArgc);` |
|  2023 |  786 | `		}` |
|     - |  787 | `		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */` |
|  4051 |  788 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);` |
|     - |  789 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  790 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  4051 |  791 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  4051 |  792 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  4051 |  793 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  794 | `		{` |
|     - |  795 | `			char zTime[32];` |
|  4051 |  796 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  4051 |  797 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  798 | `		}` |
|     - |  799 | `#ifndef __WINNT__` |
|     - |  800 | `		{` |
|     - |  801 | `			char zCwd[PATH_MAX];` |
|  4046 |  802 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  4046 |  803 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  2023 |  804 | `			}` |
|     - |  805 | `		}` |
|     - |  806 | `#endif` |
|     - |  807 | `	}` |
|     - |  808 | `	/* Report script run-time errors (now default behavior) */` |
|  4051 |  809 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|     - |  810 | `	/* Apply php.ini directives AFTER the error-report default so` |
|     - |  811 | ``	 * `-d error_reporting=0` can lower it: the -c file first, then -d`` |
|     - |  812 | `	 * overrides in CLI order (php's precedence). */` |
|  4051 |  813 | `	if( zIniFile ){` |
|     4 |  814 | `		PHL_LoadIniFile(pVm,zIniFile);` |
|     2 |  815 | `	}` |
|     - |  816 | `	{` |
|     - |  817 | `		int i;` |
|  4135 |  818 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|    88 |  819 | `			PHL_ApplyIniPair(pVm,azIniDefine[i]);` |
|    46 |  820 | `		}` |
|     - |  821 | `	}` |
|  4051 |  822 | `	if( dump_vm ){` |
|     - |  823 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  824 | `		ph7_vm_dump_v2(pVm,` |
|     - |  825 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  826 | `			0` |
|     - |  827 | `			);` |
|     1 |  828 | `	}` |
|     - |  829 | `	/*` |
|     - |  830 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  831 | `	 * should display the result.` |
|     - |  832 | `	 */` |
|     - |  833 | `	{` |
|  4051 |  834 | `		int iExitStatus = 0;` |
|  4051 |  835 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  836 | `		/* All done, cleanup the mess left behind.` |
|     - |  837 | `		*/` |
|  4051 |  838 | `		ph7_vm_release(pVm);` |
|  4051 |  839 | `		ph7_release(pEngine);` |
|     - |  840 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  4051 |  841 | `		return iExitStatus;` |
|     - |  842 | `	}` |
|  2043 |  843 | `}` |
|     - |  844 |  |
