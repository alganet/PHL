--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in interfaces: ArrayAccess, Countable, Stringable, Traversable, UnitEnum, BackedEnum are all declared
--FILE--
<?php
foreach (["ArrayAccess","Countable","Stringable","Traversable","UnitEnum","BackedEnum"] as $name) {
    echo $name, ": ", interface_exists($name) ? "yes" : "no", "\n";
}
?>
--EXPECT--
ArrayAccess: yes
Countable: yes
Stringable: yes
Traversable: yes
UnitEnum: yes
BackedEnum: yes
--CLEAN--
<?php
