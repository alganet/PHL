--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
argon2 option errors: php's five ValueError shapes in php's order
--DESCRIPTION--
The range checks run in php's order — memory range, time range, thread
validity — then the cross-parameter rule (m < 8p is "too small", raised even
when the thread count is itself unspawnable) and the thread ceiling. Each
message is php's own; all are ValueError.
--FILE--
<?php
foreach ([
    ['memory_cost' => 7],
    ['memory_cost' => PHP_INT_MAX],
    ['time_cost' => 0],
    ['memory_cost' => 7, 'time_cost' => 0],
    ['threads' => 0],
    ['threads' => 16777216],
    ['memory_cost' => 32, 'threads' => 256],
    ['memory_cost' => 16, 'threads' => 4],
] as $opts) {
    try {
        password_hash('x', PASSWORD_ARGON2ID, $opts);
        echo "HASHED\n";
    } catch (ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
Memory cost is outside of allowed memory range
Memory cost is outside of allowed memory range
Time cost is outside of allowed time range
Memory cost is outside of allowed memory range
Invalid number of threads
Invalid number of threads
Memory cost is too small
Memory cost is too small
--CLEAN--
<?php
