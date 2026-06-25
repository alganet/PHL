--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match populates a typed property that has a default (already initialized)
--FILE--
<?php
class PregMatchPropTypedDefault { public array $q = []; }
$o = new PregMatchPropTypedDefault();
$r = preg_match('/(\w+)/', 'Hi', $o->q);
echo $r . "\n";
echo $o->q[0] . "\n";
echo $o->q[1] . "\n";
?>
--EXPECT--
1
Hi
Hi
--CLEAN--
<?php
