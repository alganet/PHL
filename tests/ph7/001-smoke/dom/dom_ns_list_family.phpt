--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\NodeList, Dom\NamedNodeMap and Dom\DtdNamedNodeMap: declarations and offset rules
--FILE--
<?php
$dom_nsl_show = static function (string $n): void {
    $r = new ReflectionClass($n);
    echo '== ', $r->getName(), "\n";
    echo '  final=', var_export($r->isFinal(), true),
         ' instantiable=', var_export($r->isInstantiable(), true),
         ' parent=', var_export($r->getParentClass() ? $r->getParentClass()->getName() : null, true), "\n";
    echo '  implements=', implode(',', $r->getInterfaceNames()) ?: '-', "\n";
    foreach ($r->getProperties() as $p) {
        echo '  prop ', ($p->hasType() ? $p->getType() : '?'), ' $', $p->getName(),
             ' virtual=', var_export($p->isVirtual(), true), "\n";
    }
    foreach ($r->getMethods() as $m) {
        $a = [];
        foreach ($m->getParameters() as $q) {
            $a[] = ($q->hasType() ? $q->getType() . ' ' : '') . '$' . $q->getName();
        }
        echo '  ', implode(' ', Reflection::getModifierNames($m->getModifiers())), ' ', $m->getName(),
             '(', implode(', ', $a), ')', ($m->hasReturnType() ? ' : ' . $m->getReturnType() : ''),
             ' tentative=', var_export($m->hasTentativeReturnType(), true), "\n";
    }
};
$dom_nsl_names = ['Dom\NodeList', 'Dom\NamedNodeMap', 'Dom\DtdNamedNodeMap'];
foreach ($dom_nsl_names as $dom_nsl_n) {
    $dom_nsl_show($dom_nsl_n);
}
/* Three classes of their own: neither tree is the other's, and the 2004 pair
 * they shadow stays exactly where it was. */
var_dump(is_a('Dom\NodeList', 'DOMNodeList', true), is_a('DOMNodeList', 'Dom\NodeList', true),
    is_a('Dom\NamedNodeMap', 'DOMNamedNodeMap', true), class_exists('DOM\NODELIST'));

