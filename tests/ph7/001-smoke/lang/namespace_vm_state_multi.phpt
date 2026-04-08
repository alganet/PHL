--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Functions and classes resolve correctly in multi-namespace files
--FILE--
<?php
namespace A;

function afunc() { return "A-func"; }
class AClass { function id() { return "A-class"; } }

echo afunc(), "\n";
$a = new AClass();
echo $a->id(), "\n";

namespace B;

function bfunc() { return "B-func"; }
class BClass { function id() { return "B-class"; } }

echo bfunc(), "\n";
$b = new BClass();
echo $b->id(), "\n";

// Cross-namespace access via FQN
echo \A\afunc(), "\n";
$a2 = new \A\AClass();
echo $a2->id(), "\n";
?>
--EXPECT--
A-func
A-class
B-func
B-class
A-func
A-class
--CLEAN--
<?php
