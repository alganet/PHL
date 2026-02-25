--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ceil() with too many arguments should throw ArgumentCountError
--FILE--
<?php
ceil(1,2);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: ceil() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

