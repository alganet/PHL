--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: string second argument triggers TypeError
--FILE--
<?php
array_key_exists('key', 'not an array');
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: array_key_exists(): Argument #2 ($array) must be of type array, string given in %s
--CLEAN--
<?php

