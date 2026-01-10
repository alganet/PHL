--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Var assignment on array with key but missing value generates compile error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// This should trigger the error path for missing array entry value
// compile.c:1108-1113 - when &pCur[1] >= pGen->pIn after finding a key
array(1 =>);
?>
--EXPECTF--
%s 4 Error: array(): Missing entry value
Compile error
