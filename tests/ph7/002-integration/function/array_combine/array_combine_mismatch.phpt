--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with mismatched arrays should throw ValueError
--FILE--
<?php
array_combine(array(1), array(1,2));
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements in %s
--CLEAN--
<?php

