--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with no arguments should throw ArgumentCountError
--FILE--
<?php
arsort();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: arsort() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

