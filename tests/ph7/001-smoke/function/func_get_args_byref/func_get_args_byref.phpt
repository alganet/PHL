--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_get_args_byref returns arguments by reference
--SKIPIF--
<?php
// PHL extension: `func_get_args_byref()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: func_get_args_byref() is not a php symbol'; }
?>
--FILE--
<?php
function inc(&$v){ $v += 1; }
function wrapper(&$a){
    $args = func_get_args_byref();
    $args[0] += 1; // modify via reference
}

$a = 10;
wrapper($a);
echo $a . "\n";
?>
--EXPECT--
11
--CLEAN--
<?php
unset($args, $a);
