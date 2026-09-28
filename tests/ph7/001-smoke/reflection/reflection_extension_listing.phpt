--TEST--
Reflection: an extension lists the functions, classes, constants and directives it owns
--FILE--
<?php
// Every ReflectionExtension listing answered the EMPTY array and
// get_extension_funcs() did not exist at all. Both read one partition now, and
// both answer in php's own registration ORDER -- which is not alphabetical.
$reflExtJson = new ReflectionExtension('json');
print_r(array_keys($reflExtJson->getFunctions()));
print_r($reflExtJson->getClassNames());
print_r(array_keys($reflExtJson->getClasses()));
print_r(array_slice($reflExtJson->getConstants(), 0, 4, true));

// The map's VALUES are reflectors over the names.
echo get_class(current($reflExtJson->getFunctions())), ' ',
     current($reflExtJson->getFunctions())->getName(), ' ',
     get_class(current($reflExtJson->getClasses())), ' ',
     current($reflExtJson->getClasses())->getName(), "\n";

// get_extension_funcs() is the same list, LOWER-cased, and matches the
// extension name case-insensitively.
print_r(get_extension_funcs('json'));
var_dump(get_extension_funcs('JSON') === get_extension_funcs('json'));

// php answers FALSE for a name it does not know AND for an extension that
// registers no function at all -- Reflection is loaded and has none.
var_dump(get_extension_funcs('reflextnosuchthing'), get_extension_funcs('Reflection'));
var_dump((new ReflectionExtension('Reflection'))->getFunctions());

// A directive listing carries the CURRENT value, and belongs to the extension
// php says it does rather than to whatever its name is prefixed with.
$reflExtIni = (new ReflectionExtension('session'))->getINIEntries();
echo 'session.name=', $reflExtIni['session.name'] ?? '(absent)',
     ' cookie_path=', $reflExtIni['session.cookie_path'] ?? '(absent)',
     ' core-has-session=', var_export(isset((new ReflectionExtension('Core'))->getINIEntries()['session.name']), true), "\n";

// What an extension DECLARES about the others.
$reflExtDeps = [];
foreach (['SPL', 'session', 'mbstring', 'json'] as $reflExtDep) {
    $reflExtDeps[] = $reflExtDep . '=' . json_encode((new ReflectionExtension($reflExtDep))->getDependencies());
}
echo implode(' ', $reflExtDeps), "\n";

// Listing the table is not READING an entry, so a deprecated constant in it
// stays silent (get_defined_constants()'s own rule).
$reflExtSeen = [];
set_error_handler(function ($n, $s) use (&$reflExtSeen) { $reflExtSeen[] = $s; return true; });
$reflExtCount = count((new ReflectionExtension('random'))->getConstants());
restore_error_handler();
echo 'random constants=', $reflExtCount, ' diagnostics=', count($reflExtSeen), "\n";
--EXPECT--
Array
(
    [0] => json_encode
    [1] => json_decode
    [2] => json_validate
    [3] => json_last_error
    [4] => json_last_error_msg
)
Array
(
    [0] => JsonSerializable
    [1] => JsonException
)
Array
(
    [0] => JsonSerializable
    [1] => JsonException
)
Array
(
    [JSON_HEX_TAG] => 1
    [JSON_HEX_AMP] => 2
    [JSON_HEX_APOS] => 4
    [JSON_HEX_QUOT] => 8
)
ReflectionFunction json_encode ReflectionClass JsonSerializable
Array
(
    [0] => json_encode
    [1] => json_decode
    [2] => json_validate
    [3] => json_last_error
    [4] => json_last_error_msg
)
bool(true)
bool(false)
bool(false)
array(0) {
}
session.name=PHPSESSID cookie_path=/ core-has-session=false
SPL={"json":"Required"} session={"spl":"Optional"} mbstring={"pcre":"Required"} json=[]
random constants=2 diagnostics=0
