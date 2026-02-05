--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: microtime with invalid arguments
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test microtime with string argument (PHL treats as truthy, returns float)
$result1 = microtime("invalid");
echo is_float($result1) ? "FLOAT_OK\n" : "FLOAT_FAIL\n";

// Test microtime with too many arguments
$result2 = microtime(true, "extra");
echo is_float($result2) ? "FLOAT_OK\n" : "FLOAT_FAIL\n";
?>
--EXPECT--
FLOAT_OK
FLOAT_OK
--CLEAN--
<?php
unset($result1, $result2);
