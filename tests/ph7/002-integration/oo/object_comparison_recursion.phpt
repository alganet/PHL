--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison recursion limit
--SKIPIF--
<?php
// Same engine-design difference as object_compare_nesting_limit: PHL's
// comparison nesting guard fires where php recurses until memory exhaustion.
if (function_exists('zend_version')) { echo 'skip PHL bounds comparison nesting; php recurses until memory exhaustion'; }
?>
--FILE--
<?php
class TestObj {
    public $ref;
}
$a = new TestObj();
$b = new TestObj();
$a->ref = $b;
$b->ref = $a;
$result = $a == $b;
?>
--EXPECTF--
Error: Nesting limit reached: Infinite recursion? in %s on line %d
--CLEAN--
<?php
unset($a, $b, $result);
