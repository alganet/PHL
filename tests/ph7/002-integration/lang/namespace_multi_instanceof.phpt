--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
instanceof resolves against correct namespace in multi-namespace files
--FILE--
<?php
namespace A;
class Base {}

namespace B;
use A\Base;

class Child extends Base {}

$c = new Child();

// instanceof with unqualified name should use the import, not namespace B
echo ($c instanceof Base) ? "yes" : "no", "\n";

// instanceof with FQN
echo ($c instanceof \A\Base) ? "yes" : "no", "\n";

// instanceof with own class
echo ($c instanceof Child) ? "yes" : "no", "\n";
echo ($c instanceof \B\Child) ? "yes" : "no", "\n";
?>
--EXPECT--
yes
yes
yes
yes
--CLEAN--
<?php
