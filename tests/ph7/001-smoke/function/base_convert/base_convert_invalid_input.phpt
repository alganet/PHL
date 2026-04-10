--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: base_convert with invalid input for the base
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test base_convert with invalid character for hex base
$result = base_convert('G', 16, 10);
var_dump($result);
?>
--EXPECT--
string(1) "0"
--CLEAN--
<?php
unset($result);
