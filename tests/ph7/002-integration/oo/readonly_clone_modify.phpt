--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A cloned object's readonly property stays immutable (clone preserves the latch)
--FILE--
<?php
class ReadonlyCloneModify {
    public function __construct(public readonly int $x) {}
    public function set(int $v) { $this->x = $v; }
}
$a = new ReadonlyCloneModify(1);
$b = clone $a;
echo $b->x, "\n";
$b->set(2);
?>
--EXPECTF--
1
%s Fatal error:  Uncaught Error: Cannot modify readonly property ReadonlyCloneModify::$x%A
--CLEAN--
<?php
