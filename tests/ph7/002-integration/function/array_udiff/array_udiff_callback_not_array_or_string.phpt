--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should throw TypeError when callback argument is neither array nor string
--FILE--
<?php
array_udiff(array(1), array(2), 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #3 must be a valid callback, no array or string given in %s
--CLEAN--
<?php

