--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: abs("123") returns 123 (string->int conversion)
--FILE--
<?php
echo "abs=" . abs("123") . "\n";
?>
--EXPECT--
abs=123
--CLEAN--
<?php

