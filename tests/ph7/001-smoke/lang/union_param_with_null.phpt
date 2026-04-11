--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type parameter: int|string|null accepts null
--FILE--
<?php
function upwn_f(int|string|null $x) {
    if (is_null($x))   { echo "null\n"; }
    elseif (is_int($x)) { echo "int:", $x, "\n"; }
    else                { echo "str:[", $x, "]\n"; }
}
upwn_f(null);
upwn_f(0);
upwn_f("");
?>
--EXPECT--
null
int:0
str:[]
--CLEAN--
<?php
