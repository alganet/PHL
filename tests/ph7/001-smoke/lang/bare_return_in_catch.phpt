--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bare return inside a catch returns from the function with no fall-through
--FILE--
<?php
function rcfBare() {
    echo "x";
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        return;
    }
    echo "AFTER";
}
$r = rcfBare();
echo "\n";
echo ($r === null ? "isnull" : "notnull") . "\n";
?>
--EXPECT--
x
isnull
--CLEAN--
<?php
