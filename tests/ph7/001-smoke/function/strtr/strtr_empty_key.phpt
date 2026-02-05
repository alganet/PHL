--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strtr with array containing empty string key
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test strtr with array containing empty key
$result = strtr('hello', array('' => 'x', 'l' => 'L'));
var_dump($result);
?>
--EXPECT--
string(5 'heLlo')
--CLEAN--
<?php
unset($result);
