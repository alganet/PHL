--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed class constant may declare an INTERSECTION type
--DESCRIPTION--
The peek that decides whether a `const` is typed knew a name followed by another
name or by `|`, and neither `&` nor a leading `(`. So `const A&B K = ...` was read
as an UNTYPED constant named after the first member and refused with
`Expected '=' after class constant A`, and the DNF spelling was refused as an
invalid constant name. The refusal the type then earns is in
class_const_intersection_refused.phpt -- a class fatal is raised at declaration,
so it cannot share a file with anything that has to run.
--FILE--
<?php
class CciC implements Countable, ArrayAccess {
    public function count(): int { return 0; }
    public function offsetExists(mixed $o): bool { return false; }
    public function offsetGet(mixed $o): mixed { return null; }
    public function offsetSet(mixed $o, mixed $v): void {}
    public function offsetUnset(mixed $o): void {}
}
class CciA {
    const (Countable&ArrayAccess)|int J = 7;
}
var_dump(CciA::J);
var_dump((string) (new ReflectionClassConstant('CciA', 'J'))->getType());
echo "accepted\n";
?>
--EXPECT--
int(7)
string(27) "(Countable&ArrayAccess)|int"
accepted
--CLEAN--
<?php
