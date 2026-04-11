--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class union with null: Foo|null behaves like ?Foo
--FILE--
<?php
class UcneFoo { public $tag = "foo"; }

function a(UcneFoo|null $x) { echo $x === null ? "null" : $x->tag, "\n"; }
function b(?UcneFoo $x)     { echo $x === null ? "null" : $x->tag, "\n"; }

a(null);
a(new UcneFoo());
b(null);
b(new UcneFoo());
?>
--EXPECT--
null
foo
null
foo
--CLEAN--
<?php
