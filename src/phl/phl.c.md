# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 176/239 lines (73.64%)

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
|   364 |   45 | `static void Fatal(const char *zMsg)` |
|     4 |   46 | `{` |
|   368 |   47 | `	puts(zMsg);` |
|     - |   48 | `	/* Shutdown the library */` |
|   368 |   49 | `	ph7_lib_shutdown();` |
|     - |   50 | `	/* Exit immediately */` |
|   368 |   51 | `	exit(0);` |
|   ! 0 |   52 | `}` |
|     - |   53 | `/*` |
|     - |   54 | ` * Display the banner,a help message and exit.` |
|     - |   55 | ` */` |
|     2 |   56 | `static void Help(void)` |
|     1 |   57 | `{` |
|     3 |   58 | `	puts("phl [-h\|--help\|-b\|-i\|-l\|-v\|--version\|-r code] path/to/php_file [script args]");` |
|     - |   59 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   60 | `	puts("phl -S host:port [-t docroot] [router.php]");` |
|     - |   61 | `#endif` |
|     3 |   62 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|     3 |   63 | `	puts("\t-i: Display interpreter information and exit");` |
|     3 |   64 | `	puts("\t-l: Syntax-check (lint) the given file and exit");` |
|     3 |   65 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|     - |   66 | `#ifdef PHL_ENABLE_SERVER` |
|     3 |   67 | `	puts("\t-S host:port: Start the built-in development server");` |
|     3 |   68 | `	puts("\t-t docroot: Document root for the server (default: current directory)");` |
|     - |   69 | `#endif` |
|     3 |   70 | `	puts("\t-v, --version: Display version information and exit");` |
|     3 |   71 | `	puts("\t-h, --help: Display this message and exit");` |
|     - |   72 | `	/* Exit immediately */` |
|     3 |   73 | `	exit(0);` |
|   ! 0 |   74 | `}` |
|     - |   75 | `/*` |
|     - |   76 | ` * Display version information and exit.` |
|     - |   77 | ` */` |
|     6 |   78 | `static void Version(void)` |
|     1 |   79 | `{` |
|     7 |   80 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|     7 |   81 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|     - |   82 | `	/* Exit immediately */` |
|     7 |   83 | `	exit(0);` |
|   ! 0 |   84 | `}` |
|     - |   85 | `/*` |
|     - |   86 | ` * Display interpreter information (php -i) and exit. PHP's CLI -i is plain text` |
|     - |   87 | ` * (the phpinfo() builtin emits HTML, suited to the web SAPI), so this prints a` |
|     - |   88 | ` * concise curated subset on the terminal rather than reusing that builtin.` |
|     - |   89 | ` */` |
|     2 |   90 | `static void Info(void)` |
|   ! 0 |   91 | `{` |
|     2 |   92 | `	printf("phpinfo()\n");` |
|     2 |   93 | `	printf("PHP Version => %s\n\n", PHP_COMPAT_VERSION);` |
|     2 |   94 | `	printf("System => %s\n",` |
|     - |   95 | `#ifdef __WINNT__` |
|     - |   96 | `		"Windows NT"` |
|     - |   97 | `#elif defined(__UNIXES__)` |
|     - |   98 | `		"UNIX-Like"` |
|     - |   99 | `#else` |
|     - |  100 | `		"Other OS"` |
|     - |  101 | `#endif` |
|     - |  102 | `	);` |
|     2 |  103 | `	printf("Build Date => %s %s\n", __DATE__, __TIME__);` |
|     2 |  104 | `	printf("PHL Version => %s\n", PH7_VERSION);` |
|     2 |  105 | `	printf("PHP SAPI => cli\n");` |
|     - |  106 | `	/* Exit immediately */` |
|     2 |  107 | `	exit(0);` |
|   ! 0 |  108 | `}` |
|     - |  109 | `#ifdef __WINNT__` |
|     - |  110 | `#include <Windows.h>` |
|     - |  111 | `#else` |
|     - |  112 | `/* Assume UNIX */` |
|     - |  113 | `#include <unistd.h>` |
|     - |  114 | `#include <limits.h>` |
|     - |  115 | `#endif` |
|     - |  116 | `/*` |
|     - |  117 | ` * The following define is used by the UNIX built and have` |
|     - |  118 | ` * no particular meaning on windows.` |
|     - |  119 | ` */` |
|     - |  120 | `#ifndef STDOUT_FILENO` |
|     - |  121 | `#define STDOUT_FILENO	1` |
|     - |  122 | `#endif` |
|     - |  123 | `#ifndef PATH_MAX` |
|     - |  124 | `#define PATH_MAX 4096` |
|     - |  125 | `#endif` |
|     - |  126 | `static char zPhlBinaryPath[PATH_MAX];` |
|     - |  127 | `/*` |
|     - |  128 | ` * Expand callback for the PHP_BINARY constant.` |
|     - |  129 | ` * pUserData points to the resolved binary path.` |
|     - |  130 | ` */` |
|     2 |  131 | `static void PHL_PhpBinaryConst(ph7_value *pVal,void *pUserData)` |
|     1 |  132 | `{` |
|     3 |  133 | `	ph7_value_string(pVal,(const char *)pUserData,-1);` |
|     3 |  134 | `}` |
|     - |  135 | `/*` |
|     - |  136 | ` * Resolve the absolute path of the running interpreter.` |
|     - |  137 | ` * Falls back to argv[0] verbatim (e.g. bare PATH invocation):` |
|     - |  138 | ` * consumers spawning it again go through the shell, which re-resolves it.` |
|     - |  139 | ` */` |
|  3310 |  140 | `static const char * PHL_ResolveBinaryPath(const char *zArgv0)` |
|     5 |  141 | `{` |
|     - |  142 | `#ifdef __WINNT__` |
|     5 |  143 | `	DWORD nLen = GetModuleFileNameA(0,zPhlBinaryPath,(DWORD)sizeof(zPhlBinaryPath));` |
|     5 |  144 | `	if( nLen > 0 && nLen < sizeof(zPhlBinaryPath) ){` |
|     5 |  145 | `		return zPhlBinaryPath;` |
|     - |  146 | `	}` |
|     - |  147 | `#else` |
|  3310 |  148 | `	if( realpath(zArgv0,zPhlBinaryPath) != 0 ){` |
|  3310 |  149 | `		return zPhlBinaryPath;` |
|     - |  150 | `	}` |
|     - |  151 | `#endif` |
|   ! 0 |  152 | `	return zArgv0;` |
|  1660 |  153 | `}` |
|     - |  154 | `/*` |
|     - |  155 | ` * VM output consumer callback.` |
|     - |  156 | ` * Each time the virtual machine generates some outputs,the following` |
|     - |  157 | ` * function gets called by the underlying virtual machine to consume` |
|     - |  158 | ` * the generated output.` |
|     - |  159 | ` * All this function does is redirecting the VM output to STDOUT.` |
|     - |  160 | ` * This function is registered later via a call to ph7_vm_config()` |
|     - |  161 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|     - |  162 | ` */` |
| 10752 |  163 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|     5 |  164 | `{` |
|  5376 |  165 | `	(void)pUserData;` |
|     - |  166 | `#ifdef __WINNT__` |
|     - |  167 | `	BOOL rc;` |
|     5 |  168 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|     5 |  169 | `	if( !rc ){` |
|     - |  170 | `		/* Abort processing */` |
|   ! 0 |  171 | `		return PH7_ABORT;` |
|     - |  172 | `	}` |
|     - |  173 | `#else` |
|     - |  174 | `	ssize_t nWr;` |
| 10752 |  175 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 10752 |  176 | `	if( nWr < 0 ){` |
|     - |  177 | `		/* Abort processing */` |
|   ! 0 |  178 | `		return PH7_ABORT;` |
|     - |  179 | `	}` |
|     - |  180 | `#endif /* __WINT__ */` |
|     - |  181 | `	/* All done,VM output was redirected to STDOUT */` |
| 10757 |  182 | `	return PH7_OK;` |
|  5381 |  183 | `}` |
|     - |  184 | `/*` |
|     - |  185 | ` * Main program: Compile and execute the PHP file.` |
|     - |  186 | ` */` |
|  3837 |  187 | `int main(int argc,char **argv)` |
|     5 |  188 | `{` |
|     - |  189 | `	ph7 *pEngine; /* PH7 engine */` |
|     - |  190 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
|  3842 |  191 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
|  3842 |  192 | `	int run_code = 0;    /* Run inline code if TRUE */` |
|  3842 |  193 | `	int lint_mode = 0;   /* Syntax-check only (-l) if TRUE */` |
|  3842 |  194 | `	const char *zRunCode = 0; /* Inline code string */` |
|     - |  195 | `#ifdef PHL_ENABLE_SERVER` |
|  3842 |  196 | `	int server_mode = 0;        /* Start built-in server if TRUE */` |
|  3842 |  197 | `	const char *zServerAddr = 0; /* host:port string */` |
|  3842 |  198 | `	const char *zDocRoot = ".";  /* Document root */` |
|     - |  199 | `#endif` |
|     - |  200 | `	int n;              /* Script arguments */` |
|     - |  201 | `	int rc;` |
|     - |  202 | `	/* Process interpreter arguments first*/` |
|  3905 |  203 | `	for(n = 1 ; n < argc ; ++n ){` |
|     - |  204 | `		int c;` |
|  3723 |  205 | `		if( argv[n][0] != '-' ){` |
|     - |  206 | `			/* No more interpreter arguments */` |
|  3655 |  207 | `			break;` |
|     - |  208 | `		}` |
|     - |  209 | `		/* Check for long options */` |
|    72 |  210 | `		if( argv[n][1] == '-' ){` |
|    10 |  211 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|     7 |  212 | `				Version();` |
|     6 |  213 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|     3 |  214 | `				Help();` |
|     1 |  215 | `			}else{` |
|     - |  216 | `				/* Unknown long option */` |
|   ! 0 |  217 | `				Help();` |
|     - |  218 | `			}` |
|     4 |  219 | `			continue;` |
|     - |  220 | `		}` |
|    62 |  221 | `		c = argv[n][1];` |
|    62 |  222 | `		if( c == 'b' ){` |
|     - |  223 | `			/* Dump byte-code instructions */` |
|     3 |  224 | `			dump_vm = 1;` |
|    61 |  225 | `		}else if( c == 'l' ){` |
|     - |  226 | `			/* Syntax-check only (lint) the file argument that follows */` |
|     4 |  227 | `			lint_mode = 1;` |
|    58 |  228 | `		}else if( c == 'i' ){` |
|     - |  229 | `			/* Display interpreter information and exit */` |
|     2 |  230 | `			Info();` |
|    55 |  231 | `		}else if( c == 'r' ){` |
|     - |  232 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    10 |  233 | `			if( n + 1 >= argc ){` |
|     - |  234 | `				/* Missing code argument */` |
|   ! 0 |  235 | `				Fatal("Missing code argument for -r");` |
|   ! 0 |  236 | `			}` |
|    10 |  237 | `			zRunCode = argv[++n];` |
|    10 |  238 | `			run_code = 1;` |
|    48 |  239 | `		}else if( c == 'S' ){` |
|     - |  240 | `			/* Start built-in development server */` |
|     - |  241 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  242 | `			if( n + 1 >= argc ){` |
|   ! 0 |  243 | `				Fatal("Missing host:port argument for -S");` |
|   ! 0 |  244 | `			}` |
|    22 |  245 | `			zServerAddr = argv[++n];` |
|    22 |  246 | `			server_mode = 1;` |
|     - |  247 | `#else` |
|     - |  248 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  249 | `#endif` |
|    33 |  250 | `		}else if( c == 't' ){` |
|     - |  251 | `			/* Set document root for the server */` |
|     - |  252 | `#ifdef PHL_ENABLE_SERVER` |
|    22 |  253 | `			if( n + 1 >= argc ){` |
|   ! 0 |  254 | `				Fatal("Missing docroot argument for -t");` |
|   ! 0 |  255 | `			}` |
|    22 |  256 | `			zDocRoot = argv[++n];` |
|     - |  257 | `#else` |
|     - |  258 | `			Fatal("Built-in server not available (compiled without PHL_ENABLE_SERVER)");` |
|     - |  259 | `#endif` |
|    11 |  260 | `		}else if( c == 'v' ){` |
|     - |  261 | `			/* Display version */` |
|   ! 0 |  262 | `			Version();` |
|   ! 0 |  263 | `		}else{` |
|     - |  264 | `			/* Display a help message and exit */` |
|   ! 0 |  265 | `			Help();` |
|     - |  266 | `		}` |
|    32 |  267 | `	}` |
|     - |  268 | `#ifdef PHL_ENABLE_SERVER` |
|  3837 |  269 | `	if( server_mode ){` |
|     - |  270 | `		/* Parse host:port from zServerAddr */` |
|     - |  271 | `		char zHost[256];` |
|    22 |  272 | `		int iPort = 0;` |
|     - |  273 | `		const char *zColon;` |
|    22 |  274 | `		const char *zRouter = 0;` |
|    22 |  275 | `		zColon = strrchr(zServerAddr, ':');` |
|    22 |  276 | `		if( zColon == 0 ){` |
|   ! 0 |  277 | `			Fatal("Invalid address format. Use host:port (e.g., localhost:8080)");` |
|   ! 0 |  278 | `		}` |
|     - |  279 | `		{` |
|    22 |  280 | `			int nHostLen = (int)(zColon - zServerAddr);` |
|    22 |  281 | `			if( nHostLen >= (int)sizeof(zHost) ) nHostLen = (int)sizeof(zHost) - 1;` |
|    22 |  282 | `			memcpy(zHost, zServerAddr, nHostLen);` |
|    22 |  283 | `			zHost[nHostLen] = 0;` |
|     - |  284 | `		}` |
|    22 |  285 | `		iPort = atoi(zColon + 1);` |
|    22 |  286 | `		if( iPort <= 0 \|\| iPort > 65535 ){` |
|   ! 0 |  287 | `			Fatal("Invalid port number");` |
|   ! 0 |  288 | `		}` |
|     - |  289 | `		/* Check for optional router script */` |
|    22 |  290 | `		if( n < argc ){` |
|   ! 0 |  291 | `			zRouter = argv[n];` |
|   ! 0 |  292 | `		}` |
|    22 |  293 | `		return phl_serve(zHost, iPort, zDocRoot, zRouter, PHL_ResolveBinaryPath(argv[0]));` |
|     - |  294 | `	}` |
|     - |  295 | `#endif` |
|  3479 |  296 | `	if( n >= argc && !run_code ){` |
|   ! 0 |  297 | `		puts("Missing PHP file to compile");` |
|   ! 0 |  298 | `		Help();` |
|   ! 0 |  299 | `	}` |
|     - |  300 |  |
|     - |  301 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|     - |  302 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|     5 |  303 | `	CreateMiniDumpOnUnHandledException();` |
|     - |  304 | `#endif` |
|     - |  305 | `	/* Allocate a new PH7 engine instance */` |
|  3479 |  306 | `	rc = ph7_init(&pEngine);` |
|  3479 |  307 | `	if( rc != PH7_OK ){` |
|     - |  308 | `		/*` |
|     - |  309 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|     - |  310 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|     - |  311 | `		 */` |
|   ! 0 |  312 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|   ! 0 |  313 | `	}` |
|     - |  314 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|     - |  315 | `	 * redirect all compile-time error messages to STDOUT.` |
|     - |  316 | `	 */` |
|  3479 |  317 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|     - |  318 | `		Output_Consumer, /* Error log consumer */` |
|     - |  319 |  |
|     - |  320 | `		);` |
|     - |  321 | `	/* Optional per-allocation memory cap (PHL_MAX_ALLOC=bytes). Used to` |
|     - |  322 | `	 * deterministically exercise out-of-memory paths (see tests/ph7/003-stress).` |
|     - |  323 | `	 * Clamp to a floor above the pool bucket size (SXMEM_POOL_MAXALLOC, 32 KB)` |
|     - |  324 | `	 * so the engine can still start; VMs inherit it at creation. */` |
|     - |  325 | `	{` |
|  3479 |  326 | `		const char *zMaxAlloc = getenv("PHL_MAX_ALLOC");` |
|  3479 |  327 | `		if( zMaxAlloc ){` |
|   ! 0 |  328 | `			unsigned long uMax = strtoul(zMaxAlloc,0,10);` |
|   ! 0 |  329 | `			if( uMax > 0 ){` |
|   ! 0 |  330 | `				if( uMax < 65536UL ){` |
|   ! 0 |  331 | `					uMax = 65536UL; /* floor: keep above the pool bucket size */` |
|   ! 0 |  332 | `				}` |
|   ! 0 |  333 | `				if( uMax > 0xFFFFFFFFUL ){` |
|   ! 0 |  334 | `					uMax = 0xFFFFFFFFUL; /* clamp: nMaxRequest is a 32-bit byte count */` |
|   ! 0 |  335 | `				}` |
|   ! 0 |  336 | `				ph7_config(pEngine,PH7_CONFIG_MAX_ALLOC,(unsigned int)uMax);` |
|   ! 0 |  337 | `			}` |
|   ! 0 |  338 | `		}` |
|     - |  339 | `	}` |
|     - |  340 | `	/* Optional per-input byte cap (PHL_MAX_INPUT=bytes). Used to exercise the` |
|     - |  341 | `	 * input-size rejection path at a manageable scale (see tests/ph7/003-stress). */` |
|     - |  342 | `	{` |
|  3479 |  343 | `		const char *zMaxInput = getenv("PHL_MAX_INPUT");` |
|  3479 |  344 | `		if( zMaxInput ){` |
|   ! 0 |  345 | `			unsigned long uMax = strtoul(zMaxInput,0,10);` |
|   ! 0 |  346 | `			if( uMax > 0 ){` |
|   ! 0 |  347 | `				if( uMax > 0xFFFFFFFFUL ){` |
|   ! 0 |  348 | `					uMax = 0xFFFFFFFFUL;` |
|   ! 0 |  349 | `				}` |
|   ! 0 |  350 | `				ph7_config(pEngine,PH7_CONFIG_MAX_INPUT,(unsigned int)uMax);` |
|   ! 0 |  351 | `			}` |
|   ! 0 |  352 | `		}` |
|     - |  353 | `	}` |
|     - |  354 | `	/* Syntax-check only mode (-l): compile the target file, print PHP's summary` |
|     - |  355 | `	 * line and exit without executing. The error consumer installed above` |
|     - |  356 | `	 * already prints any parse error; ph7_compile_file leaves *pVm NULL on a` |
|     - |  357 | `	 * compile/IO error, so only a successful compile owns a VM to release. */` |
|  3479 |  358 | `	if( lint_mode ){` |
|     - |  359 | `		const char *zFile;` |
|     4 |  360 | `		if( n >= argc ){` |
|     - |  361 | ``			/* No file argument (e.g. `-l` alone, or `-l` mixed with `-r`). */`` |
|   ! 0 |  362 | `			ph7_release(pEngine);` |
|   ! 0 |  363 | `			puts("No input file specified");` |
|   ! 0 |  364 | `			return 255;` |
|     - |  365 | `		}` |
|     4 |  366 | `		zFile = argv[n];` |
|     4 |  367 | `		rc = ph7_compile_file(pEngine,zFile,&pVm,0);` |
|     4 |  368 | `		if( rc == PH7_OK ){` |
|     2 |  369 | `			printf("No syntax errors detected in %s\n",zFile);` |
|     2 |  370 | `			ph7_vm_release(pVm);` |
|     3 |  371 | `		}else if( rc == PH7_IO_ERR ){` |
|   ! 0 |  372 | `			printf("Could not open input file: %s\n",zFile);` |
|   ! 0 |  373 | `		}else{` |
|     2 |  374 | `			printf("Errors parsing %s\n",zFile);` |
|     - |  375 | `		}` |
|     4 |  376 | `		ph7_release(pEngine);` |
|     4 |  377 | `		return (rc == PH7_OK) ? 0 : 255;` |
|     - |  378 | `	}` |
|     - |  379 | `	/* Now,it's time to compile our PHP file */` |
|  3475 |  380 | `	if( run_code ){` |
|     - |  381 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    10 |  382 | `		rc = ph7_compile_v2(` |
|     4 |  383 | `			pEngine, /* PH7 Engine */` |
|     4 |  384 | `			zRunCode, /* Source code */` |
|     - |  385 | `			-1,       /* Let API compute length */` |
|     - |  386 | `			&pVm,     /* OUT: Compiled PHP program */` |
|     - |  387 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|     - |  388 | `			);` |
|    10 |  389 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   ! 0 |  390 | `			if( rc == PH7_VM_ERR ){` |
|   ! 0 |  391 | `				Fatal("VM initialization error");` |
|   ! 0 |  392 | `			}else{` |
|     - |  393 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   ! 0 |  394 | `				Fatal("Compile error");` |
|     - |  395 | `			}` |
|   ! 0 |  396 | `		}` |
|     6 |  397 | `	}else{` |
|  3467 |  398 | `		rc = ph7_compile_file(` |
|  1640 |  399 | `			pEngine, /* PH7 Engine */` |
|  3462 |  400 | `			argv[n], /* Path to the PHP file to compile */` |
|     - |  401 | `			&pVm,    /* OUT: Compiled PHP program */` |
|     - |  402 |  |
|     - |  403 | `			);` |
|  3467 |  404 | `		if( rc != PH7_OK ){ /* Compile error */` |
|   368 |  405 | `			if( rc == PH7_IO_ERR ){` |
|   ! 0 |  406 | `				Fatal("IO error while opening the target file");` |
|   368 |  407 | `			}else if( rc == PH7_VM_ERR ){` |
|   ! 0 |  408 | `				Fatal("VM initialization error");` |
|   ! 0 |  409 | `			}else{` |
|     - |  410 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|   368 |  411 | `				Fatal("Compile error");` |
|     - |  412 | `			}` |
|   182 |  413 | `		}` |
|     - |  414 | `	}` |
|     - |  415 | `	/*` |
|     - |  416 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|     - |  417 | `	 * We will install the VM output consumer callback defined above` |
|     - |  418 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|     - |  419 | `	 */` |
|  3293 |  420 | `	rc = ph7_vm_config(pVm,` |
|     - |  421 | `		PH7_VM_CONFIG_OUTPUT,` |
|     - |  422 | `		Output_Consumer,    /* Output Consumer callback */` |
|     - |  423 |  |
|     - |  424 | `		);` |
|  3293 |  425 | `	if( rc != PH7_OK ){` |
|   ! 0 |  426 | `		Fatal("Error while installing the VM output consumer callback");` |
|   ! 0 |  427 | `	}` |
|     - |  428 | `	/* Define PHP_BINARY: absolute path of this interpreter */` |
|  4937 |  429 | `	ph7_create_constant(pVm,"PHP_BINARY",PHL_PhpBinaryConst,` |
|  3288 |  430 | `		(void *)PHL_ResolveBinaryPath(argv[0]));` |
|     - |  431 | `	/* Register the script arguments as $argv[] plus the matching $argc count and` |
|     - |  432 | `	 * the CLI $_SERVER entries, matching PHP: $argv[0] is the script path (file` |
|     - |  433 | `	 * mode) or the literal "Standard input code" (-r mode), followed by the` |
|     - |  434 | `	 * script's own arguments.` |
|     - |  435 | `	 */` |
|     - |  436 | `	{` |
|  3293 |  437 | `		const char *zScriptName = run_code ? "Standard input code" : argv[n];` |
|  3293 |  438 | `		int argv_count = 0;` |
|     - |  439 | `		ph7_value *pArgc;` |
|     - |  440 | `		/* Count only the entries actually inserted: PH7_VM_CONFIG_ARGV_ENTRY skips` |
|     - |  441 | `		 * an empty string, so counting unconditionally would leave $argc greater` |
|     - |  442 | ``		 * than count($argv) for an empty argument (e.g. `phl s.php "" x`). */`` |
|  3293 |  443 | `		if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,zScriptName) == PH7_OK ){` |
|  3293 |  444 | `			argv_count++;` |
|  1644 |  445 | `		}` |
|     - |  446 | `		/* The script's own arguments follow: in file mode argv[n] is the script` |
|     - |  447 | `		 * (registered above), so they start at n+1; in -r mode they start at n. */` |
|  3323 |  448 | `		for( n = run_code ? n : n + 1; n < argc ; ++n ){` |
|    35 |  449 | `			if( ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]) == PH7_OK ){` |
|    33 |  450 | `				argv_count++;` |
|    14 |  451 | `			}` |
|    20 |  452 | `		}` |
|     - |  453 | `		/* $argc: a plain integer global equal to count($argv). */` |
|  3293 |  454 | `		pArgc = ph7_new_scalar(pVm);` |
|  3293 |  455 | `		if( pArgc ){` |
|  3293 |  456 | `			ph7_value_int(pArgc,argv_count);` |
|  3293 |  457 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_CREATE_VAR,"argc",pArgc);` |
|  3293 |  458 | `			ph7_release_value(pVm,pArgc);` |
|  1644 |  459 | `		}` |
|     - |  460 | `		/* $_SERVER entries frameworks read at CLI bootstrap. SCRIPT_FILENAME is` |
|     - |  461 | `		 * already set to the script path by PH7_HashmapCreateSuper. */` |
|  3293 |  462 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"SCRIPT_NAME",zScriptName,-1);` |
|  3293 |  463 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PHP_SELF",zScriptName,-1);` |
|  3293 |  464 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"DOCUMENT_ROOT","",0);` |
|     - |  465 | `		{` |
|     - |  466 | `			char zTime[32];` |
|  3293 |  467 | `			snprintf(zTime,sizeof(zTime),"%ld",(long)time(0));` |
|  3293 |  468 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"REQUEST_TIME",zTime,-1);` |
|     - |  469 | `		}` |
|     - |  470 | `#ifndef __WINNT__` |
|     - |  471 | `		{` |
|     - |  472 | `			char zCwd[PATH_MAX];` |
|  3288 |  473 | `			if( getcwd(zCwd,sizeof(zCwd)) ){` |
|  3288 |  474 | `				ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,"PWD",zCwd,-1);` |
|  1644 |  475 | `			}` |
|     - |  476 | `		}` |
|     - |  477 | `#endif` |
|     - |  478 | `	}` |
|     - |  479 | `	/* Report script run-time errors (now default behavior) */` |
|  3293 |  480 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
|  3293 |  481 | `	if( dump_vm ){` |
|     - |  482 | `		/* Dump PH7 byte-code instructions */` |
|     3 |  483 | `		ph7_vm_dump_v2(pVm,` |
|     - |  484 | `			Output_Consumer, /* Dump consumer callback */` |
|     - |  485 |  |
|     - |  486 | `			);` |
|     1 |  487 | `	}` |
|     - |  488 | `	/*` |
|     - |  489 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|     - |  490 | `	 * should display the result.` |
|     - |  491 | `	 */` |
|     - |  492 | `	{` |
|  3293 |  493 | `		int iExitStatus = 0;` |
|  3293 |  494 | `		ph7_vm_exec(pVm,&iExitStatus);` |
|     - |  495 | `		/* All done, cleanup the mess left behind.` |
|     - |  496 | `		*/` |
|  3293 |  497 | `		ph7_vm_release(pVm);` |
|  3293 |  498 | `		ph7_release(pEngine);` |
|     - |  499 | `		/* Propagate the script exit status (set via exit()/die()) */` |
|  3293 |  500 | `		return iExitStatus;` |
|     - |  501 | `	}` |
|  1662 |  502 | `}` |
|     - |  503 |  |
