--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scope keyword is an intersection member only where php can name it
--DESCRIPTION--
php substitutes the resolved class NAME for `self`/`parent` in an intersection
while the body compiles, so the type stands in a named class, interface or enum
-- and is refused wherever there is no name to put there: a closure, a trait, an
anonymous class. `static` is refused everywhere, its called class being unknown
until the call. This engine accepted all of them. The shapes php ACCEPTS are in
type_scope_keyword_accepts.phpt -- a compile fatal runs nothing, so they cannot
share a file with one.
--FILE--
<?php
class TsiC implements Countable, ArrayAccess {
    public function count(): int { return 0; }
    public function offsetExists(mixed $o): bool { return false; }
    public function offsetGet(mixed $o): mixed { return null; }
    public function offsetSet(mixed $o, mixed $v): void {}
    public function offsetUnset(mixed $o): void {}
}
trait TsiT { public function f(self&Countable $a) {} }
echo "unreachable\n";
?>
--EXPECTF--
%AType self cannot be part of an intersection type%A
--CLEAN--
<?php
