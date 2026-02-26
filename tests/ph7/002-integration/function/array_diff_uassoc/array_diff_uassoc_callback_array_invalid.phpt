--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
When only two arguments are provided the second is treated as callback; an
array of wrong shape should trigger the "array callback must have exactly two members" message.
--FILE--
<?php
// use a three-element array so PHP complains about the wrong number of members
array_diff_uassoc(array(), array('a','b','c'));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_uassoc(): Argument #2 must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php

