--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_uassoc propagates an exception thrown by the key-comparison callback and unwinds
--FILE--
<?php
try {
    array_diff_uassoc(['a' => 1, 'b' => 2], ['a' => 1], function ($a, $b) {
        throw new Exception('boom');
    });
    echo 'NOT REACHED' . PHP_EOL;
} catch (Exception $e) {
    echo 'caught: ' . $e->getMessage() . PHP_EOL;
}
echo 'after' . PHP_EOL;
?>
--EXPECT--
caught: boom
after
