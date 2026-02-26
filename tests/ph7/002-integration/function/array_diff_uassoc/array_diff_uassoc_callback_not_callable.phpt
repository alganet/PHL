--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Callback must be a valid callable; integer should trigger TypeError
--FILE--
<?php
array_diff_uassoc(array(), array(), 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_uassoc(): Argument #3 must be a valid callback, no array or string given in %s
--CLEAN--
<?php

