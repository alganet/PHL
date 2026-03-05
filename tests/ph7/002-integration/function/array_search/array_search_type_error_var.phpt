--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a non-array variable as haystack triggers TypeError
--FILE--
<?php
$a = 'not an array';
array_search('value', $a);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_search(): Argument #2 ($haystack) must be of type array, %s given in %s
--CLEAN--
<?php
unset($a);
