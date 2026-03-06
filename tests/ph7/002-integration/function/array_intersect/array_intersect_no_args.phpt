--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect with no arguments triggers ArgumentCountError
--FILE--
<?php
array_intersect();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_intersect() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

