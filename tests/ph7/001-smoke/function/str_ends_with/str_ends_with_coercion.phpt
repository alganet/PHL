--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with coerces scalar needle to string
--FILE--
<?php
echo "int="       . (str_ends_with("abc123",    123)   ? 'true' : 'false') . "\n";
echo "intzero="   . (str_ends_with("xyz0",      0)     ? 'true' : 'false') . "\n";
echo "float="     . (str_ends_with("pi=3.14",   3.14)  ? 'true' : 'false') . "\n";
echo "booltrue="  . (str_ends_with("test1",     true)  ? 'true' : 'false') . "\n";
echo "boolfalse=" . (str_ends_with("hello",     false) ? 'true' : 'false') . "\n";
?>
--EXPECT--
int=true
intzero=true
float=true
booltrue=true
boolfalse=true
--CLEAN--
<?php

