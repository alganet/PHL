--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
define() refuses a name already taken: FALSE under a warning, and the first value stays
--FILE--
<?php
// php answers a second define() of a name with FALSE and E_WARNING, and keeps
// the value the name already had -- engine constants included, and the three
// keyword constants and __COMPILER_HALT_OFFSET__ by spelling. The warning names
// the folded key: the namespace part lowercased, the last segment as written.
// The retired third argument only warns, and a class-constant name throws.
set_error_handler(function ($no, $msg) {
    echo "[$no] $msg\n";
    return true;
});
function show($label, $r) {
    echo $label, ': ', var_export($r, true), "\n";
}
show('first', define('RDEF_ONE', 1));
show('again', define('RDEF_ONE', 2));
show('value', RDEF_ONE);
show('lower is another name', define('rdef_one', 3));
show('ns first', define('Rdef\Ns\cC', 1));
show('ns folded', define('rDEF\nS\cC', 2));
show('ns last segment differs', define('Rdef\Ns\CC', 3));
show('ns value', constant('Rdef\Ns\cC'));
const RDEF_CONST = 10;
show('over const', define('RDEF_CONST', 11));
show('const value', RDEF_CONST);
foreach (['M_PI', 'E_ALL', 'PHP_EOL', 'SORT_STRING', 'true', 'FALSE', 'Null',
          '__COMPILER_HALT_OFFSET__', 'nulls'] as $n) {
    show($n, define($n, 1));
}
show('M_PI value', M_PI);
show('E_ALL value', E_ALL);
show('leading backslash is its own name', define('\RDEF_ONE', 4));
show('case-insensitive', define('RDEF_CI', 1, true));
show('no alias', defined('rdef_ci'));
show('third argument false', define('RDEF_CI2', 1, false));
function rdef_twice() {
    return define('RDEF_IN_FN', __LINE__);
}
show('in function', rdef_twice());
show('in function again', rdef_twice());
try {
    define('Foo::BAR', 1);
} catch (\ValueError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
show('class-constant name not defined', defined('Foo::BAR'));
--EXPECT--
first: true
[2] Constant RDEF_ONE already defined, this will be an error in PHP 9
again: false
value: 1
lower is another name: true
ns first: true
[2] Constant rdef\ns\cC already defined, this will be an error in PHP 9
ns folded: false
ns last segment differs: true
ns value: 1
[2] Constant RDEF_CONST already defined, this will be an error in PHP 9
over const: false
const value: 10
[2] Constant M_PI already defined, this will be an error in PHP 9
M_PI: false
[2] Constant E_ALL already defined, this will be an error in PHP 9
E_ALL: false
[2] Constant PHP_EOL already defined, this will be an error in PHP 9
PHP_EOL: false
[2] Constant SORT_STRING already defined, this will be an error in PHP 9
SORT_STRING: false
[2] Constant true already defined, this will be an error in PHP 9
true: false
[2] Constant FALSE already defined, this will be an error in PHP 9
FALSE: false
[2] Constant Null already defined, this will be an error in PHP 9
Null: false
[2] Constant __COMPILER_HALT_OFFSET__ already defined, this will be an error in PHP 9
__COMPILER_HALT_OFFSET__: false
nulls: true
M_PI value: 3.141592653589793
E_ALL value: 30719
leading backslash is its own name: true
[2] define(): Argument #3 ($case_insensitive) is ignored since declaration of case-insensitive constants is no longer supported
case-insensitive: true
no alias: false
third argument false: true
in function: true
[2] Constant RDEF_IN_FN already defined, this will be an error in PHP 9
in function again: false
ValueError: define(): Argument #1 ($constant_name) cannot be a class constant
class-constant name not defined: false
