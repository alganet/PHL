# src/phl/phl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 89/115 lines (77.39%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `/*` |
|    - |    7 | ` * The PHL interpreter is a simple stand-alone PHP interpreter that allows` |
|    - |    8 | ` * the user to enter and execute PHP files against a PH7 engine.` |
|    - |    9 | ` * To start the phl program, just type "phl" followed by the name of the PHP file` |
|    - |   10 | ` * to compile and execute. That is, the first argument is to the interpreter, the rest` |
|    - |   11 | ` * are scripts arguments, press "Enter" and the PHP code will be executed.` |
|    - |   12 | ` * If something goes wrong while processing the PHP script due to a compile-time error` |
|    - |   13 | ` * your error output (STDOUT) should display the compile-time error messages.` |
|    - |   14 | ` *` |
|    - |   15 | ` * Usage example of the phl interpreter:` |
|    - |   16 | ` *   phl examples/hello_world.php` |
|    - |   17 | ` * Running the interpreter with script arguments` |
|    - |   18 | ` *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s` |
|    - |   19 | ` *` |
|    - |   20 | ` * Command line options:` |
|    - |   21 | ` *   -b: Dump PH7 byte-code instructions` |
|    - |   22 | ` *   -h: Display this help message` |
|    - |   23 | ` *` |
|    - |   24 | ` * The PHL interpreter package includes more than 70 PHP scripts to test ranging from` |
|    - |   25 | ` * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting,` |
|    - |   26 | ` * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many` |
|    - |   27 | ` * more. These scripts are available in the scripts directory from the zip archive.` |
|    - |   28 | ` */` |
|    - |   29 | `#include <stdio.h>` |
|    - |   30 | `#include <stdlib.h>` |
|    - |   31 | `#include <string.h>` |
|    - |   32 | `/* Make sure this header file is available.*/` |
|    - |   33 | `#include "ph7.h"` |
|    - |   34 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|    - |   35 | `#define MINIDUMP_IMPLEMENTATION` |
|    - |   36 | `#include "minidump.h"` |
|    - |   37 | `#endif` |
|    - |   38 | `/*` |
|    - |   39 | ` * Display an error message and exit.` |
|    - |   40 | ` */` |
|  238 |   41 | `static void Fatal(const char *zMsg)` |
|    1 |   42 |  |
|  239 |   43 | `	puts(zMsg);` |
|    - |   44 | `	/* Shutdown the library */` |
|  239 |   45 | `	ph7_lib_shutdown();` |
|    - |   46 | `	/* Exit immediately */` |
|  239 |   47 | `	exit(0);` |
|  ! 0 |   48 |  |
|    - |   49 | `/*` |
|    - |   50 | ` * Display the banner,a help message and exit.` |
|    - |   51 | ` */` |
|    2 |   52 | `static void Help(void)` |
|    1 |   53 |  |
|    3 |   54 | `	puts("phl [-h\|--help\|-b\|-v\|--version\|-r code] path/to/php_file [script args]");` |
|    3 |   55 | `	puts("\t-b: Dump PH7 byte-code instructions");` |
|    3 |   56 | `	puts("\t-r code: Run code from command line (no tags needed)");` |
|    3 |   57 | `	puts("\t-v, --version: Display version information and exit");` |
|    3 |   58 | `	puts("\t-h, --help: Display this message and exit");` |
|    - |   59 | `	/* Exit immediately */` |
|    3 |   60 | `	exit(0);` |
|  ! 0 |   61 |  |
|    - |   62 | `/*` |
|    - |   63 | ` * Display version information and exit.` |
|    - |   64 | ` */` |
|    2 |   65 | `static void Version(void)` |
|    1 |   66 |  |
|    3 |   67 | `	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");` |
|    3 |   68 | `	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");` |
|    - |   69 | `	/* Exit immediately */` |
|    3 |   70 | `	exit(0);` |
|  ! 0 |   71 |  |
|    - |   72 | `#ifdef __WINNT__` |
|    - |   73 | `#include <Windows.h>` |
|    - |   74 | `#else` |
|    - |   75 | `/* Assume UNIX */` |
|    - |   76 | `#include <unistd.h>` |
|    - |   77 | `#endif` |
|    - |   78 | `/*` |
|    - |   79 | ` * The following define is used by the UNIX built and have` |
|    - |   80 | ` * no particular meaning on windows.` |
|    - |   81 | ` */` |
|    - |   82 | `#ifndef STDOUT_FILENO` |
|    - |   83 | `#define STDOUT_FILENO	1` |
|    - |   84 | `#endif` |
|    - |   85 | `/*` |
|    - |   86 | ` * VM output consumer callback.` |
|    - |   87 | ` * Each time the virtual machine generates some outputs,the following` |
|    - |   88 | ` * function gets called by the underlying virtual machine to consume` |
|    - |   89 | ` * the generated output.` |
|    - |   90 | ` * All this function does is redirecting the VM output to STDOUT.` |
|    - |   91 | ` * This function is registered later via a call to ph7_vm_config()` |
|    - |   92 | ` * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.` |
|    - |   93 | ` */` |
| 5274 |   94 | `static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)` |
|    2 |   95 |  |
| 2637 |   96 | `	(void)pUserData;` |
|    - |   97 | `#ifdef __WINNT__` |
|    - |   98 | `	BOOL rc;` |
|    2 |   99 | `	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);` |
|    2 |  100 | `	if( !rc ){` |
|    - |  101 | `		/* Abort processing */` |
|  ! 0 |  102 | `		return PH7_ABORT;` |
|    - |  103 | `	}` |
|    - |  104 | `#else` |
|    - |  105 | `	ssize_t nWr;` |
| 5274 |  106 | `	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);` |
| 5274 |  107 | `	if( nWr < 0 ){` |
|    - |  108 | `		/* Abort processing */` |
|  ! 0 |  109 | `		return PH7_ABORT;` |
|    - |  110 | `	}` |
|    - |  111 | `#endif /* __WINT__ */` |
|    - |  112 | `	/* All done,VM output was redirected to STDOUT */` |
| 5276 |  113 | `	return PH7_OK;` |
| 2639 |  114 |  |
|    - |  115 | `/*` |
|    - |  116 | ` * Main program: Compile and execute the PHP file.` |
|    - |  117 | ` */` |
| 2095 |  118 | `int main(int argc,char **argv)` |
|    2 |  119 |  |
|    - |  120 | `	ph7 *pEngine; /* PH7 engine */` |
|    - |  121 | `	ph7_vm *pVm;  /* Compiled PHP program */` |
| 2097 |  122 | `	int dump_vm = 0;    /* Dump VM instructions if TRUE */` |
| 2097 |  123 | `	int run_code = 0;    /* Run inline code if TRUE */` |
| 2097 |  124 | `	const char *zRunCode = 0; /* Inline code string */` |
|    - |  125 | `	int n;              /* Script arguments */` |
|    - |  126 | `	int rc;` |
|    - |  127 | `	/* Process interpreter arguments first*/` |
| 2109 |  128 | `	for(n = 1 ; n < argc ; ++n ){` |
|    - |  129 | `		int c;` |
| 1990 |  130 | `		if( argv[n][0] != '-' ){` |
|    - |  131 | `			/* No more interpreter arguments */` |
| 1976 |  132 | `			break;` |
|    - |  133 | `		}` |
|    - |  134 | `		/* Check for long options */` |
|   15 |  135 | `		if( argv[n][1] == '-' ){` |
|    5 |  136 | `			if( strcmp(argv[n], "--version") == 0 ){` |
|    3 |  137 | `				Version();` |
|    4 |  138 | `			}else if( strcmp(argv[n], "--help") == 0 ){` |
|    3 |  139 | `				Help();` |
|    1 |  140 | `			}else{` |
|    - |  141 | `				/* Unknown long option */` |
|  ! 0 |  142 | `				Help();` |
|    - |  143 | `			}` |
|    2 |  144 | `			continue;` |
|    - |  145 | `		}` |
|   11 |  146 | `		c = argv[n][1];` |
|   11 |  147 | `		if( c == 'b' ){` |
|    - |  148 | `			/* Dump byte-code instructions */` |
|    3 |  149 | `			dump_vm = 1;` |
|   10 |  150 | `		}else if( c == 'r' ){` |
|    - |  151 | `			/* Run inline PHP code from next argument (php -r style) */` |
|    9 |  152 | `			if( n + 1 >= argc ){` |
|    - |  153 | `				/* Missing code argument */` |
|  ! 0 |  154 | `				Fatal("Missing code argument for -r");` |
|  ! 0 |  155 | `			}` |
|    9 |  156 | `			zRunCode = argv[++n];` |
|    9 |  157 | `			run_code = 1;` |
|    4 |  158 | `		}else if( c == 'v' ){` |
|    - |  159 | `			/* Display version */` |
|  ! 0 |  160 | `			Version();` |
|  ! 0 |  161 | `		}else{` |
|    - |  162 | `			/* Display a help message and exit */` |
|  ! 0 |  163 | `			Help();` |
|    - |  164 | `		}` |
|    6 |  165 | `	}` |
| 2095 |  166 | `	if( n >= argc && !run_code ){` |
|  ! 0 |  167 | `		puts("Missing PHP file to compile");` |
|  ! 0 |  168 | `		Help();` |
|  ! 0 |  169 | `	}` |
|    - |  170 |  |
|    - |  171 | `#if defined(__WINNT__) && defined(PH7_DEBUG)` |
|    - |  172 | `	/* Install an unhandled exception minidump handler for Windows debug builds */` |
|    2 |  173 | `	CreateMiniDumpOnUnHandledException();` |
|    - |  174 | `#endif` |
|    - |  175 | `	/* Allocate a new PH7 engine instance */` |
| 1863 |  176 | `	rc = ph7_init(&pEngine);` |
| 1863 |  177 | `	if( rc != PH7_OK ){` |
|    - |  178 | `		/*` |
|    - |  179 | `		 * If the supplied memory subsystem is so sick that we are unable` |
|    - |  180 | `		 * to allocate a tiny chunk of memory,there is no much we can do here.` |
|    - |  181 | `		 */` |
|  ! 0 |  182 | `		Fatal("Error while allocating a new PH7 engine instance");` |
|  ! 0 |  183 | `	}` |
|    - |  184 | `	/* Set an error log consumer callback. This callback [Output_Consumer()] will` |
|    - |  185 | `	 * redirect all compile-time error messages to STDOUT.` |
|    - |  186 | `	 */` |
| 1863 |  187 | `	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,` |
|    - |  188 | `		Output_Consumer, /* Error log consumer */` |
|    - |  189 |  |
|    - |  190 | `		);` |
|    - |  191 | `	/* Now,it's time to compile our PHP file */` |
| 1863 |  192 | `	if( run_code ){` |
|    - |  193 | `		/* Compile inline PHP code string (PHP only - no tags needed) */` |
|    9 |  194 | `		rc = ph7_compile_v2(` |
|    4 |  195 | `			pEngine, /* PH7 Engine */` |
|    4 |  196 | `			zRunCode, /* Source code */` |
|    - |  197 | `			-1,       /* Let API compute length */` |
|    - |  198 | `			&pVm,     /* OUT: Compiled PHP program */` |
|    - |  199 | `			PH7_PHP_ONLY /* Inline PHP, no tags expected */` |
|    - |  200 | `			);` |
|    9 |  201 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  ! 0 |  202 | `			if( rc == PH7_VM_ERR ){` |
|  ! 0 |  203 | `				Fatal("VM initialization error");` |
|  ! 0 |  204 | `			}else{` |
|    - |  205 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  ! 0 |  206 | `				Fatal("Compile error");` |
|    - |  207 | `			}` |
|  ! 0 |  208 | `		}` |
|    5 |  209 | `	}else{` |
| 1855 |  210 | `		rc = ph7_compile_file(` |
|  867 |  211 | `			pEngine, /* PH7 Engine */` |
| 1853 |  212 | `			argv[n], /* Path to the PHP file to compile */` |
|    - |  213 | `			&pVm,    /* OUT: Compiled PHP program */` |
|    - |  214 |  |
|    - |  215 | `			);` |
| 1855 |  216 | `		if( rc != PH7_OK ){ /* Compile error */` |
|  239 |  217 | `			if( rc == PH7_IO_ERR ){` |
|  ! 0 |  218 | `				Fatal("IO error while opening the target file");` |
|  239 |  219 | `			}else if( rc == PH7_VM_ERR ){` |
|  ! 0 |  220 | `				Fatal("VM initialization error");` |
|  ! 0 |  221 | `			}else{` |
|    - |  222 | `				/* Compile-time error, your output (STDOUT) should display the error messages */` |
|  239 |  223 | `				Fatal("Compile error");` |
|    - |  224 | `			}` |
|  119 |  225 | `		}` |
|    - |  226 | `	}` |
|    - |  227 | `	/*` |
|    - |  228 | `	 * Now we have our script compiled,it's time to configure our VM.` |
|    - |  229 | `	 * We will install the VM output consumer callback defined above` |
|    - |  230 | `	 * so that we can consume the VM output and redirect it to STDOUT.` |
|    - |  231 | `	 */` |
| 1744 |  232 | `	rc = ph7_vm_config(pVm,` |
|    - |  233 | `		PH7_VM_CONFIG_OUTPUT,` |
|    - |  234 | `		Output_Consumer,    /* Output Consumer callback */` |
|    - |  235 |  |
|    - |  236 | `		);` |
| 1744 |  237 | `	if( rc != PH7_OK ){` |
|  ! 0 |  238 | `		Fatal("Error while installing the VM output consumer callback");` |
|  ! 0 |  239 | `	}` |
|    - |  240 | `	/* Register script arguments so we can access them later using the $argv[]` |
|    - |  241 | `	 * array from the compiled PHP program. For regular file execution we need` |
|    - |  242 | `	 * to register the arguments after the script file, while for inline code` |
|    - |  243 | `	 * (-r) the arguments start at the current index.` |
|    - |  244 | `	 */` |
| 1744 |  245 | `	if( run_code ){` |
|   11 |  246 | `		for( ; n < argc ; ++n ){` |
|    2 |  247 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|    1 |  248 | `		}` |
|    5 |  249 | `	}else{` |
| 1756 |  250 | `		for( n = n + 1; n < argc ; ++n ){` |
|   22 |  251 | `			ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);` |
|   12 |  252 | `		}` |
|    - |  253 | `	}` |
|    - |  254 | `	/* Report script run-time errors (now default behavior) */` |
| 1744 |  255 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);` |
| 1744 |  256 | `	if( dump_vm ){` |
|    - |  257 | `		/* Dump PH7 byte-code instructions */` |
|    3 |  258 | `		ph7_vm_dump_v2(pVm,` |
|    - |  259 | `			Output_Consumer, /* Dump consumer callback */` |
|    - |  260 |  |
|    - |  261 | `			);` |
|    1 |  262 | `	}` |
|    - |  263 | `	/*` |
|    - |  264 | `	 * And finally, execute our program. Note that your output (STDOUT in our case)` |
|    - |  265 | `	 * should display the result.` |
|    - |  266 | `	 */` |
| 1744 |  267 | `	ph7_vm_exec(pVm,0);` |
|    - |  268 | `	/* All done, cleanup the mess left behind.` |
|    - |  269 | `	*/` |
| 1740 |  270 | `	ph7_vm_release(pVm);` |
| 1740 |  271 | `	ph7_release(pEngine);` |
| 1740 |  272 | `	return 0;` |
|    2 |  273 |  |
|    - |  274 |  |
