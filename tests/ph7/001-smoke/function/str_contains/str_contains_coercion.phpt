--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains coerces scalar needle to string
--FILE--
<?php
echo "int="     . (str_contains("test123",  123)   ? 'true' : 'false') . "\n";
echo "intzero=" . (str_contains("a0b",      0)     ? 'true' : 'false') . "\n";
echo "float="   . (str_contains("pi=3.14",  3.14)  ? 'true' : 'false') . "\n";
echo "booltrue=" . (str_contains("a1b",     true)  ? 'true' : 'false') . "\n";
echo "boolfalse=" . (str_contains("abc",    false) ? 'true' : 'false') . "\n";
?>
--EXPECT--
int=true
intzero=true
float=true
booltrue=true
boolfalse=true
--CLEAN--
<?php

