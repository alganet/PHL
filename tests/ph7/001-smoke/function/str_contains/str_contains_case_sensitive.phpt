--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains is case-sensitive
--FILE--
<?php
echo "lower=" . (str_contains("Hello World", "hello") ? 'true' : 'false') . "\n";
echo "upper=" . (str_contains("Hello World", "WORLD") ? 'true' : 'false') . "\n";
?>
--EXPECT--
lower=false
upper=false
--CLEAN--
<?php

