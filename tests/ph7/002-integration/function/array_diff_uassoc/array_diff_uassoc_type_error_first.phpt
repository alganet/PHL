--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-array first argument should raise TypeError
--FILE--
<?php
array_diff_uassoc('foo', array(), function($a,$b){return 0;});
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_uassoc(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

