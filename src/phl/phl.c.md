# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 313/412 lines (75.97%)

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
|   456 |   71 | `static void FatalSilent(void)` |
|     4 |   72 | `{` |
|   460 |   73 | `	ph7_lib_shutdown();` |
|   460 |   74 | `	exit(255);` |
|   ! 0 |   75 | `}` |
|     - |   76 | `/*` |
|     - |   77 | ` * Display the banner,a help message and exit.` |
|     - |   78 | ` */` |
|     2 |   79 | `static void Help(void)` |
|     1 |   80 | `{` |
|     3 |   81 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|     - |   82 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   83 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   84 | `#endif` |
|     3 |   85 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     3 |   86 | `	puts("\t-i: Display interpreter information and exit");` |
|     3 |   87 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     3 |   88 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   89 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   90 | `	puts("\t-S host:port: Start the built-in development server");` |
|     3 |   91 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   92 | `#endif` |
|     3 |   93 | `	puts("\t-v, --version: Display version information and exit");` |
|     3 |   94 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   95 | `	/* Exit immediately */` |
|     3 |   96 | `	exit(0);` |
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
|     - |  146 | `#ifndef PATH_MAX` |
|     - |  147 | `#define PATH_MAX 4096` |
|     - |  148 | `#endif` |
|     - |  149 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  150 | `/*` |
|     - |  151 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  152 | ` * pUserData points to the resolved binary path.` |
|     - |  153 | ` */` |
|    12 |  154 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  155 | `{` |
|    13 |  156 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|    13 |  157 | `}` |
|     - |  158 | `/*` |
|     - |  159 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  160 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  161 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  162 | ` */` |
|  3430 |  163 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  164 | `{` |
|     - |  165 | `#ifdef __WINNT__` |
|     5 |  166 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  167 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  168 | `		return zPhlBinaryPath;` |
|     - |  169 | `	}` |
|     - |  170 | `#else` |
|  3430 |  171 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3430 |  172 | `		return zPhlBinaryPath;` |
|     - |  173 | `	}` |
|     - |  174 | `#endif` |
|   ! 0 |  175 | `	return zArgv0;` |
|  1720 |  176 | `}` |
|     - |  177 | `/*` |
|     - |  178 | ` * VM output consumer callback.` |
|     - |  179 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  180 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  181 | ` * the generated output.` |
|     - |  182 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  183 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  184 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  185 | ` */` |
| 13164 |  186 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  187 | `{` |
|  6582 |  188 | `	(void)pUserData;` |
|     - |  189 | `#ifdef __WINNT__` |
|     - |  190 | `	BOOL rc;` |
|     5 |  191 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  192 | `	if( !rc ){` |
|     - |  193 | `		/* Abort processing */` |
|   ! 0 |  194 | `		return PH7_ABORT;` |
|     - |  195 | `	}` |
|     - |  196 | `#else` |
|     - |  197 | `	ssize_t nWr;` |
| 13164 |  198 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 13164 |  199 | `	if( nWr < 0 ){` |
|     - |  200 | `		/* Abort processing */` |
|   ! 0 |  201 | `		return PH7_ABORT;` |
|     - |  202 | `	}` |
|     - |  203 | `#endif /* __WINT__ */` |
|     - |  204 | `	/* All done,VM output was redirected to STDOUT */` |
| 13169 |  205 | `	return PH7_OK;` |
|  6587 |  206 | `}` |
|     - |  207 | `/*` |
|     - |  208 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  209 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  210 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  211 | ` * [uFloor, uCeil]; returns` |
|     - |  212 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  213 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  214 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  215 | ` */` |
| 14536 |  216 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  217 | `{` |
| 14541 |  218 | `	const char *zVal = getenv(zName);` |
| 14541 |  219 | `	char *zEnd = 0;` |
|     - |  220 | `	unsigned long uMax;` |
| 14541 |  221 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 14527 |  222 | `		return 0;` |
|     - |  223 | `	}` |
|     - |  224 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  225 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    16 |  226 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  227 | `		return 0;` |
|     - |  228 | `	}` |
|    16 |  229 | `	errno = 0;` |
|    16 |  230 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    16 |  231 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  232 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  233 | `	}` |
|    16 |  234 | `	if( uMax < uFloor ){` |
|   ! 0 |  235 | `		uMax = uFloor;` |
|   ! 0 |  236 | `	}` |
|    16 |  237 | `	if( uMax > uCeil ){` |
|   ! 0 |  238 | `		uMax = uCeil;` |
|   ! 0 |  239 | `	}` |
|    16 |  240 | `	*pOut = uMax;` |
|    16 |  241 | `	return 1;` |
|  7273 |  242 | `}` |
|     - |  243 | `/*` |
|     - |  244 | ` * Apply one "name=value" php.ini directive to the VM (used by -d and each` |
|     - |  245 | ` * -c file line). Trims surrounding whitespace and one layer of quotes off` |
|     - |  246 | ` * the value, php.ini style.` |
|     - |  247 | ` */` |
|    22 |  248 | `static void PHL_ApplyIniPair(ph7_vm *pVm,const char *zPair)` |
|     1 |  249 | `{` |
|     - |  250 | `	char zName[128];` |
|     - |  251 | `	char zValue[512];` |
|    23 |  252 | `	const char *zEq = strchr(zPair,'=');` |
|     - |  253 | `	const char *zEnd;` |
|     - |  254 | `	size_t n;` |
|    23 |  255 | `	if( zEq == 0 ){` |
|     - |  256 | `		/* php: a bare -d name defines the entry with value "1" */` |
|     2 |  257 | `		zEq = zPair + strlen(zPair);` |
|     1 |  258 | `	}` |
|     - |  259 | `	/* name: trim */` |
|    23 |  260 | `	while( *zPair == ' ' \|\| *zPair == '\t' ){ zPair++; }` |
|    23 |  261 | `	zEnd = zEq;` |
|    42 |  262 | `	while( zEnd > zPair && (zEnd[-1] == ' ' \|\| zEnd[-1] == '\t') ){ zEnd--; }` |
|    23 |  263 | `	n = (size_t)(zEnd - zPair);` |
|    23 |  264 | `	if( n == 0 \|\| n >= sizeof(zName) ){` |
|   ! 0 |  265 | `		return;` |
|     - |  266 | `	}` |
|    23 |  267 | `	memcpy(zName,zPair,n);` |
|    23 |  268 | `	zName[n] = 0;` |
|     - |  269 | `	/* value: trim + unquote */` |
|    23 |  270 | `	if( *zEq == '=' ){` |
|    21 |  271 | `		const char *zV = zEq + 1;` |
|     - |  272 | `		const char *zVEnd;` |
|    29 |  273 | `		while( *zV == ' ' \|\| *zV == '\t' ){ zV++; }` |
|    21 |  274 | `		zVEnd = zV + strlen(zV);` |
|    39 |  275 | `		while( zVEnd > zV && (zVEnd[-1] == ' ' \|\| zVEnd[-1] == '\t'` |
|    32 |  276 | `		    \|\| zVEnd[-1] == '\r' \|\| zVEnd[-1] == '\n') ){ zVEnd--; }` |
|    21 |  277 | `		if( zVEnd - zV >= 2 && (zV[0] == '"' \|\| zV[0] == '\'') && zVEnd[-1] == zV[0] ){` |
|     4 |  278 | `			zV++;` |
|     4 |  279 | `			zVEnd--;` |
|     2 |  280 | `		}` |
|    21 |  281 | `		n = (size_t)(zVEnd - zV);` |
|    21 |  282 | `		if( n >= sizeof(zValue) ){` |
|   ! 0 |  283 | `			n = sizeof(zValue) - 1;` |
|   ! 0 |  284 | `		}` |
|    21 |  285 | `		memcpy(zValue,zV,n);` |
|    21 |  286 | `		zValue[n] = 0;` |
|    11 |  287 | `	}else{` |
|     - |  288 | `		/* default "on" flag; avoid strcpy (MSVC C4996 under /WX) */` |
|     2 |  289 | `		zValue[0] = '1';` |
|     2 |  290 | `		zValue[1] = 0;` |
|     - |  291 | `	}` |
|    23 |  292 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_INI_ENTRY,zName,zValue);` |
|    12 |  293 | `}` |
|     - |  294 | `/*` |
|     - |  295 | ` * Load php.ini directives from a -c file: name=value lines; [sections],` |
|     - |  296 | ` * empty lines and ;/# comments are ignored (enough of php's ini grammar` |
|     - |  297 | ` * for CLI configuration).` |
|     - |  298 | ` */` |
|     4 |  299 | `static void PHL_LoadIniFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 |  300 | `{` |
|     - |  301 | `	char zLine[768];` |
|     4 |  302 | `	FILE *pFile = fopen(zPath,"r");` |
|     4 |  303 | `	if( pFile == 0 ){` |
|   ! 0 |  304 | `		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);` |
|   ! 0 |  305 | `		return;` |
|     - |  306 | `	}` |
|    20 |  307 | `	while( fgets(zLine,sizeof(zLine),pFile) ){` |
|    16 |  308 | `		const char *z = zLine;` |
|    16 |  309 | `		while( *z == ' ' \|\| *z == '\t' ){ z++; }` |
|    16 |  310 | `		if( *z == 0 \|\| *z == ';' \|\| *z == '#' \|\| *z == '[' \|\| *z == '\n' \|\| *z == '\r' ){` |
|     8 |  311 | `			continue;` |
|     - |  312 | `		}` |
|     8 |  313 | `		PHL_ApplyIniPair(pVm,z);` |
|   ! 0 |  314 | `	}` |
|     4 |  315 | `	fclose(pFile);` |
|     2 |  316 | `}` |
|     - |  317 | `/*` |
|     - |  318 | ` * Return TRUE when standard input is a pipe/redirect rather than an interactive` |
|     - |  319 | ` * terminal. php reads the script from stdin in exactly that case; a terminal` |
|     - |  320 | ` * would block waiting for input, so there we keep the usage error instead.` |
|     - |  321 | ` */` |
|    10 |  322 | `static int PHL_StdinIsPipe(void)` |
|   ! 0 |  323 | `{` |
|     - |  324 | `#ifdef __UNIXES__` |
|    10 |  325 | `	return !isatty(0);` |
|     - |  326 | `#else` |
|   ! 0 |  327 | `	return 0;` |
|     - |  328 | `#endif` |
|   ! 0 |  329 | `}` |
|     - |  330 | `/*` |
|     - |  331 | ` * Slurp all of standard input into a heap buffer (NUL-terminated). Returns the` |
|     - |  332 | ` * buffer (caller owns it, though the process exits shortly after) and writes the` |
|     - |  333 | ` * byte length to *pnLen, or NULL on allocation failure.` |
|     - |  334 | ` */` |
|    12 |  335 | `static char * PHL_SlurpStdin(int *pnLen)` |
|   ! 0 |  336 | `{` |
|    12 |  337 | `	size_t nCap = 8192, nUsed = 0;` |
|    12 |  338 | `	char *zBuf = (char *)malloc(nCap);` |
|    12 |  339 | `	if( zBuf == 0 ){ return 0; }` |
|     6 |  340 | `	for(;;){` |
|     - |  341 | `		size_t nRd;` |
|    12 |  342 | `		if( nUsed + 4096 + 1 > nCap ){` |
|     - |  343 | `			char *zNew;` |
|   ! 0 |  344 | `			nCap *= 2;` |
|   ! 0 |  345 | `			zNew = (char *)realloc(zBuf, nCap);` |
|   ! 0 |  346 | `			if( zNew == 0 ){ free(zBuf); return 0; }` |
|   ! 0 |  347 | `			zBuf = zNew;` |
|   ! 0 |  348 | `		}` |
|    12 |  349 | `		nRd = fread(zBuf + nUsed, 1, 4096, stdin);` |
|    12 |  350 | `		nUsed += nRd;` |
|    12 |  351 | `		if( nRd < 4096 ){ break; }` |
|   ! 0 |  352 | `	}` |
|    12 |  353 | `	zBuf[nUsed] = 0;` |
|    12 |  354 | `	*pnLen = (int)nUsed;` |
|    12 |  355 | `	return zBuf;` |
|     6 |  356 | `}` |
|     - |  357 | `/*` |
|     - |  358 | ` * Main program: Compile and execute the PHP file.` |
|     - |  359 | ` */` |
|  4091 |  360 | `int main(int argc,char **argv)` |
|     5 |  361 | `{` |
|     - |  362 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  363 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  4096 |  364 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  4096 |  365 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  4096 |  366 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  4096 |  367 | `	const char *zRunCode = 0; /* Inline code string */` |
|  4096 |  368 | `	int stdin_code = 0;       /* Execute a script read from stdin if TRUE */` |
|  4096 |  369 | `	char *zStdinCode = 0;     /* Script slurped from stdin */` |
|  4096 |  370 | `	int nStdinCode = 0;       /* Length of the stdin script */` |
|  4096 |  371 | ``	int dash_dash = 0;        /* Saw `--`: read from stdin, rest are script args */`` |
|     - |  372 | `#ifdef PHL_ENABLE_SERVER` |
|  4096 |  373 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  4096 |  374 | `	const char *zServerAddr = 0; /* host:port string */` |
|  4096 |  375 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  376 | `#endif` |
|     - |  377 | `	int n;              /* Script arguments */` |
|     - |  378 | `	int rc;` |
|     - |  379 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|  4096 |  380 | `	int nIniDefine = 0;` |
|  4096 |  381 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|     - |  382 | `	/* Process interpreter arguments first*/` |
|  4201 |  383 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  384 | `		int c;` |
|  3947 |  385 | `		if( argv[n][0] != '-' ){` |
|     - |  386 | `			/* No more interpreter arguments */` |
|  3835 |  387 | `			break;` |
|     - |  388 | `		}` |
|     - |  389 | `		/* Check for long options */` |
|   115 |  390 | `		if( argv[n][1] == '-' ){` |
|    18 |  391 | `			if( argv[n][2] == 0 ){` |
|     - |  392 | ``				/* php CLI parity: a bare `--` ends interpreter options; the`` |
|     - |  393 | ``				 * script is read from stdin and everything after `--` becomes`` |
|     - |  394 | `				 * the script's own arguments ($argv[1..]). */` |
|     2 |  395 | `				dash_dash = 1;` |
|     2 |  396 | `				n++;` |
|     2 |  397 | `				break;` |
|     - |  398 | `			}` |
|    16 |  399 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  400 | `				Version();` |
|    12 |  401 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  402 | `				Help();` |
|     8 |  403 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|     - |  404 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|     - |  405 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|     - |  406 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|     - |  407 | `				 * does not exist. Implemented as an inline snippet riding the` |
|     - |  408 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|     - |  409 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|     - |  410 | `				static char zReflCode[768];` |
|     7 |  411 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|     - |  412 | `				const char *zName;` |
|     - |  413 | `				const char *zChk;` |
|     7 |  414 | `				if( n + 1 >= argc ){` |
|   ! 0 |  415 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|   ! 0 |  416 | `				}` |
|     7 |  417 | `				zName = argv[++n];` |
|    71 |  418 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  419 | `					char ch = *zChk;` |
|    65 |  420 | `					if( !(ch == '_' \|\| ch == '\\'` |
|    58 |  421 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|     4 |  422 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  423 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|   ! 0 |  424 | `					}` |
|    33 |  425 | `				}` |
|     7 |  426 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  427 | `					FatalCode("Name too long for --rf/--rc",1);` |
|   ! 0 |  428 | `				}` |
|     - |  429 | `				{` |
|     - |  430 | `					/* Double the namespace separators: inside the snippet's` |
|     - |  431 | `					 * double-quoted string a lone backslash could form an` |
|     - |  432 | `					 * escape sequence ("App\name" -> newline). */` |
|     - |  433 | `					static char zEsc[512];` |
|     7 |  434 | `					char *pOut = zEsc;` |
|    71 |  435 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  436 | `						if( *zChk == '\\' ){` |
|   ! 0 |  437 | `							*pOut++ = '\\';` |
|   ! 0 |  438 | `						}` |
|    65 |  439 | `						*pOut++ = *zChk;` |
|    33 |  440 | `					}` |
|     7 |  441 | `					*pOut = 0;` |
|     7 |  442 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|     - |  443 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|     - |  444 | `						zWhat,zEsc);` |
|     - |  445 | `				}` |
|     7 |  446 | `				zRunCode = zReflCode;` |
|     7 |  447 | `				run_code = 1;` |
|     4 |  448 | `			}else{` |
|     - |  449 | `				/* Unknown long option */` |
|   ! 0 |  450 | `				Help();` |
|     - |  451 | `			}` |
|    11 |  452 | `			continue;` |
|     - |  453 | `		}` |
|    99 |  454 | `		c = argv[n][1];` |
|    99 |  455 | `		if( c == 'b' ){` |
|     - |  456 | `			/* Dump byte-code instructions */` |
|     3 |  457 | `			dump_vm = 1;` |
|    97 |  458 | `		}else if( c == 'l' ){` |
|     - |  459 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  460 | `			lint_mode = 1;` |
|    94 |  461 | `		}else if( c == 'i' ){` |
|     - |  462 | `			/* Display interpreter information and exit */` |
|     2 |  463 | `			Info();` |
|    91 |  464 | `		}else if( c == 'f' ){` |
|     - |  465 | ``			/* php CLI parity: `-f <file>` explicitly names the script to run.`` |
|     - |  466 | `			 * The path follows as the next argument, which the positional` |
|     - |  467 | `			 * file handling below already consumes, so treat -f as a no-op. */` |
|   ! 0 |  468 | `			continue;` |
|    90 |  469 | `		}else if( c == 'r' ){` |
|     - |  470 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    20 |  471 | `			if( n + 1 >= argc ){` |
|     - |  472 | `				/* Missing code argument */` |
|   ! 0 |  473 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  474 | `			}` |
|    20 |  475 | `			zRunCode = argv[++n];` |
|    20 |  476 | `			run_code = 1;` |
|    80 |  477 | `		}else if( c == 'S' ){` |
|     - |  478 | `			/* Start built-in development server */` |
|     - |  479 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  480 | `			if( n + 1 >= argc ){` |
|   ! 0 |  481 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  482 | `			}` |
|    26 |  483 | `			zServerAddr = argv[++n];` |
|    26 |  484 | `			server_mode = 1;` |
|     - |  485 | `#else` |
|     - |  486 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  487 | `#endif` |
|    58 |  488 | `		}else if( c == 't' ){` |
|     - |  489 | `			/* Set document root for the server */` |
|     - |  490 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  491 | `			if( n + 1 >= argc ){` |
|   ! 0 |  492 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  493 | `			}` |
|    26 |  494 | `			zDocRoot = argv[++n];` |
|     - |  495 | `#else` |
|     - |  496 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  497 | `#endif` |
|    32 |  498 | `		}else if( c == 'v' ){` |
|     - |  499 | `			/* Display version */` |
|   ! 0 |  500 | `			Version();` |
|    19 |  501 | `		}else if( c == 'd' ){` |
|     - |  502 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|     - |  503 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|    15 |  504 | `			if( n + 1 >= argc ){` |
|   ! 0 |  505 | `				FatalCode("Missing name=value argument for -d",1);` |
|   ! 0 |  506 | `			}` |
|    15 |  507 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|    15 |  508 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|     8 |  509 | `			}else{` |
|   ! 0 |  510 | `				FatalCode("Too many -d directives",1);` |
|     1 |  511 | `			}` |
|    11 |  512 | `		}else if( c == 'c' ){` |
|     - |  513 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|     - |  514 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|     4 |  515 | `			if( n + 1 >= argc ){` |
|   ! 0 |  516 | `				FatalCode("Missing file argument for -c",1);` |
|   ! 0 |  517 | `			}` |
|     4 |  518 | `			zIniFile = argv[++n];` |
|     2 |  519 | `		}else{` |
|     - |  520 | `			/* Display a help message and exit */` |
|   ! 0 |  521 | `			Help();` |
|     - |  522 | `		}` |
|    51 |  523 | `	}` |
|     - |  524 | `#ifdef PHL_ENABLE_SERVER` |
|  4091 |  525 | `	if( server_mode ){` |
|     - |  526 | `		/* Parse host:port from zServerAddr */` |
|     - |  527 | `		char zHost[256];` |
|    26 |  528 | `		int iPort = 0;` |
|     - |  529 | `		const char *zColon;` |
|    26 |  530 | `		const char *zRouter = 0;` |
|    26 |  531 | `		zColon = strrchr(zServerAddr, ':');` |
|    26 |  532 | `		if( zColon == 0 ){` |
|   ! 0 |  533 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  534 | `		}` |
|     - |  535 | `		{` |
|    26 |  536 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    26 |  537 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    26 |  538 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    26 |  539 | `			zHost[nHostLen] = 0;` |
|     - |  540 | `		}` |
|    26 |  541 | `		iPort = atoi(zColon + 1);` |
|    26 |  542 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  543 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  544 | `		}` |
|     - |  545 | `		/* Check for optional router script */` |
|    26 |  546 | `		if( n < argc ){` |
|   ! 0 |  547 | `			zRouter = argv[n];` |
|   ! 0 |  548 | `		}` |
|    26 |  549 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  550 | `	}` |
|     - |  551 | `#endif` |
|  4065 |  552 | `	if( (n >= argc \|\| dash_dash) && !run_code ){` |
|     - |  553 | ``		/* No file and no -r: php reads the script from stdin. `--` forces this`` |
|     - |  554 | `		 * (rest are script args); otherwise only when stdin is a pipe/redirect` |
|     - |  555 | `		 * (an interactive terminal would just block). */` |
|    12 |  556 | `		if( dash_dash \|\| PHL_StdinIsPipe() ){` |
|    12 |  557 | `			zStdinCode = PHL_SlurpStdin(&nStdinCode);` |
|    12 |  558 | `			if( zStdinCode == 0 ){` |
|   ! 0 |  559 | `				FatalCode("IO error while reading standard input",1);` |
|   ! 0 |  560 | `			}` |
|    12 |  561 | `			stdin_code = 1;` |
|     6 |  562 | `		}else{` |
|   ! 0 |  563 | `			puts("Missing PHP file to compile");` |
|   ! 0 |  564 | `			Help();` |
|     - |  565 | `		}` |
|     6 |  566 | `	}` |
|     - |  567 |  |
|     - |  568 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  569 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  570 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  571 | `#endif` |
|     - |  572 | `	/* Allocate a new PH7 engine instance */` |
|  3641 |  573 | `	rc = ph7_init(&pEngine);` |
|  3641 |  574 | `	if( rc != PH7_OK ){` |
|     - |  575 | `		/*` |
|     - |  576 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  577 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  578 | `		 */` |
|   ! 0 |  579 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  580 | `	}` |
|     - |  581 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  582 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  583 | `	 */` |
|  3641 |  584 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  585 | `		Output_Consumer, /* Error log consumer */` |
|     - |  586 | `		0 /* NULL: Callback Private data */` |
|     - |  587 | `		);` |
|     - |  588 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  589 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  590 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  591 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  592 | `	{` |
|     - |  593 | `		unsigned long uMax;` |
|     - |  594 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  3641 |  595 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  596 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  597 | `		}` |
|     - |  598 | `	}` |
|     - |  599 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  600 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  601 | `	{` |
|     - |  602 | `		unsigned long uMax;` |
|  3641 |  603 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  604 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  605 | `		}` |
|     - |  606 | `	}` |
|     - |  607 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  608 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  609 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  610 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  3641 |  611 | `	if( lint_mode ){` |
|     - |  612 | `		const char *zFile;` |
|     4 |  613 | `		if( n >= argc ){` |
|     - |  614 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  615 | `			ph7_release(pEngine);` |
|   ! 0 |  616 | `			puts("No input file specified");` |
|   ! 0 |  617 | `			return 255;` |
|     - |  618 | `		}` |
|     4 |  619 | `		zFile = argv[n];` |
|     4 |  620 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  621 | `		if( rc == PH7_OK ){` |
|     2 |  622 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  623 | `			ph7_vm_release(pVm);` |
|     3 |  624 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  625 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  626 | `		}else{` |
|     2 |  627 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  628 | `		}` |
|     4 |  629 | `		ph7_release(pEngine);` |
|     4 |  630 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  631 | `	}` |
|     - |  632 | `	/* Now,it's time to compile our PHP file */` |
|  3637 |  633 | `	if( run_code ){` |
|     - |  634 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    26 |  635 | `		rc = ph7_compile_v2(` |
|    12 |  636 | `			pEngine, /* PH7 Engine */` |
|    12 |  637 | `			zRunCode, /* Source code */` |
|     - |  638 | `			-1,       /* Let API compute length */` |
|     - |  639 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  640 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  641 | `			);` |
|    26 |  642 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  643 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  644 | `				Fatal("VM initialization error");` |
|   ! 0 |  645 | `			}else{` |
|     - |  646 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  647 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|   ! 0 |  648 | `				FatalSilent();` |
|     - |  649 | `			}` |
|     2 |  650 | `		}` |
|  3625 |  651 | `	}else if( stdin_code ){` |
|     - |  652 | `		/* Script read from stdin: compile it like a file (PHP tags expected). */` |
|    12 |  653 | `		rc = ph7_compile_v2(` |
|     6 |  654 | `			pEngine,     /* PH7 Engine */` |
|     6 |  655 | `			zStdinCode,  /* Source code slurped from stdin */` |
|     6 |  656 | `			nStdinCode,  /* Its byte length */` |
|     - |  657 | `			&pVm,        /* OUT: Compiled PHP program */` |
|     - |  658 | `			0            /* IN: tag mode, like a file */` |
|     - |  659 | `			);` |
|    12 |  660 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  661 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  662 | `				Fatal("VM initialization error");` |
|   ! 0 |  663 | `			}else{` |
|   ! 0 |  664 | `				FatalSilent();` |
|     - |  665 | `			}` |
|   ! 0 |  666 | `		}` |
|     6 |  667 | `	}else{` |
|  3601 |  668 | `		rc = ph7_compile_file(` |
|  1684 |  669 | `			pEngine, /* PH7 Engine */` |
|  3596 |  670 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  671 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  672 | `			0        /* IN: Compile flags */` |
|     - |  673 | `			);` |
|  3601 |  674 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   460 |  675 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  676 | `				FatalCode("IO error while opening the target file",1);` |
|   460 |  677 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  678 | `				Fatal("VM initialization error");` |
|   ! 0 |  679 | `			}else{` |
|     - |  680 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  681 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   460 |  682 | `				FatalSilent();` |
|     - |  683 | `			}` |
|   228 |  684 | `		}` |
|     - |  685 | `	}` |
|     - |  686 | `	/*` |
|     - |  687 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  688 | `	 * We will install the VM output consumer callback defined above` |
|     - |  689 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  690 | `	 */` |
|  3409 |  691 | `	rc = ph7_vm_config(pVm,` |
|     - |  692 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  693 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  694 | `		0                   /* Callback private data */` |
|     - |  695 | `		);` |
|  3409 |  696 | `	if( rc != PH7_OK ){` |
|   ! 0 |  697 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  698 | `	}` |
|     - |  699 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  700 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  701 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  702 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  703 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  704 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  705 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  706 | `	 *                          floor 2) */` |
|     - |  707 | `	{` |
|     - |  708 | `		unsigned long uMax;` |
|  3409 |  709 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  710 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  711 | `		}` |
|  3409 |  712 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    11 |  713 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  714 | `		}` |
|     - |  715 | `	}` |
|     - |  716 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  5111 |  717 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3404 |  718 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  719 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  720 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  721 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  722 | `	 * script's own arguments.` |
|     - |  723 | `	 */` |
|     - |  724 | `	{` |
|  3409 |  725 | `		const char *zScriptName = (run_code \|\| stdin_code) ? "Standard input code" : argv[n];` |
|  3409 |  726 | `		int argv_count = 0;` |
|     - |  727 | `		ph7_value *pArgc;` |
|     - |  728 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  729 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  730 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3409 |  731 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3409 |  732 | `			argv_count++;` |
|  1702 |  733 | `		}` |
|     - |  734 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  735 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3443 |  736 | `		for( n = (run_code \|\| stdin_code) ? n : n + 1; n < argc ; ++n ){` |
|    39 |  737 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    37 |  738 | `				argv_count++;` |
|    16 |  739 | `			}` |
|    22 |  740 | `		}` |
|     - |  741 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3409 |  742 | `		pArgc = ph7_new_scalar(pVm);` |
|  3409 |  743 | `		if( pArgc ){` |
|  3409 |  744 | `			ph7_value_int(pArgc,argv_count);` |
|  3409 |  745 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3409 |  746 | `			ph7_release_value(pVm,pArgc);` |
|  1702 |  747 | `		}` |
|     - |  748 | `		/* Mirror $argv/$argc into $_SERVER['argv']/$_SERVER['argc'] (php CLI). */` |
|  3409 |  749 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ARGV);` |
|     - |  750 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  751 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3409 |  752 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3409 |  753 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3409 |  754 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  755 | `		{` |
|     - |  756 | `			char zTime[32];` |
|  3409 |  757 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3409 |  758 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  759 | `		}` |
|     - |  760 | `#ifndef __WINNT__` |
|     - |  761 | `		{` |
|     - |  762 | `			char zCwd[PATH_MAX];` |
|  3404 |  763 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3404 |  764 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1702 |  765 | `			}` |
|     - |  766 | `		}` |
|     - |  767 | `#endif` |
|     - |  768 | `	}` |
|     - |  769 | `	/* Report script run-time errors (now default behavior) */` |
|  3409 |  770 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|     - |  771 | `	/* Apply php.ini directives AFTER the error-report default so` |
|     - |  772 | ``	 * `-d error_reporting=0` can lower it: the -c file first, then -d`` |
|     - |  773 | `	 * overrides in CLI order (php's precedence). */` |
|  3409 |  774 | `	if( zIniFile ){` |
|     4 |  775 | `		PHL_LoadIniFile(pVm,zIniFile);` |
|     2 |  776 | `	}` |
|     - |  777 | `	{` |
|     - |  778 | `		int i;` |
|  3423 |  779 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|    15 |  780 | `			PHL_ApplyIniPair(pVm,azIniDefine[i]);` |
|     8 |  781 | `		}` |
|     - |  782 | `	}` |
|  3409 |  783 | `	if( dump_vm ){` |
|     - |  784 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  785 | `		ph7_vm_dump_v2(pVm,` |
|     - |  786 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  787 | `			0` |
|     - |  788 | `			);` |
|     1 |  789 | `	}` |
|     - |  790 | `	/*` |
|     - |  791 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  792 | `	 * should display the result.` |
|     - |  793 | `	 */` |
|     - |  794 | `	{` |
|  3409 |  795 | `		int iExitStatus = 0;` |
|  3409 |  796 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  797 | `		/* All done, cleanup the mess left behind.` |
|     - |  798 | `		*/` |
|  3409 |  799 | `		ph7_vm_release(pVm);` |
|  3409 |  800 | `		ph7_release(pEngine);` |
|     - |  801 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3409 |  802 | `		return iExitStatus;` |
|     - |  803 | `	}` |
|  1722 |  804 | `}` |
|     - |  805 |  |
