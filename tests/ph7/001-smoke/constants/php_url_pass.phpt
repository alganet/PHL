--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_PASS constant
--FILE--
<?php
echo "PHP_URL_PASS=" . PHP_URL_PASS . "\n";
?>
--EXPECTF--
PHP_URL_PASS=%d
--CLEAN--
<?php

