--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: reserved keywords are accepted as labels
--FILE--
<?php
function naklf($class, $type, $default) {
    echo "class=$class type=$type default=$default\n";
}
naklf(class: "Foo", type: "int", default: "x");
naklf(default: "y", type: "str", class: "Bar");
?>
--EXPECT--
class=Foo type=int default=x
class=Bar type=str default=y
--CLEAN--
<?php
