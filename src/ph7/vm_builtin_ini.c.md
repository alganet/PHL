# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 21/22 lines (95.45%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    7 | `/*` |
|    - |    8 | ` * php.ini subsystem + INI API (NEWPLAN band D): a lazily seeded directive` |
|    - |    9 | ` * table (defaults merged with the CLI's -d/-c entries, drained from the VM's` |
|    - |   10 | ` * aIniCli queue by the __ini_cli() thunk) behind ini_get / ini_set /` |
|    - |   11 | ` * ini_restore / ini_get_all / get_cfg_var. Live-wired directives dispatch to` |
|    - |   12 | ` * the real knobs (error_reporting(), the session state, the default` |
|    - |   13 | ` * timezone) so the INI view and the engine agree.` |
|    - |   14 | ` */` |
|    - |   15 |  |
|    - |   16 | `/* array __ini_cli(void) — the queued -d/-c directives, in order */` |
|   12 |   17 | `static int vm_builtin_ini_cli(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   18 | `{` |
|    - |   19 | `	ph7_value *pArr,*pV;` |
|    - |   20 | `	VmIniEntry *aEntry;` |
|    - |   21 | `	sxu32 n;` |
|    6 |   22 | `	SXUNUSED(nArg);` |
|    6 |   23 | `	SXUNUSED(apArg);` |
|   13 |   24 | `	pArr = ph7_context_new_array(pCtx);` |
|   13 |   25 | `	pV = ph7_context_new_scalar(pCtx);` |
|   13 |   26 | `	if( pArr == 0 \|\| pV == 0 ){` |
|  ! 0 |   27 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |   28 | `	}` |
|   13 |   29 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pCtx->pVm->aIniCli);` |
|   27 |   30 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIniCli) ; n++ ){` |
|   14 |   31 | `		ph7_value_string(pV,aEntry[n].sValue.zString,(int)aEntry[n].sValue.nByte);` |
|   14 |   32 | `		ph7_array_add_strkey_elem(pArr,aEntry[n].sName.zString,pV);` |
|   14 |   33 | `		ph7_value_reset_string_cursor(pV);` |
|    7 |   34 | `	}` |
|   13 |   35 | `	ph7_result_value(pCtx,pArr);` |
|   13 |   36 | `	return PH7_OK;` |
|    7 |   37 | `}` |
|    - |   38 |  |
|    - |   39 | `static const char zIniLib[] =` |
|    - |   40 | `"class __IniS {"` |
|    - |   41 | `" public static $t = null;"` |
|    - |   42 | `"}"` |
|    - |   43 | `"function __ini_seed(){"` |
|    - |   44 | `" if( __IniS::$t !== null ){ return; }"` |
|    - |   45 | `" $t = ["` |
|    - |   46 | `"  'allow_url_fopen' => ['1', 6],"` |
|    - |   47 | `"  'arg_separator.output' => ['&', 7],"` |
|    - |   48 | `"  'auto_detect_line_endings' => ['', 7],"` |
|    - |   49 | `"  'date.timezone' => ['UTC', 7],"` |
|    - |   50 | `"  'default_charset' => ['UTF-8', 7],"` |
|    - |   51 | `"  'default_mimetype' => ['text/html', 7],"` |
|    - |   52 | `"  'display_errors' => ['1', 7],"` |
|    - |   53 | `"  'error_reporting' => ['32767', 7],"` |
|    - |   54 | `"  'include_path' => ['.', 7],"` |
|    - |   55 | `"  'max_execution_time' => ['0', 7],"` |
|    - |   56 | `"  'memory_limit' => ['-1', 7],"` |
|    - |   57 | `"  'post_max_size' => ['8M', 6],"` |
|    - |   58 | `"  'precision' => ['14', 7],"` |
|    - |   59 | `"  'serialize_precision' => ['-1', 7],"` |
|    - |   60 | `"  'session.name' => ['PHPSESSID', 7],"` |
|    - |   61 | `"  'session.save_path' => ['', 7],"` |
|    - |   62 | `"  'short_open_tag' => ['', 6],"` |
|    - |   63 | `"  'upload_max_filesize' => ['2M', 6],"` |
|    - |   64 | `" ];"` |
|    - |   65 | `" foreach( __ini_cli() as $k => $v ){"` |
|    - |   66 | `"  if( isset($t[$k]) ){"` |
|    - |   67 | `"   $t[$k][0] = (string)$v;"` |
|    - |   68 | `"  }else{"` |
|    - |   69 | `"   $t[$k] = [(string)$v, 7];"` |
|    - |   70 | `"  }"` |
|    - |   71 | `" }"` |
|    - |   72 | `" $seeded = [];"` |
|    - |   73 | `" foreach( $t as $k => $pair ){"` |
|    - |   74 | `"  $seeded[$k] = ['g' => $pair[0], 'l' => $pair[0], 'a' => $pair[1]];"` |
|    - |   75 | `" }"` |
|    - |   76 | `" __IniS::$t = $seeded;"` |
|    - |   77 | `" /* boot-apply the CLI values for the live-wired knobs (the engine knobs"` |
|    - |   78 | `"  * error_reporting/date.timezone were already applied C-side) */"` |
|    - |   79 | `" if( $seeded['session.name']['g'] !== 'PHPSESSID' ){"` |
|    - |   80 | `"  __SessS::$name = $seeded['session.name']['g'];"` |
|    - |   81 | `" }"` |
|    - |   82 | `" if( $seeded['session.save_path']['g'] !== '' ){"` |
|    - |   83 | `"  __SessS::$path = rtrim($seeded['session.save_path']['g'], '/');"` |
|    - |   84 | `" }"` |
|    - |   85 | `"}"` |
|    - |   86 | `"function __ini_rt_get($name){"` |
|    - |   87 | `" /* live-wired reads: the runtime knob is the truth */"` |
|    - |   88 | `" if( $name === 'error_reporting' ){ return (string)error_reporting(); }"` |
|    - |   89 | `" if( $name === 'session.name' ){ return __SessS::$name; }"` |
|    - |   90 | `" if( $name === 'session.save_path' ){"` |
|    - |   91 | `"  return __SessS::$path === '' ? __IniS::$t[$name]['l'] : __SessS::$path;"` |
|    - |   92 | `" }"` |
|    - |   93 | `" return __IniS::$t[$name]['l'];"` |
|    - |   94 | `"}"` |
|    - |   95 | `"function __ini_rt_set($name, $value){"` |
|    - |   96 | `" if( $name === 'error_reporting' ){ error_reporting((int)$value); return; }"` |
|    - |   97 | `" if( $name === 'session.name' ){ __SessS::$name = $value; return; }"` |
|    - |   98 | `" if( $name === 'session.save_path' ){ __SessS::$path = rtrim($value, '/'); return; }"` |
|    - |   99 | `" if( $name === 'date.timezone' && preg_match('/^(UTC\|GMT)$/i', $value) ){"` |
|    - |  100 | `"  date_default_timezone_set($value);"` |
|    - |  101 | `" }"` |
|    - |  102 | `"}"` |
|    - |  103 | `"function ini_get($option){"` |
|    - |  104 | `" __ini_seed();"` |
|    - |  105 | `" $option = (string)$option;"` |
|    - |  106 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  107 | `" return __ini_rt_get($option);"` |
|    - |  108 | `"}"` |
|    - |  109 | `"function ini_set($option, $value){"` |
|    - |  110 | `" __ini_seed();"` |
|    - |  111 | `" $option = (string)$option;"` |
|    - |  112 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  113 | `" if( (__IniS::$t[$option]['a'] & INI_USER) === 0 ){ return false; }"` |
|    - |  114 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  115 | `"  trigger_error('ini_set(): Session ini settings cannot be changed after"` |
|    - |  116 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  117 | `"  return false;"` |
|    - |  118 | `" }"` |
|    - |  119 | `" $old = __ini_rt_get($option);"` |
|    - |  120 | `" $value = is_bool($value) ? ($value ? '1' : '') : (string)$value;"` |
|    - |  121 | `" __IniS::$t[$option]['l'] = $value;"` |
|    - |  122 | `" __ini_rt_set($option, $value);"` |
|    - |  123 | `" return $old;"` |
|    - |  124 | `"}"` |
|    - |  125 | `"function ini_restore($option){"` |
|    - |  126 | `" __ini_seed();"` |
|    - |  127 | `" $option = (string)$option;"` |
|    - |  128 | `" if( !isset(__IniS::$t[$option]) ){ return null; }"` |
|    - |  129 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  130 | `"  trigger_error('ini_restore(): Session ini settings cannot be changed after"` |
|    - |  131 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  132 | `"  return null;"` |
|    - |  133 | `" }"` |
|    - |  134 | `" $g = __IniS::$t[$option]['g'];"` |
|    - |  135 | `" __IniS::$t[$option]['l'] = $g;"` |
|    - |  136 | `" __ini_rt_set($option, $g);"` |
|    - |  137 | `" return null;"` |
|    - |  138 | `"}"` |
|    - |  139 | `"function ini_get_all($extension = null, $details = true){"` |
|    - |  140 | `" __ini_seed();"` |
|    - |  141 | `" $known = ['Core' => true, 'session' => true, 'date' => true, 'standard' => true];"` |
|    - |  142 | `" if( $extension !== null && !isset($known[(string)$extension]) ){"` |
|    - |  143 | `"  trigger_error('ini_get_all(): Extension \"' . $extension . '\" cannot be"` |
|    - |  144 | `" found', E_USER_WARNING);"` |
|    - |  145 | `"  return false;"` |
|    - |  146 | `" }"` |
|    - |  147 | `" $out = [];"` |
|    - |  148 | `" foreach( __IniS::$t as $name => $e ){"` |
|    - |  149 | `"  if( $extension !== null && $extension !== 'Core' && $extension !== 'standard' ){"` |
|    - |  150 | `"   if( strncmp($name, $extension . '.', strlen($extension) + 1) !== 0 ){ continue; }"` |
|    - |  151 | `"  }elseif( $extension !== null ){"` |
|    - |  152 | `"   if( strpos($name, 'session.') === 0 \|\| strpos($name, 'date.') === 0 ){ continue; }"` |
|    - |  153 | `"  }"` |
|    - |  154 | `"  $cur = __ini_rt_get($name);"` |
|    - |  155 | `"  if( $details ){"` |
|    - |  156 | `"   $out[$name] = ['global_value' => $e['g'], 'local_value' => $cur,"` |
|    - |  157 | `"    'access' => $e['a']];"` |
|    - |  158 | `"  }else{"` |
|    - |  159 | `"   $out[$name] = $cur;"` |
|    - |  160 | `"  }"` |
|    - |  161 | `" }"` |
|    - |  162 | `" ksort($out);"` |
|    - |  163 | `" return $out;"` |
|    - |  164 | `"}"` |
|    - |  165 | `"function get_cfg_var($option){"` |
|    - |  166 | `" __ini_seed();"` |
|    - |  167 | `" $option = (string)$option;"` |
|    - |  168 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  169 | `" return __IniS::$t[$option]['g'];"` |
|    - |  170 | `"}"` |
|    - |  171 | `;` |
|    - |  172 |  |
| 3916 |  173 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|    5 |  174 | `{` |
| 3921 |  175 | `	ph7_create_function(&(*pVm),"__ini_cli",vm_builtin_ini_cli,0);` |
| 3921 |  176 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zIniLib,sizeof(zIniLib)-1);` |
|    5 |  177 | `}` |
|    - |  178 |  |
|    - |  179 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  180 |  |
|    - |  181 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  182 | `/* Tiny build: no INI API (builtin layer disabled) */` |
|    - |  183 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  184 | `#endif` |
|    - |  185 |  |
