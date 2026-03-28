--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Leading backslash bypasses namespace resolution and use imports
--FILE--
<?php
namespace App;

class Foo { function id() { return "App-Foo"; } }

namespace Other;

// use import for Foo
use App\Foo;

// \App\Foo is absolute - should always resolve to App\Foo, not Other\App\Foo
$a = new \App\Foo();
echo $a->id(), "\n";

// Verify use import still works for unqualified
$b = new Foo();
echo $b->id(), "\n";
?>
--EXPECT--
App-Foo
App-Foo
--CLEAN--
<?php
