--TEST--
Reflection: the extension every internal name belongs to
--FILE--
<?php
// Every internal name sits in ONE extension, and Reflection reports it. Only
// names both engines place identically are pinned here; the engine's own
// PH7-isms have no php row to agree with.
foreach (['strlen', 'date', 'preg_match', 'json_encode', 'session_start',
          'bcadd', 'iconv_strlen', 'token_get_all', 'mb_strlen', 'array_map',
          'spl_autoload_register', 'ctype_alpha', 'hash', 'filter_var',
          'cal_to_jd', 'random_int'] as $reflExtFn) {
    printf("%-22s %s\n", $reflExtFn, (new ReflectionFunction($reflExtFn))->getExtensionName());
}
foreach (['ArrayObject', 'DateTime', 'stdClass', 'Closure', 'JsonSerializable',
          'ReflectionClass', 'SessionHandler', 'PhpToken', 'BcMath\\Number',
          'Random\\Randomizer', 'Traversable'] as $reflExtCl) {
    printf("%-22s %s\n", $reflExtCl, (new ReflectionClass($reflExtCl))->getExtensionName());
}
foreach (['PHP_EOL', 'DATE_ATOM', 'JSON_PRETTY_PRINT', 'T_STRING', 'M_PI',
          'PREG_SPLIT_NO_EMPTY', 'DIRECTORY_SEPARATOR', 'CASE_UPPER',
          'FILTER_VALIDATE_INT', 'CAL_GREGORIAN'] as $reflExtK) {
    printf("%-22s %s\n", $reflExtK, (new ReflectionConstant($reflExtK))->getExtensionName());
}

// A METHOD belongs to its DECLARING class's extension, so an inherited one
// still names SPL even when the subclass is a script's own.
class ReflExtSub extends ArrayObject {}
echo 'method=', (new ReflectionMethod('ReflExtSub', 'count'))->getExtensionName(),
     ' own=', (new ReflectionMethod('DateTime', 'format'))->getExtensionName(), "\n";

// A userland target belongs to no extension at all: php answers false for the
// name and null for the reflector.
function reflExtUserFn() {}
define('REFL_EXT_USER_K', 1);
echo 'userfn=', var_export((new ReflectionFunction('reflExtUserFn'))->getExtensionName(), true),
     '/', var_export((new ReflectionFunction('reflExtUserFn'))->getExtension(), true),
     ' userclass=', var_export((new ReflectionClass('ReflExtSub'))->getExtensionName(), true),
     ' userconst=', var_export((new ReflectionConstant('REFL_EXT_USER_K'))->getExtensionName(), true), "\n";

// getExtension() hands back the ReflectionExtension of the SAME name.
$reflExtObj = (new ReflectionFunction('date'))->getExtension();
echo get_class($reflExtObj), ' ', $reflExtObj->getName(), ' ', $reflExtObj->name, "\n";

// The constructor matches case-insensitively and keeps php's own spelling.
$reflExtSeen = [];
foreach (['DATE', 'spl', 'CORE', 'Json'] as $reflExtName) {
    $reflExtSeen[] = $reflExtName . '=>' . (new ReflectionExtension($reflExtName))->getName();
}
echo implode(' ', $reflExtSeen), "\n";
// An extension the engine ships WHOLE reports as loaded: ext/tokenizer's two
// functions, its PhpToken class and all 154 of its constants were here while
// extension_loaded() answered false and it had no partition of its own.
echo 'tokenizer=', var_export(extension_loaded('tokenizer'), true),
     ' listed=', var_export(in_array('tokenizer', get_loaded_extensions(), true), true),
     ' version=', var_export(is_string(phpversion('tokenizer')), true), "\n";

try {
    new ReflectionExtension('reflextnosuchthing');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
--EXPECT--
strlen                 Core
date                   date
preg_match             pcre
json_encode            json
session_start          session
bcadd                  bcmath
iconv_strlen           iconv
token_get_all          tokenizer
mb_strlen              mbstring
array_map              standard
spl_autoload_register  SPL
ctype_alpha            ctype
hash                   hash
filter_var             filter
cal_to_jd              calendar
random_int             random
ArrayObject            SPL
DateTime               date
stdClass               Core
Closure                Core
JsonSerializable       json
ReflectionClass        Reflection
SessionHandler         session
PhpToken               tokenizer
BcMath\Number          bcmath
Random\Randomizer      random
Traversable            Core
PHP_EOL                Core
DATE_ATOM              date
JSON_PRETTY_PRINT      json
T_STRING               tokenizer
M_PI                   standard
PREG_SPLIT_NO_EMPTY    pcre
DIRECTORY_SEPARATOR    standard
CASE_UPPER             standard
FILTER_VALIDATE_INT    filter
CAL_GREGORIAN          calendar
method=SPL own=date
userfn=false/NULL userclass=false userconst=false
ReflectionExtension date date
DATE=>date spl=>SPL CORE=>Core Json=>json
tokenizer=true listed=true version=true
ReflectionException: Extension "reflextnosuchthing" does not exist
