--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: default arm catches unmatched values
--FILE--
<?php
$check = function ($v) {
    return match ($v) {
        1 => 'one',
        2 => 'two',
        default => 'other',
    };
};
echo $check(1), "\n";
echo $check(2), "\n";
echo $check(99), "\n";
echo $check('hello'), "\n";
?>
--EXPECT--
one
two
other
other
--CLEAN--
<?php
unset($check);
