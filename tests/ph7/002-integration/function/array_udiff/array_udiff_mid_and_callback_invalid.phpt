--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause

--TEST--
array_udiff validates the callback (last arg) before the middle arrays, so a bad callback is named even when a middle array is also invalid (was a bare skip; PHL checked arrays first and named the wrong argument)
--FILE--
<?php
array_udiff(array(1), "not an array", 123);
?>
--EXPECTF--
%AFatal error:%AUncaught TypeError: array_udiff(): Argument #3 must be a valid callback, no array or string given%A
--CLEAN--
<?php

