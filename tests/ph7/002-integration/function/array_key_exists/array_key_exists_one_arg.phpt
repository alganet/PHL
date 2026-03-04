--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: one argument triggers ArgumentCountError
--FILE--
<?php
array_key_exists('key');
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_key_exists() expects exactly 2 arguments, 1 given in %s
--CLEAN--
<?php

