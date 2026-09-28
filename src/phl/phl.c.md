# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 339/438 lines (77.40%)

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
|     - |   71 | `/* The engine this process built, so the silent-fatal exit can give it back:` |
|     - |   72 | ` * ph7_lib_shutdown() releases the LIBRARY, and an engine instance owns a mutex` |
|     - |   73 | ` * of its own. Without this the fatal path leaks it, which is invisible in` |
|     - |   74 | ` * ordinary use (the process is exiting) and turns every leak-detecting run over` |
|     - |   75 | ` * a corpus that spawns a failing child into a wall of reports. */` |
|     - |   76 | `static ph7 *pFatalEngine = 0;` |
|   780 |   77 | `static void FatalSilent(void)` |
|     4 |   78 | `{` |
|   784 |   79 | `	if( pFatalEngine ){` |
|   784 |   80 | `		ph7_release(pFatalEngine);` |
|   784 |   81 | `		pFatalEngine = 0;` |
|   390 |   82 | `	}` |
|   784 |   83 | `	ph7_lib_shutdown();` |
|   784 |   84 | `	exit(255);` |
|   ! 0 |   85 | `}` |
|     - |   86 | `/*` |
|     - |   87 | ` * Display the banner,a help message and exit.` |
|     - |   88 | ` */` |
|     2 |   89 | `static void Help(void)` |
|     2 |   90 | `{` |
|     4 |   91 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|     - |   92 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |   93 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   94 | `#endif` |
|     4 |   95 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     4 |   96 | `	puts("\t-i: Display interpreter information and exit");` |
|     4 |   97 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     4 |   98 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   99 | `#ifdef PHL_ENABLE_SERVER` |
|     4 |  100 | `	puts("\t-S host:port: Start the built-in development server");` |
|     4 |  101 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |  102 | `#endif` |
|     4 |  103 | `	puts("\t-v, --version: Display version information and exit");` |
|     4 |  104 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |  105 | `	/* Exit immediately */` |
|     4 |  106 | `	exit(0);` |
|   ! 0 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * Display version information and exit.` |
|     - |  110 | ` */` |
|     6 |  111 | `static void Version(void)` |
|     1 |  112 | `{` |
|     7 |  113 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |  114 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |  115 | `	/* Exit immediately */` |
|     7 |  116 | `	exit(0);` |
|   ! 0 |  117 | `}` |
|     - |  118 | `/*` |
|     - |  119 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |  120 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |  121 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |  122 | ` */` |
|     2 |  123 | `static void Info(void)` |
|   ! 0 |  124 | `{` |
|     2 |  125 | `	printf("phpinfo()\n");` |
|     2 |  126 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  127 | `	printf("System => %s\n",` |
|     - |  128 | `#ifdef __WINNT__` |
|     - |  129 | `		"Windows NT"` |
|     - |  130 | `#elif defined(__UNIXES__)` |
|     - |  131 | `		"UNIX-Like"` |
|     - |  132 | `#else` |
|     - |  133 | `		"Other OS"` |
|     - |  134 | `#endif` |
|     - |  135 | `	);` |
|     2 |  136 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  137 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  138 | `	printf("PHP SAPI => cli\n");` |
|     - |  139 | `	/* Exit immediately */` |
|     2 |  140 | `	exit(0);` |
|   ! 0 |  141 | `}` |
|     - |  142 | `#ifdef __WINNT__` |
|     - |  143 | `#include <Windows.h>` |
|     - |  144 | `#else` |
|     - |  145 | `/* Assume UNIX */` |
|     - |  146 | `#include <unistd.h>` |
|     - |  147 | `#include <limits.h>` |
|     - |  148 | `#endif` |
|     - |  149 | `/*` |
|     - |  150 | ` * The following define is used by the UNIX built and have` |
|     - |  151 | ` * no particular meaning on windows.` |
|     - |  152 | ` */` |
|     - |  153 | `#ifndef STDOUT_FILENO` |
|     - |  154 | `#define STDOUT_FILENO	1` |
|     - |  155 | `#endif` |
|     - |  156 | `#ifndef STDERR_FILENO` |
|     - |  157 | `#define STDERR_FILENO	2` |
|     - |  158 | `#endif` |
|     - |  159 | `#ifndef PATH_MAX` |
|     - |  160 | `#define PATH_MAX 4096` |
|     - |  161 | `#endif` |
|     - |  162 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  163 | `/*` |
|     - |  164 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  165 | ` * pUserData points to the resolved binary path.` |
|     - |  166 | ` */` |
|   466 |  167 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     5 |  168 | `{` |
|   471 |  169 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|   471 |  170 | `}` |
|     - |  171 | `/*` |
|     - |  172 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  173 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  174 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  175 | ` */` |
|  4958 |  176 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  177 | `{` |
|     - |  178 | `#ifdef __WINNT__` |
|     5 |  179 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  180 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  181 | `		return zPhlBinaryPath;` |
|     - |  182 | `	}` |
|     - |  183 | `#else` |
|  4958 |  184 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  4958 |  185 | `		return zPhlBinaryPath;` |
|     - |  186 | `	}` |
|     - |  187 | `#endif` |
|   ! 0 |  188 | `	return zArgv0;` |
|  2484 |  189 | `}` |
|     - |  190 | `/*` |
|     - |  191 | ` * VM output consumer callback.` |
|     - |  192 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  193 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  194 | ` * the generated output.` |
|     - |  195 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  196 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  197 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  198 | ` */` |
| 42100 |  199 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  200 | `{` |
| 21050 |  201 | `	(void)pUserData;` |
|     - |  202 | `#ifdef __WINNT__` |
|     - |  203 | `	BOOL rc;` |
|     5 |  204 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  205 | `	if( !rc ){` |
|     - |  206 | `		/* Abort processing */` |
|   ! 0 |  207 | `		return PH7_ABORT;` |
|     - |  208 | `	}` |
|     - |  209 | `#else` |
|     - |  210 | `	ssize_t nWr;` |
| 42100 |  211 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 42100 |  212 | `	if( nWr < 0 ){` |
|     - |  213 | `		/* Abort processing */` |
|   ! 0 |  214 | `		return PH7_ABORT;` |
|     - |  215 | `	}` |
|     - |  216 | `#endif /* __WINT__ */` |
|     - |  217 | `	/* All done,VM output was redirected to STDOUT */` |
| 42105 |  218 | `	return PH7_OK;` |
| 21055 |  219 | `}` |
|     - |  220 | `/*` |
|     - |  221 | ` * VM diagnostics consumer (PH7_VM_CONFIG_ERR_STREAM): the log copy of a runtime` |
|     - |  222 | ` * warning/notice/deprecation and the uncaught-exception fatal go here — STDERR —` |
|     - |  223 | ` * so program STDOUT stays clean, matching stock CLI php.` |
|     - |  224 | ` */` |
|   864 |  225 | `static int Error_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  226 | `{` |
|   432 |  227 | `	(void)pUserData;` |
|     - |  228 | `#ifdef __WINNT__` |
|     - |  229 | `	BOOL rc;` |
|     5 |  230 | `	rc = WriteFile(GetStdHandle(STD_ERROR_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  231 | `	if( !rc ){` |
|     - |  232 | `		/* Abort processing */` |
|   ! 0 |  233 | `		return PH7_ABORT;` |
|     - |  234 | `	}` |
|     - |  235 | `#else` |
|     - |  236 | `	ssize_t nWr;` |
|   864 |  237 | `	nWr = write(STDERR_FILENO,pOutput,nOutputLen);` |
|   864 |  238 | `	if( nWr < 0 ){` |
|     - |  239 | `		/* Abort processing */` |
|   ! 0 |  240 | `		return PH7_ABORT;` |
|     - |  241 | `	}` |
|     - |  242 | `#endif /* __WINNT__ */` |
|     - |  243 | `	/* All done, VM diagnostics were redirected to STDERR */` |
|   869 |  244 | `	return PH7_OK;` |
|   437 |  245 | `}` |
|     - |  246 | `/*` |
|     - |  247 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  248 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  249 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  250 | ` * [uFloor, uCeil]; returns` |
|     - |  251 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  252 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  253 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  254 | ` */` |
| 21272 |  255 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  256 | `{` |
| 21277 |  257 | `	const char *zVal = getenv(zName);` |
| 21277 |  258 | `	char *zEnd = 0;` |
|     - |  259 | `	unsigned long uMax;` |
| 21277 |  260 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 21263 |  261 | `		return 0;` |
|     - |  262 | `	}` |
|     - |  263 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  264 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    17 |  265 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  266 | `		return 0;` |
|     - |  267 | `	}` |
|    17 |  268 | `	errno = 0;` |
|    17 |  269 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    17 |  270 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  271 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  272 | `	}` |
|    17 |  273 | `	if( uMax < uFloor ){` |
|   ! 0 |  274 | `		uMax = uFloor;` |
|   ! 0 |  275 | `	}` |
|    17 |  276 | `	if( uMax > uCeil ){` |
|   ! 0 |  277 | `		uMax = uCeil;` |
|   ! 0 |  278 | `	}` |
|    17 |  279 | `	*pOut = uMax;` |
|    17 |  280 | `	return 1;` |
| 10641 |  281 | `}` |
|     - |  282 | `/*` |
|     - |  283 | ` * Apply one "name=value" php.ini directive to the VM (used by -d and each` |
|     - |  284 | ` * -c file line). Trims surrounding whitespace and one layer of quotes off` |
|     - |  285 | ` * the value, php.ini style.` |
|     - |  286 | ` */` |
|   108 |  287 | `static void PHL_ApplyIniPair(ph7_vm *pVm,const char *zPair)` |
|     4 |  288 | `{` |
|     - |  289 | `	char zName[128];` |
|     - |  290 | `	char zValue[512];` |
|   112 |  291 | `	const char *zEq = strchr(zPair,'=');` |
|     - |  292 | `	const char *zEnd;` |
|     - |  293 | `	size_t n;` |
|   112 |  294 | `	if( zEq == 0 ){` |
|     - |  295 | `		/* php: a bare -d name defines the entry with value "1" */` |
|     2 |  296 | `		zEq = zPair + strlen(zPair);` |
|     1 |  297 | `	}` |
|     - |  298 | `	/* name: trim */` |
|   112 |  299 | `	while( *zPair == ' ' \|\| *zPair == '\t' ){ zPair++; }` |
|   112 |  300 | `	zEnd = zEq;` |
|   174 |  301 | `	while( zEnd > zPair && (zEnd[-1] == ' ' \|\| zEnd[-1] == '\t') ){ zEnd--; }` |
|   112 |  302 | `	n = (size_t)(zEnd - zPair);` |
|   112 |  303 | `	if( n == 0 \|\| n >= sizeof(zName) ){` |
|   ! 0 |  304 | `		return;` |
|     - |  305 | `	}` |
|   112 |  306 | `	memcpy(zName,zPair,n);` |
|   112 |  307 | `	zName[n] = 0;` |
|     - |  308 | `	/* value: trim + unquote */` |
|   112 |  309 | `	if( *zEq == '=' ){` |
|   110 |  310 | `		const char *zV = zEq + 1;` |
|     - |  311 | `		const char *zVEnd;` |
|   118 |  312 | `		while( *zV == ' ' \|\| *zV == '\t' ){ zV++; }` |
|   110 |  313 | `		zVEnd = zV + strlen(zV);` |
|   171 |  314 | `		while( zVEnd > zV && (zVEnd[-1] == ' ' \|\| zVEnd[-1] == '\t'` |
|   118 |  315 | `		    \|\| zVEnd[-1] == '\r' \|\| zVEnd[-1] == '\n') ){ zVEnd--; }` |
|   110 |  316 | `		if( zVEnd - zV >= 2 && (zV[0] == '"' \|\| zV[0] == '\'') && zVEnd[-1] == zV[0] ){` |
|     4 |  317 | `			zV++;` |
|     4 |  318 | `			zVEnd--;` |
|     2 |  319 | `		}` |
|   110 |  320 | `		n = (size_t)(zVEnd - zV);` |
|   110 |  321 | `		if( n >= sizeof(zValue) ){` |
|   ! 0 |  322 | `			n = sizeof(zValue) - 1;` |
|   ! 0 |  323 | `		}` |
|   110 |  324 | `		memcpy(zValue,zV,n);` |
|   110 |  325 | `		zValue[n] = 0;` |
|    57 |  326 | `	}else{` |
|     - |  327 | `		/* default "on" flag; avoid strcpy (MSVC C4996 under /WX) */` |
|     2 |  328 | `		zValue[0] = '1';` |
|     2 |  329 | `		zValue[1] = 0;` |
|     - |  330 | `	}` |
|   112 |  331 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_INI_ENTRY,zName,zValue);` |
|    58 |  332 | `}` |
|     - |  333 | `/*` |
|     - |  334 | ` * Load php.ini directives from a -c file: name=value lines; [sections],` |
|     - |  335 | ` * empty lines and ;/# comments are ignored (enough of php's ini grammar` |
|     - |  336 | ` * for CLI configuration).` |
|     - |  337 | ` */` |
|     4 |  338 | `static void PHL_LoadIniFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 |  339 | `{` |
|     - |  340 | `	char zLine[768];` |
|     4 |  341 | `	FILE *pFile = fopen(zPath,"r");` |
|     4 |  342 | `	if( pFile == 0 ){` |
|   ! 0 |  343 | `		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);` |
|   ! 0 |  344 | `		return;` |
|     - |  345 | `	}` |
|    20 |  346 | `	while( fgets(zLine,sizeof(zLine),pFile) ){` |
|    16 |  347 | `		const char *z = zLine;` |
|    16 |  348 | `		while( *z == ' ' \|\| *z == '\t' ){ z++; }` |
|    16 |  349 | `		if( *z == 0 \|\| *z == ';' \|\| *z == '#' \|\| *z == '[' \|\| *z == '\n' \|\| *z == '\r' ){` |
|     8 |  350 | `			continue;` |
|     - |  351 | `		}` |
|     8 |  352 | `		PHL_ApplyIniPair(pVm,z);` |
|   ! 0 |  353 | `	}` |
|     4 |  354 | `	fclose(pFile);` |
|     2 |  355 | `}` |
|     - |  356 | `/*` |
|     - |  357 | ` * Return TRUE when standard input is a pipe/redirect rather than an interactive` |
|     - |  358 | ` * terminal. php reads the script from stdin in exactly that case; a terminal` |
|     - |  359 | ` * would block waiting for input, so there we keep the usage error instead.` |
|     - |  360 | ` */` |
|    10 |  361 | `static int PHL_StdinIsPipe(void)` |
|     2 |  362 | `{` |
|     - |  363 | `#ifdef __UNIXES__` |
|    10 |  364 | `	return !isatty(0);` |
|     - |  365 | `#else` |
|     2 |  366 | `	return 0;` |
|     - |  367 | `#endif` |
|     2 |  368 | `}` |
|     - |  369 | `/*` |
|     - |  370 | ` * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the` |
|     - |  371 | ` * buffer (caller owns it, though the process exits shortly after) and writes the` |
|     - |  372 | ` * byte length to *pnLen, or NULL on allocation failure.` |
|     - |  373 | ` */` |
|    12 |  374 | `static char * PHL_SlurpStdin(int *pnLen)` |
|   ! 0 |  375 | `{` |
|    12 |  376 | `	size_t nCap = 8192, nUsed = 0;` |
|    12 |  377 | `	char *zBuf = (char *)malloc(nCap);` |
|    12 |  378 | `	if( zBuf == 0 ){ return 0; }` |
|     6 |  379 | `	for(;;){` |
|     - |  380 | `		size_t nRd;` |
|    12 |  381 | `		if( nUsed + 4096 + 1 > nCap ){` |
|     - |  382 | `			char *zNew;` |
|   ! 0 |  383 | `			nCap *= 2;` |
|   ! 0 |  384 | `			zNew = (char *)realloc(zBuf, nCap);` |
|   ! 0 |  385 | `			if( zNew == 0 ){ free(zBuf); return 0; }` |
|   ! 0 |  386 | `			zBuf = zNew;` |
|   ! 0 |  387 | `		}` |
|    12 |  388 | `		nRd = fread(zBuf + nUsed, 1, 4096, stdin);` |
|    12 |  389 | `		nUsed += nRd;` |
|    12 |  390 | `		if( nRd < 4096 ){ break; }` |
|   ! 0 |  391 | `	}` |
|    12 |  392 | `	zBuf[nUsed] = 0;` |
|    12 |  393 | `	*pnLen = (int)nUsed;` |
|    12 |  394 | `	return zBuf;` |
|     6 |  395 | `}` |
|     - |  396 | `/*` |
|     - |  397 | ` * Main program: Compile and execute the PHP file.` |
|     - |  398 | ` */` |
|  6101 |  399 | `int main(int argc,char **argv)` |
|     5 |  400 | `{` |
|     - |  401 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  402 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  6106 |  403 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  6106 |  404 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  6106 |  405 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  6106 |  406 | `	const char *zRunCode = 0; /* Inline code string */` |
|  6106 |  407 | `	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */` |
|  6106 |  408 | `	char *zStdinCode = 0;     /* Script slurped from stdin */` |
|  6106 |  409 | `	int nStdinCode = 0;       /* Length of the stdin script */` |
|  6106 |  410 | ``	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */`` |
|     - |  411 | `#ifdef PHL_ENABLE_SERVER` |
|  6106 |  412 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  6106 |  413 | `	const char *zServerAddr = 0; /* host:port string */` |
|  6106 |  414 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  415 | `#endif` |
|     - |  416 | `	int n;              /* Script arguments */` |
|     - |  417 | `	int rc;` |
|     - |  418 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|  6106 |  419 | `	int nIniDefine = 0;` |
|  6106 |  420 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|     - |  421 | `	/* Process interpreter arguments first*/` |
|  6313 |  422 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  423 | `		int c;` |
|  5891 |  424 | `		if( argv[n][0] != '-' ){` |
|     - |  425 | `			/* No more interpreter arguments */` |
|  5677 |  426 | `			break;` |
|     - |  427 | `		}` |
|     - |  428 | `		/* Check for long options */` |
|   218 |  429 | `		if( argv[n][1] == '-' ){` |
|    18 |  430 | `			if( argv[n][2] == 0 ){` |
|     - |  431 | ``				/* php CLI parity: a bare `--` ends interpreter options; the`` |
|     - |  432 | ``				 * script is read from stdin and everything after `--` becomes`` |
|     - |  433 | `				 * the script's own arguments ($argv[1..]). */` |
|     2 |  434 | `				dash_dash = 1;` |
|     2 |  435 | `				n++;` |
|     2 |  436 | `				break;` |
|     - |  437 | `			}` |
|    16 |  438 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  439 | `				Version();` |
|    12 |  440 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  441 | `				Help();` |
|     8 |  442 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|     - |  443 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|     - |  444 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|     - |  445 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|     - |  446 | `				 * does not exist. Implemented as an inline snippet riding the` |
|     - |  447 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|     - |  448 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|     - |  449 | `				static char zReflCode[768];` |
|     7 |  450 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|     - |  451 | `				const char *zName;` |
|     - |  452 | `				const char *zChk;` |
|     7 |  453 | `				if( n + 1 >= argc ){` |
|   ! 0 |  454 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|   ! 0 |  455 | `				}` |
|     7 |  456 | `				zName = argv[++n];` |
|    71 |  457 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  458 | `					char ch = *zChk;` |
|    65 |  459 | `					if( !(ch == '_' \|\| ch == '\\'` |
|    58 |  460 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|     4 |  461 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  462 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|   ! 0 |  463 | `					}` |
|    33 |  464 | `				}` |
|     7 |  465 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  466 | `					FatalCode("Name too long for --rf/--rc",1);` |
|   ! 0 |  467 | `				}` |
|     - |  468 | `				{` |
|     - |  469 | `					/* Double the namespace separators: inside the snippet's` |
|     - |  470 | `					 * double-quoted string a lone backslash could form an` |
|     - |  471 | `					 * escape sequence ("App\name" -> newline). */` |
|     - |  472 | `					static char zEsc[512];` |
|     7 |  473 | `					char *pOut = zEsc;` |
|    71 |  474 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  475 | `						if( *zChk == '\\' ){` |
|   ! 0 |  476 | `							*pOut++ = '\\';` |
|   ! 0 |  477 | `						}` |
|    65 |  478 | `						*pOut++ = *zChk;` |
|    33 |  479 | `					}` |
|     7 |  480 | `					*pOut = 0;` |
|     7 |  481 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|     - |  482 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|     - |  483 | `						zWhat,zEsc);` |
|     - |  484 | `				}` |
|     7 |  485 | `				zRunCode = zReflCode;` |
|     7 |  486 | `				run_code = 1;` |
|     4 |  487 | `			}else{` |
|     - |  488 | `				/* Unknown long option */` |
|   ! 0 |  489 | `				Help();` |
|     - |  490 | `			}` |
|    11 |  491 | `			continue;` |
|     - |  492 | `		}` |
|   202 |  493 | `		c = argv[n][1];` |
|   202 |  494 | `		if( c == 'b' ){` |
|     - |  495 | `			/* Dump byte-code instructions */` |
|     3 |  496 | `			dump_vm = 1;` |
|   201 |  497 | `		}else if( c == 'l' ){` |
|     - |  498 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  499 | `			lint_mode = 1;` |
|   198 |  500 | `		}else if( c == 'i' ){` |
|     - |  501 | `			/* Display interpreter information and exit */` |
|     2 |  502 | `			Info();` |
|   195 |  503 | `		}else if( c == 'f' ){` |
|     - |  504 | ``			/* php CLI parity: `-f <file>` explicitly names the script to run.`` |
|     - |  505 | `			 * The path follows as the next argument, which the positional` |
|     - |  506 | `			 * file handling below already consumes, so treat -f as a no-op. */` |
|   ! 0 |  507 | `			continue;` |
|   194 |  508 | `		}else if( c == 'r' ){` |
|     - |  509 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    24 |  510 | `			if( n + 1 >= argc ){` |
|     - |  511 | `				/* Missing code argument */` |
|   ! 0 |  512 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  513 | `			}` |
|    24 |  514 | `			zRunCode = argv[++n];` |
|    24 |  515 | `			run_code = 1;` |
|   183 |  516 | `		}else if( c == 'S' ){` |
|     - |  517 | `			/* Start built-in development server */` |
|     - |  518 | `#ifdef PHL_ENABLE_SERVER` |
|    32 |  519 | `			if( n + 1 >= argc ){` |
|   ! 0 |  520 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  521 | `			}` |
|    32 |  522 | `			zServerAddr = argv[++n];` |
|    32 |  523 | `			server_mode = 1;` |
|     - |  524 | `#else` |
|     - |  525 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  526 | `#endif` |
|   156 |  527 | `		}else if( c == 't' ){` |
|     - |  528 | `			/* Set document root for the server */` |
|     - |  529 | `#ifdef PHL_ENABLE_SERVER` |
|    32 |  530 | `			if( n + 1 >= argc ){` |
|   ! 0 |  531 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  532 | `			}` |
|    32 |  533 | `			zDocRoot = argv[++n];` |
|     - |  534 | `#else` |
|     - |  535 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  536 | `#endif` |
|   124 |  537 | `		}else if( c == 'v' ){` |
|     - |  538 | `			/* Display version */` |
|   ! 0 |  539 | `			Version();` |
|   108 |  540 | `		}else if( c == 'd' ){` |
|     - |  541 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|     - |  542 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|   104 |  543 | `			if( n + 1 >= argc ){` |
|   ! 0 |  544 | `				FatalCode("Missing name=value argument for -d",1);` |
|   ! 0 |  545 | `			}` |
|   104 |  546 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|   104 |  547 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|    54 |  548 | `			}else{` |
|   ! 0 |  549 | `				FatalCode("Too many -d directives",1);` |
|     4 |  550 | `			}` |
|    54 |  551 | `		}else if( c == 'c' ){` |
|     - |  552 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|     - |  553 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|     4 |  554 | `			if( n + 1 >= argc ){` |
|   ! 0 |  555 | `				FatalCode("Missing file argument for -c",1);` |
|   ! 0 |  556 | `			}` |
|     4 |  557 | `			zIniFile = argv[++n];` |
|     2 |  558 | `		}else{` |
|     - |  559 | `			/* Display a help message and exit */` |
|   ! 0 |  560 | `			Help();` |
|     - |  561 | `		}` |
|   103 |  562 | `	}` |
|     - |  563 | `#ifdef PHL_ENABLE_SERVER` |
|  6101 |  564 | `	if( server_mode ){` |
|     - |  565 | `		/* Parse host:port from zServerAddr */` |
|     - |  566 | `		char zHost[256];` |
|    32 |  567 | `		int iPort = 0;` |
|     - |  568 | `		const char *zColon;` |
|    32 |  569 | `		const char *zRouter = 0;` |
|    32 |  570 | `		zColon = strrchr(zServerAddr, ':');` |
|    32 |  571 | `		if( zColon == 0 ){` |
|   ! 0 |  572 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  573 | `		}` |
|     - |  574 | `		{` |
|    32 |  575 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    32 |  576 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    32 |  577 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    32 |  578 | `			zHost[nHostLen] = 0;` |
|     - |  579 | `		}` |
|    32 |  580 | `		iPort = atoi(zColon + 1);` |
|    32 |  581 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  582 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  583 | `		}` |
|     - |  584 | `		/* Check for optional router script */` |
|    32 |  585 | `		if( n < argc ){` |
|   ! 0 |  586 | `			zRouter = argv[n];` |
|   ! 0 |  587 | `		}` |
|    32 |  588 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  589 | `	}` |
|     - |  590 | `#endif` |
|  6069 |  591 | `	if( (n >= argc \|\| dash_dash) && !run_code ){` |
|     - |  592 | ``		/* No file and no -r: php reads the script from stdin. `--` forces this`` |
|     - |  593 | `		 * (rest are script args); otherwise only when stdin is a pipe/redirect` |
|     - |  594 | `		 * (an interactive terminal would just block). */` |
|    14 |  595 | `		if( dash_dash \|\| PHL_StdinIsPipe() ){` |
|    12 |  596 | `			zStdinCode = PHL_SlurpStdin(&nStdinCode);` |
|    12 |  597 | `			if( zStdinCode == 0 ){` |
|   ! 0 |  598 | `				FatalCode("IO error while reading standard input",1);` |
|   ! 0 |  599 | `			}` |
|    12 |  600 | `			stdin_code = 1;` |
|     6 |  601 | `		}else{` |
|     2 |  602 | `			puts("Missing PHP file to compile");` |
|     2 |  603 | `			Help();` |
|     - |  604 | `		}` |
|     6 |  605 | `	}` |
|     - |  606 |  |
|     - |  607 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  608 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  609 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  610 | `#endif` |
|     - |  611 | `	/* Allocate a new PH7 engine instance */` |
|  5325 |  612 | `	rc = ph7_init(&pEngine);` |
|  5325 |  613 | `	pFatalEngine = pEngine;` |
|  5325 |  614 | `	if( rc != PH7_OK ){` |
|     - |  615 | `		/*` |
|     - |  616 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  617 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  618 | `		 */` |
|   ! 0 |  619 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  620 | `	}` |
|     - |  621 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  622 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  623 | `	 */` |
|  5325 |  624 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  625 | `		Output_Consumer, /* Error log consumer */` |
|     - |  626 | `		0 /* NULL: Callback Private data */` |
|     - |  627 | `		);` |
|     - |  628 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  629 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  630 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  631 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  632 | `	{` |
|     - |  633 | `		unsigned long uMax;` |
|     - |  634 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  5325 |  635 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  636 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  637 | `		}` |
|     - |  638 | `	}` |
|     - |  639 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  640 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  641 | `	{` |
|     - |  642 | `		unsigned long uMax;` |
|  5325 |  643 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  644 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  645 | `		}` |
|     - |  646 | `	}` |
|     - |  647 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  648 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  649 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  650 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  5325 |  651 | `	if( lint_mode ){` |
|     - |  652 | `		const char *zFile;` |
|     4 |  653 | `		if( n >= argc ){` |
|     - |  654 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  655 | `			ph7_release(pEngine);` |
|   ! 0 |  656 | `			pFatalEngine = 0;` |
|   ! 0 |  657 | `			puts("No input file specified");` |
|   ! 0 |  658 | `			return 255;` |
|     - |  659 | `		}` |
|     4 |  660 | `		zFile = argv[n];` |
|     4 |  661 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  662 | `		if( rc == PH7_OK ){` |
|     2 |  663 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  664 | `			ph7_vm_release(pVm);` |
|     3 |  665 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  666 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  667 | `		}else{` |
|     2 |  668 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  669 | `		}` |
|     4 |  670 | `		ph7_release(pEngine);` |
|     4 |  671 | `		pFatalEngine = 0;` |
|     4 |  672 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  673 | `	}` |
|     - |  674 | `	/* Now,it's time to compile our PHP file */` |
|  5321 |  675 | `	if( run_code ){` |
|     - |  676 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    30 |  677 | `		rc = ph7_compile_v2(` |
|    14 |  678 | `			pEngine, /* PH7 Engine */` |
|    14 |  679 | `			zRunCode, /* Source code */` |
|     - |  680 | `			-1,       /* Let API compute length */` |
|     - |  681 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  682 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  683 | `			);` |
|    30 |  684 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  685 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  686 | `				Fatal("VM initialization error");` |
|   ! 0 |  687 | `			}else{` |
|     - |  688 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  689 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|   ! 0 |  690 | `				FatalSilent();` |
|     - |  691 | `			}` |
|     2 |  692 | `		}` |
|  5307 |  693 | `	}else if( stdin_code ){` |
|     - |  694 | `		/* Script read from stdin: compile it like a file (PHP tags expected). */` |
|    12 |  695 | `		rc = ph7_compile_v2(` |
|     6 |  696 | `			pEngine,     /* PH7 Engine */` |
|     6 |  697 | `			zStdinCode,  /* Source code slurped from stdin */` |
|     6 |  698 | `			nStdinCode,  /* Its byte length */` |
|     - |  699 | `			&pVm,        /* OUT: Compiled PHP program */` |
|     - |  700 | `			0            /* IN: tag mode, like a file */` |
|     - |  701 | `			);` |
|    12 |  702 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  703 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  704 | `				Fatal("VM initialization error");` |
|   ! 0 |  705 | `			}else{` |
|   ! 0 |  706 | `				FatalSilent();` |
|     - |  707 | `			}` |
|   ! 0 |  708 | `		}` |
|     6 |  709 | `	}else{` |
|  5281 |  710 | `		rc = ph7_compile_file(` |
|  2443 |  711 | `			pEngine, /* PH7 Engine */` |
|  5276 |  712 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  713 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  714 | `			0        /* IN: Compile flags */` |
|     - |  715 | `			);` |
|  5281 |  716 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   784 |  717 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  718 | `				FatalCode("IO error while opening the target file",1);` |
|   784 |  719 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  720 | `				Fatal("VM initialization error");` |
|   ! 0 |  721 | `			}else{` |
|     - |  722 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  723 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   784 |  724 | `				FatalSilent();` |
|     - |  725 | `			}` |
|   390 |  726 | `		}` |
|     - |  727 | `	}` |
|     - |  728 | `	/*` |
|     - |  729 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  730 | `	 * We will install the VM output consumer callback defined above` |
|     - |  731 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  732 | `	 */` |
|  4931 |  733 | `	rc = ph7_vm_config(pVm,` |
|     - |  734 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  735 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  736 | `		0                   /* Callback private data */` |
|     - |  737 | `		);` |
|  4931 |  738 | `	if( rc != PH7_OK ){` |
|   ! 0 |  739 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  740 | `	}` |
|     - |  741 | `	/* Diagnostics stream: route the log copy of runtime warnings/notices and the` |
|     - |  742 | `	 * uncaught-exception fatal to STDERR (gated by log_errors), so program STDOUT` |
|     - |  743 | `	 * stays clean like stock CLI php. */` |
|  4931 |  744 | `	rc = ph7_vm_config(pVm,` |
|     - |  745 | `		PH7_VM_CONFIG_ERR_STREAM,` |
|     - |  746 | `		Error_Consumer,     /* Diagnostics (STDERR) consumer callback */` |
|     - |  747 | `		0                   /* Callback private data */` |
|     - |  748 | `		);` |
|  4931 |  749 | `	if( rc != PH7_OK ){` |
|   ! 0 |  750 | `		Fatal("Error while installing the VM diagnostics consumer callback");` |
|   ! 0 |  751 | `	}` |
|     - |  752 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  753 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  754 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  755 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  756 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  757 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  758 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  759 | `	 *                          floor 2) */` |
|     - |  760 | `	{` |
|     - |  761 | `		unsigned long uMax;` |
|  4931 |  762 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  763 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  764 | `		}` |
|  4931 |  765 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  766 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  767 | `		}` |
|     - |  768 | `	}` |
|     - |  769 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  7394 |  770 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  4926 |  771 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  772 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  773 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  774 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  775 | `	 * script's own arguments.` |
|     - |  776 | `	 */` |
|     - |  777 | `	{` |
|  4931 |  778 | `		const char *zScriptName = (run_code \|\| stdin_code) ? "Standard input code" : argv[n];` |
|  4931 |  779 | `		int argv_count = 0;` |
|     - |  780 | `		ph7_value *pArgc;` |
|     - |  781 | `		/* Count only the entries actually inserted, so $argc can never disagree` |
|     - |  782 | `		 * with count($argv) if a registration fails. */` |
|  4931 |  783 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  4931 |  784 | `			argv_count++;` |
|  2463 |  785 | `		}` |
|     - |  786 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  787 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  5007 |  788 | `		for( n = (run_code \|\| stdin_code) ? n : n + 1; n < argc ; ++n ){` |
|    81 |  789 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    81 |  790 | `				argv_count++;` |
|    38 |  791 | `			}` |
|    43 |  792 | `		}` |
|     - |  793 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  4931 |  794 | `		pArgc = ph7_new_scalar(pVm);` |
|  4931 |  795 | `		if( pArgc ){` |
|  4931 |  796 | `			ph7_value_int(pArgc,argv_count);` |
|  4931 |  797 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  4931 |  798 | `			ph7_release_value(pVm,pArgc);` |
|  2463 |  799 | `		}` |
|     - |  800 | `		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */` |
|  4931 |  801 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);` |
|     - |  802 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  803 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  4931 |  804 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  4931 |  805 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  4931 |  806 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  807 | `		{` |
|     - |  808 | `			char zTime[32];` |
|  4931 |  809 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  4931 |  810 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  811 | `		}` |
|     - |  812 | `#ifndef __WINNT__` |
|     - |  813 | `		{` |
|     - |  814 | `			char zCwd[PATH_MAX];` |
|  4926 |  815 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  4926 |  816 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  2463 |  817 | `			}` |
|     - |  818 | `		}` |
|     - |  819 | `#endif` |
|     - |  820 | `	}` |
|     - |  821 | `	/* Report script run-time errors (now default behavior) */` |
|  4931 |  822 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|     - |  823 | `	/* Apply php.ini directives AFTER the error-report default so` |
|     - |  824 | ``	 * `-d error_reporting=0` can lower it: the -c file first, then -d`` |
|     - |  825 | `	 * overrides in CLI order (php's precedence). */` |
|  4931 |  826 | `	if( zIniFile ){` |
|     4 |  827 | `		PHL_LoadIniFile(pVm,zIniFile);` |
|     2 |  828 | `	}` |
|     - |  829 | `	{` |
|     - |  830 | `		int i;` |
|  5031 |  831 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|   104 |  832 | `			PHL_ApplyIniPair(pVm,azIniDefine[i]);` |
|    54 |  833 | `		}` |
|     - |  834 | `	}` |
|  4931 |  835 | `	if( dump_vm ){` |
|     - |  836 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  837 | `		ph7_vm_dump_v2(pVm,` |
|     - |  838 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  839 | `			0` |
|     - |  840 | `			);` |
|     1 |  841 | `	}` |
|     - |  842 | `	/*` |
|     - |  843 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  844 | `	 * should display the result.` |
|     - |  845 | `	 */` |
|     - |  846 | `	{` |
|  4931 |  847 | `		int iExitStatus = 0;` |
|  4931 |  848 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  849 | `		/* All done, cleanup the mess left behind.` |
|     - |  850 | `		*/` |
|  4931 |  851 | `		ph7_vm_release(pVm);` |
|  4931 |  852 | `		ph7_release(pEngine);` |
|  4931 |  853 | `		pFatalEngine = 0;` |
|     - |  854 | `		/* The stdin slurp outlives compilation (the compiler keeps pointers` |
|     - |  855 | `		 * into the source text), so it is freed only here, after the VM. */` |
|  4931 |  856 | `		if( zStdinCode ){` |
|    12 |  857 | `			free(zStdinCode);` |
|     6 |  858 | `		}` |
|     - |  859 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  4931 |  860 | `		return iExitStatus;` |
|     - |  861 | `	}` |
|  2486 |  862 | `}` |
|     - |  863 |  |
