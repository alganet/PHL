--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
continue inside a catch resumes the loop at the right place (for's post-expression included)
--FILE--
<?php
// foreach: every iteration catches and continues
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "c$v;"; continue; }
    echo "unreachable;";
}
echo "\n";
// for: the continue must still run the post-expression ($i++), or this never ends
for ($i = 0; $i < 4; $i++) {
    try {
        if ($i == 1) { throw new Exception("x"); }
        echo "t$i;";
    } catch (Exception $e) { echo "c$i;"; continue; }
    echo "a$i;";
}
echo "\n";
// while
$n = 0;
while ($n < 5) {
    $n++;
    try {
        if ($n % 2) { throw new Exception("x"); }
        echo "e$n;";
    } catch (Exception $e) { echo "o$n;"; continue; }
    echo "t$n;";
}
echo "\n";
// do..while: continue re-tests the condition, break ends it
$d = 0;
do {
    $d++;
    try { throw new Exception("x"); }
    catch (Exception $e) {
        echo "d$d;";
        if ($d < 3) { continue; }
        break;
    }
} while ($d < 10);
echo "\n";
// continue 2 out of a catch in the inner loop
foreach ([1, 2] as $a) {
    foreach ([1, 2] as $b) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "c$a$b;"; continue 2; }
    }
    echo "unreachable;";
}
echo "\n";
?>
--EXPECT--
c1;c2;c3;
t0;a0;c1;t2;a2;t3;a3;
o1;e2;t2;o3;e4;t4;o5;
d1;d2;d3;
c11;c21;
--CLEAN--
<?php
