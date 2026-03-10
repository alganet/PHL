--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with more than 2 arguments throws ArgumentCountError
--FILE--
<?php
count(array(), 0, "extra");
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: count() expects at most 2 arguments, 3 given in %s
--CLEAN--
<?php

