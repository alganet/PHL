--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dynamic class names do not resolve through namespaces
--FILE--
<?php
namespace App;
class Foo { function id() { return "App-Foo"; } }

namespace App;
// Dynamic with FQN works
$class = "App\\Foo";
$obj = new $class();
echo $obj->id(), "\n";

// Short name is NOT resolved dynamically through namespace
echo class_exists("Foo") ? "BUG" : "not resolved", "\n";
echo class_exists("App\\Foo") ? "fqn exists" : "BUG", "\n";
?>
--EXPECT--
App-Foo
not resolved
fqn exists
--CLEAN--
<?php
