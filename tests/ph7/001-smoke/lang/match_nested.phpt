--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: nested match inside another match arm
--FILE--
<?php
$outer = 1;
$inner = 'b';
$r = match ($outer) {
    1 => match ($inner) {
        'a' => 'one-a',
        'b' => 'one-b',
        default => 'one-?',
    },
    2 => 'two',
    default => 'other',
};
echo $r, "\n";
?>
--EXPECT--
one-b
--CLEAN--
<?php
unset($outer, $inner, $r);
