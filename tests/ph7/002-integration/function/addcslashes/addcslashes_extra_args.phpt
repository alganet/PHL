--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes() with too many arguments should raise ArgumentCountError
--FILE--
<?php
addcslashes('a','b','c');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: addcslashes() expects exactly 2 arguments, %d given in %s
--CLEAN--
<?php

