--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: no arguments triggers ArgumentCountError
--FILE--
<?php
array_fill_keys();
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_fill_keys() expects exactly 2 arguments, 0 given in %s
--CLEAN--
<?php

