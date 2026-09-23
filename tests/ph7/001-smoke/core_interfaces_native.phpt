--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The eleven core interfaces are declared from C, with php's typed contracts
--DESCRIPTION--
The contracts themselves are the whole surface here: no method has a body, so
every difference is in the DECLARATION. php's `interface Throwable extends
Stringable` is the load-bearing one — the embedded PHP redeclared __toString()
on Throwable instead, so no Exception was ever Stringable — and the rest is the
type information a chunk cannot express: a TENTATIVE return type on nearly every
method of ArrayAccess/Countable/JsonSerializable/Iterator/IteratorAggregate,
`mixed` on ArrayAccess's offsets, and php's declaration ORDER, which Reflection
prints. An interface also never carries php's `<iterateable>` tag: php installs
the iteration handler when a CLASS implements Traversable, so the tag follows
the implementer and not the contract.
--FILE--
<?php
function ifaceSig(ReflectionMethod $m) {
    $args = [];
    foreach ($m->getParameters() as $p) {
        $args[] = ($p->hasType() ? $p->getType() . ' ' : '') . '$' . $p->getName();
    }
    $ret = '';
    if ($m->hasReturnType()) {
        $ret = ': ' . $m->getReturnType();
    } elseif ($m->hasTentativeReturnType()) {
        $ret = ': @' . $m->getTentativeReturnType();
    }
    return ($m->isStatic() ? 'static ' : '') . $m->getName()
        . '(' . implode(', ', $args) . ')' . $ret
        . ($m->getDeclaringClass()->getName() === $m->class ? '' : '');
}
function ifaceDump($name) {
    $r = new ReflectionClass($name);
    $parents = $r->getInterfaceNames();
    echo $name, $parents ? ' extends ' . implode(', ', $parents) : '', "\n";
    foreach ($r->getMethods() as $m) {
        echo '  ', ifaceSig($m),
            $m->getDeclaringClass()->getName() === $name
                ? '' : '   [inherits ' . $m->getDeclaringClass()->getName() . ']',
            "\n";
    }
}

echo "-- the declarations, in php's own order\n";
foreach (['Traversable', 'Stringable', 'Throwable', 'ArrayAccess', 'Countable',
          'JsonSerializable', 'UnitEnum', 'BackedEnum', 'Iterator',
          'IteratorAggregate', 'Serializable'] as $i) {
    ifaceDump($i);
}

echo "-- Throwable extends Stringable, so every exception is one\n";
var_dump(new Exception('x') instanceof Stringable);
var_dump(new TypeError('t') instanceof Stringable);
var_dump(is_subclass_of('Throwable', 'Stringable'));
var_dump((new ReflectionClass('Throwable'))->implementsInterface('Stringable'));

echo "-- <iterateable> follows the implementer, never the contract\n";
interface IfaceContract extends Iterator {}
class IfaceWalk implements Iterator {
    public function current(): mixed { return 1; }
    public function next(): void {}
    public function key(): mixed { return 0; }
    public function valid(): bool { return false; }
    public function rewind(): void {}
}
foreach (['Iterator', 'IfaceContract', 'IteratorAggregate', 'IfaceWalk'] as $c) {
    $line = strtok((string)new ReflectionClass($c), "\n");
    echo str_contains($line, '<iterateable>') ? 'tagged  ' : 'plain   ', $c, "\n";
}

echo "-- the contracts still bind\n";
class IfaceCount implements Countable { public function count(): int { return 7; } }
var_dump(count(new IfaceCount));
class IfaceOffsets implements ArrayAccess {
    public function offsetExists(mixed $o): bool { return $o === 'k'; }
    public function offsetGet(mixed $o): mixed { return "[$o]"; }
    public function offsetSet(mixed $o, mixed $v): void { echo "set $o=$v\n"; }
    public function offsetUnset(mixed $o): void { echo "unset $o\n"; }
}
$ifaceArr = new IfaceOffsets;
var_dump(isset($ifaceArr['k']), isset($ifaceArr['z']), $ifaceArr['q']);
$ifaceArr['a'] = 1;
unset($ifaceArr['a']);
class IfaceJson implements JsonSerializable { public function jsonSerialize(): mixed { return ['j' => 1]; } }
echo json_encode(new IfaceJson), "\n";
class IfaceAgg implements IteratorAggregate {
    public function getIterator(): Traversable { return new ArrayIterator([4, 5]); }
}
foreach (new IfaceAgg as $v) { echo $v; }
echo "\n";
enum IfaceEnum: string { case Up = 'u'; }
var_dump(IfaceEnum::from('u') === IfaceEnum::Up, IfaceEnum::tryFrom('nope'),
    IfaceEnum::Up instanceof BackedEnum, IfaceEnum::Up instanceof UnitEnum);
--EXPECT--
-- the declarations, in php's own order
Traversable
Stringable
  __toString(): string
Throwable extends Stringable
  getMessage(): string
  getCode()
  getFile(): string
  getLine(): int
  getTrace(): array
  getPrevious(): ?Throwable
  getTraceAsString(): string
  __toString(): string   [inherits Stringable]
ArrayAccess
  offsetExists(mixed $offset): @bool
  offsetGet(mixed $offset): @mixed
  offsetSet(mixed $offset, mixed $value): @void
  offsetUnset(mixed $offset): @void
Countable
  count(): @int
JsonSerializable
  jsonSerialize(): @mixed
UnitEnum
  static cases(): array
BackedEnum extends UnitEnum
  static from(string|int $value): static
  static tryFrom(string|int $value): ?static
  static cases(): array   [inherits UnitEnum]
Iterator extends Traversable
  current(): @mixed
  next(): @void
  key(): @mixed
  valid(): @bool
  rewind(): @void
IteratorAggregate extends Traversable
  getIterator(): @Traversable
Serializable
  serialize()
  unserialize(string $data)
-- Throwable extends Stringable, so every exception is one
bool(true)
bool(true)
bool(true)
bool(true)
-- <iterateable> follows the implementer, never the contract
plain   Iterator
plain   IfaceContract
plain   IteratorAggregate
tagged  IfaceWalk
-- the contracts still bind
int(7)
bool(true)
bool(false)
string(3) "[q]"
set a=1
unset a
{"j":1}
45
bool(true)
NULL
bool(true)
bool(true)
