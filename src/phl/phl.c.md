# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 273/358 lines (76.26%)

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
|     - |   34 | `/* Make sure this header file is available.*/` |
|     - |   35 | `#include "ph7.h"` |
|     - |   36 | `#ifdef PHL_ENABLE_SERVER` |
|     - |   37 | `#include "server.h"` |
|     - |   38 | `#endif` |
|     - |   39 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |   40 | `#define MINIDUMP_IMPLEMENTATION` |
|     - |   41 | `#include "minidump.h"` |
|     - |   42 | `#endif` |
|     - |   43 | `/*` |
|     - |   44 | ` * Display an error message and exit.` |
|     - |   45 | ` */` |
|   ! 0 |   46 | `static void FatalCode(const char *zMsg,int iCode)` |
|   ! 0 |   47 | `{` |
|   ! 0 |   48 | `	puts(zMsg);` |
|     - |   49 | `	/* Shutdown the library */` |
|   ! 0 |   50 | `	ph7_lib_shutdown();` |
|     - |   51 | `	/* Exit immediately */` |
|   ! 0 |   52 | `	exit(iCode);` |
|   ! 0 |   53 | `}` |
|     - |   54 | `/*` |
|     - |   55 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|     - |   56 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|     - |   57 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|     - |   58 | ` */` |
|   ! 0 |   59 | `static void Fatal(const char *zMsg)` |
|   ! 0 |   60 | `{` |
|   ! 0 |   61 | `	FatalCode(zMsg,255);` |
|   ! 0 |   62 | `}` |
|     - |   63 | `/*` |
|     - |   64 | ` * Exit 255 without printing anything: used when the diagnostic has already been` |
|     - |   65 | ` * emitted by the error consumer (a compile/parse error), which is exactly what` |
|     - |   66 | ` * php does — it prints the parse error and nothing more.` |
|     - |   67 | ` */` |
|   452 |   68 | `static void FatalSilent(void)` |
|     4 |   69 | `{` |
|   456 |   70 | `	ph7_lib_shutdown();` |
|   456 |   71 | `	exit(255);` |
|   ! 0 |   72 | `}` |
|     - |   73 | `/*` |
|     - |   74 | ` * Display the banner,a help message and exit.` |
|     - |   75 | ` */` |
|     2 |   76 | `static void Help(void)` |
|     1 |   77 | `{` |
|     3 |   78 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code\|--rf name\|--rc name\|-d name=value\|-c inifile] path/to/php_file [script args]");` |
|     - |   79 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   80 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   81 | `#endif` |
|     3 |   82 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     3 |   83 | `	puts("\t-i: Display interpreter information and exit");` |
|     3 |   84 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     3 |   85 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   86 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   87 | `	puts("\t-S host:port: Start the built-in development server");` |
|     3 |   88 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   89 | `#endif` |
|     3 |   90 | `	puts("\t-v, --version: Display version information and exit");` |
|     3 |   91 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   92 | `	/* Exit immediately */` |
|     3 |   93 | `	exit(0);` |
|   ! 0 |   94 | `}` |
|     - |   95 | `/*` |
|     - |   96 | ` * Display version information and exit.` |
|     - |   97 | ` */` |
|     6 |   98 | `static void Version(void)` |
|     1 |   99 | `{` |
|     7 |  100 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |  101 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |  102 | `	/* Exit immediately */` |
|     7 |  103 | `	exit(0);` |
|   ! 0 |  104 | `}` |
|     - |  105 | `/*` |
|     - |  106 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |  107 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |  108 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |  109 | ` */` |
|     2 |  110 | `static void Info(void)` |
|   ! 0 |  111 | `{` |
|     2 |  112 | `	printf("phpinfo()\n");` |
|     2 |  113 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  114 | `	printf("System => %s\n",` |
|     - |  115 | `#ifdef __WINNT__` |
|     - |  116 | `		"Windows NT"` |
|     - |  117 | `#elif defined(__UNIXES__)` |
|     - |  118 | `		"UNIX-Like"` |
|     - |  119 | `#else` |
|     - |  120 | `		"Other OS"` |
|     - |  121 | `#endif` |
|     - |  122 | `	);` |
|     2 |  123 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  124 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  125 | `	printf("PHP SAPI => cli\n");` |
|     - |  126 | `	/* Exit immediately */` |
|     2 |  127 | `	exit(0);` |
|   ! 0 |  128 | `}` |
|     - |  129 | `#ifdef __WINNT__` |
|     - |  130 | `#include <Windows.h>` |
|     - |  131 | `#else` |
|     - |  132 | `/* Assume UNIX */` |
|     - |  133 | `#include <unistd.h>` |
|     - |  134 | `#include <limits.h>` |
|     - |  135 | `#endif` |
|     - |  136 | `/*` |
|     - |  137 | ` * The following define is used by the UNIX built and have` |
|     - |  138 | ` * no particular meaning on windows.` |
|     - |  139 | ` */` |
|     - |  140 | `#ifndef STDOUT_FILENO` |
|     - |  141 | `#define STDOUT_FILENO	1` |
|     - |  142 | `#endif` |
|     - |  143 | `#ifndef PATH_MAX` |
|     - |  144 | `#define PATH_MAX 4096` |
|     - |  145 | `#endif` |
|     - |  146 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  147 | `/*` |
|     - |  148 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  149 | ` * pUserData points to the resolved binary path.` |
|     - |  150 | ` */` |
|     2 |  151 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  152 | `{` |
|     3 |  153 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|     3 |  154 | `}` |
|     - |  155 | `/*` |
|     - |  156 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  157 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  158 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  159 | ` */` |
|  3386 |  160 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  161 | `{` |
|     - |  162 | `#ifdef __WINNT__` |
|     5 |  163 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  164 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  165 | `		return zPhlBinaryPath;` |
|     - |  166 | `	}` |
|     - |  167 | `#else` |
|  3386 |  168 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3386 |  169 | `		return zPhlBinaryPath;` |
|     - |  170 | `	}` |
|     - |  171 | `#endif` |
|   ! 0 |  172 | `	return zArgv0;` |
|  1698 |  173 | `}` |
|     - |  174 | `/*` |
|     - |  175 | ` * VM output consumer callback.` |
|     - |  176 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  177 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  178 | ` * the generated output.` |
|     - |  179 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  180 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  181 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  182 | ` */` |
| 12246 |  183 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  184 | `{` |
|  6123 |  185 | `	(void)pUserData;` |
|     - |  186 | `#ifdef __WINNT__` |
|     - |  187 | `	BOOL rc;` |
|     5 |  188 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  189 | `	if( !rc ){` |
|     - |  190 | `		/* Abort processing */` |
|   ! 0 |  191 | `		return PH7_ABORT;` |
|     - |  192 | `	}` |
|     - |  193 | `#else` |
|     - |  194 | `	ssize_t nWr;` |
| 12246 |  195 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 12246 |  196 | `	if( nWr < 0 ){` |
|     - |  197 | `		/* Abort processing */` |
|   ! 0 |  198 | `		return PH7_ABORT;` |
|     - |  199 | `	}` |
|     - |  200 | `#endif /* __WINT__ */` |
|     - |  201 | `	/* All done,VM output was redirected to STDOUT */` |
| 12251 |  202 | `	return PH7_OK;` |
|  6128 |  203 | `}` |
|     - |  204 | `/*` |
|     - |  205 | ` * Parse an unsigned-long testing knob from the environment (PHL_MAX_ALLOC /` |
|     - |  206 | ` * PHL_MAX_INPUT / PHL_MAX_RECURSION / PHL_MAX_NATIVE_DEPTH). Returns 1 and writes` |
|     - |  207 | ` * *pOut on a valid, strictly-positive, fully-numeric value clamped to` |
|     - |  208 | ` * [uFloor, uCeil]; returns` |
|     - |  209 | ` * 0 (leaving *pOut untouched) when the var is unset, empty, non-numeric, has` |
|     - |  210 | ` * trailing garbage, or is zero — so a typo like "-1" or "abc" is ignored` |
|     - |  211 | ` * rather than silently reinterpreted (strtoul would wrap "-1" to ULONG_MAX).` |
|     - |  212 | ` */` |
| 14352 |  213 | `static int PHL_EnvULong(const char *zName,unsigned long uFloor,unsigned long uCeil,unsigned long *pOut)` |
|     5 |  214 | `{` |
| 14357 |  215 | `	const char *zVal = getenv(zName);` |
| 14357 |  216 | `	char *zEnd = 0;` |
|     - |  217 | `	unsigned long uMax;` |
| 14357 |  218 | `	if( zVal == 0 \|\| zVal[0] == 0 ){` |
| 14343 |  219 | `		return 0;` |
|     - |  220 | `	}` |
|     - |  221 | `	/* Reject a leading sign outright: strtoul silently negates "-1" to` |
|     - |  222 | `	 * ULONG_MAX, turning a typo into an effectively-unlimited cap. */` |
|    17 |  223 | `	if( zVal[0] == '-' \|\| zVal[0] == '+' ){` |
|   ! 0 |  224 | `		return 0;` |
|     - |  225 | `	}` |
|    17 |  226 | `	errno = 0;` |
|    17 |  227 | `	uMax = strtoul(zVal,&zEnd,10);` |
|    17 |  228 | `	if( errno != 0 \|\| zEnd == zVal \|\| *zEnd != 0 \|\| uMax == 0 ){` |
|   ! 0 |  229 | `		return 0; /* non-numeric, trailing junk, overflow, or zero */` |
|     - |  230 | `	}` |
|    17 |  231 | `	if( uMax < uFloor ){` |
|   ! 0 |  232 | `		uMax = uFloor;` |
|   ! 0 |  233 | `	}` |
|    17 |  234 | `	if( uMax > uCeil ){` |
|   ! 0 |  235 | `		uMax = uCeil;` |
|   ! 0 |  236 | `	}` |
|    17 |  237 | `	*pOut = uMax;` |
|    17 |  238 | `	return 1;` |
|  7181 |  239 | `}` |
|     - |  240 | `/*` |
|     - |  241 | ` * Apply one "name=value" php.ini directive to the VM (used by -d and each` |
|     - |  242 | ` * -c file line). Trims surrounding whitespace and one layer of quotes off` |
|     - |  243 | ` * the value, php.ini style.` |
|     - |  244 | ` */` |
|    16 |  245 | `static void PHL_ApplyIniPair(ph7_vm *pVm,const char *zPair)` |
|   ! 0 |  246 | `{` |
|     - |  247 | `	char zName[128];` |
|     - |  248 | `	char zValue[512];` |
|    16 |  249 | `	const char *zEq = strchr(zPair,'=');` |
|     - |  250 | `	const char *zEnd;` |
|     - |  251 | `	size_t n;` |
|    16 |  252 | `	if( zEq == 0 ){` |
|     - |  253 | `		/* php: a bare -d name defines the entry with value "1" */` |
|     2 |  254 | `		zEq = zPair + strlen(zPair);` |
|     1 |  255 | `	}` |
|     - |  256 | `	/* name: trim */` |
|    16 |  257 | `	while( *zPair == ' ' \|\| *zPair == '\t' ){ zPair++; }` |
|    16 |  258 | `	zEnd = zEq;` |
|    32 |  259 | `	while( zEnd > zPair && (zEnd[-1] == ' ' \|\| zEnd[-1] == '\t') ){ zEnd--; }` |
|    16 |  260 | `	n = (size_t)(zEnd - zPair);` |
|    16 |  261 | `	if( n == 0 \|\| n >= sizeof(zName) ){` |
|   ! 0 |  262 | `		return;` |
|     - |  263 | `	}` |
|    16 |  264 | `	memcpy(zName,zPair,n);` |
|    16 |  265 | `	zName[n] = 0;` |
|     - |  266 | `	/* value: trim + unquote */` |
|    16 |  267 | `	if( *zEq == '=' ){` |
|    14 |  268 | `		const char *zV = zEq + 1;` |
|     - |  269 | `		const char *zVEnd;` |
|    22 |  270 | `		while( *zV == ' ' \|\| *zV == '\t' ){ zV++; }` |
|    14 |  271 | `		zVEnd = zV + strlen(zV);` |
|    29 |  272 | `		while( zVEnd > zV && (zVEnd[-1] == ' ' \|\| zVEnd[-1] == '\t'` |
|    26 |  273 | `		    \|\| zVEnd[-1] == '\r' \|\| zVEnd[-1] == '\n') ){ zVEnd--; }` |
|    14 |  274 | `		if( zVEnd - zV >= 2 && (zV[0] == '"' \|\| zV[0] == '\'') && zVEnd[-1] == zV[0] ){` |
|     4 |  275 | `			zV++;` |
|     4 |  276 | `			zVEnd--;` |
|     2 |  277 | `		}` |
|    14 |  278 | `		n = (size_t)(zVEnd - zV);` |
|    14 |  279 | `		if( n >= sizeof(zValue) ){` |
|   ! 0 |  280 | `			n = sizeof(zValue) - 1;` |
|   ! 0 |  281 | `		}` |
|    14 |  282 | `		memcpy(zValue,zV,n);` |
|    14 |  283 | `		zValue[n] = 0;` |
|     7 |  284 | `	}else{` |
|     - |  285 | `		/* default "on" flag; avoid strcpy (MSVC C4996 under /WX) */` |
|     2 |  286 | `		zValue[0] = '1';` |
|     2 |  287 | `		zValue[1] = 0;` |
|     - |  288 | `	}` |
|    16 |  289 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_INI_ENTRY,zName,zValue);` |
|     8 |  290 | `}` |
|     - |  291 | `/*` |
|     - |  292 | ` * Load php.ini directives from a -c file: name=value lines; [sections],` |
|     - |  293 | ` * empty lines and ;/# comments are ignored (enough of php's ini grammar` |
|     - |  294 | ` * for CLI configuration).` |
|     - |  295 | ` */` |
|     4 |  296 | `static void PHL_LoadIniFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 |  297 | `{` |
|     - |  298 | `	char zLine[768];` |
|     4 |  299 | `	FILE *pFile = fopen(zPath,"r");` |
|     4 |  300 | `	if( pFile == 0 ){` |
|   ! 0 |  301 | `		fprintf(stderr,"Could not open php.ini file: %s\n",zPath);` |
|   ! 0 |  302 | `		return;` |
|     - |  303 | `	}` |
|    20 |  304 | `	while( fgets(zLine,sizeof(zLine),pFile) ){` |
|    16 |  305 | `		const char *z = zLine;` |
|    16 |  306 | `		while( *z == ' ' \|\| *z == '\t' ){ z++; }` |
|    16 |  307 | `		if( *z == 0 \|\| *z == ';' \|\| *z == '#' \|\| *z == '[' \|\| *z == '\n' \|\| *z == '\r' ){` |
|     8 |  308 | `			continue;` |
|     - |  309 | `		}` |
|     8 |  310 | `		PHL_ApplyIniPair(pVm,z);` |
|   ! 0 |  311 | `	}` |
|     4 |  312 | `	fclose(pFile);` |
|     2 |  313 | `}` |
|     - |  314 | `/*` |
|     - |  315 | ` * Main program: Compile and execute the PHP file.` |
|     - |  316 | ` */` |
|  4051 |  317 | `int main(int argc,char **argv)` |
|     5 |  318 | `{` |
|     - |  319 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  320 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  4056 |  321 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  4056 |  322 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  4056 |  323 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  4056 |  324 | `	const char *zRunCode = 0; /* Inline code string */` |
|     - |  325 | `#ifdef PHL_ENABLE_SERVER` |
|  4056 |  326 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  4056 |  327 | `	const char *zServerAddr = 0; /* host:port string */` |
|  4056 |  328 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  329 | `#endif` |
|     - |  330 | `	int n;              /* Script arguments */` |
|     - |  331 | `	int rc;` |
|     - |  332 | `	const char *azIniDefine[64]; /* -d name=value directives, in order */` |
|  4056 |  333 | `	int nIniDefine = 0;` |
|  4056 |  334 | `	const char *zIniFile = 0;    /* -c php.ini path */` |
|     - |  335 | `	/* Process interpreter arguments first*/` |
|  4153 |  336 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  337 | `		int c;` |
|  3903 |  338 | `		if( argv[n][0] != '-' ){` |
|     - |  339 | `			/* No more interpreter arguments */` |
|  3801 |  340 | `			break;` |
|     - |  341 | `		}` |
|     - |  342 | `		/* Check for long options */` |
|   106 |  343 | `		if( argv[n][1] == '-' ){` |
|    16 |  344 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  345 | `				Version();` |
|    12 |  346 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  347 | `				Help();` |
|     8 |  348 | `			}else if( strcmp(argv[n], "--rf") == 0 \|\| strcmp(argv[n], "--rc") == 0 ){` |
|     - |  349 | ``				/* php CLI parity: `--rf <function>` / `--rc <class>` print the`` |
|     - |  350 | `				 * Reflection export (the __toString machinery is byte-exact vs` |
|     - |  351 | ``				 * php) and exit 1 with `Exception: <message>` when the target`` |
|     - |  352 | `				 * does not exist. Implemented as an inline snippet riding the` |
|     - |  353 | `				 * -r code path; the NAME is charset-validated (identifier +` |
|     - |  354 | `				 * namespace separators) so it embeds safely in the snippet. */` |
|     - |  355 | `				static char zReflCode[768];` |
|     7 |  356 | `				const char *zWhat = (argv[n][3] == 'f') ? "ReflectionFunction" : "ReflectionClass";` |
|     - |  357 | `				const char *zName;` |
|     - |  358 | `				const char *zChk;` |
|     7 |  359 | `				if( n + 1 >= argc ){` |
|   ! 0 |  360 | `					FatalCode("Missing name argument for --rf/--rc",1);` |
|   ! 0 |  361 | `				}` |
|     7 |  362 | `				zName = argv[++n];` |
|    71 |  363 | `				for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  364 | `					char ch = *zChk;` |
|    65 |  365 | `					if( !(ch == '_' \|\| ch == '\\'` |
|    58 |  366 | `					   \|\| (ch >= 'a' && ch <= 'z') \|\| (ch >= 'A' && ch <= 'Z')` |
|     4 |  367 | `					   \|\| (ch >= '0' && ch <= '9')) ){` |
|   ! 0 |  368 | `						FatalCode("Invalid name for --rf/--rc",1);` |
|   ! 0 |  369 | `					}` |
|    33 |  370 | `				}` |
|     7 |  371 | `				if( strlen(zName) > 250 ){` |
|   ! 0 |  372 | `					FatalCode("Name too long for --rf/--rc",1);` |
|   ! 0 |  373 | `				}` |
|     - |  374 | `				{` |
|     - |  375 | `					/* Double the namespace separators: inside the snippet's` |
|     - |  376 | `					 * double-quoted string a lone backslash could form an` |
|     - |  377 | `					 * escape sequence ("App\name" -> newline). */` |
|     - |  378 | `					static char zEsc[512];` |
|     7 |  379 | `					char *pOut = zEsc;` |
|    71 |  380 | `					for( zChk = zName ; *zChk ; zChk++ ){` |
|    65 |  381 | `						if( *zChk == '\\' ){` |
|   ! 0 |  382 | `							*pOut++ = '\\';` |
|   ! 0 |  383 | `						}` |
|    65 |  384 | `						*pOut++ = *zChk;` |
|    33 |  385 | `					}` |
|     7 |  386 | `					*pOut = 0;` |
|     7 |  387 | `					snprintf(zReflCode,sizeof(zReflCode),` |
|     - |  388 | `						"try { echo new %s(\"%s\"), \"\\n\"; } catch (Throwable $e) { echo \"Exception: \", $e->getMessage(), \"\\n\"; exit(1); }",` |
|     - |  389 | `						zWhat,zEsc);` |
|     - |  390 | `				}` |
|     7 |  391 | `				zRunCode = zReflCode;` |
|     7 |  392 | `				run_code = 1;` |
|     4 |  393 | `			}else{` |
|     - |  394 | `				/* Unknown long option */` |
|   ! 0 |  395 | `				Help();` |
|     - |  396 | `			}` |
|    11 |  397 | `			continue;` |
|     - |  398 | `		}` |
|    90 |  399 | `		c = argv[n][1];` |
|    90 |  400 | `		if( c == 'b' ){` |
|     - |  401 | `			/* Dump byte-code instructions */` |
|     3 |  402 | `			dump_vm = 1;` |
|    89 |  403 | `		}else if( c == 'l' ){` |
|     - |  404 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  405 | `			lint_mode = 1;` |
|    86 |  406 | `		}else if( c == 'i' ){` |
|     - |  407 | `			/* Display interpreter information and exit */` |
|     2 |  408 | `			Info();` |
|    83 |  409 | `		}else if( c == 'r' ){` |
|     - |  410 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    18 |  411 | `			if( n + 1 >= argc ){` |
|     - |  412 | `				/* Missing code argument */` |
|   ! 0 |  413 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  414 | `			}` |
|    18 |  415 | `			zRunCode = argv[++n];` |
|    18 |  416 | `			run_code = 1;` |
|    72 |  417 | `		}else if( c == 'S' ){` |
|     - |  418 | `			/* Start built-in development server */` |
|     - |  419 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  420 | `			if( n + 1 >= argc ){` |
|   ! 0 |  421 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  422 | `			}` |
|    26 |  423 | `			zServerAddr = argv[++n];` |
|    26 |  424 | `			server_mode = 1;` |
|     - |  425 | `#else` |
|     - |  426 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  427 | `#endif` |
|    51 |  428 | `		}else if( c == 't' ){` |
|     - |  429 | `			/* Set document root for the server */` |
|     - |  430 | `#ifdef PHL_ENABLE_SERVER` |
|    26 |  431 | `			if( n + 1 >= argc ){` |
|   ! 0 |  432 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  433 | `			}` |
|    26 |  434 | `			zDocRoot = argv[++n];` |
|     - |  435 | `#else` |
|     - |  436 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  437 | `#endif` |
|    25 |  438 | `		}else if( c == 'v' ){` |
|     - |  439 | `			/* Display version */` |
|   ! 0 |  440 | `			Version();` |
|    12 |  441 | `		}else if( c == 'd' ){` |
|     - |  442 | `			/* php CLI parity: -d name=value defines a php.ini entry` |
|     - |  443 | `			 * (repeatable; applied to the VM after compile, in order). */` |
|     8 |  444 | `			if( n + 1 >= argc ){` |
|   ! 0 |  445 | `				FatalCode("Missing name=value argument for -d",1);` |
|   ! 0 |  446 | `			}` |
|     8 |  447 | `			if( nIniDefine < (int)(sizeof(azIniDefine)/sizeof(azIniDefine[0])) ){` |
|     8 |  448 | `				azIniDefine[nIniDefine++] = argv[++n];` |
|     4 |  449 | `			}else{` |
|   ! 0 |  450 | `				FatalCode("Too many -d directives",1);` |
|   ! 0 |  451 | `			}` |
|     8 |  452 | `		}else if( c == 'c' ){` |
|     - |  453 | `			/* php CLI parity: -c file loads php.ini directives from a file` |
|     - |  454 | `			 * (name=value lines; [sections] and ;/# comments ignored). */` |
|     4 |  455 | `			if( n + 1 >= argc ){` |
|   ! 0 |  456 | `				FatalCode("Missing file argument for -c",1);` |
|   ! 0 |  457 | `			}` |
|     4 |  458 | `			zIniFile = argv[++n];` |
|     2 |  459 | `		}else{` |
|     - |  460 | `			/* Display a help message and exit */` |
|   ! 0 |  461 | `			Help();` |
|     - |  462 | `		}` |
|    46 |  463 | `	}` |
|     - |  464 | `#ifdef PHL_ENABLE_SERVER` |
|  4051 |  465 | `	if( server_mode ){` |
|     - |  466 | `		/* Parse host:port from zServerAddr */` |
|     - |  467 | `		char zHost[256];` |
|    26 |  468 | `		int iPort = 0;` |
|     - |  469 | `		const char *zColon;` |
|    26 |  470 | `		const char *zRouter = 0;` |
|    26 |  471 | `		zColon = strrchr(zServerAddr, ':');` |
|    26 |  472 | `		if( zColon == 0 ){` |
|   ! 0 |  473 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  474 | `		}` |
|     - |  475 | `		{` |
|    26 |  476 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    26 |  477 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    26 |  478 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    26 |  479 | `			zHost[nHostLen] = 0;` |
|     - |  480 | `		}` |
|    26 |  481 | `		iPort = atoi(zColon + 1);` |
|    26 |  482 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  483 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  484 | `		}` |
|     - |  485 | `		/* Check for optional router script */` |
|    26 |  486 | `		if( n < argc ){` |
|   ! 0 |  487 | `			zRouter = argv[n];` |
|   ! 0 |  488 | `		}` |
|    26 |  489 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  490 | `	}` |
|     - |  491 | `#endif` |
|  4025 |  492 | `	if( n >= argc && !run_code ){` |
|   ! 0 |  493 | `		puts("Missing PHP file to compile");` |
|   ! 0 |  494 | `		Help();` |
|   ! 0 |  495 | `	}` |
|     - |  496 |  |
|     - |  497 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  498 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  499 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  500 | `#endif` |
|     - |  501 | `	/* Allocate a new PH7 engine instance */` |
|  4025 |  502 | `	rc = ph7_init(&pEngine);` |
|  4025 |  503 | `	if( rc != PH7_OK ){` |
|     - |  504 | `		/*` |
|     - |  505 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  506 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  507 | `		 */` |
|   ! 0 |  508 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  509 | `	}` |
|     - |  510 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  511 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  512 | `	 */` |
|  4025 |  513 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  514 | `		Output_Consumer, /* Error log consumer */` |
|     - |  515 | `		0 /* NULL: Callback Private data */` |
|     - |  516 | `		);` |
|     - |  517 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  518 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  519 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  520 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  521 | `	{` |
|     - |  522 | `		unsigned long uMax;` |
|     - |  523 | `		/* floor: keep above the pool bucket size; clamp: nMaxRequest is 32-bit */` |
|  4025 |  524 | `		if( PHL_EnvULong("PHL_MAX_ALLOC",65536UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  525 | `			ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  526 | `		}` |
|     - |  527 | `	}` |
|     - |  528 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  529 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  530 | `	{` |
|     - |  531 | `		unsigned long uMax;` |
|  4025 |  532 | `		if( PHL_EnvULong("PHL_MAX_INPUT",1UL,0xFFFFFFFFUL,&uMax) ){` |
|   ! 0 |  533 | `			ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  534 | `		}` |
|     - |  535 | `	}` |
|     - |  536 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  537 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  538 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  539 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  4025 |  540 | `	if( lint_mode ){` |
|     - |  541 | `		const char *zFile;` |
|     4 |  542 | `		if( n >= argc ){` |
|     - |  543 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  544 | `			ph7_release(pEngine);` |
|   ! 0 |  545 | `			puts("No input file specified");` |
|   ! 0 |  546 | `			return 255;` |
|     - |  547 | `		}` |
|     4 |  548 | `		zFile = argv[n];` |
|     4 |  549 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  550 | `		if( rc == PH7_OK ){` |
|     2 |  551 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  552 | `			ph7_vm_release(pVm);` |
|     3 |  553 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  554 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  555 | `		}else{` |
|     2 |  556 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  557 | `		}` |
|     4 |  558 | `		ph7_release(pEngine);` |
|     4 |  559 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  560 | `	}` |
|     - |  561 | `	/* Now,it's time to compile our PHP file */` |
|  4021 |  562 | `	if( run_code ){` |
|     - |  563 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|   229 |  564 | `		rc = ph7_compile_v2(` |
|   215 |  565 | `			pEngine, /* PH7 Engine */` |
|   215 |  566 | `			zRunCode, /* Source code */` |
|     - |  567 | `			-1,       /* Let API compute length */` |
|     - |  568 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  569 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  570 | `			);` |
|   229 |  571 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  572 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  573 | `				Fatal("VM initialization error");` |
|   ! 0 |  574 | `			}else{` |
|     - |  575 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  576 | `				 * error consumer; php adds nothing else, it just exits 255. */` |
|   ! 0 |  577 | `				FatalSilent();` |
|     - |  578 | `			}` |
|   ! 0 |  579 | `		}` |
|   218 |  580 | `	}else{` |
|  3795 |  581 | `		rc = ph7_compile_file(` |
|  1895 |  582 | `			pEngine, /* PH7 Engine */` |
|  3790 |  583 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  584 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  585 | `			0        /* IN: Compile flags */` |
|     - |  586 | `			);` |
|  3795 |  587 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   456 |  588 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  589 | `				FatalCode("IO error while opening the target file",1);` |
|   456 |  590 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  591 | `				Fatal("VM initialization error");` |
|   ! 0 |  592 | `			}else{` |
|     - |  593 | `				/* Compile-time error. The diagnostic has already been printed by the` |
|     - |  594 | `				 * error consumer; php prints nothing further and exits 255. */` |
|   456 |  595 | `				FatalSilent();` |
|     - |  596 | `			}` |
|   226 |  597 | `		}` |
|     - |  598 | `	}` |
|     - |  599 | `	/*` |
|     - |  600 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  601 | `	 * We will install the VM output consumer callback defined above` |
|     - |  602 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  603 | `	 */` |
|  3795 |  604 | `	rc = ph7_vm_config(pVm,` |
|     - |  605 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  606 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  607 | `		0                   /* Callback private data */` |
|     - |  608 | `		);` |
|  3795 |  609 | `	if( rc != PH7_OK ){` |
|   ! 0 |  610 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  611 | `	}` |
|     - |  612 | `	/* Optional recursion caps via the environment (like PHL_MAX_ALLOC). The host` |
|     - |  613 | `	 * defaults are PHP-parity — PHP call depth is UNBOUNDED (heap-bound) and only` |
|     - |  614 | `	 * the native VmByteCodeExec nesting is capped — so these knobs are for tests` |
|     - |  615 | `	 * and embedders that want a tighter bound, not to raise a low default.` |
|     - |  616 | `	 *   PHL_MAX_RECURSION   -> PH7_VM_CONFIG_RECURSION_DEPTH (PHP call depth; any` |
|     - |  617 | `	 *                          positive value is a cap, PHL_EnvULong rejects 0)` |
|     - |  618 | `	 *   PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH   (native nesting;` |
|     - |  619 | `	 *                          floor 2) */` |
|     - |  620 | `	{` |
|     - |  621 | `		unsigned long uMax;` |
|  3365 |  622 | `		if( PHL_EnvULong("PHL_MAX_RECURSION",1UL,0x7FFFFFFFUL,&uMax) ){` |
|     5 |  623 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_RECURSION_DEPTH,(int)uMax);` |
|     2 |  624 | `		}` |
|  3365 |  625 | `		if( PHL_EnvULong("PHL_MAX_NATIVE_DEPTH",2UL,0x7FFFFFFFUL,&uMax) ){` |
|    12 |  626 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_NATIVE_DEPTH,(int)uMax);` |
|     5 |  627 | `		}` |
|     - |  628 | `	}` |
|     - |  629 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  5045 |  630 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3360 |  631 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  632 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  633 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  634 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  635 | `	 * script's own arguments.` |
|     - |  636 | `	 */` |
|     - |  637 | `	{` |
|  3365 |  638 | `		const char *zScriptName = run_code ? "Standard input code" : argv[n];` |
|  3365 |  639 | `		int argv_count = 0;` |
|     - |  640 | `		ph7_value *pArgc;` |
|     - |  641 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  642 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  643 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3365 |  644 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3365 |  645 | `			argv_count++;` |
|  1680 |  646 | `		}` |
|     - |  647 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  648 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3395 |  649 | `		for( n = run_code ? n : n + 1; n < argc ; ++n ){` |
|    35 |  650 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    33 |  651 | `				argv_count++;` |
|    14 |  652 | `			}` |
|    20 |  653 | `		}` |
|     - |  654 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3365 |  655 | `		pArgc = ph7_new_scalar(pVm);` |
|  3365 |  656 | `		if( pArgc ){` |
|  3365 |  657 | `			ph7_value_int(pArgc,argv_count);` |
|  3365 |  658 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3365 |  659 | `			ph7_release_value(pVm,pArgc);` |
|  1680 |  660 | `		}` |
|     - |  661 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  662 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3365 |  663 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3365 |  664 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3365 |  665 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  666 | `		{` |
|     - |  667 | `			char zTime[32];` |
|  3365 |  668 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3365 |  669 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  670 | `		}` |
|     - |  671 | `#ifndef __WINNT__` |
|     - |  672 | `		{` |
|     - |  673 | `			char zCwd[PATH_MAX];` |
|  3360 |  674 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3360 |  675 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1680 |  676 | `			}` |
|     - |  677 | `		}` |
|     - |  678 | `#endif` |
|     - |  679 | `	}` |
|     - |  680 | `	/* Report script run-time errors (now default behavior) */` |
|  3365 |  681 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|     - |  682 | `	/* Apply php.ini directives AFTER the error-report default so` |
|     - |  683 | ``	 * `-d error_reporting=0` can lower it: the -c file first, then -d`` |
|     - |  684 | `	 * overrides in CLI order (php's precedence). */` |
|  3365 |  685 | `	if( zIniFile ){` |
|     4 |  686 | `		PHL_LoadIniFile(pVm,zIniFile);` |
|     2 |  687 | `	}` |
|     - |  688 | `	{` |
|     - |  689 | `		int i;` |
|  3373 |  690 | `		for( i = 0 ; i < nIniDefine ; i++ ){` |
|     8 |  691 | `			PHL_ApplyIniPair(pVm,azIniDefine[i]);` |
|     4 |  692 | `		}` |
|     - |  693 | `	}` |
|  3365 |  694 | `	if( dump_vm ){` |
|     - |  695 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  696 | `		ph7_vm_dump_v2(pVm,` |
|     - |  697 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  698 | `			0` |
|     - |  699 | `			);` |
|     1 |  700 | `	}` |
|     - |  701 | `	/*` |
|     - |  702 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  703 | `	 * should display the result.` |
|     - |  704 | `	 */` |
|     - |  705 | `	{` |
|  3365 |  706 | `		int iExitStatus = 0;` |
|  3365 |  707 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  708 | `		/* All done, cleanup the mess left behind.` |
|     - |  709 | `		*/` |
|  3365 |  710 | `		ph7_vm_release(pVm);` |
|  3365 |  711 | `		ph7_release(pEngine);` |
|     - |  712 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3365 |  713 | `		return iExitStatus;` |
|     - |  714 | `	}` |
|  1700 |  715 | `}` |
|     - |  716 |  |
