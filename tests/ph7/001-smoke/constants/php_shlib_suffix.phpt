--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_SHLIB_SUFFIX is 'so' on Unix-like
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') echo 'skip';
?>
--FILE--
<?php
echo "PHP_SHLIB_SUFFIX=" . PHP_SHLIB_SUFFIX . "\n";
?>
--EXPECT--
PHP_SHLIB_SUFFIX=so
--CLEAN--
<?php

