--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullable parameter combined with pass-by-reference preserves both flags
--FILE--
<?php
function setOrDefault(?string &$val, string $default): void {
    if ($val === null) {
        $val = $default;
    }
}

$x = null;
setOrDefault($x, "hello");
echo $x . "\n";

$y = "original";
setOrDefault($y, "hello");
echo $y . "\n";
?>
--EXPECT--
hello
original
--CLEAN--
<?php

