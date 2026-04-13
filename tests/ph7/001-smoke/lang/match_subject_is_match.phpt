--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: another match expression can be the subject
--FILE--
<?php
$x = 1;
$r = match (match ($x) { 1 => 'a', 2 => 'b' }) {
    'a' => 'got-a',
    'b' => 'got-b',
};
echo $r, "\n";
$x = 2;
$r2 = match (match ($x) { 1 => 'a', 2 => 'b' }) {
    'a' => 'got-a',
    'b' => 'got-b',
};
echo $r2, "\n";
?>
--EXPECT--
got-a
got-b
--CLEAN--
<?php
unset($x, $r, $r2);
