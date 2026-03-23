--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options with no arguments throws ArgumentCountError
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
assert_options();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: assert_options() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

