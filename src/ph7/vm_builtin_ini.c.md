# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 33/37 lines (89.19%)

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
|   20 |   17 | `static int vm_builtin_ini_cli(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |   18 | `{` |
|    - |   19 | `	ph7_value *pArr,*pV;` |
|    - |   20 | `	VmIniEntry *aEntry;` |
|    - |   21 | `	sxu32 n;` |
|   10 |   22 | `	SXUNUSED(nArg);` |
|   10 |   23 | `	SXUNUSED(apArg);` |
|   24 |   24 | `	pArr = ph7_context_new_array(pCtx);` |
|   24 |   25 | `	pV = ph7_context_new_scalar(pCtx);` |
|   24 |   26 | `	if( pArr == 0 \|\| pV == 0 ){` |
|  ! 0 |   27 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |   28 | `	}` |
|   24 |   29 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pCtx->pVm->aIniCli);` |
|   44 |   30 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIniCli) ; n++ ){` |
|   21 |   31 | `		ph7_value_string(pV,aEntry[n].sValue.zString,(int)aEntry[n].sValue.nByte);` |
|   21 |   32 | `		ph7_array_add_strkey_elem(pArr,aEntry[n].sName.zString,pV);` |
|   21 |   33 | `		ph7_value_reset_string_cursor(pV);` |
|   11 |   34 | `	}` |
|   24 |   35 | `	ph7_result_value(pCtx,pArr);` |
|   24 |   36 | `	return PH7_OK;` |
|   14 |   37 | `}` |
|    - |   38 |  |
|    - |   39 | `/* void __ini_apply_err(string $name, int $on) — mirror the display_errors /` |
|    - |   40 | ` * log_errors gate into the C-side VM fields so an ini_set() at runtime reaches` |
|    - |   41 | ` * the diagnostic emitter (VmEmitDiagnostic). The -d/-c path already applies` |
|    - |   42 | ` * these in PH7_VM_CONFIG_INI_ENTRY; the caller (__ini_rt_set) passes an already` |
|    - |   43 | ` * php-coerced 0/1. */` |
|    4 |   44 | `static int vm_builtin_ini_apply_err(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   45 | `{` |
|    - |   46 | `	const char *zName;` |
|    5 |   47 | `	int nName = 0;` |
|    - |   48 | `	int bOn;` |
|    5 |   49 | `	if( nArg < 2 ){` |
|  ! 0 |   50 | `		return PH7_OK;` |
|    - |   51 | `	}` |
|    5 |   52 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    5 |   53 | `	bOn = ph7_value_to_int(apArg[1]) != 0;` |
|    5 |   54 | `	if( nName == (int)sizeof("display_errors")-1 && SyMemcmp(zName,"display_errors",(sxu32)nName) == 0 ){` |
|    5 |   55 | `		pCtx->pVm->bDisplayErrors = bOn;` |
|    2 |   56 | `	}else if( nName == (int)sizeof("log_errors")-1 && SyMemcmp(zName,"log_errors",(sxu32)nName) == 0 ){` |
|  ! 0 |   57 | `		pCtx->pVm->bLogErrors = bOn;` |
|  ! 0 |   58 | `	}` |
|    5 |   59 | `	return PH7_OK;` |
|    3 |   60 | `}` |
|    - |   61 |  |
|    - |   62 | `static const char zIniLib[] =` |
|    - |   63 | `"class __IniS {"` |
|    - |   64 | `" public static $t = null;"` |
|    - |   65 | `"}"` |
|    - |   66 | `"function __ini_seed(){"` |
|    - |   67 | `" if( __IniS::$t !== null ){ return; }"` |
|    - |   68 | `" $t = ["` |
|    - |   69 | `"  'allow_url_fopen' => ['1', 6],"` |
|    - |   70 | `"  'arg_separator.output' => ['&', 7],"` |
|    - |   71 | `"  'auto_detect_line_endings' => ['', 7],"` |
|    - |   72 | `"  'date.timezone' => ['UTC', 7],"` |
|    - |   73 | `"  'default_charset' => ['UTF-8', 7],"` |
|    - |   74 | `"  'default_mimetype' => ['text/html', 7],"` |
|    - |   75 | `"  'display_errors' => ['', 7],"` |
|    - |   76 | `"  'error_log' => ['', 7],"` |
|    - |   77 | `"  'error_reporting' => ['30719', 7],"` |
|    - |   78 | `"  'log_errors' => ['1', 7],"` |
|    - |   79 | `"  'highlight.comment' => ['#FF8000', 7],"` |
|    - |   80 | `"  'highlight.default' => ['#0000BB', 7],"` |
|    - |   81 | `"  'highlight.html' => ['#000000', 7],"` |
|    - |   82 | `"  'highlight.keyword' => ['#007700', 7],"` |
|    - |   83 | `"  'highlight.string' => ['#DD0000', 7],"` |
|    - |   84 | `"  'include_path' => ['.', 7],"` |
|    - |   85 | `"  'max_execution_time' => ['0', 7],"` |
|    - |   86 | `"  'memory_limit' => ['-1', 7],"` |
|    - |   87 | `"  'post_max_size' => ['8M', 6],"` |
|    - |   88 | `"  'precision' => ['14', 7],"` |
|    - |   89 | `"  'serialize_precision' => ['-1', 7],"` |
|    - |   90 | `"  'session.name' => ['PHPSESSID', 7],"` |
|    - |   91 | `"  'session.save_path' => ['', 7],"` |
|    - |   92 | `"  'short_open_tag' => ['', 6],"` |
|    - |   93 | `"  'upload_max_filesize' => ['2M', 6],"` |
|    - |   94 | `"  'zend.assertions' => ['-1', 7],"` |
|    - |   95 | `" ];"` |
|    - |   96 | `" foreach( __ini_cli() as $k => $v ){"` |
|    - |   97 | `"  if( isset($t[$k]) ){"` |
|    - |   98 | `"   $t[$k][0] = (string)$v;"` |
|    - |   99 | `"  }else{"` |
|    - |  100 | `"   $t[$k] = [(string)$v, 7];"` |
|    - |  101 | `"  }"` |
|    - |  102 | `" }"` |
|    - |  103 | `" $seeded = [];"` |
|    - |  104 | `" foreach( $t as $k => $pair ){"` |
|    - |  105 | `"  $seeded[$k] = ['g' => $pair[0], 'l' => $pair[0], 'a' => $pair[1]];"` |
|    - |  106 | `" }"` |
|    - |  107 | `" __IniS::$t = $seeded;"` |
|    - |  108 | `" /* boot-apply the CLI values for the live-wired knobs (the engine knobs"` |
|    - |  109 | `"  * error_reporting/date.timezone were already applied C-side) */"` |
|    - |  110 | `" if( $seeded['session.name']['g'] !== 'PHPSESSID' ){"` |
|    - |  111 | `"  __SessS::$name = $seeded['session.name']['g'];"` |
|    - |  112 | `" }"` |
|    - |  113 | `" if( $seeded['session.save_path']['g'] !== '' ){"` |
|    - |  114 | `"  __SessS::$path = rtrim($seeded['session.save_path']['g'], '/');"` |
|    - |  115 | `" }"` |
|    - |  116 | `"}"` |
|    - |  117 | `"function __ini_rt_get($name){"` |
|    - |  118 | `" /* live-wired reads: the runtime knob is the truth */"` |
|    - |  119 | `" if( $name === 'error_reporting' ){ return (string)error_reporting(); }"` |
|    - |  120 | `" if( $name === 'session.name' ){ return __SessS::$name; }"` |
|    - |  121 | `" if( $name === 'session.save_path' ){"` |
|    - |  122 | `"  return __SessS::$path === '' ? __IniS::$t[$name]['l'] : __SessS::$path;"` |
|    - |  123 | `" }"` |
|    - |  124 | `" return __IniS::$t[$name]['l'];"` |
|    - |  125 | `"}"` |
|    - |  126 | `"function __ini_truthy($v){"` |
|    - |  127 | `" /* zend_ini_parse_bool semantics, matching the C-side VmIniBool used by the"` |
|    - |  128 | `"  * -d/-c path: on/yes/true, else a non-zero integer parse. */"` |
|    - |  129 | `" $v = strtolower(trim((string)$v));"` |
|    - |  130 | `" if( $v === 'on' \|\| $v === 'yes' \|\| $v === 'true' ){ return true; }"` |
|    - |  131 | `" return (int)$v !== 0;"` |
|    - |  132 | `"}"` |
|    - |  133 | `"function __ini_rt_set($name, $value){"` |
|    - |  134 | `" if( $name === 'error_reporting' ){ error_reporting((int)$value); return; }"` |
|    - |  135 | `" if( $name === 'display_errors' \|\| $name === 'log_errors' ){"` |
|    - |  136 | `"  __ini_apply_err($name, __ini_truthy($value) ? 1 : 0);"` |
|    - |  137 | `"  return;"` |
|    - |  138 | `" }"` |
|    - |  139 | `" if( $name === 'session.name' ){ __SessS::$name = $value; return; }"` |
|    - |  140 | `" if( $name === 'session.save_path' ){ __SessS::$path = rtrim($value, '/'); return; }"` |
|    - |  141 | `" if( $name === 'date.timezone' && preg_match('/^(UTC\|GMT)$/i', $value) ){"` |
|    - |  142 | `"  date_default_timezone_set($value);"` |
|    - |  143 | `" }"` |
|    - |  144 | `"}"` |
|    - |  145 | `"function ini_get($option){"` |
|    - |  146 | `" __ini_seed();"` |
|    - |  147 | `" $option = (string)$option;"` |
|    - |  148 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  149 | `" return __ini_rt_get($option);"` |
|    - |  150 | `"}"` |
|    - |  151 | `"function ini_set($option, $value){"` |
|    - |  152 | `" __ini_seed();"` |
|    - |  153 | `" $option = (string)$option;"` |
|    - |  154 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  155 | `" if( (__IniS::$t[$option]['a'] & INI_USER) === 0 ){ return false; }"` |
|    - |  156 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  157 | `"  trigger_error('ini_set(): Session ini settings cannot be changed after"` |
|    - |  158 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  159 | `"  return false;"` |
|    - |  160 | `" }"` |
|    - |  161 | `" if( $option === 'zend.assertions' &&"` |
|    - |  162 | `"     (__IniS::$t[$option]['g'] === '-1' \|\| (string)$value === '-1') ){"` |
|    - |  163 | `"  /* php: the -1 (compiled-out) state is a php.ini-only switch */"` |
|    - |  164 | `"  trigger_error('zend.assertions may be completely enabled or disabled only"` |
|    - |  165 | `" in php.ini', E_USER_WARNING);"` |
|    - |  166 | `"  return false;"` |
|    - |  167 | `" }"` |
|    - |  168 | `" $old = __ini_rt_get($option);"` |
|    - |  169 | `" $value = is_bool($value) ? ($value ? '1' : '') : (string)$value;"` |
|    - |  170 | `" __IniS::$t[$option]['l'] = $value;"` |
|    - |  171 | `" __ini_rt_set($option, $value);"` |
|    - |  172 | `" return $old;"` |
|    - |  173 | `"}"` |
|    - |  174 | `"function ini_restore($option){"` |
|    - |  175 | `" __ini_seed();"` |
|    - |  176 | `" $option = (string)$option;"` |
|    - |  177 | `" if( !isset(__IniS::$t[$option]) ){ return null; }"` |
|    - |  178 | `" if( strncmp($option, 'session.', 8) === 0 && headers_sent() ){"` |
|    - |  179 | `"  trigger_error('ini_restore(): Session ini settings cannot be changed after"` |
|    - |  180 | `" headers have already been sent', E_USER_WARNING);"` |
|    - |  181 | `"  return null;"` |
|    - |  182 | `" }"` |
|    - |  183 | `" $g = __IniS::$t[$option]['g'];"` |
|    - |  184 | `" __IniS::$t[$option]['l'] = $g;"` |
|    - |  185 | `" __ini_rt_set($option, $g);"` |
|    - |  186 | `" return null;"` |
|    - |  187 | `"}"` |
|    - |  188 | `"function ini_get_all($extension = null, $details = true){"` |
|    - |  189 | `" __ini_seed();"` |
|    - |  190 | `" $known = ['Core' => true, 'session' => true, 'date' => true, 'standard' => true];"` |
|    - |  191 | `" if( $extension !== null && !isset($known[(string)$extension]) ){"` |
|    - |  192 | `"  trigger_error('ini_get_all(): Extension \"' . $extension . '\" cannot be"` |
|    - |  193 | `" found', E_USER_WARNING);"` |
|    - |  194 | `"  return false;"` |
|    - |  195 | `" }"` |
|    - |  196 | `" $out = [];"` |
|    - |  197 | `" foreach( __IniS::$t as $name => $e ){"` |
|    - |  198 | `"  if( $extension !== null && $extension !== 'Core' && $extension !== 'standard' ){"` |
|    - |  199 | `"   if( strncmp($name, $extension . '.', strlen($extension) + 1) !== 0 ){ continue; }"` |
|    - |  200 | `"  }elseif( $extension !== null ){"` |
|    - |  201 | `"   if( strpos($name, 'session.') === 0 \|\| strpos($name, 'date.') === 0 ){ continue; }"` |
|    - |  202 | `"  }"` |
|    - |  203 | `"  $cur = __ini_rt_get($name);"` |
|    - |  204 | `"  if( $details ){"` |
|    - |  205 | `"   $out[$name] = ['global_value' => $e['g'], 'local_value' => $cur,"` |
|    - |  206 | `"    'access' => $e['a']];"` |
|    - |  207 | `"  }else{"` |
|    - |  208 | `"   $out[$name] = $cur;"` |
|    - |  209 | `"  }"` |
|    - |  210 | `" }"` |
|    - |  211 | `" ksort($out);"` |
|    - |  212 | `" return $out;"` |
|    - |  213 | `"}"` |
|    - |  214 | `"function get_cfg_var($option){"` |
|    - |  215 | `" __ini_seed();"` |
|    - |  216 | `" $option = (string)$option;"` |
|    - |  217 | `" if( !isset(__IniS::$t[$option]) ){ return false; }"` |
|    - |  218 | `" return __IniS::$t[$option]['g'];"` |
|    - |  219 | `"}"` |
|    - |  220 | `;` |
|    - |  221 |  |
| 4528 |  222 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|    5 |  223 | `{` |
| 4533 |  224 | `	ph7_create_function(&(*pVm),"__ini_cli",vm_builtin_ini_cli,0);` |
| 4533 |  225 | `	ph7_create_function(&(*pVm),"__ini_apply_err",vm_builtin_ini_apply_err,0);` |
| 4533 |  226 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zIniLib,sizeof(zIniLib)-1);` |
|    5 |  227 | `}` |
|    - |  228 |  |
|    - |  229 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  230 |  |
|    - |  231 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  232 | `/* Tiny build: no INI API (builtin layer disabled) */` |
|    - |  233 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  234 | `#endif` |
|    - |  235 |  |
