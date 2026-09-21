--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types: PHL accepts-and-materializes a whole float into int (flags cannot tell 1.0 from 4/2 — a recorded divergence)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
declare(strict_types=1);
// php strict rejects the float literal, but PHL's dual-flagged whole-real
// shape also comes out of arithmetic/builtins where php produces a genuine
// INT (pow(2,3) is php int(8), PHL whole-real float(8)). PHL cannot tell
// them apart by flags, so strict mode accepts BOTH and materializes the
// int — rejecting would break the php-valid wrsInt(pow(2,3)). Leniency,
// recorded.
function wrsInt(int $a) { var_dump($a); }
wrsInt(pow(2,3));
wrsInt(1.0);
wrsInt(5);
echo "done\n";
?>
--EXPECT--
int(8)
int(1)
int(5)
done
--CLEAN--
<?php
