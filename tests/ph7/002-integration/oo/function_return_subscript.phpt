--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expression dereferencing: function return subscript
--FILE--
<?php
function getArr() {
    return [10, 20, 30];
}
echo getArr()[0], "\n";
echo getArr()[1], "\n";
echo getArr()[2], "\n";
?>
--EXPECT--
10
20
30
--CLEAN--
<?php
