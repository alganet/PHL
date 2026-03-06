--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_keys with no arguments triggers ArgumentCountError
--FILE--
<?php
array_keys();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_keys() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

