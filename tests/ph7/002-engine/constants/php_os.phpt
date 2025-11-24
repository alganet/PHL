--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_OS constant value
--FILE--
<?php
echo "PHP_OS=" . PHP_OS . "\n";
?>
--EXPECTF--
PHP_OS=%s
