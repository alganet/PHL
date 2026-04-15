--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: engine-raised ArgumentCountError caught via Throwable
--FILE--
<?php
try {
    abs();
} catch (Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
ArgumentCountError:abs() expects exactly 1 argument, 0 given
--CLEAN--
<?php
