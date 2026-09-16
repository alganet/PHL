--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Top-level constant declarations with complex constant expressions (the guard said "requires PH7 builtin parser" but really hid a call in a const expression, which php rejects; that case now lives in 002-integration/parse/const_function)
--FILE--
<?php
const CSTMT_WELCOME = "Hello" . " " . "World";
const CSTMT_FIVE = 2 + 3;
const CSTMT_ARR = [1, 2, 3];
const CSTMT_REF = CSTMT_FIVE * 2;
const CSTMT_BOOL = 10 > 3;
const CSTMT_TERNARY = true ? 'yes' : 'no';

echo CSTMT_WELCOME, "\n";
echo CSTMT_FIVE, "\n";
echo CSTMT_ARR[2], "\n";
echo CSTMT_REF, "\n";
echo var_export(CSTMT_BOOL, true), "\n";
echo CSTMT_TERNARY, "\n";
?>
--EXPECT--
Hello World
5
3
10
true
yes
--CLEAN--
<?php
