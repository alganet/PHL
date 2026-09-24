--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mt_srand()'s $mode picks the GENERATOR: MT_RAND_PHP is php's legacy twist, its legacy range scaling and its two deprecations
--FILE--
<?php
// MT_RAND_PHP and MT_RAND_MT19937 were both undefined constants, so the call that
// asks for php's legacy generator -- the only reason the argument exists -- was a
// fatal, and spelled with a literal 1 it was accepted and DROPPED: the program
// got the modern sequence and no diagnostic, which is the one outcome a caller
// reproducing recorded numbers cannot detect.
// php 8.3 deprecated both the symbol and the mode, and the notices are collected
// through a handler rather than printed: an error handler sees them in both
// engines, where DISPLAY depends on each installation's error_reporting.
$mtx_dep = [];
set_error_handler(function ($mtx_n, $mtx_m) use (&$mtx_dep) { $mtx_dep[$mtx_m] = ($mtx_dep[$mtx_m] ?? 0) + 1; return true; });
var_dump(MT_RAND_MT19937, MT_RAND_PHP);

// php's legacy twist reads the low bit of the previous word, so the sequence is
// a different one from the first draw on...
foreach ([MT_RAND_MT19937, MT_RAND_PHP] as $mtx_mode) {
    mt_srand(42, $mtx_mode);
    $mtx_raw = [];
    for ($mtx_i = 0; $mtx_i < 6; $mtx_i++) { $mtx_raw[] = mt_rand(); }
    echo "mode $mtx_mode raw: ", implode(' ', $mtx_raw), "\n";
}

// ...and the RANGE mapping is part of the same choice: php keeps its old
// scale-through-a-double for MT_RAND_PHP, which is biased and is what the
// recorded numbers were produced with.
foreach ([MT_RAND_MT19937, MT_RAND_PHP] as $mtx_mode) {
    foreach ([[1, 100], [0, 1], [-5, -1], [0, PHP_INT_MAX], [7, 7]] as [$mtx_lo, $mtx_hi]) {
        mt_srand(1234, $mtx_mode);
        $mtx_vals = [];
        for ($mtx_i = 0; $mtx_i < 5; $mtx_i++) { $mtx_vals[] = mt_rand($mtx_lo, $mtx_hi); }
        echo "mode $mtx_mode range($mtx_lo,$mtx_hi): ", implode(' ', $mtx_vals), "\n";
    }
}

// A null seed is "no seed": php reseeds from entropy for it, where this read it
// as the integer 0 -- so `mt_srand($cfg['seed'] ?? null)` pinned every run to one
// sequence.
mt_srand(null); $mtx_a = mt_rand();
mt_srand(null); $mtx_b = mt_rand();
mt_srand(0);    $mtx_c = mt_rand();
var_dump($mtx_a === $mtx_b, $mtx_a === $mtx_c);

// A span wider than the signed range wraps around the minimum in php's legacy
// scaling rather than saturating.
foreach ([[PHP_INT_MIN, PHP_INT_MAX], [0, PHP_INT_MAX], [PHP_INT_MIN, 0]] as [$mtx_lo, $mtx_hi]) {
    mt_srand(1, MT_RAND_PHP);
    echo "wide($mtx_lo,$mtx_hi): ", mt_rand($mtx_lo, $mtx_hi), ' ', mt_rand($mtx_lo, $mtx_hi), "\n";
}

// srand() is the same function under another name, mode included.
srand(7, MT_RAND_PHP);
echo 'srand PHP-mode: ', rand(), ' ', rand(1, 1000), "\n";
srand(7, MT_RAND_MT19937);
echo 'srand default:  ', rand(), ' ', rand(1, 1000), "\n";

// php compares the argument against MT_RAND_PHP for EQUALITY, so every other
// value -- an out-of-range one included -- selects the standard generator, and
// seeding again with no mode at all goes back to it.
foreach ([0, 1, 2, 5, -1, PHP_INT_MAX, PHP_INT_MIN] as $mtx_mode) {
    mt_srand(1, $mtx_mode);
    echo "mode $mtx_mode => ", mt_rand(), "\n";
}
mt_srand(1);
echo 'no mode => ', mt_rand(), "\n";

restore_error_handler();
ksort($mtx_dep);
foreach ($mtx_dep as $mtx_msg => $mtx_count) { echo "deprecated x$mtx_count: $mtx_msg\n"; }
?>
--EXPECT--
int(0)
int(1)
mode 0 raw: 804318771 1710563033 2041643438 393923207 1571945013 1674373667
mode 1 raw: 1354439493 1710563033 2041643438 1748058097 586813251 478617429
mode 0 range(1,100): 76 72 7 66 17
mode 0 range(0,1): 1 1 0 1 0
mode 0 range(-5,-1): -5 -4 -4 -5 -4
mode 0 range(0,9223372036854775807): 9180274287129881391 5863084412769568038 2068099408570805452 5005706978665513624 6653183006627102351
mode 0 range(7,7): 7 7 7 7 7
mode 1 range(1,100): 82 50 63 19 56
mode 1 range(0,1): 1 0 1 0 1
mode 1 range(-5,-1): -1 -3 -2 -5 -3
mode 1 range(0,9223372036854775807): 7478298516360527872 4590137141006172160 5737940599374348288 1721812583209500672 5137625379665608704
mode 1 range(7,7): 7 7 7 7 7
bool(false)
bool(false)
wide(-9223372036854775808,9223372036854775807): 1465392573097967616 -9092651073658683392
wide(0,9223372036854775807): 5344382304976371712 65360481598046208
wide(-9223372036854775808,0): -3878989731878404096 -9158011555256729600
srand PHP-mode: 1989119265 769
srand default:  163870807 893
mode 0 => 895547922
mode 1 => 1244335972
mode 2 => 895547922
mode 5 => 895547922
mode -1 => 895547922
mode 9223372036854775807 => 895547922
mode -9223372036854775808 => 895547922
no mode => 895547922
deprecated x7: Constant MT_RAND_PHP is deprecated since 8.3, as it uses a biased non-standard variant of Mt19937
deprecated x11: The MT_RAND_PHP variant of Mt19937 is deprecated
