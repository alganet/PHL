--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
10k-deep recursion running inside an include'd file (BYTECODE.md stage 3)
--DESCRIPTION--
include executes via VmEvalChunk/VmLocalExec (a native re-entry); the deep
PHP recursion inside it must still be heap-bound, and a second 10k descent
that calls include at the bottom exercises depth accounting across the
include boundary.
--SKIPIF--
<?php if (!getenv('PHL_MAX_RECURSION')) { echo "skip needs PHL_MAX_RECURSION (run: make test-stress)"; } ?>
--FILE--
<?php
$inc = __DIR__ . '/recursion_10k_include.inc.tmp.php';
file_put_contents($inc,
    '<?php function deepInc(int $n): int { return $n === 0 ? 0 : 1 + deepInc($n - 1); } return deepInc(10000);');
echo "in-include=", include($inc), "\n";
function descendThenInclude(int $n, string $inc): int {
    if ($n === 0) {
        return include($inc); // include at the bottom of a 10k chain
    }
    return descendThenInclude($n - 1, $inc);
}
// deepInc is already defined by the first include; make the second chunk reuse it
file_put_contents($inc, '<?php return deepInc(10000);');
echo "include-at-depth=", descendThenInclude(10000, $inc), "\n";
unlink($inc);
?>
--EXPECT--
in-include=10000
include-at-depth=10000
--CLEAN--
<?php
@unlink(__DIR__ . '/recursion_10k_include.inc.tmp.php');
