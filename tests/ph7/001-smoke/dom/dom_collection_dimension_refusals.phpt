--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A collection subscript reads and does nothing else, php's own split
--FILE--
<?php
// php gave the two collections a READ handler only, so every spelling that
// would STORE through the subscript is the plain `Cannot use object of type C
// as array` an object without ArrayAccess gets -- a store, an append, an
// unset, a compound assign and the `??=` whose read came back empty. A
// compound assign gets it even for an offset the READ refuses, because php's
// pair asks the reader first and reports the store's Error when it answers
// nothing. `$list[0]++` is neither: the read hands back an object, the
// increment fails on the object, and nothing is written.
$doc = new DOMDocument;
$doc->loadXML('<r a="1"><k/><j/></r>');
$list = $doc->documentElement->childNodes;
$map = $doc->documentElement->attributes;

function dom_dim_refusal($label, callable $body) {
    try {
        $body();
        echo "$label => ran\n";
    } catch (Throwable $ex) {
        echo "$label => " . get_class($ex) . ': ' . $ex->getMessage() . "\n";
    }
}
dom_dim_refusal('$list[0] = 1',    function () use ($list) { $list[0] = 1; });
dom_dim_refusal('$list[] = 1',     function () use ($list) { $list[] = 1; });
dom_dim_refusal('$map["a"] = 1',   function () use ($map) { $map['a'] = 1; });
dom_dim_refusal('unset($list[0])', function () use ($list) { unset($list[0]); });
dom_dim_refusal('unset($map["a"])',function () use ($map) { unset($map['a']); });
dom_dim_refusal('$list[9] .= "x"', function () use ($list) { $list[9] .= 'x'; });
dom_dim_refusal('$list[0] .= "x"', function () use ($list) { $list[0] .= 'x'; });
dom_dim_refusal('$map[-1] .= "x"', function () use ($map) { $map[-1] .= 'x'; });
dom_dim_refusal('$list[9] += 1',   function () use ($list) { $list[9] += 1; });
dom_dim_refusal('$list[] .= "x"',  function () use ($list) { $list[] .= 'x'; });
dom_dim_refusal('$list[9] ??= 1',  function () use ($list) { $list[9] ??= 1; });
dom_dim_refusal('$map[-1] ??= 1',  function () use ($map) { $map[-1] ??= 1; });
dom_dim_refusal('$list[0] ??= 1',  function () use ($list) {
    var_dump(($list[0] ??= 1)->nodeName);
});
dom_dim_refusal('$list[0]++',      function () use ($list) { $list[0]++; });
dom_dim_refusal('$map[-1]++',      function () use ($map) { $map[-1]++; });

// A by-reference argument is php's write-context fetch: the notice fires for
// the NULL a miss leaves and stays silent for a node, which is a handle and
// loses nothing. The read still happens where the subscript is written.
echo "=== by-reference argument ===\n";
set_error_handler(function ($n, $s) { echo "  notice: $s\n"; return true; });
$take = function (&$slot) { $slot = 'written'; };
$take($list[0]);
$take($list[9]);
$take($map[0]);
restore_error_handler();
echo $doc->saveXML($doc->documentElement), "\n";

// The keyless spelling reaches the handler with no offset at all, and php has
// its own sentence for that one.
echo "=== keyless ===\n";
dom_dim_refusal('$take($list[])', function () use ($list, $take) { $take($list[]); });

// A user subclass inherits the HANDLER, php's rule -- so its own offsetGet is
// not consulted for a read even though it declares ArrayAccess, while its
// offsetSet still takes the write the parent has no room for.
echo "=== subclass ===\n";
class DomDimSubList extends DOMNodeList implements ArrayAccess {
    public function offsetExists(mixed $o): bool { echo "  offsetExists\n"; return true; }
    public function offsetGet(mixed $o): mixed { echo "  offsetGet\n"; return 'userland'; }
    public function offsetSet(mixed $o, mixed $v): void { echo "  offsetSet\n"; }
    public function offsetUnset(mixed $o): void { echo "  offsetUnset\n"; }
}
$sub = new DomDimSubList;
var_dump($sub[0], isset($sub[0]), count($sub));
$sub[0] = 1;
unset($sub[0]);
// ...and the STORE half of every shape that ends in one goes to it too: the
// read answered nothing, and where the collections themselves have nowhere to
// put the value the subclass does.
$sub[9] ??= 1;
$sub[9] .= 'x';
set_error_handler(function ($n, $s) { echo "  notice: $s\n"; return true; });
$sub[9]++;
restore_error_handler();

// A collection with nothing behind it still screens the range first.
echo "=== unowned ===\n";
var_dump((new DOMNodeList)[0], (new DOMNodeList)[-1], (new DOMNamedNodeMap)[0]);
dom_dim_refusal('(new DOMNamedNodeMap)[-1]', function () { (new DOMNamedNodeMap)[-1]; });
?>
--EXPECT--
$list[0] = 1 => Error: Cannot use object of type DOMNodeList as array
$list[] = 1 => Error: Cannot use object of type DOMNodeList as array
$map["a"] = 1 => Error: Cannot use object of type DOMNamedNodeMap as array
unset($list[0]) => Error: Cannot use object of type DOMNodeList as array
unset($map["a"]) => Error: Cannot use object of type DOMNamedNodeMap as array
$list[9] .= "x" => Error: Cannot use object of type DOMNodeList as array
$list[0] .= "x" => Error: Object of class DOMElement could not be converted to string
$map[-1] .= "x" => Error: Cannot use object of type DOMNamedNodeMap as array
$list[9] += 1 => Error: Cannot use object of type DOMNodeList as array
$list[] .= "x" => Error: Cannot use object of type DOMNodeList as array
$list[9] ??= 1 => Error: Cannot use object of type DOMNodeList as array
$map[-1] ??= 1 => ValueError: must be between 0 and 2147483647
string(1) "k"
$list[0] ??= 1 => ran
$list[0]++ => TypeError: Cannot increment DOMElement
$map[-1]++ => ValueError: must be between 0 and 2147483647
=== by-reference argument ===
  notice: Indirect modification of overloaded element of DOMNodeList has no effect
<r a="1"><k/><j/></r>
=== keyless ===
$take($list[]) => Error: Cannot access DOMNodeList without offset
=== subclass ===
NULL
bool(false)
int(0)
  offsetSet
  offsetUnset
  offsetSet
  offsetSet
  notice: Indirect modification of overloaded element of DomDimSubList has no effect
=== unowned ===
NULL
NULL
NULL
(new DOMNamedNodeMap)[-1] => ValueError: must be between 0 and 2147483647
