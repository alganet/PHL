--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: implements interface + instanceof
--FILE--
<?php
interface AnonIfaceA { function f(); }
$o = new class implements AnonIfaceA { function f() { return "ok"; } };
echo $o->f(), " ", var_export($o instanceof AnonIfaceA, true), "\n";
?>
--EXPECT--
ok true
--CLEAN--
<?php
unset($o);
