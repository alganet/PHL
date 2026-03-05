--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a non-scalar value as strict parameter triggers TypeError
--FILE--
<?php
array_search('x', array(1, 2), array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_search(): Argument #3 ($strict) must be of type bool, %s given in %s
--CLEAN--
<?php

