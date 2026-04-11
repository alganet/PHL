--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type parameter: Foo|Bar accepts instances of either class
--FILE--
<?php
class UfFoo { public $name = "foo"; }
class UfBar { public $name = "bar"; }

function take(UfFoo|UfBar $x) {
    echo $x->name, "\n";
}
take(new UfFoo());
take(new UfBar());
?>
--EXPECT--
foo
bar
--CLEAN--
<?php
