--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PHP_URL_HOST should exist
--FILE--
<?php
echo PHP_URL_HOST . "\n";
?>
--EXPECTF--
%d
--CLEAN--
<?php

