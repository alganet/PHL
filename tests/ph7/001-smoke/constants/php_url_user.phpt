--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_USER constant
--FILE--
<?php
echo "PHP_URL_USER=" . PHP_URL_USER . "\n";
?>
--EXPECTF--
PHP_URL_USER=%d
--CLEAN--
<?php

