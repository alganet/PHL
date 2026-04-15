--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: assigning an object to int-typed promoted property throws TypeError
--FILE--
<?php
class CppMismatch {
    public function __construct(public int $x) {}
}
class CppThing {}
$m = new CppMismatch(1);
try {
    $m->x = new CppThing();
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo $m->x, "\n";
?>
--EXPECT--
caught: Cannot assign CppThing to property CppMismatch::$x of type int
1
--CLEAN--
<?php
