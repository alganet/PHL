--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-only: PHP rejects a closure/arrow-fn property default'; ?>
--TEST--
A `new` inside a closure/arrow-fn body is NOT a constant-expression `new`, so the
"New expressions are not supported in this context" reject must not fire for it
--FILE--
<?php
// Regression guard for GenStateInitHasNewExpr: the new-expression scan must skip
// over a nested closure / arrow-fn body (its `new` runs at call time). PHL accepts
// a closure/arrow-fn property default (PHP does not — hence --SKIPIF above).
class C {
    public $factory = fn() => new stdClass();
}
$c = new C();
$make = $c->factory;
echo get_class($make()), "\n";
?>
--EXPECT--
stdClass
--CLEAN--
<?php
