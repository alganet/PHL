--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator that loops with a yield inside a try resumes correctly across many iterations (send + throw)
--FILE--
<?php
// Repeatedly re-entering a try around the yield must not corrupt the generator
// (the resumed body frame must survive OP_POP_EXCEPTION) nor accumulate frames.
function g() {
    $sum = 0;
    while (true) {
        try {
            $v = yield $sum;
        } catch (Exception $e) {
            $v = 0; // a throw contributes nothing
        }
        $sum += $v;
    }
}
$g = g();
$g->current();
$total = 0;
for ($i = 1; $i <= 50; $i++) {
    if ($i % 4 == 0) {
        $cur = $g->throw(new Exception("skip"));
    } else {
        $cur = $g->send($i);
        $total += $i;
    }
}
echo "cur=$cur\n";
echo "total=$total\n";
echo "cur==total: ", var_export($cur === $total, true), "\n";
?>
--EXPECT--
cur=963
total=963
cur==total: true
