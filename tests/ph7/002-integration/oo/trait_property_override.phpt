--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class property from trait with compatible defaults
--FILE--
<?php
trait Defaults {
    public $name = "same";
    public $extra = "from trait";
}
class MyClass {
    use Defaults;
    public $name = "same";
}
$obj = new MyClass();
echo $obj->name, "\n";
echo $obj->extra, "\n";
?>
--EXPECT--
same
from trait
--CLEAN--
<?php
