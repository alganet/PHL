--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with null callback throws TypeError
--FILE--
<?php
$a = array(1, 2);
array_walk_recursive($a, null);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_walk_recursive(): Argument #2 ($callback) must be a valid callback, no array or string given in %s
--CLEAN--
<?php
unset($a);
