/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The PHL interpreter is a simple stand-alone PHP interpreter that allows
 * the user to enter and execute PHP files against a PH7 engine. 
 * To start the phl program, just type "phl" followed by the name of the PHP file
 * to compile and execute. That is, the first argument is to the interpreter, the rest
 * are scripts arguments, press "Enter" and the PHP code will be executed.
 * If something goes wrong while processing the PHP script due to a compile-time error
 * your error output (STDOUT) should display the compile-time error messages.
 *
 * Usage example of the phl interpreter:
 *   phl examples/hello_world.php
 * Running the interpreter with script arguments
 *    phl scripts/mp3_tag.php /usr/local/path/to/my_mp3s
 *
 * Command line options:
 *   -b: Dump PH7 byte-code instructions
 *   -h: Display this help message
 *
 * The PHL interpreter package includes more than 70 PHP scripts to test ranging from
 * simple hello world programs to XML processing, ZIP archive extracting, MP3 tag extracting, 
 * UUID generation, JSON encoding/decoding, INI processing, Base32 encoding/decoding and many
 * more. These scripts are available in the scripts directory from the zip archive.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Make sure this header file is available.*/
#include "ph7.h"
/* 
 * Display an error message and exit.
 */
static void Fatal(const char *zMsg)
{
	puts(zMsg);
	/* Shutdown the library */
	ph7_lib_shutdown();
	/* Exit immediately */
	exit(0);
}
/*
 * Display the banner,a help message and exit.
 */
static void Help(void)
{
	puts("phl [-h|--help|-b|-v|--version] path/to/php_file [script args]");
	puts("\t-b: Dump PH7 byte-code instructions");
	puts("\t-v, --version: Display version information and exit");
	puts("\t-h, --help: Display this message and exit");
	/* Exit immediately */
	exit(0);
}
/*
 * Display version information and exit.
 */
static void Version(void)
{
	puts("PHL " PH7_VERSION " (cli) (built " __DATE__ " " __TIME__ ")");
	puts("Copyright (c) 2011-2014 Symisc Systems, 2025 Alexandre Gomes Gaigalas");
	/* Exit immediately */
	exit(0);
}
#ifdef __WINNT__
#include <Windows.h>
#else
/* Assume UNIX */
#include <unistd.h>
#endif
/*
 * The following define is used by the UNIX built and have
 * no particular meaning on windows.
 */
#ifndef STDOUT_FILENO
#define STDOUT_FILENO	1
#endif
/*
 * VM output consumer callback.
 * Each time the virtual machine generates some outputs,the following
 * function gets called by the underlying virtual machine to consume
 * the generated output.
 * All this function does is redirecting the VM output to STDOUT.
 * This function is registered later via a call to ph7_vm_config()
 * with a configuration verb set to: PH7_VM_CONFIG_OUTPUT.
 */
static int Output_Consumer(const void *pOutput,unsigned int nOutputLen,void *pUserData /* Unused */)
{
#ifdef __WINNT__
	BOOL rc;
	rc = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pOutput,(DWORD)nOutputLen,0,0);
	if( !rc ){
		/* Abort processing */
		return PH7_ABORT;
	}
#else
	ssize_t nWr;
	nWr = write(STDOUT_FILENO,pOutput,nOutputLen);
	if( nWr < 0 ){
		/* Abort processing */
		return PH7_ABORT;
	}
#endif /* __WINT__ */
	/* All done,VM output was redirected to STDOUT */
	return PH7_OK;
}
/*
 * Main program: Compile and execute the PHP file. 
 */
int main(int argc,char **argv)
{
	ph7 *pEngine; /* PH7 engine */
	ph7_vm *pVm;  /* Compiled PHP program */
	int dump_vm = 0;    /* Dump VM instructions if TRUE */
	int n;              /* Script arguments */
	int rc;
	/* Process interpreter arguments first*/
	for(n = 1 ; n < argc ; ++n ){
		int c;
		if( argv[n][0] != '-' ){
			/* No more interpreter arguments */
			break;
		}
		/* Check for long options */
		if( argv[n][1] == '-' ){
			if( strcmp(argv[n], "--version") == 0 ){
				Version();
			}else if( strcmp(argv[n], "--help") == 0 ){
				Help();
			}else{
				/* Unknown long option */
				Help();
			}
			continue;
		}
		c = argv[n][1];
		if( c == 'b' ){
			/* Dump byte-code instructions */
			dump_vm = 1;
		}else if( c == 'v' ){
			/* Display version */
			Version();
		}else{
			/* Display a help message and exit */
			Help();
		}
	}
	if( n >= argc ){
		puts("Missing PHP file to compile");
		Help();
	}
	/* Allocate a new PH7 engine instance */
	rc = ph7_init(&pEngine);
	if( rc != PH7_OK ){
		/*
		 * If the supplied memory subsystem is so sick that we are unable
		 * to allocate a tiny chunk of memory,there is no much we can do here.
		 */
		Fatal("Error while allocating a new PH7 engine instance");
	}
	/* Set an error log consumer callback. This callback [Output_Consumer()] will
	 * redirect all compile-time error messages to STDOUT.
	 */
	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,
		Output_Consumer, /* Error log consumer */
		0 /* NULL: Callback Private data */
		);
	/* Now,it's time to compile our PHP file */
	rc = ph7_compile_file(
		pEngine, /* PH7 Engine */
		argv[n], /* Path to the PHP file to compile */
		&pVm,    /* OUT: Compiled PHP program */
		0        /* IN: Compile flags */
		);
	if( rc != PH7_OK ){ /* Compile error */
		if( rc == PH7_IO_ERR ){
			Fatal("IO error while opening the target file");
		}else if( rc == PH7_VM_ERR ){
			Fatal("VM initialization error");
		}else{
			/* Compile-time error, your output (STDOUT) should display the error messages */
			Fatal("Compile error");
		}
	}
	/*
	 * Now we have our script compiled,it's time to configure our VM.
	 * We will install the VM output consumer callback defined above
	 * so that we can consume the VM output and redirect it to STDOUT.
	 */
	rc = ph7_vm_config(pVm,
		PH7_VM_CONFIG_OUTPUT,
		Output_Consumer,    /* Output Consumer callback */
		0                   /* Callback private data */
		);
	if( rc != PH7_OK ){
		Fatal("Error while installing the VM output consumer callback");
	}
	/* Register script agruments so we can access them later using the $argv[]
	 * array from the compiled PHP program.
	 */
	for( n = n + 1; n < argc ; ++n ){
		ph7_vm_config(pVm,PH7_VM_CONFIG_ARGV_ENTRY,argv[n]/* Argument value */);
	}
	/* Report script run-time errors (now default behavior) */
	ph7_vm_config(pVm,PH7_VM_CONFIG_ERR_REPORT);
	if( dump_vm ){
		/* Dump PH7 byte-code instructions */
		ph7_vm_dump_v2(pVm,
			Output_Consumer, /* Dump consumer callback */
			0
			);
	}
	/*
	 * And finally, execute our program. Note that your output (STDOUT in our case)
	 * should display the result.
	 */
	ph7_vm_exec(pVm,0);
	/* All done, cleanup the mess left behind.
	*/
	ph7_vm_release(pVm);
	ph7_release(pEngine);
	return 0;
}
