--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto out of a try body runs the finallys it crosses, before jumping
--FILE--
<?php
// one finally, at global scope
foreach ([1, 2, 3] as $v) {
    try { echo "t$v;"; goto out1; }
    finally { echo "f$v;"; }
}
out1:
echo "\n";
// nested finallys drain innermost first
try {
    try { echo "a;"; goto out2; }
    finally { echo "if;"; }
} finally { echo "of;"; }
out2:
echo "\n";
// inside a function
function gotFn() {
    try { echo "t;"; goto out; }
    finally { echo "f;"; }
    out:
    return "r";
}
echo gotFn(), "\n";
// a label inside the same try is an ordinary jump: the finally is NOT run early
try {
    echo "x;";
    goto skip;
    echo "unreachable;";
    skip:
    echo "y;";
} finally { echo "z;"; }
echo "\n";
// a generator body compiles try/catch/finally INLINE — a different finally driver,
// same rule: the crossed finally runs before the jump (this used to crash)
function gotGen() {
    foreach ([1, 2, 3] as $v) {
        try { echo "t$v;"; yield $v; goto gout; }
        finally { echo "f$v;"; }
    }
    gout:
    yield 99;
}
foreach (gotGen() as $x) { echo "y$x;"; }
echo "\n";
?>
--EXPECT--
t1;f1;
a;if;of;
t;f;r
x;y;z;
t1;y1;f1;y99;
--CLEAN--
<?php
