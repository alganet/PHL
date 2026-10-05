--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `const` statement binds where it runs: a name taken before it warns and keeps the first value
--FILE--
<?php
namespace Crd\Ns {
// php binds a global `const` where its statement RUNS (ZEND_DECLARE_CONST), so
// the name is not there before it, and a second declaration of a name already
// taken -- by an earlier `const`, by define(), or by the engine -- warns that it
// is already defined and KEEPS the first value. The warning names the folded
// key and blames the `const` keyword's line for every name in its list; the
// initializer runs first, so its side effects happen even when it is refused.
set_error_handler(function ($no, $msg, $file, $line) {
    echo "[$no] $msg @$line\n";
    return true;
});
\var_dump(\defined('Crd\Ns\ONE'));
try {
    echo ONE, "\n";
} catch (\Error $e) {
    echo \get_class($e), ': ', $e->getMessage(), "\n";
}
const ONE = 1;
const ONE = 2;
\var_dump(ONE);
const LIST_A = 'a', ONE
    = 3, LIST_B =
    'b';
\var_dump(ONE, LIST_A, LIST_B);
\define('Crd\Ns\VIA_DEFINE', 'define');
const VIA_DEFINE = 'const';
\var_dump(VIA_DEFINE);
const PHP_EOL = 'shadow';
\var_dump(PHP_EOL, \PHP_EOL !== 'shadow');
class Side { public function __construct() { echo "initializer ran\n"; } }
const ONE = new Side();
\var_dump(ONE);
\var_dump(\define('Crd\Ns\ONE', 4), ONE);
}
namespace {
// An engine constant is taken too.
const M_PI = 3;
var_dump(M_PI);
echo "done\n";
}
--EXPECT--
bool(false)
Error: Undefined constant "Crd\Ns\ONE"
[2] Constant crd\ns\ONE already defined, this will be an error in PHP 9 @20
int(1)
[2] Constant crd\ns\ONE already defined, this will be an error in PHP 9 @22
int(1)
string(1) "a"
string(1) "b"
[2] Constant crd\ns\VIA_DEFINE already defined, this will be an error in PHP 9 @27
string(6) "define"
string(6) "shadow"
bool(true)
initializer ran
[2] Constant crd\ns\ONE already defined, this will be an error in PHP 9 @32
int(1)
[2] Constant crd\ns\ONE already defined, this will be an error in PHP 9 @34
bool(false)
int(1)
[2] Constant M_PI already defined, this will be an error in PHP 9 @38
float(3.141592653589793)
done
