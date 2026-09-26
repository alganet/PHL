--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Destructuring an object reads its dimensions, both spellings
--FILE--
<?php
// `[$a, $b] = $obj` is a subscript READ per POSITION in php -- one
// read_dimension call for each target that is there, skipping the holes -- so
// an ArrayAccess implementer, an ArrayObject and (since php 8.3's handlers) a
// DOMNodeList all come apart the way an array does. PHL took EVERY object for
// a non-array source: it warned `Cannot use object as array` and assigned NULL
// to every target, so the shape every modern SPL and DOM example is written in
// silently produced nulls. The KEYED spelling was the same answer for every
// source except the one shape that reached the writable-container fast path
// underneath. An object with no dimension reader at all is php's catchable
// Error, not that warning.
class DestructureLog implements ArrayAccess {
    public array $data;
    public function __construct(array $data) { $this->data = $data; }
    public function offsetExists(mixed $o): bool { echo "  exists\n"; return isset($this->data[$o]); }
    public function offsetGet(mixed $o): mixed {
        echo "  get(", var_export($o, true), ")\n";
        return $this->data[$o] ?? null;
    }
    public function offsetSet(mixed $o, mixed $v): void { $this->data[$o] = $v; }
    public function offsetUnset(mixed $o): void { unset($this->data[$o]); }
}
class DestructurePlain { public $k = 1; }

// The label goes first so the accessor's own lines land under it in the order
// php calls them, which is what this pins.
function destructure_say(string $label, callable $body) {
    echo "$label:\n";
    try {
        $said = $body();
        echo "  => $said\n";
    } catch (Throwable $ex) {
        echo "  => ", get_class($ex), ': ', $ex->getMessage(), "\n";
    }
}

$acc = new DestructureLog([10, 20, 30]);
destructure_say('[$a,$b]', function () use ($acc) {
    [$a, $b] = $acc; return "$a/$b";
});
destructure_say('[,,$c] skips the holes', function () use ($acc) {
    [, , $c] = $acc; return $c;
});
destructure_say('[$a,,$c]', function () use ($acc) {
    [$a, , $c] = $acc; return "$a/$c";
});
destructure_say('past the end', function () use ($acc) {
    [$a, $b, $c, $d] = $acc;
    return var_export([$a, $b, $c, $d], true);
});
destructure_say('list() spelling', function () use ($acc) {
    list($a, $b) = $acc; return "$a/$b";
});
// The KEYED spelling goes through the same accessor.
destructure_say('["k"=>$a]', function () {
    $named = new DestructureLog(['k' => 'K', 'j' => 'J']);
    ['j' => $b, 'k' => $a] = $named; return "$a/$b";
});
// An object with no dimension reader: php's Error, raised before any target is
// written, in BOTH spellings.
destructure_say('[$a] = plain object', function () {
    [$a] = new DestructurePlain; return 'ran';
});
destructure_say('["k"=>$a] = plain object', function () {
    ['k' => $a] = new DestructurePlain; return 'ran';
});
// A target keeps its previous value when the source refuses, and the refusal
// is routed like every other catchable Error raised mid-expression: the
// enclosing try catches it and the script CARRIES ON. (It did not: the
// destructure's own throw path had no arm for a try in the same frame, so a
// caught refusal at top level ended the script in silence.)
$kept = 'untouched';
try { [$kept] = new DestructurePlain; } catch (Throwable $ex) { echo "  caught\n"; }
var_dump($kept);
echo "still running\n";
function destructure_in_function() {
    try { ['k' => $a] = new DestructurePlain; } catch (Throwable $ex) { echo "  caught inside\n"; }
    return 'returned';
}
echo destructure_in_function(), "\n";
try {
    try { [$a] = new DestructurePlain; } catch (TypeError $ex) { echo "  wrong handler\n"; }
} catch (Error $ex) {
    echo "  caught by the outer try\n";
}
foreach ([1, 2] as $round) {
    try { [$a] = new DestructurePlain; } catch (Throwable $ex) { echo "  round $round\n"; }
}

// An accessor that THROWS mid-run abandons the destructure where php abandons
// it: the targets already assigned keep what they got, the one whose read
// failed and every later one keep what they had.
echo "=== a throwing accessor ===\n";
class DestructureBoom implements ArrayAccess {
    public function offsetExists(mixed $o): bool { return true; }
    public function offsetGet(mixed $o): mixed {
        echo "  get($o)\n";
        if ($o == 1) { throw new RuntimeException('boom'); }
        return "v$o";
    }
    public function offsetSet(mixed $o, mixed $v): void { }
    public function offsetUnset(mixed $o): void { }
}
$one = 'ONE'; $two = 'TWO'; $three = 'THREE';
try { [$one, $two, $three] = new DestructureBoom; }
catch (Throwable $ex) { echo "  caught: ", $ex->getMessage(), "\n"; }
var_dump($one, $two, $three);

// The engine's own containers, and the two DOM collections whose handlers are
// not ArrayAccess at all.
echo "=== engine containers ===\n";
[$x, $y] = new ArrayObject([1, 2]);
var_dump($x, $y);
$fixed = new SplFixedArray(2);
$fixed[0] = 'f0';
[$p] = $fixed;
var_dump($p);
$doc = new DOMDocument;
$doc->loadXML('<r a="1" b="2"><k/><j/></r>');
[$first, $second] = $doc->documentElement->childNodes;
var_dump($first->nodeName, $second->nodeName);
['a' => $attr] = $doc->documentElement->attributes;
var_dump($attr->nodeName);
?>
--EXPECT--
[$a,$b]:
  get(0)
  get(1)
  => 10/20
[,,$c] skips the holes:
  get(2)
  => 30
[$a,,$c]:
  get(0)
  get(2)
  => 10/30
past the end:
  get(0)
  get(1)
  get(2)
  get(3)
  => array (
  0 => 10,
  1 => 20,
  2 => 30,
  3 => NULL,
)
list() spelling:
  get(0)
  get(1)
  => 10/20
["k"=>$a]:
  get('j')
  get('k')
  => K/J
[$a] = plain object:
  => Error: Cannot use object of type DestructurePlain as array
["k"=>$a] = plain object:
  => Error: Cannot use object of type DestructurePlain as array
  caught
string(9) "untouched"
still running
  caught inside
returned
  caught by the outer try
  round 1
  round 2
=== a throwing accessor ===
  get(0)
  get(1)
  caught: boom
string(2) "v0"
string(3) "TWO"
string(5) "THREE"
=== engine containers ===
int(1)
int(2)
string(2) "f0"
string(1) "k"
string(1) "j"
string(1) "a"
