/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)
{
	while( zIn < zEnd ){
		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){
			/* Got one */
			return &zIn[1];
		}
		/* Advance the cursor */
		zIn++;
	}
	/* No such option */
	return 0;
}
/*
 * Check if a long option argument [i.e: --opt] is available in the command
 * line string. Return a pointer to the start of the stream on success.
 * NULL otherwise.
 */
static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)
{
	const char *zOpt;
	while( zIn < zEnd ){
		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){
			zIn += 2;
			zOpt = zIn;
			while( zIn < zEnd && !SyisSpace(zIn[0]) ){
				if( zIn[0] == '=' /* --opt=val */){
					break;
				}
				zIn++;
			}
			/* Test */
			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){
				/* Got one,return it's value */
				return zIn;
			}

		}else{
			zIn++;
		}
	}
	/* No such option */
	return 0;
}
/*
 * Long option [i.e: --opt] arguments private data structure.
 */
struct getopt_long_opt
{
	const char *zArgIn,*zArgEnd; /* Command line arguments */
	ph7_value *pWorker;  /* Worker variable*/
	ph7_value *pArray;   /* getopt() return value */
	ph7_context *pCtx;   /* Call Context */
};
/* Forward declaration */
static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);
/*
 * Extract short or long argument option values.
 */
static void VmExtractOptArgValue(
	ph7_value *pArray,  /* getopt() return value */
	ph7_value *pWorker, /* Worker variable */
	const char *zArg,   /* Argument stream */
	const char *zArgEnd,/* End of the argument stream  */
	int need_val,       /* TRUE to fetch option argument */
	ph7_context *pCtx,  /* Call Context */
	const char *zName   /* Option name */)
{
	ph7_value_bool(pWorker,0);
	if( !need_val ){
		/*
		 * Option does not need arguments.
		 * Insert the option name and a boolean FALSE.
		 */
		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
	}else{
		const char *zCur;
		/* Extract option argument */
		zArg++;
		if( zArg < zArgEnd && zArg[0] == '=' ){
			zArg++;
		}
		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
			zArg++;
		}
		if( zArg >= zArgEnd || zArg[0] == '-' ){
			/*
			 * Argument not found.
			 * Insert the option name and a boolean FALSE.
			 */
			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
			return;
		}
		/* Delimit the value */
		zCur = zArg;
		if( zArg[0] == '\'' || zArg[0] == '"' ){
			int d = zArg[0];
			/* Delimt the argument */
			zArg++;
			zCur = zArg;
			while( zArg < zArgEnd ){
				if( zArg[0] == d && zArg[-1] != '\\' ){
					/* Delimiter found,exit the loop  */
					break;
				}
				zArg++;
			}
			/* Save the value */
			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
			if( zArg < zArgEnd ){ zArg++; }
		}else{
			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){
				zArg++;
			}
			/* Save the value */
			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
		}
		/*
		 * Check if we are dealing with multiple values.
		 * If so,create an array to hold them,rather than a scalar variable.
		 */
		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
			zArg++;
		}
		if( zArg < zArgEnd && zArg[0] != '-' ){
			ph7_value *pOptArg; /* Array of option arguments */
			pOptArg = ph7_context_new_array(pCtx);
			if( pOptArg == 0 ){
				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			}else{
				/* Insert the first value */
				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */
				for(;;){
					if( zArg >= zArgEnd || zArg[0] == '-' ){
						/* No more value */
						break;
					}
					/* Delimit the value */
					zCur = zArg;
					if( zArg < zArgEnd && zArg[0] == '\\' ){
						zArg++;
						zCur = zArg;
					}
					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){
						zArg++;
					}
					/* Reset the string cursor */
					ph7_value_reset_string_cursor(pWorker);
					/* Save the value */
					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));
					/* Insert */
					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */
					/* Jump trailing white spaces */
					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){
						zArg++;
					}
				}
				/* Insert the option arg array */
				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */
				/* Safely release */
				ph7_context_release_value(pCtx,pOptArg);
			}
		}else{
			/* Single value */
			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */
		}
	}
}
/*
 * array getopt(string $options[,array $longopts ])
 *   Gets options from the command line argument list.
 * Parameters
 *  $options
 *   Each character in this string will be used as option characters
 *   and matched against options passed to the script starting with
 *   a single hyphen (-). For example, an option string "x" recognizes
 *   an option -x. Only a-z, A-Z and 0-9 are allowed.
 *  $longopts
 *   An array of options. Each element in this array will be used as option
 *   strings and matched against options passed to the script starting with
 *   two hyphens (--). For example, an longopts element "opt" recognizes an
 *   option --opt.
 * Return
 *  This function will return an array of option / argument pairs or FALSE
 *  on failure.
 */
PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;
	struct getopt_long_opt sLong;
	ph7_value *pArray,*pWorker;
	SyBlob *pArg;
	int nByte;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract option arguments */
	zIn  = ph7_value_to_string(apArg[0],&nByte);
	zEnd = &zIn[nByte];
	/* Point to the string representation of the $argv[] array */
	pArg = &pCtx->pVm->sArgv;
	/* Create a new empty array and a worker variable */
	pArray = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBlobLength(pArg) < 1 ){
		/* Empty command line,return the empty array*/
		ph7_result_value(pCtx,pArray);
		/* Everything will be released automatically when we return
		 * from this function.
		 */
		return PH7_OK;
	}
	zArgIn = (const char *)SyBlobData(pArg);
	zArgEnd = &zArgIn[SyBlobLength(pArg)];
	/* Fill the long option structure */
	sLong.pArray = pArray;
	sLong.pWorker = pWorker;
	sLong.zArgIn =  zArgIn;
	sLong.zArgEnd = zArgEnd;
	sLong.pCtx = pCtx;
	/* Start processing */
	while( zIn < zEnd ){
		int c = zIn[0];
		int need_val = 0;
		/* Advance the stream cursor */
		zIn++;
		/* Ignore non-alphanum characters */
		if( !SyisAlphaNum(c) ){
			continue;
		}
		if( zIn < zEnd && zIn[0] == ':' ){
			zIn++;
			need_val = 1;
			if( zIn < zEnd && zIn[0] == ':' ){
				zIn++;
			}
		}
		/* Find option */
		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);
		if( zArg == 0 ){
			/* No such option */
			continue;
		}
		/* Extract option argument value */
		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);
	}
	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){
		/* Process long options */
		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);
	}
	/* Return the option array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory, everything will be released
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * Array walker callback used for processing long options values.
 */
static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;
	const char *zArg,*zOpt,*zEnd;
	int need_value = 0;
	int nByte;
	/* Value must be of type string */
	if( !ph7_value_is_string(pValue) ){
		/* Simply ignore */
		return PH7_OK;
	}
	zOpt = ph7_value_to_string(pValue,&nByte);
	if( nByte < 1 ){
		/* Empty string,ignore */
		return PH7_OK;
	}
	zEnd = &zOpt[nByte - 1];
	if( zEnd[0] == ':' ){
		char *zTerm;
		/* Try to extract a value */
		need_value = 1;
		while( zEnd >= zOpt && zEnd[0] == ':' ){
			zEnd--;
		}
		if( zOpt >= zEnd ){
			/* Empty string,ignore */
			SXUNUSED(pKey);
			return PH7_OK;
		}
		zEnd++;
		zTerm = (char *)zEnd;
		zTerm[0] = 0;
	}else{
		zEnd = &zOpt[nByte];
	}
	/* Find the option */
	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);
	if( zArg == 0 ){
		/* No such option,return immediately */
		return PH7_OK;
	}
	/* Try to extract a value */
	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);
	return PH7_OK;
}
