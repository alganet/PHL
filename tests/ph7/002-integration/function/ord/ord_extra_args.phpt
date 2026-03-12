--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord() with too many arguments should emit ArgumentCountError
--FILE--
<?php
ord('a', 'b');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: ord() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