/* A view with nothing behind it is a whole object, as the collection is. */
foreach ($dom_nsl_names as $dom_nsl_n) {
    $dom_nsl_o = new $dom_nsl_n();
    echo '-- ', $dom_nsl_n, "\n";
    /* The object's own handle number is not pinned here: a shared-interpreter
     * run has allocated a different count of objects by this line. */
    var_dump(get_object_vars($dom_nsl_o), (array) $dom_nsl_o,
        count($dom_nsl_o), $dom_nsl_o->length, $dom_nsl_o->item(0));
    var_dump(iterator_to_array($dom_nsl_o->getIterator()));
    foreach ($dom_nsl_o as $dom_nsl_k => $dom_nsl_v) {
        echo "reached $dom_nsl_k\n";
    }
    if (method_exists($dom_nsl_o, 'getNamedItem')) {
        var_dump($dom_nsl_o->getNamedItem('x'), $dom_nsl_o->getNamedItemNS(null, 'x'));
    }
    var_dump(serialize($dom_nsl_o));
    try {
        $dom_nsl_x = clone $dom_nsl_o;
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    /* item() REFUSES out of range on a map and answers null on a list, and the
     * ValueError names the declaring class. */
    foreach ([-1, PHP_INT_MAX] as $dom_nsl_i) {
        try {
            var_dump($dom_nsl_o->item($dom_nsl_i));
        } catch (Throwable $e) {
            echo get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
}

/* The offset rules, which are the namespaced family's own: an int, a float
 * (truncated) and -- for the three by-name views -- a string. A list takes a
 * string only in php's canonical integer spelling. null, bool, array and object
 * are refused outright, where the 2004 classes cast every one of them. */
$dom_nsl_keys = ['int' => 0, 'neg' => -1, 'max' => PHP_INT_MAX, 'float' => 1.9,
    'str' => 'x', 'numstr' => '12', 'padstr' => '007', 'estr' => '',
    'null' => null, 'bool' => true, 'arr' => [1], 'obj' => new stdClass()];
foreach (['Dom\NodeList', 'Dom\NamedNodeMap', 'Dom\DtdNamedNodeMap', 'Dom\HTMLCollection'] as $dom_nsl_n) {
    $dom_nsl_o = new $dom_nsl_n();
    foreach ($dom_nsl_keys as $dom_nsl_t => $dom_nsl_k) {
        foreach (['read', 'isset'] as $dom_nsl_face) {
            try {
                $dom_nsl_v = $dom_nsl_face === 'read' ? $dom_nsl_o[$dom_nsl_k] : isset($dom_nsl_o[$dom_nsl_k]);
                echo $dom_nsl_n, ' [', $dom_nsl_t, '] ', $dom_nsl_face, ' = ', var_export($dom_nsl_v, true), "\n";
            } catch (Throwable $e) {
                echo $dom_nsl_n, ' [', $dom_nsl_t, '] ', $dom_nsl_face, ': ',
                    get_class($e), ': ', $e->getMessage(), "\n";
            }
        }
    }
    /* Every write face is the engine's own sentence -- none of the four words one. */
    foreach (['store', 'unset'] as $dom_nsl_face) {
        try {
            if ($dom_nsl_face === 'store') {
                $dom_nsl_o[0] = 1;
            } else {
                unset($dom_nsl_o[0]);
            }
        } catch (Throwable $e) {
            echo $dom_nsl_n, ' ', $dom_nsl_face, ': ', get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
}
echo "done\n";
--EXPECT--
== Dom\NodeList
  final=false instantiable=true parent=NULL
  implements=IteratorAggregate,Traversable,Countable
  prop int $length virtual=true
  public count() : int tentative=false
  public getIterator() : Iterator tentative=false
  public item(int $index) : ?Dom\Node tentative=false
== Dom\NamedNodeMap
  final=false instantiable=true parent=NULL
  implements=IteratorAggregate,Traversable,Countable
  prop int $length virtual=true
  public item(int $index) : ?Dom\Attr tentative=false
  public getNamedItem(string $qualifiedName) : ?Dom\Attr tentative=false
  public getNamedItemNS(?string $namespace, string $localName) : ?Dom\Attr tentative=false
  public count() : int tentative=false
  public getIterator() : Iterator tentative=false
== Dom\DtdNamedNodeMap
  final=false instantiable=true parent=NULL
  implements=IteratorAggregate,Traversable,Countable
  prop int $length virtual=true
  public item(int $index) : Dom\Entity|Dom\Notation|null tentative=false
  public getNamedItem(string $qualifiedName) : Dom\Entity|Dom\Notation|null tentative=false
  public getNamedItemNS(?string $namespace, string $localName) : Dom\Entity|Dom\Notation|null tentative=false
  public count() : int tentative=false
  public getIterator() : Iterator tentative=false
bool(false)
bool(false)
bool(false)
bool(true)
-- Dom\NodeList
array(0) {
}
array(0) {
}
int(0)
int(0)
NULL
array(0) {
}
string(24) "O:12:"Dom\NodeList":0:{}"
Error: Trying to clone an uncloneable object of class Dom\NodeList
NULL
NULL
-- Dom\NamedNodeMap
array(0) {
}
array(0) {
}
int(0)
int(0)
NULL
array(0) {
}
NULL
NULL
string(28) "O:16:"Dom\NamedNodeMap":0:{}"
Error: Trying to clone an uncloneable object of class Dom\NamedNodeMap
ValueError: Dom\NamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647
ValueError: Dom\NamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647
-- Dom\DtdNamedNodeMap
array(0) {
}
array(0) {
}
int(0)
int(0)
NULL
array(0) {
}
NULL
NULL
string(31) "O:19:"Dom\DtdNamedNodeMap":0:{}"
Error: Trying to clone an uncloneable object of class Dom\DtdNamedNodeMap
ValueError: Dom\DtdNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647
ValueError: Dom\DtdNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647
Dom\NodeList [int] read = NULL
Dom\NodeList [int] isset = false
Dom\NodeList [neg] read = NULL
Dom\NodeList [neg] isset = false
Dom\NodeList [max] read = NULL
Dom\NodeList [max] isset = false
Dom\NodeList [float] read = NULL
Dom\NodeList [float] isset = false
Dom\NodeList [str] read: TypeError: Cannot access offset of type string on Dom\NodeList
Dom\NodeList [str] isset: TypeError: Cannot access offset of type string in isset or empty
Dom\NodeList [numstr] read = NULL
Dom\NodeList [numstr] isset = false
Dom\NodeList [padstr] read: TypeError: Cannot access offset of type string on Dom\NodeList
Dom\NodeList [padstr] isset: TypeError: Cannot access offset of type string in isset or empty
Dom\NodeList [estr] read: TypeError: Cannot access offset of type string on Dom\NodeList
Dom\NodeList [estr] isset: TypeError: Cannot access offset of type string in isset or empty
Dom\NodeList [null] read: TypeError: Cannot access offset of type null on Dom\NodeList
Dom\NodeList [null] isset: TypeError: Cannot access offset of type null in isset or empty
Dom\NodeList [bool] read: TypeError: Cannot access offset of type bool on Dom\NodeList
Dom\NodeList [bool] isset: TypeError: Cannot access offset of type bool in isset or empty
Dom\NodeList [arr] read: TypeError: Cannot access offset of type array on Dom\NodeList
Dom\NodeList [arr] isset: TypeError: Cannot access offset of type array in isset or empty
Dom\NodeList [obj] read: TypeError: Cannot access offset of type stdClass on Dom\NodeList
Dom\NodeList [obj] isset: TypeError: Cannot access offset of type stdClass in isset or empty
Dom\NodeList store: Error: Cannot use object of type Dom\NodeList as array
Dom\NodeList unset: Error: Cannot use object of type Dom\NodeList as array
Dom\NamedNodeMap [int] read = NULL
Dom\NamedNodeMap [int] isset = false
Dom\NamedNodeMap [neg] read = NULL
Dom\NamedNodeMap [neg] isset = false
Dom\NamedNodeMap [max] read = NULL
Dom\NamedNodeMap [max] isset = false
Dom\NamedNodeMap [float] read = NULL
Dom\NamedNodeMap [float] isset = false
Dom\NamedNodeMap [str] read = NULL
Dom\NamedNodeMap [str] isset = false
Dom\NamedNodeMap [numstr] read = NULL
Dom\NamedNodeMap [numstr] isset = false
Dom\NamedNodeMap [padstr] read = NULL
Dom\NamedNodeMap [padstr] isset = false
Dom\NamedNodeMap [estr] read = NULL
Dom\NamedNodeMap [estr] isset = false
Dom\NamedNodeMap [null] read: TypeError: Cannot access offset of type null on Dom\NamedNodeMap
Dom\NamedNodeMap [null] isset: TypeError: Cannot access offset of type null in isset or empty
Dom\NamedNodeMap [bool] read: TypeError: Cannot access offset of type bool on Dom\NamedNodeMap
Dom\NamedNodeMap [bool] isset: TypeError: Cannot access offset of type bool in isset or empty
Dom\NamedNodeMap [arr] read: TypeError: Cannot access offset of type array on Dom\NamedNodeMap
Dom\NamedNodeMap [arr] isset: TypeError: Cannot access offset of type array in isset or empty
Dom\NamedNodeMap [obj] read: TypeError: Cannot access offset of type stdClass on Dom\NamedNodeMap
Dom\NamedNodeMap [obj] isset: TypeError: Cannot access offset of type stdClass in isset or empty
Dom\NamedNodeMap store: Error: Cannot use object of type Dom\NamedNodeMap as array
Dom\NamedNodeMap unset: Error: Cannot use object of type Dom\NamedNodeMap as array
Dom\DtdNamedNodeMap [int] read = NULL
Dom\DtdNamedNodeMap [int] isset = false
Dom\DtdNamedNodeMap [neg] read = NULL
Dom\DtdNamedNodeMap [neg] isset = false
Dom\DtdNamedNodeMap [max] read = NULL
Dom\DtdNamedNodeMap [max] isset = false
Dom\DtdNamedNodeMap [float] read = NULL
Dom\DtdNamedNodeMap [float] isset = false
Dom\DtdNamedNodeMap [str] read = NULL
Dom\DtdNamedNodeMap [str] isset = false
Dom\DtdNamedNodeMap [numstr] read = NULL
Dom\DtdNamedNodeMap [numstr] isset = false
Dom\DtdNamedNodeMap [padstr] read = NULL
Dom\DtdNamedNodeMap [padstr] isset = false
Dom\DtdNamedNodeMap [estr] read = NULL
Dom\DtdNamedNodeMap [estr] isset = false
Dom\DtdNamedNodeMap [null] read: TypeError: Cannot access offset of type null on Dom\DtdNamedNodeMap
Dom\DtdNamedNodeMap [null] isset: TypeError: Cannot access offset of type null in isset or empty
Dom\DtdNamedNodeMap [bool] read: TypeError: Cannot access offset of type bool on Dom\DtdNamedNodeMap
Dom\DtdNamedNodeMap [bool] isset: TypeError: Cannot access offset of type bool in isset or empty
Dom\DtdNamedNodeMap [arr] read: TypeError: Cannot access offset of type array on Dom\DtdNamedNodeMap
Dom\DtdNamedNodeMap [arr] isset: TypeError: Cannot access offset of type array in isset or empty
Dom\DtdNamedNodeMap [obj] read: TypeError: Cannot access offset of type stdClass on Dom\DtdNamedNodeMap
Dom\DtdNamedNodeMap [obj] isset: TypeError: Cannot access offset of type stdClass in isset or empty
Dom\DtdNamedNodeMap store: Error: Cannot use object of type Dom\DtdNamedNodeMap as array
Dom\DtdNamedNodeMap unset: Error: Cannot use object of type Dom\DtdNamedNodeMap as array
Dom\HTMLCollection [int] read = NULL
Dom\HTMLCollection [int] isset = false
Dom\HTMLCollection [neg] read = NULL
Dom\HTMLCollection [neg] isset = false
Dom\HTMLCollection [max] read = NULL
Dom\HTMLCollection [max] isset = false
Dom\HTMLCollection [float] read = NULL
Dom\HTMLCollection [float] isset = false
Dom\HTMLCollection [str] read = NULL
Dom\HTMLCollection [str] isset = false
Dom\HTMLCollection [numstr] read = NULL
Dom\HTMLCollection [numstr] isset = false
Dom\HTMLCollection [padstr] read = NULL
Dom\HTMLCollection [padstr] isset = false
Dom\HTMLCollection [estr] read = NULL
Dom\HTMLCollection [estr] isset = false
Dom\HTMLCollection [null] read: TypeError: Cannot access offset of type null on Dom\HTMLCollection
Dom\HTMLCollection [null] isset: TypeError: Cannot access offset of type null in isset or empty
Dom\HTMLCollection [bool] read: TypeError: Cannot access offset of type bool on Dom\HTMLCollection
Dom\HTMLCollection [bool] isset: TypeError: Cannot access offset of type bool in isset or empty
Dom\HTMLCollection [arr] read: TypeError: Cannot access offset of type array on Dom\HTMLCollection
Dom\HTMLCollection [arr] isset: TypeError: Cannot access offset of type array in isset or empty
Dom\HTMLCollection [obj] read: TypeError: Cannot access offset of type stdClass on Dom\HTMLCollection
Dom\HTMLCollection [obj] isset: TypeError: Cannot access offset of type stdClass in isset or empty
Dom\HTMLCollection store: Error: Cannot use object of type Dom\HTMLCollection as array
Dom\HTMLCollection unset: Error: Cannot use object of type Dom\HTMLCollection as array
done
