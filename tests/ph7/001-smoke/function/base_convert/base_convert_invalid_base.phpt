--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert throws ValueError for a base outside 2..36
--FILE--
<?php
// The last row (a base above 2^32) guards against a 32-bit truncation wrapping
// it back into the valid 2..36 window.
foreach ([[1, 10], [37, 10], [16, 1], [16, 99], [4294967298, 10]] as [$from, $to]) {
    try {
        base_convert("ff", $from, $to);
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)
base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)
base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)
base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)
base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)
--CLEAN--
<?php
