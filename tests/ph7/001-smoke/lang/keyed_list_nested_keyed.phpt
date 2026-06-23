--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: keyed nested in keyed
--FILE--
<?php
["a" => ["b" => $z]] = ["a" => ["b" => 7]];
echo "$z\n";
?>
--EXPECT--
7
--CLEAN--
<?php
unset($z);
