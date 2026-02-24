--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes() with too many arguments should raise ArgumentCountError
--FILE--
<?php
addslashes('a','b');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: addslashes() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

