--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with no arguments throws ArgumentCountError
--FILE--
<?php
count();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: count() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

