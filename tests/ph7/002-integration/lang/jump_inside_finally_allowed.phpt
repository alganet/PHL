--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a jump that stays INSIDE a finally is legal, and so is return — only leaving one is not
--FILE--
<?php
// break targeting a loop declared inside the finally
foreach ([1, 2] as $v) {
    try { echo "t$v;"; }
    finally {
        foreach ([1, 2, 3] as $w) { if ($w == 2) { break; } echo "w$w;"; }
        echo "f$v;";
    }
}
echo "\n";
// continue targeting a loop inside the finally
try { echo "t;"; }
finally { for ($i = 0; $i < 3; $i++) { if ($i == 1) { continue; } echo "i$i;"; } }
echo "\n";
// goto to a label inside the same finally
try { echo "t;"; }
finally { goto in; echo "unreachable;"; in: echo "f;"; }
echo "\n";
// return from a finally is php-legal (it even overrides the try's value)
function jifFn() {
    try { echo "t;"; return "try"; }
    finally { return "fin"; }
}
echo jifFn(), "\n";
?>
--EXPECT--
t1;w1;f1;t2;w1;f2;
t;i0;i2;
t;f;
t;fin
--CLEAN--
<?php
