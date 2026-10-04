--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\ChildNode, Dom\ParentNode and Dom\HTMLCollection: the namespaced API's declarations
--FILE--
<?php
$dom_nsc_show = static function (string $n): void {
    $r = new ReflectionClass($n);
    echo '== ', $r->getName(), "\n";
    echo '  interface=', var_export($r->isInterface(), true),
         ' final=', var_export($r->isFinal(), true),
         ' instantiable=', var_export($r->isInstantiable(), true), "\n";
    echo '  implements=', implode(',', $r->getInterfaceNames()) ?: '-', "\n";
    foreach ($r->getProperties() as $p) {
        echo '  prop ', ($p->hasType() ? $p->getType() : '?'), ' $', $p->getName(),
             ' virtual=', var_export($p->isVirtual(), true),
             ' default=', var_export($p->hasDefaultValue(), true), "\n";
    }
    foreach ($r->getMethods() as $m) {
        $a = [];
        foreach ($m->getParameters() as $q) {
            $a[] = ($q->hasType() ? $q->getType() . ' ' : '') . ($q->isVariadic() ? '...' : '') . '$' . $q->getName();
        }
        echo '  ', implode(' ', Reflection::getModifierNames($m->getModifiers())), ' ', $m->getName(),
             '(', implode(', ', $a), ')', ($m->hasReturnType() ? ' : ' . $m->getReturnType() : ''),
             ' tentative=', var_export($m->hasTentativeReturnType(), true), "\n";
    }
};
foreach (['Dom\ChildNode', 'Dom\ParentNode', 'Dom\HTMLCollection'] as $dom_nsc_name) {
    $dom_nsc_show($dom_nsc_name);
}
/* The names are class names: case folds, and the old API's own two mixins stay
 * distinct classes beside them. */
var_dump(interface_exists('DOM\CHILDNODE'), interface_exists('dom\parentnode'),
    class_exists('dom\htmlcollection'));
var_dump(interface_exists('DOMChildNode'), interface_exists('DOMParentNode'));
var_dump(is_a('Dom\HTMLCollection', 'DOMNodeList', true));

/* A collection with nothing behind it is a whole object in php: it counts zero,
 * answers null at every index and every name, and iterates empty. */
$dom_nsc_c = new Dom\HTMLCollection();
var_dump(count($dom_nsc_c), $dom_nsc_c->length, $dom_nsc_c->item(0), $dom_nsc_c->item(7),
    $dom_nsc_c->namedItem('x'), $dom_nsc_c->namedItem(''));
var_dump(iterator_to_array($dom_nsc_c->getIterator()));
foreach ($dom_nsc_c as $dom_nsc_k => $dom_nsc_v) {
    echo "reached $dom_nsc_k\n";
}
var_dump($dom_nsc_c instanceof Countable, $dom_nsc_c instanceof IteratorAggregate,
    $dom_nsc_c instanceof Traversable, $dom_nsc_c instanceof Iterator);
var_dump(get_object_vars($dom_nsc_c), (array) $dom_nsc_c);
try {
    $dom_nsc_c->item(-1);
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
var_dump($dom_nsc_c->item(-1));
echo "done\n";
--EXPECT--
== Dom\ChildNode
  interface=true final=false instantiable=false
  implements=-
  abstract public remove() : void tentative=false
  abstract public before(Dom\Node|string ...$nodes) : void tentative=false
  abstract public after(Dom\Node|string ...$nodes) : void tentative=false
  abstract public replaceWith(Dom\Node|string ...$nodes) : void tentative=false
== Dom\ParentNode
  interface=true final=false instantiable=false
  implements=-
  abstract public append(Dom\Node|string ...$nodes) : void tentative=false
  abstract public prepend(Dom\Node|string ...$nodes) : void tentative=false
  abstract public replaceChildren(Dom\Node|string ...$nodes) : void tentative=false
  abstract public querySelector(string $selectors) : ?Dom\Element tentative=false
  abstract public querySelectorAll(string $selectors) : Dom\NodeList tentative=false
== Dom\HTMLCollection
  interface=false final=false instantiable=true
  implements=IteratorAggregate,Traversable,Countable
  prop int $length virtual=true default=false
  public item(int $index) : ?Dom\Element tentative=false
  public namedItem(string $key) : ?Dom\Element tentative=false
  public count() : int tentative=false
  public getIterator() : Iterator tentative=false
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
int(0)
int(0)
NULL
NULL
NULL
NULL
array(0) {
}
bool(true)
bool(true)
bool(true)
bool(false)
array(0) {
}
array(0) {
}
NULL
done
