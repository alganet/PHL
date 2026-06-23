--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: array index target
--FILE--
<?php
$arr = [];
["k" => $arr[0]] = ["k" => 8];
echo $arr[0], "\n";
?>
--EXPECT--
8
--CLEAN--
<?php
unset($arr);
