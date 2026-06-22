--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: modifying a declared property after init is a fatal Error
--FILE--
<?php
readonly class RoClassModify {
    public int $x;
    public function __construct() { $this->x = 1; }
}
$o = new RoClassModify();
echo $o->x, "\n";
$o->x = 2;
?>
--EXPECTF--
1
%s Fatal error:  Uncaught Error: Cannot modify readonly property RoClassModify::$x%A
--CLEAN--
<?php
