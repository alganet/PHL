--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect propagates an exception thrown by the comparison callback and unwinds
--FILE--
<?php
try {
    array_uintersect([1, 2, 3], [2, 3], function ($a, $b) {
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
