--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-array second argument should raise TypeError
--FILE--
<?php
array_diff_key(array(), 'foo');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_key(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

