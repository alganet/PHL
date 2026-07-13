--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
constant() retrieves defined constants; undefined names throw Error (php 8)
--DESCRIPTION--
Rewritten cross-engine with the band A #4 fix: an undefined constant is a
catchable Error ("Undefined constant \"X\""), not the old PHL notice + NULL
this test used to enshrine.
--SKIPIF--
<?php
if (!function_exists('constant')) { echo 'skip: constant not available'; }
?>
--FILE--
<?php
define('TEST_CONST', 42);
$val = constant('TEST_CONST');
if ($val === 42) { echo "defined_ok\n"; } else { echo "defined_failed\n"; }
try {
    constant('UNDEFINED_CONST');
    echo "no_throw\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
defined_ok
caught: Undefined constant "UNDEFINED_CONST"
--CLEAN--
<?php
unset($val);
