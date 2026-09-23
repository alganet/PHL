--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`[]` is a write target only, and a call argument settles it at runtime
--FILE--
<?php
// Every placement php ACCEPTS.
$a = [];
$a[] = 1;
$a[][] = 2;
$a[][0] = 3;
$a[1][] = 4;
$a[] += 5;
$a[]++;
$x = 9;
$a[] =& $x;
foreach ([6] as $a[]) {
}
[$a[]] = [7];
list($a[]) = [8];
echo count($a), "\n";

$m = [];
preg_match("/x/", "x", $m[]);
echo count($m[0]), "\n";

// A by-REFERENCE parameter appends and binds — php cannot know that at compile
// time, which is why this one placement is a runtime decision.
function append_byref(&$slot) { $slot = "bound"; }
$b = [];
append_byref($b[]);
print_r($b);

$c = ["k" => []];
append_byref($c["k"][]);
print_r($c);

append_byref($undefined_base[]);
print_r($undefined_base);

// ...and a by-VALUE one is php's Error. Declared BELOW the call on purpose: with
// the signature already compiled php refuses the call outright at compile time,
// where PHL always decides at the call itself.
$d = [];
try {
    append_byval($d[]);
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

function append_byval($v) { echo "never\n"; }
?>
--EXPECT--
9
1
Array
(
    [0] => bound
)
Array
(
    [k] => Array
        (
            [0] => bound
        )

)
Array
(
    [0] => bound
)
Error: Cannot use [] for reading
--CLEAN--
<?php
unset($a, $b, $c, $d, $m, $x, $undefined_base);
