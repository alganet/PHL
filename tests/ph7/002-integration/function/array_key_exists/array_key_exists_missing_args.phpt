--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: no arguments triggers ArgumentCountError
--FILE--
<?php
array_key_exists();
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_key_exists() expects exactly 2 arguments, 0 given in %s
--CLEAN--
<?php

