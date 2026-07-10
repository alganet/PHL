--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: strtr ignores an empty-string key in the replace_pairs array
--SKIPIF--
<?php
// PHL-only: PHP produces the same string but also emits an
// "Ignoring replacement of empty string" warning (error-format divergence, PLAN §3.7).
if (function_exists('zend_version')) echo 'skip';
?>
--FILE--
<?php
// The empty key is ignored; 'l' => 'L' still applies to every 'l'.
$result = strtr('hello', array('' => 'x', 'l' => 'L'));
var_dump($result);
?>
--EXPECT--
string(5) "heLLo"
--CLEAN--
<?php
unset($result);
