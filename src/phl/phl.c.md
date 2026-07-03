# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 180/243 lines (74.07%)

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
|     - |   33 | `/* Make sure this header file is available.*/` |
|     - |   34 | `#include "ph7.h"` |
|     - |   35 | `#ifdef PHL_ENABLE_SERVER` |
|     - |   36 | `#include "server.h"` |
|     - |   37 | `#endif` |
|     - |   38 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |   39 | `#define MINIDUMP_IMPLEMENTATION` |
|     - |   40 | `#include "minidump.h"` |
|     - |   41 | `#endif` |
|     - |   42 | `/*` |
|     - |   43 | ` * Display an error message and exit.` |
|     - |   44 | ` */` |
|   372 |   45 | `static void FatalCode(const char *zMsg,int iCode)` |
|     4 |   46 | `{` |
|   376 |   47 | `	puts(zMsg);` |
|     - |   48 | `	/* Shutdown the library */` |
|   376 |   49 | `	ph7_lib_shutdown();` |
|     - |   50 | `	/* Exit immediately */` |
|   376 |   51 | `	exit(iCode);` |
|   ! 0 |   52 | `}` |
|     - |   53 | `/*` |
|     - |   54 | ` * php-parity default: fatal engine/compile failures exit 255 (php exits 255` |
|     - |   55 | ` * on a fatal compile error); usage and IO errors use FatalCode(msg, 1)` |
|     - |   56 | ` * directly, mirroring php's exit 1 for bad invocations / unopenable input.` |
|     - |   57 | ` */` |
|   372 |   58 | `static void Fatal(const char *zMsg)` |
|     4 |   59 | `{` |
|   376 |   60 | `	FatalCode(zMsg,255);` |
|   186 |   61 | `}` |
|     - |   62 | `/*` |
|     - |   63 | ` * Display the banner,a help message and exit.` |
|     - |   64 | ` */` |
|     2 |   65 | `static void Help(void)` |
|     1 |   66 | `{` |
|     3 |   67 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code] path/to/php_file [script args]");` |
|     - |   68 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   69 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   70 | `#endif` |
|     3 |   71 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     3 |   72 | `	puts("\t-i: Display interpreter information and exit");` |
|     3 |   73 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     3 |   74 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   75 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   76 | `	puts("\t-S host:port: Start the built-in development server");` |
|     3 |   77 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   78 | `#endif` |
|     3 |   79 | `	puts("\t-v, --version: Display version information and exit");` |
|     3 |   80 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   81 | `	/* Exit immediately */` |
|     3 |   82 | `	exit(0);` |
|   ! 0 |   83 | `}` |
|     - |   84 | `/*` |
|     - |   85 | ` * Display version information and exit.` |
|     - |   86 | ` */` |
|     6 |   87 | `static void Version(void)` |
|     1 |   88 | `{` |
|     7 |   89 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |   90 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |   91 | `	/* Exit immediately */` |
|     7 |   92 | `	exit(0);` |
|   ! 0 |   93 | `}` |
|     - |   94 | `/*` |
|     - |   95 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |   96 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |   97 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |   98 | ` */` |
|     2 |   99 | `static void Info(void)` |
|   ! 0 |  100 | `{` |
|     2 |  101 | `	printf("phpinfo()\n");` |
|     2 |  102 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |  103 | `	printf("System => %s\n",` |
|     - |  104 | `#ifdef __WINNT__` |
|     - |  105 | `		"Windows NT"` |
|     - |  106 | `#elif defined(__UNIXES__)` |
|     - |  107 | `		"UNIX-Like"` |
|     - |  108 | `#else` |
|     - |  109 | `		"Other OS"` |
|     - |  110 | `#endif` |
|     - |  111 | `	);` |
|     2 |  112 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  113 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  114 | `	printf("PHP SAPI => cli\n");` |
|     - |  115 | `	/* Exit immediately */` |
|     2 |  116 | `	exit(0);` |
|   ! 0 |  117 | `}` |
|     - |  118 | `#ifdef __WINNT__` |
|     - |  119 | `#include <Windows.h>` |
|     - |  120 | `#else` |
|     - |  121 | `/* Assume UNIX */` |
|     - |  122 | `#include <unistd.h>` |
|     - |  123 | `#include <limits.h>` |
|     - |  124 | `#endif` |
|     - |  125 | `/*` |
|     - |  126 | ` * The following define is used by the UNIX built and have` |
|     - |  127 | ` * no particular meaning on windows.` |
|     - |  128 | ` */` |
|     - |  129 | `#ifndef STDOUT_FILENO` |
|     - |  130 | `#define STDOUT_FILENO	1` |
|     - |  131 | `#endif` |
|     - |  132 | `#ifndef PATH_MAX` |
|     - |  133 | `#define PATH_MAX 4096` |
|     - |  134 | `#endif` |
|     - |  135 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  136 | `/*` |
|     - |  137 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  138 | ` * pUserData points to the resolved binary path.` |
|     - |  139 | ` */` |
|     2 |  140 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  141 | `{` |
|     3 |  142 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|     3 |  143 | `}` |
|     - |  144 | `/*` |
|     - |  145 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  146 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  147 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  148 | ` */` |
|  3410 |  149 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  150 | `{` |
|     - |  151 | `#ifdef __WINNT__` |
|     5 |  152 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  153 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  154 | `		return zPhlBinaryPath;` |
|     - |  155 | `	}` |
|     - |  156 | `#else` |
|  3410 |  157 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3410 |  158 | `		return zPhlBinaryPath;` |
|     - |  159 | `	}` |
|     - |  160 | `#endif` |
|   ! 0 |  161 | `	return zArgv0;` |
|  1710 |  162 | `}` |
|     - |  163 | `/*` |
|     - |  164 | ` * VM output consumer callback.` |
|     - |  165 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  166 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  167 | ` * the generated output.` |
|     - |  168 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  169 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  170 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  171 | ` */` |
| 11392 |  172 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  173 | `{` |
|  5696 |  174 | `	(void)pUserData;` |
|     - |  175 | `#ifdef __WINNT__` |
|     - |  176 | `	BOOL rc;` |
|     5 |  177 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  178 | `	if( !rc ){` |
|     - |  179 | `		/* Abort processing */` |
|   ! 0 |  180 | `		return PH7_ABORT;` |
|     - |  181 | `	}` |
|     - |  182 | `#else` |
|     - |  183 | `	ssize_t nWr;` |
| 11392 |  184 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 11392 |  185 | `	if( nWr < 0 ){` |
|     - |  186 | `		/* Abort processing */` |
|   ! 0 |  187 | `		return PH7_ABORT;` |
|     - |  188 | `	}` |
|     - |  189 | `#endif /* __WINT__ */` |
|     - |  190 | `	/* All done,VM output was redirected to STDOUT */` |
| 11397 |  191 | `	return PH7_OK;` |
|  5701 |  192 | `}` |
|     - |  193 | `/*` |
|     - |  194 | ` * Main program: Compile and execute the PHP file.` |
|     - |  195 | ` */` |
|  3949 |  196 | `int main(int argc,char **argv)` |
|     5 |  197 | `{` |
|     - |  198 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  199 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  3954 |  200 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  3954 |  201 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  3954 |  202 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  3954 |  203 | `	const char *zRunCode = 0; /* Inline code string */` |
|     - |  204 | `#ifdef PHL_ENABLE_SERVER` |
|  3954 |  205 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  3954 |  206 | `	const char *zServerAddr = 0; /* host:port string */` |
|  3954 |  207 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  208 | `#endif` |
|     - |  209 | `	int n;              /* Script arguments */` |
|     - |  210 | `	int rc;` |
|     - |  211 | `	/* Process interpreter arguments first*/` |
|  4017 |  212 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  213 | `		int c;` |
|  3831 |  214 | `		if( argv[n][0] != '-' ){` |
|     - |  215 | `			/* No more interpreter arguments */` |
|  3763 |  216 | `			break;` |
|     - |  217 | `		}` |
|     - |  218 | `		/* Check for long options */` |
|    72 |  219 | `		if( argv[n][1] == '-' ){` |
|    10 |  220 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  221 | `				Version();` |
|     6 |  222 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  223 | `				Help();` |
|     1 |  224 | `			}else{` |
|     - |  225 | `				/* Unknown long option */` |
|   ! 0 |  226 | `				Help();` |
|     - |  227 | `			}` |
|     4 |  228 | `			continue;` |
|     - |  229 | `		}` |
|    62 |  230 | `		c = argv[n][1];` |
|    62 |  231 | `		if( c == 'b' ){` |
|     - |  232 | `			/* Dump byte-code instructions */` |
|     3 |  233 | `			dump_vm = 1;` |
|    61 |  234 | `		}else if( c == 'l' ){` |
|     - |  235 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  236 | `			lint_mode = 1;` |
|    58 |  237 | `		}else if( c == 'i' ){` |
|     - |  238 | `			/* Display interpreter information and exit */` |
|     2 |  239 | `			Info();` |
|    55 |  240 | `		}else if( c == 'r' ){` |
|     - |  241 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    10 |  242 | `			if( n + 1 >= argc ){` |
|     - |  243 | `				/* Missing code argument */` |
|   ! 0 |  244 | `				FatalCode("Missing code argument for -r",1);` |
|   ! 0 |  245 | `			}` |
|    10 |  246 | `			zRunCode = argv[++n];` |
|    10 |  247 | `			run_code = 1;` |
|    48 |  248 | `		}else if( c == 'S' ){` |
|     - |  249 | `			/* Start built-in development server */` |
|     - |  250 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  251 | `			if( n + 1 >= argc ){` |
|   ! 0 |  252 | `				FatalCode("Missing host:port argument for -S",1);` |
|   ! 0 |  253 | `			}` |
|    22 |  254 | `			zServerAddr = argv[++n];` |
|    22 |  255 | `			server_mode = 1;` |
|     - |  256 | `#else` |
|     - |  257 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  258 | `#endif` |
|    33 |  259 | `		}else if( c == 't' ){` |
|     - |  260 | `			/* Set document root for the server */` |
|     - |  261 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  262 | `			if( n + 1 >= argc ){` |
|   ! 0 |  263 | `				FatalCode("Missing docroot argument for -t",1);` |
|   ! 0 |  264 | `			}` |
|    22 |  265 | `			zDocRoot = argv[++n];` |
|     - |  266 | `#else` |
|     - |  267 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  268 | `#endif` |
|    11 |  269 | `		}else if( c == 'v' ){` |
|     - |  270 | `			/* Display version */` |
|   ! 0 |  271 | `			Version();` |
|   ! 0 |  272 | `		}else{` |
|     - |  273 | `			/* Display a help message and exit */` |
|   ! 0 |  274 | `			Help();` |
|     - |  275 | `		}` |
|    32 |  276 | `	}` |
|     - |  277 | `#ifdef PHL_ENABLE_SERVER` |
|  3949 |  278 | `	if( server_mode ){` |
|     - |  279 | `		/* Parse host:port from zServerAddr */` |
|     - |  280 | `		char zHost[256];` |
|    22 |  281 | `		int iPort = 0;` |
|     - |  282 | `		const char *zColon;` |
|    22 |  283 | `		const char *zRouter = 0;` |
|    22 |  284 | `		zColon = strrchr(zServerAddr, ':');` |
|    22 |  285 | `		if( zColon == 0 ){` |
|   ! 0 |  286 | `			FatalCode("Invalid address format. Use host:port (e.g., localhost:8080)",1);` |
|   ! 0 |  287 | `		}` |
|     - |  288 | `		{` |
|    22 |  289 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    22 |  290 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    22 |  291 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    22 |  292 | `			zHost[nHostLen] = 0;` |
|     - |  293 | `		}` |
|    22 |  294 | `		iPort = atoi(zColon + 1);` |
|    22 |  295 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  296 | `			FatalCode("Invalid port number",1);` |
|   ! 0 |  297 | `		}` |
|     - |  298 | `		/* Check for optional router script */` |
|    22 |  299 | `		if( n < argc ){` |
|   ! 0 |  300 | `			zRouter = argv[n];` |
|   ! 0 |  301 | `		}` |
|    22 |  302 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  303 | `	}` |
|     - |  304 | `#endif` |
|  3583 |  305 | `	if( n >= argc && !run_code ){` |
|   ! 0 |  306 | `		puts("Missing PHP file to compile");` |
|   ! 0 |  307 | `		Help();` |
|   ! 0 |  308 | `	}` |
|     - |  309 |  |
|     - |  310 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  311 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  312 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  313 | `#endif` |
|     - |  314 | `	/* Allocate a new PH7 engine instance */` |
|  3583 |  315 | `	rc = ph7_init(&pEngine);` |
|  3583 |  316 | `	if( rc != PH7_OK ){` |
|     - |  317 | `		/*` |
|     - |  318 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  319 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  320 | `		 */` |
|   ! 0 |  321 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  322 | `	}` |
|     - |  323 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  324 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  325 | `	 */` |
|  3583 |  326 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  327 | `		Output_Consumer, /* Error log consumer */` |
|     - |  328 |  |
|     - |  329 | `		);` |
|     - |  330 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  331 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  332 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  333 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  334 | `	{` |
|  3583 |  335 | `		const char *zMaxAlloc = getenv("PHL_MAX_ALLOC");` |
|  3583 |  336 | `		if( zMaxAlloc ){` |
|   ! 0 |  337 | `			unsigned long uMax = strtoul(zMaxAlloc,0,10);` |
|   ! 0 |  338 | `			if( uMax > 0 ){` |
|   ! 0 |  339 | `				if( uMax < 65536UL ){` |
|   ! 0 |  340 | `					uMax = 65536UL; /* floor: keep above the pool bucket size */` |
|   ! 0 |  341 | `				}` |
|   ! 0 |  342 | `				if( uMax > 0xFFFFFFFFUL ){` |
|   ! 0 |  343 | `					uMax = 0xFFFFFFFFUL; /* clamp: nMaxRequest is a 32-bit byte count */` |
|   ! 0 |  344 | `				}` |
|   ! 0 |  345 | `				ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  346 | `			}` |
|   ! 0 |  347 | `		}` |
|     - |  348 | `	}` |
|     - |  349 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  350 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  351 | `	{` |
|  3583 |  352 | `		const char *zMaxInput = getenv("PHL_MAX_INPUT");` |
|  3583 |  353 | `		if( zMaxInput ){` |
|   ! 0 |  354 | `			unsigned long uMax = strtoul(zMaxInput,0,10);` |
|   ! 0 |  355 | `			if( uMax > 0 ){` |
|   ! 0 |  356 | `				if( uMax > 0xFFFFFFFFUL ){` |
|   ! 0 |  357 | `					uMax = 0xFFFFFFFFUL;` |
|   ! 0 |  358 | `				}` |
|   ! 0 |  359 | `				ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  360 | `			}` |
|   ! 0 |  361 | `		}` |
|     - |  362 | `	}` |
|     - |  363 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  364 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  365 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  366 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  3583 |  367 | `	if( lint_mode ){` |
|     - |  368 | `		const char *zFile;` |
|     4 |  369 | `		if( n >= argc ){` |
|     - |  370 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  371 | `			ph7_release(pEngine);` |
|   ! 0 |  372 | `			puts("No input file specified");` |
|   ! 0 |  373 | `			return 255;` |
|     - |  374 | `		}` |
|     4 |  375 | `		zFile = argv[n];` |
|     4 |  376 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  377 | `		if( rc == PH7_OK ){` |
|     2 |  378 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  379 | `			ph7_vm_release(pVm);` |
|     3 |  380 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  381 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  382 | `		}else{` |
|     2 |  383 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  384 | `		}` |
|     4 |  385 | `		ph7_release(pEngine);` |
|     4 |  386 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  387 | `	}` |
|     - |  388 | `	/* Now,it's time to compile our PHP file */` |
|  3579 |  389 | `	if( run_code ){` |
|     - |  390 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    10 |  391 | `		rc = ph7_compile_v2(` |
|     4 |  392 | `			pEngine, /* PH7 Engine */` |
|     4 |  393 | `			zRunCode, /* Source code */` |
|     - |  394 | `			-1,       /* Let API compute length */` |
|     - |  395 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  396 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  397 | `			);` |
|    10 |  398 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  399 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  400 | `				Fatal("VM initialization error");` |
|   ! 0 |  401 | `			}else{` |
|     - |  402 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   ! 0 |  403 | `				Fatal("Compile error");` |
|     - |  404 | `			}` |
|   ! 0 |  405 | `		}` |
|     6 |  406 | `	}else{` |
|  3571 |  407 | `		rc = ph7_compile_file(` |
|  1690 |  408 | `			pEngine, /* PH7 Engine */` |
|  3566 |  409 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  410 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  411 |  |
|     - |  412 | `			);` |
|  3571 |  413 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   376 |  414 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  415 | `				FatalCode("IO error while opening the target file",1);` |
|   376 |  416 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  417 | `				Fatal("VM initialization error");` |
|   ! 0 |  418 | `			}else{` |
|     - |  419 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   376 |  420 | `				Fatal("Compile error");` |
|     - |  421 | `			}` |
|   186 |  422 | `		}` |
|     - |  423 | `	}` |
|     - |  424 | `	/*` |
|     - |  425 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  426 | `	 * We will install the VM output consumer callback defined above` |
|     - |  427 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  428 | `	 */` |
|  3393 |  429 | `	rc = ph7_vm_config(pVm,` |
|     - |  430 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  431 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  432 |  |
|     - |  433 | `		);` |
|  3393 |  434 | `	if( rc != PH7_OK ){` |
|   ! 0 |  435 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  436 | `	}` |
|     - |  437 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  5087 |  438 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3388 |  439 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  440 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  441 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  442 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  443 | `	 * script's own arguments.` |
|     - |  444 | `	 */` |
|     - |  445 | `	{` |
|  3393 |  446 | `		const char *zScriptName = run_code ? "Standard input code" : argv[n];` |
|  3393 |  447 | `		int argv_count = 0;` |
|     - |  448 | `		ph7_value *pArgc;` |
|     - |  449 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  450 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  451 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3393 |  452 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3393 |  453 | `			argv_count++;` |
|  1694 |  454 | `		}` |
|     - |  455 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  456 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3423 |  457 | `		for( n = run_code ? n : n + 1; n < argc ; ++n ){` |
|    35 |  458 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    33 |  459 | `				argv_count++;` |
|    14 |  460 | `			}` |
|    20 |  461 | `		}` |
|     - |  462 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3393 |  463 | `		pArgc = ph7_new_scalar(pVm);` |
|  3393 |  464 | `		if( pArgc ){` |
|  3393 |  465 | `			ph7_value_int(pArgc,argv_count);` |
|  3393 |  466 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3393 |  467 | `			ph7_release_value(pVm,pArgc);` |
|  1694 |  468 | `		}` |
|     - |  469 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  470 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3393 |  471 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3393 |  472 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3393 |  473 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  474 | `		{` |
|     - |  475 | `			char zTime[32];` |
|  3393 |  476 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3393 |  477 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  478 | `		}` |
|     - |  479 | `#ifndef __WINNT__` |
|     - |  480 | `		{` |
|     - |  481 | `			char zCwd[PATH_MAX];` |
|  3388 |  482 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3388 |  483 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1694 |  484 | `			}` |
|     - |  485 | `		}` |
|     - |  486 | `#endif` |
|     - |  487 | `	}` |
|     - |  488 | `	/* Report script run-time errors (now default behavior) */` |
|  3393 |  489 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|  3393 |  490 | `	if( dump_vm ){` |
|     - |  491 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  492 | `		ph7_vm_dump_v2(pVm,` |
|     - |  493 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  494 |  |
|     - |  495 | `			);` |
|     1 |  496 | `	}` |
|     - |  497 | `	/*` |
|     - |  498 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  499 | `	 * should display the result.` |
|     - |  500 | `	 */` |
|     - |  501 | `	{` |
|  3393 |  502 | `		int iExitStatus = 0;` |
|  3393 |  503 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  504 | `		/* All done, cleanup the mess left behind.` |
|     - |  505 | `		*/` |
|  3393 |  506 | `		ph7_vm_release(pVm);` |
|  3393 |  507 | `		ph7_release(pEngine);` |
|     - |  508 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3393 |  509 | `		return iExitStatus;` |
|     - |  510 | `	}` |
|  1712 |  511 | `}` |
|     - |  512 |  |
