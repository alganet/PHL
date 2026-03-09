--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace with no arguments throws ArgumentCountError
--FILE--
<?php
array_replace();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_replace() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

