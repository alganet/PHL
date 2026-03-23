--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with no arguments should throw ArgumentCountError
--FILE--
<?php
asort();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: asort() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

