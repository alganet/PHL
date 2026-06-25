--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_object_id() hands out sequential ids starting at 1 (objects held alive)
--FILE--
<?php
class C {}
$a = new C; $b = new C; $c = new C;
echo spl_object_id($a), "\n";
echo spl_object_id($b), "\n";
echo spl_object_id($c), "\n";
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
