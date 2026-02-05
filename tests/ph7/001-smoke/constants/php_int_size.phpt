--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_INT_SIZE constant value
--FILE--
<?php
echo "PHP_INT_SIZE=" . PHP_INT_SIZE . "\n";
?>
--EXPECT--
PHP_INT_SIZE=8
--CLEAN--
<?php

