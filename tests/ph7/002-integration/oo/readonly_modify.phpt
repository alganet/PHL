--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Modifying a readonly property after initialization is a fatal Error
--FILE--
<?php
class ReadonlyModify {
    public function __construct(public readonly int $x) {}
}
$o = new ReadonlyModify(1);
echo $o->x, "\n";
$o->x = 2;
?>
--EXPECTF--
1
%s Fatal error:  Uncaught Error: Cannot modify readonly property ReadonlyModify::$x%A
--CLEAN--
<?php
