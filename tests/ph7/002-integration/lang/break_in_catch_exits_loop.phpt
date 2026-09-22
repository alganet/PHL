--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break inside a catch leaves the loop (every loop kind), it does not re-run the iteration
--FILE--
<?php
// foreach
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "fe$v;"; break; }
}
echo "\n";
// for
for ($i = 0; $i < 3; $i++) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "for$i;"; break; }
}
echo "\n";
// while
$n = 0;
while (true) {
    $n++;
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "wh$n;"; break; }
}
echo "\n";
// do..while
$d = 0;
do {
    $d++;
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "do$d;"; break; }
} while ($d < 10);
echo "\n";
// the throw comes from a nested call, several frames below the try
function bicBoom() { throw new Exception("deep"); }
function bicRun() {
    $seen = [];
    foreach ([1, 2, 3] as $v) {
        try { bicBoom(); }
        catch (Exception $e) { $seen[] = "d$v"; break; }
    }
    return implode(",", $seen);
}
echo bicRun(), "\n";
// break inside a catch inside a switch inside a loop targets the switch
foreach ([1, 2] as $v) {
    switch ($v) {
        default:
            try { throw new Exception("x"); }
            catch (Exception $e) { echo "sw$v;"; break; }
            echo "unreachable;";
    }
    echo "after$v;";
}
echo "\n";
?>
--EXPECT--
fe1;
for0;
wh1;
do1;
d1
sw1;after1;sw2;after2;
--CLEAN--
<?php
