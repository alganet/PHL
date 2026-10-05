--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_register() and spl_autoload_unregister() refuse an uncallable callback with a TypeError
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
class C {
    private function p($c) {}
    private static function sp($c) {}
    static function ok($c) {}
}
function al($c) {}
function t($f, ...$a) {
    try {
        var_dump($f(...$a));
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
foreach (['nope_fn', [new C, 'p'], ['C', 'sp'], ['C', 'zz'], ['Nope', 'zz'], 42, [1, 2], 'C::p'] as $cb) {
    t('spl_autoload_register', $cb);
    t('spl_autoload_unregister', $cb);
}
t('spl_autoload_unregister', null);
// $throw is ignored: the screen still throws, and false only earns a notice
t('spl_autoload_register', 'nope_fn', false);
t('spl_autoload_register', 'al', false);
t('spl_autoload_register', ['C', 'ok'], true, true);
// null names the default implementation, like no argument at all
t('spl_autoload_register', null, true, true);
t('spl_autoload_register');
var_dump(spl_autoload_functions());
t('spl_autoload_unregister', 'spl_autoload');
t('spl_autoload_unregister', 'spl_autoload');
t('spl_autoload_unregister', 'al');
t('spl_autoload_unregister', 'strlen');
var_dump(spl_autoload_functions());
?>
--EXPECT--
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, function "nope_fn" not found or invalid function name
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, function "nope_fn" not found or invalid function name
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, cannot access private method C::p()
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, cannot access private method C::p()
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, cannot access private method C::sp()
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, cannot access private method C::sp()
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, class C does not have a method "zz"
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, class C does not have a method "zz"
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, class "Nope" not found
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, class "Nope" not found
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, no array or string given
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, no array or string given
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, first array member is not a valid class name or object
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, first array member is not a valid class name or object
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, non-static method C::p() cannot be called statically
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, non-static method C::p() cannot be called statically
spl_autoload_unregister(): Argument #1 ($callback) must be a valid callback, no array or string given
spl_autoload_register(): Argument #1 ($callback) must be a valid callback or null, function "nope_fn" not found or invalid function name
[8] spl_autoload_register(): Argument #2 ($do_throw) has been ignored, spl_autoload_register() will always throw
bool(true)
bool(true)
bool(true)
bool(true)
array(3) {
  [0]=>
  string(12) "spl_autoload"
  [1]=>
  array(2) {
    [0]=>
    string(1) "C"
    [1]=>
    string(2) "ok"
  }
  [2]=>
  string(2) "al"
}
bool(true)
bool(false)
bool(true)
bool(false)
array(1) {
  [0]=>
  array(2) {
    [0]=>
    string(1) "C"
    [1]=>
    string(2) "ok"
  }
}
