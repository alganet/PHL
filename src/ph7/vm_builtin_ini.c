/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * php.ini subsystem + INI API (NEWPLAN band D): a lazily seeded directive
 * table (defaults merged with the CLI's -d/-c entries, drained from the VM's
 * aIniCli queue by the __ini_cli() thunk) behind ini_get / ini_set /
 * ini_restore / ini_get_all / get_cfg_var. Live-wired directives dispatch to
 * the real knobs (error_reporting(), the session state, the default
 * timezone) so the INI view and the engine agree.
 */

/* array __ini_cli(void) — the queued -d/-c directives, in order */
static int vm_builtin_ini_cli(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArr,*pV;
	VmIniEntry *aEntry;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	aEntry = (VmIniEntry *)SySetBasePtr(&pCtx->pVm->aIniCli);
	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIniCli) ; n++ ){
		ph7_value_string(pV,aEntry[n].sValue.zString,(int)aEntry[n].sValue.nByte);
		ph7_array_add_strkey_elem(pArr,aEntry[n].sName.zString,pV);
		ph7_value_reset_string_cursor(pV);
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}

static const char zIniLib[] =
"class __IniS {"
" public static $t = null;"
"}"
"function __ini_seed(){"
" if( __IniS::$t !== null ){ return; }"
" $t = ["
"  'allow_url_fopen' => ['1', 6],"
"  'arg_separator.output' => ['&', 7],"
"  'auto_detect_line_endings' => ['', 7],"
"  'date.timezone' => ['UTC', 7],"
"  'default_charset' => ['UTF-8', 7],"
"  'default_mimetype' => ['text/html', 7],"
"  'display_errors' => ['1', 7],"
"  'error_reporting' => ['32767', 7],"
"  'include_path' => ['.', 7],"
"  'max_execution_time' => ['0', 7],"
"  'memory_limit' => ['-1', 7],"
"  'post_max_size' => ['8M', 6],"
"  'precision' => ['14', 7],"
"  'serialize_precision' => ['-1', 7],"
"  'session.name' => ['PHPSESSID', 7],"
"  'session.save_path' => ['', 7],"
"  'short_open_tag' => ['', 6],"
"  'upload_max_filesize' => ['2M', 6],"
" ];"
" foreach( __ini_cli() as $k => $v ){"
"  if( isset($t[$k]) ){"
"   $t[$k][0] = (string)$v;"
"  }else{"
"   $t[$k] = [(string)$v, 7];"
"  }"
" }"
" $seeded = [];"
" foreach( $t as $k => $pair ){"
"  $seeded[$k] = ['g' => $pair[0], 'l' => $pair[0], 'a' => $pair[1]];"
" }"
" __IniS::$t = $seeded;"
" /* boot-apply the CLI values for the live-wired knobs (the engine knobs"
"  * error_reporting/date.timezone were already applied C-side) */"
" if( $seeded['session.name']['g'] !== 'PHPSESSID' ){"
"  __SessS::$name = $seeded['session.name']['g'];"
" }"
" if( $seeded['session.save_path']['g'] !== '' ){"
"  __SessS::$path = rtrim($seeded['session.save_path']['g'], '/');"
" }"
"}"
"function __ini_rt_get($name){"
" /* live-wired reads: the runtime knob is the truth */"
" if( $name === 'error_reporting' ){ return (string)error_reporting(); }"
" if( $name === 'session.name' ){ return __SessS::$name; }"
" if( $name === 'session.save_path' ){"
"  return __SessS::$path === '' ? __IniS::$t[$name]['l'] : __SessS::$path;"
" }"
" return __IniS::$t[$name]['l'];"
"}"
"function __ini_rt_set($name, $value){"
" if( $name === 'error_reporting' ){ error_reporting((int)$value); return; }"
" if( $name === 'session.name' ){ __SessS::$name = $value; return; }"
" if( $name === 'session.save_path' ){ __SessS::$path = rtrim($value, '/'); return; }"
" if( $name === 'date.timezone' && preg_match('/^(UTC|GMT)$/i', $value) ){"
"  date_default_timezone_set($value);"
" }"
"}"
"function ini_get($option){"
" __ini_seed();"
" $option = (string)$option;"
" if( !isset(__IniS::$t[$option]) ){ return false; }"
" return __ini_rt_get($option);"
"}"
"function ini_set($option, $value){"
" __ini_seed();"
" $option = (string)$option;"
" if( !isset(__IniS::$t[$option]) ){ return false; }"
" if( (__IniS::$t[$option]['a'] & INI_USER) === 0 ){ return false; }"
" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"
"  trigger_error('ini_set(): Session ini settings cannot be changed after"
" headers have already been sent', E_USER_WARNING);"
"  return false;"
" }"
" $old = __ini_rt_get($option);"
" $value = is_bool($value) ? ($value ? '1' : '') : (string)$value;"
" __IniS::$t[$option]['l'] = $value;"
" __ini_rt_set($option, $value);"
" return $old;"
"}"
"function ini_restore($option){"
" __ini_seed();"
" $option = (string)$option;"
" if( !isset(__IniS::$t[$option]) ){ return null; }"
" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"
"  trigger_error('ini_restore(): Session ini settings cannot be changed after"
" headers have already been sent', E_USER_WARNING);"
"  return null;"
" }"
" $g = __IniS::$t[$option]['g'];"
" __IniS::$t[$option]['l'] = $g;"
" __ini_rt_set($option, $g);"
" return null;"
"}"
"function ini_get_all($extension = null, $details = true){"
" __ini_seed();"
" $known = ['Core' => true, 'session' => true, 'date' => true, 'standard' => true];"
" if( $extension !== null && !isset($known[(string)$extension]) ){"
"  trigger_error('ini_get_all(): Extension \"' . $extension . '\" cannot be"
" found', E_USER_WARNING);"
"  return false;"
" }"
" $out = [];"
" foreach( __IniS::$t as $name => $e ){"
"  if( $extension !== null && $extension !== 'Core' && $extension !== 'standard' ){"
"   if( strncmp($name, $extension . '.', strlen($extension) + 1) !== 0 ){ continue; }"
"  }elseif( $extension !== null ){"
"   if( strpos($name, 'session.') === 0 || strpos($name, 'date.') === 0 ){ continue; }"
"  }"
"  $cur = __ini_rt_get($name);"
"  if( $details ){"
"   $out[$name] = ['global_value' => $e['g'], 'local_value' => $cur,"
"    'access' => $e['a']];"
"  }else{"
"   $out[$name] = $cur;"
"  }"
" }"
" ksort($out);"
" return $out;"
"}"
"function get_cfg_var($option){"
" __ini_seed();"
" $option = (string)$option;"
" if( !isset(__IniS::$t[$option]) ){ return false; }"
" return __IniS::$t[$option]['g'];"
"}"
;

PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)
{
	ph7_create_function(&(*pVm),"__ini_cli",vm_builtin_ini_cli,0);
	return PH7_VmEvalBuiltinChunk(&(*pVm),zIniLib,sizeof(zIniLib)-1);
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no INI API (builtin layer disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
#endif
