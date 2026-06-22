--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
++ on an uninitialized readonly property reports the read error first (ordering parity)
--FILE--
<?php
class RoIncrUninit {
    public readonly int $x;
    public function bump(): void { $this->x++; }
}
(new RoIncrUninit)->bump();
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: Typed property RoIncrUninit::$x must not be accessed before initialization%A
--CLEAN--
<?php
