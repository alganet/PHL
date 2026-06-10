--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exceptions from callables invoked via call_user_func unwind
--DESCRIPTION--
call_user_func()/call_user_func_array() on a throwing callable (an __invoke
object, an array [obj, method] callable, or a closure) must propagate the
exception instead of returning FALSE and continuing.
--FILE--
<?php
class InvokeCuf_Boom {
    public function __invoke() { throw new Exception("invoke"); }
    public function m()        { throw new Exception("method"); }
}
$b = new InvokeCuf_Boom();

try { call_user_func($b); echo "no\n"; }
catch (Exception $e) { echo "a: ", $e->getMessage(), "\n"; }

try { call_user_func([$b, 'm']); echo "no\n"; }
catch (Exception $e) { echo "b: ", $e->getMessage(), "\n"; }

try { call_user_func_array(function ($x) { throw new Exception("cf-$x"); }, ['z']); echo "no\n"; }
catch (Exception $e) { echo "c: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
a: invoke
b: method
c: cf-z
--CLEAN--
<?php
unset($b);
