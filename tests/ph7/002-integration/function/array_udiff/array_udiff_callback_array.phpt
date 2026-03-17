--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should throw TypeError when callback is an array with wrong shape
--FILE--
<?php
array_udiff(array(1), array(2));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #2 must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php

