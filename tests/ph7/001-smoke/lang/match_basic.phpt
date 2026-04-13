--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: single-value arms assigned to a variable
--FILE--
<?php
$v = 2;
$r = match ($v) {
    1 => 'one',
    2 => 'two',
    3 => 'three',
};
echo $r, "\n";
?>
--EXPECT--
two
--CLEAN--
<?php
unset($v, $r);
