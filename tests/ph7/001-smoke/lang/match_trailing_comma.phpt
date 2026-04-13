--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: trailing comma after the last arm is allowed
--FILE--
<?php
$r = match (2) {
    1 => 'one',
    2 => 'two',
    3 => 'three',
};
echo $r, "\n";
$r2 = match ('x') {
    'x' => 'ex',
    'y' => 'why',
};
echo $r2, "\n";
?>
--EXPECT--
two
ex
--CLEAN--
<?php
unset($r, $r2);
