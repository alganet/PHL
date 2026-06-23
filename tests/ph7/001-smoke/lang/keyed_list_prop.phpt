--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: object property target
--FILE--
<?php
class KeyedListProp { public $p; }
$o = new KeyedListProp;
["k" => $o->p] = ["k" => 5];
echo $o->p, "\n";
?>
--EXPECT--
5
--CLEAN--
<?php
unset($o);
