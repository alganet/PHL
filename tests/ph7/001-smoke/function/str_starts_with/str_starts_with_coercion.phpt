--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with coerces scalar needle to string
--FILE--
<?php
echo "int="       . (str_starts_with("123abc",   123)   ? 'true' : 'false') . "\n";
echo "intzero="   . (str_starts_with("0xyz",     0)     ? 'true' : 'false') . "\n";
echo "float="     . (str_starts_with("3.14pi",   3.14)  ? 'true' : 'false') . "\n";
echo "booltrue="  . (str_starts_with("1abc",     true)  ? 'true' : 'false') . "\n";
echo "boolfalse=" . (str_starts_with("hello",    false) ? 'true' : 'false') . "\n";
?>
--EXPECT--
int=true
intzero=true
float=true
booltrue=true
boolfalse=true
--CLEAN--
<?php

