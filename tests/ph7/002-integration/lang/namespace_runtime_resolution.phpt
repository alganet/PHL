--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Runtime namespace state tracks execution point, not last compiled
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
namespace A;
class Foo { function id() { return "A-Foo"; } }

namespace B;
class Foo { function id() { return "B-Foo"; } }

namespace B;
$class = "Foo";
$obj = new $class();
echo $obj->id(), "\n";

namespace A;
$class2 = "Foo";
$obj2 = new $class2();
echo $obj2->id(), "\n";
?>
--EXPECT--
B-Foo
A-Foo
--CLEAN--
<?php
