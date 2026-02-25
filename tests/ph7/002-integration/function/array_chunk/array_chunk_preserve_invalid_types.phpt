--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with array preserve_keys should throw TypeError
--FILE--
<?php
$array = array(1,2,3);
array_chunk($array,2,array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_chunk(): Argument #3 ($preserve_keys) must be of type bool, array given in %s
--CLEAN--
<?php
unset($array);
