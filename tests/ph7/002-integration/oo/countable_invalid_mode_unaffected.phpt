--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() ValueError on invalid mode is unaffected by the Countable dispatch path
--FILE--
<?php
try {
    $a = [1, 2, 3];
    count($a, 99);
} catch (ValueError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECTF--
caught: count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE
--CLEAN--
<?php
