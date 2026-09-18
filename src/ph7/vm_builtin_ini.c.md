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
|   16 |   17 | `static int vm_builtin_ini_cli(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |   18 | `{` |
|    - |   19 | `	ph7_value *pArr,*pV;` |
|    - |   20 | `	VmIniEntry *aEntry;` |
|    - |   21 | `	sxu32 n;` |
|    8 |   22 | `	SXUNUSED(nArg);` |
|    8 |   23 | `	SXUNUSED(apArg);` |
|   18 |   24 | `	pArr = ph7_context_new_array(pCtx);` |
|   18 |   25 | `	pV = ph7_context_new_scalar(pCtx);` |
|   18 |   26 | `	if( pArr == 0 \|\| pV == 0 ){` |
|  ! 0 |   27 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |   28 | `	}` |
|   18 |   29 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pCtx->pVm->aIniCli);` |
|   34 |   30 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIniCli) ; n++ ){` |
|   17 |   31 | `		ph7_value_string(pV,aEntry[n].sValue.zString,(int)aEntry[n].sValue.nByte);` |
|   17 |   32 | `		ph7_array_add_strkey_elem(pArr,aEntry[n].sName.zString,pV);` |
|   17 |   33 | `		ph7_value_reset_string_cursor(pV);` |
|    9 |   34 | `	}` |
|   18 |   35 | `	ph7_result_value(pCtx,pArr);` |
|   18 |   36 | `	return PH7_OK;` |
|   10 |   37 | `}` |
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
|    - |   53 | `"  'error_reporting' => ['30719', 7],"` |
|    - |   54 | `"  'highlight.comment' => ['#FF8000', 7],"` |
|    - |   55 | `"  'highlight.default' => ['#0000BB', 7],"` |
|    - |   56 | `"  'highlight.html' => ['#000000', 7],"` |
|    - |   57 | `"  'highlight.keyword' => ['#007700', 7],"` |
|    - |   58 | `"  'highlight.string' => ['#DD0000', 7],"` |
|    - |   59 | `"  'include_path' => ['.', 7],"` |
|    - |   60 | `"  'max_execution_time' => ['0', 7],"` |
|    - |   61 | `"  'memory_limit' => ['-1', 7],"` |
|    - |   62 | `"  'post_max_size' => ['8M', 6],"` |
|    - |   63 | `"  'precision' => ['14', 7],"` |
|    - |   64 | `"  'serialize_precision' => ['-1', 7],"` |
|    - |   65 | `"  'session.name' => ['PHPSESSID', 7],"` |
|    - |   66 | `"  'session.save_path' => ['', 7],"` |
|    - |   67 | `"  'short_open_tag' => ['', 6],"` |
|    - |   68 | `"  'upload_max_filesize' => ['2M', 6],"` |
|    - |   69 | `"  'zend.assertions' => ['-1', 7],"` |
|    - |   70 | `" ];"` |
|    - |   71 | `" foreach( __ini_cli() as $k => $v ){"` |
|    - |   72 | `"  if( isset($t[$k]) ){"` |
|    - |   73 | `"   $t[$k][0] = (string)$v;"` |
|    - |   74 | `"  }else{"` |
|    - |   75 | `"   $t[$k] = [(string)$v, 7];"` |
|    - |   76 | `"  }"` |
|    - |   77 | `" }"` |
|    - |   78 | `" $seeded = [];"` |
|    - |   79 | `" foreach( $t as $k => $pair ){"` |
|    - |   80 | `"  $seeded[$k] = ['g' => $pair[0], 'l' => $pair[0], 'a' => $pair[1]];"` |
|    - |   81 | `" }"` |
|    - |   82 | `" __IniS::$t = $seeded;"` |
|    - |   83 | `" /* boot-apply the CLI values for the live-wired knobs (the engine knobs"` |
|    - |   84 | `"  * error_reporting/date.timezone were already applied C-side) */"` |
|    - |   85 | `" if( $seeded['session.name']['g'] !== 'PHPSESSID' ){"` |
|    - |   86 | `"  __SessS::$name = $seeded['session.name']['g'];"` |
|    - |   87 | `" }"` |
|    - |   88 | `" if( $seeded['session.save_path']['g'] !== '' ){"` |
|    - |   89 | `"  __SessS::$path = rtrim($seeded['session.save_path']['g'], '/');"` |
|    - |   90 | `" }"` |
|    - |   91 | `"}"` |
|    - |   92 | `"function __ini_rt_get($name){"` |
|    - |   93 | `" /* live-wired reads: the runtime knob is the truth */"` |
|    - |   94 | `" if( $name === 'error_reporting' ){ return (string)error_reporting(); }"` |
|    - |   95 | `" if( $name === 'session.name' ){ return __SessS::$name; }"` |
|    - |   96 | `" if( $name === 'session.save_path' ){"` |
|    - |   97 | `"  return __SessS::$path === '' ? __IniS::$t[$name]['l'] : __SessS::$path;"` |
|    - |   98 | `" }"` |
|    - |   99 | `" return __IniS::$t[$name]['l'];"` |
|    - |  100 | `"}"` |
|    - |  101 | `"function __ini_rt_set($name, $value){"` |
|    - |  102 | `" if( $name === 'error_reporting' ){ error_reporting((int)$value); return; }"` |
|    - |  103 | `" if( $name === 'session.name' ){ __SessS::$name = $value; return; }"` |
|    - |  104 | `" if( $name === 'session.save_path' ){ __SessS::$path = rtrim($value, '/'); return; }"` |
|    - |  105 | `" if( $name === 'date.timezone' && preg_match('/^(UTC\|GMT)$/i', $value) ){"` |
|    - |  106 | `"  date_default_timezone_set($value);"` |
|    - |  107 | `" }"` |
|    - |  108 | `"}"` |
|    - |  109 | `"function ini_get($option){"` |
|    - |  110 | `" __ini_seed();"` |
|    - |  111 | `" $option = (string)$option;"` |
|    - |  112 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  113 | `" return __ini_rt_get($option);"` |
|    - |  114 | `"}"` |
|    - |  115 | `"function ini_set($option, $value){"` |
|    - |  116 | `" __ini_seed();"` |
|    - |  117 | `" $option = (string)$option;"` |
|    - |  118 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  119 | `" if( (__IniS::$t[$option]['a'] & INI_USER) === 0 ){ return false; }"` |
|    - |  120 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  121 | `"  trigger_error('ini_set(): Session ini settings cannot be changed after"` |
|    - |  122 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  123 | `"  return false;"` |
|    - |  124 | `" }"` |
|    - |  125 | `" if( $option === 'zend.assertions' &&"` |
|    - |  126 | `"     (__IniS::$t[$option]['g'] === '-1' \|\| (string)$value === '-1') ){"` |
|    - |  127 | `"  /* php: the -1 (compiled-out) state is a php.ini-only switch */"` |
|    - |  128 | `"  trigger_error('zend.assertions may be completely enabled or disabled only"` |
|    - |  129 | `" in php.ini', E_USER_WARNING);"` |
|    - |  130 | `"  return false;"` |
|    - |  131 | `" }"` |
|    - |  132 | `" $old = __ini_rt_get($option);"` |
|    - |  133 | `" $value = is_bool($value) ? ($value ? '1' : '') : (string)$value;"` |
|    - |  134 | `" __IniS::$t[$option]['l'] = $value;"` |
|    - |  135 | `" __ini_rt_set($option, $value);"` |
|    - |  136 | `" return $old;"` |
|    - |  137 | `"}"` |
|    - |  138 | `"function ini_restore($option){"` |
|    - |  139 | `" __ini_seed();"` |
|    - |  140 | `" $option = (string)$option;"` |
|    - |  141 | `" if( !isset(__IniS::$t[$option]) ){ return null; }"` |
|    - |  142 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  143 | `"  trigger_error('ini_restore(): Session ini settings cannot be changed after"` |
|    - |  144 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  145 | `"  return null;"` |
|    - |  146 | `" }"` |
|    - |  147 | `" $g = __IniS::$t[$option]['g'];"` |
|    - |  148 | `" __IniS::$t[$option]['l'] = $g;"` |
|    - |  149 | `" __ini_rt_set($option, $g);"` |
|    - |  150 | `" return null;"` |
|    - |  151 | `"}"` |
|    - |  152 | `"function ini_get_all($extension = null, $details = true){"` |
|    - |  153 | `" __ini_seed();"` |
|    - |  154 | `" $known = ['Core' => true, 'session' => true, 'date' => true, 'standard' => true];"` |
|    - |  155 | `" if( $extension !== null && !isset($known[(string)$extension]) ){"` |
|    - |  156 | `"  trigger_error('ini_get_all(): Extension \"' . $extension . '\" cannot be"` |
|    - |  157 | `" found', E_USER_WARNING);"` |
|    - |  158 | `"  return false;"` |
|    - |  159 | `" }"` |
|    - |  160 | `" $out = [];"` |
|    - |  161 | `" foreach( __IniS::$t as $name => $e ){"` |
|    - |  162 | `"  if( $extension !== null && $extension !== 'Core' && $extension !== 'standard' ){"` |
|    - |  163 | `"   if( strncmp($name, $extension . '.', strlen($extension) + 1) !== 0 ){ continue; }"` |
|    - |  164 | `"  }elseif( $extension !== null ){"` |
|    - |  165 | `"   if( strpos($name, 'session.') === 0 \|\| strpos($name, 'date.') === 0 ){ continue; }"` |
|    - |  166 | `"  }"` |
|    - |  167 | `"  $cur = __ini_rt_get($name);"` |
|    - |  168 | `"  if( $details ){"` |
|    - |  169 | `"   $out[$name] = ['global_value' => $e['g'], 'local_value' => $cur,"` |
|    - |  170 | `"    'access' => $e['a']];"` |
|    - |  171 | `"  }else{"` |
|    - |  172 | `"   $out[$name] = $cur;"` |
|    - |  173 | `"  }"` |
|    - |  174 | `" }"` |
|    - |  175 | `" ksort($out);"` |
|    - |  176 | `" return $out;"` |
|    - |  177 | `"}"` |
|    - |  178 | `"function get_cfg_var($option){"` |
|    - |  179 | `" __ini_seed();"` |
|    - |  180 | `" $option = (string)$option;"` |
|    - |  181 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  182 | `" return __IniS::$t[$option]['g'];"` |
|    - |  183 | `"}"` |
|    - |  184 | `;` |
|    - |  185 |  |
| 3876 |  186 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|    5 |  187 | `{` |
| 3881 |  188 | `	ph7_create_function(&(*pVm),"__ini_cli",vm_builtin_ini_cli,0);` |
| 3881 |  189 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zIniLib,sizeof(zIniLib)-1);` |
|    5 |  190 | `}` |
|    - |  191 |  |
|    - |  192 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  193 |  |
|    - |  194 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  195 | `/* Tiny build: no INI API (builtin layer disabled) */` |
|    - |  196 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  197 | `#endif` |
|    - |  198 |  |
