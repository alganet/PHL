--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match populates a declared untyped property passed as a by-ref target
--FILE--
<?php
class PregMatchPropDeclared { public $p; }
$o = new PregMatchPropDeclared();
$r = preg_match('/(\w+) (\w+)/', 'Hello World', $o->p);
echo $r . "\n";
echo $o->p[0] . "\n";
echo $o->p[1] . "\n";
echo $o->p[2] . "\n";
?>
--EXPECT--
1
Hello World
Hello
World
--CLEAN--
<?php
