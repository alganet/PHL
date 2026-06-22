--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
-- on a readonly property currently holding null still throws (not a silent no-op)
--FILE--
<?php
class RoIncrNull {
    public readonly ?int $x;
    public function __construct() { $this->x = null; }
    public function dec(): void { $this->x--; }
}
(new RoIncrNull)->dec();
?>
--EXPECTF--
%AFatal error:  Uncaught Error: Cannot modify readonly property RoIncrNull::$x%A
--CLEAN--
<?php
