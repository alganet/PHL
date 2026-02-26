--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-array middle argument should raise TypeError
--FILE--
<?php
array_diff_uassoc(array(), 'foo', function($a,$b){return 0;});
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_uassoc(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

