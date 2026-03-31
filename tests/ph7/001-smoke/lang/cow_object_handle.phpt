--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: objects still pass by handle (mutation visible)
--FILE--
<?php
class cow_object_handleFoo { public $v = 1; }
$a = new cow_object_handleFoo();
$b = $a;
$b->v = 99;
echo $a->v . "\n";
echo $b->v . "\n";
?>
--EXPECT--
99
99
--CLEAN--
<?php
unset($a, $b);

