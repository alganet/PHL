--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two arguments and second is not an array or string should trigger appropriate message
--FILE--
<?php
array_diff_uassoc(array(), 5);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_uassoc(): Argument #2 must be a valid callback, no array or string given in %s
--CLEAN--
<?php

