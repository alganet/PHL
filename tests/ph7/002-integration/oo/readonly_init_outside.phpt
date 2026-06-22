--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Initializing a readonly property from outside the class scope is a fatal Error
--FILE--
<?php
class ReadonlyInitOutside {
    public readonly int $x;
}
$o = new ReadonlyInitOutside();
$o->x = 5;
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: %Areadonly property ReadonlyInitOutside::$x from global scope%A
--CLEAN--
<?php
