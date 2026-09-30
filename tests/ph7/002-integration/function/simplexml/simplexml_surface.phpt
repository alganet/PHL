--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/simplexml: the inventory, the two classes and their signatures
--FILE--
<?php
/* ext/simplexml's declared surface: two classes, three functions, one door
 * from ext/dom, and not one property on any of it. */
echo "-- the extension\n";
var_dump(extension_loaded('SimpleXML'));
echo implode(', ', get_extension_funcs('SimpleXML')), "\n";
$ext = new ReflectionExtension('SimpleXML');
echo implode(', ', $ext->getClassNames()), "\n";
var_dump($ext->getConstants());

echo "-- every function's signature\n";
foreach (['simplexml_load_file', 'simplexml_load_string', 'simplexml_import_dom',
          'dom_import_simplexml'] as $name) {
    echo (new ReflectionFunction($name))->__toString(), "\n";
}
echo "dom_import_simplexml belongs to ",
     (new ReflectionFunction('dom_import_simplexml'))->getExtensionName(), "\n";

echo "-- the class\n";
$c = new ReflectionClass('SimpleXMLElement');
printf("final=%d abstract=%d parent=%s interfaces=%s\n",
    (int) $c->isFinal(), (int) $c->isAbstract(),
    var_export($c->getParentClass() ? $c->getParentClass()->getName() : null, true),
    implode(',', $c->getInterfaceNames()));
printf("properties=%d constants=%d\n", count($c->getProperties()), count($c->getConstants()));
foreach ($c->getMethods() as $m) {
    echo $m->__toString(), "\n";
}

echo "-- SimpleXMLIterator adds nothing\n";
$i = new ReflectionClass('SimpleXMLIterator');
printf("parent=%s own=%d interfaces=%s\n", $i->getParentClass()->getName(),
    count(array_filter($i->getMethods(), fn($m) => $m->getDeclaringClass()->getName() === 'SimpleXMLIterator')),
    implode(',', $i->getInterfaceNames()));

echo "-- what an instance is NOT\n";
$x = simplexml_load_string('<r a="1"><c>one</c></r>');
var_dump($x instanceof ArrayAccess, $x instanceof JsonSerializable,
         $x instanceof Countable, $x instanceof Traversable, $x instanceof Stringable);
var_dump(property_exists($x, '__res'), property_exists($x, 'c'), isset($x->c));
try { serialize($x); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- the class name travels\n";
class SxSurfaceKid extends SimpleXMLElement {}
$k = simplexml_load_string('<r><c>1</c></r>', 'SxSurfaceKid');
echo get_class($k), ' ', get_class($k->c), ' ', get_class($k->c[0]), ' ',
     get_class($k->children()), ' ', get_class($k->attributes()), "\n";
foreach (['NoSuchClassAtAll', 'stdClass'] as $bad) {
    try { simplexml_load_string('<r/>', $bad); }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
--EXPECT--
-- the extension
bool(true)
simplexml_load_file, simplexml_load_string, simplexml_import_dom
SimpleXMLElement, SimpleXMLIterator
array(0) {
}
-- every function's signature
Function [ <internal:SimpleXML> function simplexml_load_file ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $filename ]
    Parameter #1 [ <optional> ?string $class_name = SimpleXMLElement::class ]
    Parameter #2 [ <optional> int $options = 0 ]
    Parameter #3 [ <optional> string $namespace_or_prefix = "" ]
    Parameter #4 [ <optional> bool $is_prefix = false ]
  }
  - Return [ SimpleXMLElement|false ]
}

Function [ <internal:SimpleXML> function simplexml_load_string ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> ?string $class_name = SimpleXMLElement::class ]
    Parameter #2 [ <optional> int $options = 0 ]
    Parameter #3 [ <optional> string $namespace_or_prefix = "" ]
    Parameter #4 [ <optional> bool $is_prefix = false ]
  }
  - Return [ SimpleXMLElement|false ]
}

Function [ <internal:SimpleXML> function simplexml_import_dom ] {

  - Parameters [2] {
    Parameter #0 [ <required> object $node ]
    Parameter #1 [ <optional> ?string $class_name = SimpleXMLElement::class ]
  }
  - Return [ ?SimpleXMLElement ]
}

Function [ <internal:dom> function dom_import_simplexml ] {

  - Parameters [1] {
    Parameter #0 [ <required> object $node ]
  }
  - Return [ DOMAttr|DOMElement ]
}

dom_import_simplexml belongs to dom
-- the class
final=0 abstract=0 parent=NULL interfaces=Stringable,Countable,RecursiveIterator,Traversable,Iterator
properties=0 constants=0
Method [ <internal:SimpleXML> public method xpath ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $expression ]
  }
  - Tentative return [ array|false|null ]
}

Method [ <internal:SimpleXML> public method registerXPathNamespace ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $prefix ]
    Parameter #1 [ <required> string $namespace ]
  }
  - Tentative return [ bool ]
}

Method [ <internal:SimpleXML> public method asXML ] {

  - Parameters [1] {
    Parameter #0 [ <optional> ?string $filename = null ]
  }
  - Tentative return [ string|bool ]
}

Method [ <internal:SimpleXML> public method saveXML ] {

  - Parameters [1] {
    Parameter #0 [ <optional> ?string $filename = null ]
  }
  - Tentative return [ string|bool ]
}

Method [ <internal:SimpleXML> public method getNamespaces ] {

  - Parameters [1] {
    Parameter #0 [ <optional> bool $recursive = false ]
  }
  - Tentative return [ array ]
}

Method [ <internal:SimpleXML> public method getDocNamespaces ] {

  - Parameters [2] {
    Parameter #0 [ <optional> bool $recursive = false ]
    Parameter #1 [ <optional> bool $fromRoot = true ]
  }
  - Tentative return [ array|false ]
}

Method [ <internal:SimpleXML> public method children ] {

  - Parameters [2] {
    Parameter #0 [ <optional> ?string $namespaceOrPrefix = null ]
    Parameter #1 [ <optional> bool $isPrefix = false ]
  }
  - Tentative return [ ?SimpleXMLElement ]
}

Method [ <internal:SimpleXML> public method attributes ] {

  - Parameters [2] {
    Parameter #0 [ <optional> ?string $namespaceOrPrefix = null ]
    Parameter #1 [ <optional> bool $isPrefix = false ]
  }
  - Tentative return [ ?SimpleXMLElement ]
}

Method [ <internal:SimpleXML, ctor> public method __construct ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <optional> int $options = 0 ]
    Parameter #2 [ <optional> bool $dataIsURL = false ]
    Parameter #3 [ <optional> string $namespaceOrPrefix = "" ]
    Parameter #4 [ <optional> bool $isPrefix = false ]
  }
}

Method [ <internal:SimpleXML> public method addChild ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $qualifiedName ]
    Parameter #1 [ <optional> ?string $value = null ]
    Parameter #2 [ <optional> ?string $namespace = null ]
  }
  - Tentative return [ ?SimpleXMLElement ]
}

Method [ <internal:SimpleXML> public method addAttribute ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $qualifiedName ]
    Parameter #1 [ <required> string $value ]
    Parameter #2 [ <optional> ?string $namespace = null ]
  }
  - Tentative return [ void ]
}

Method [ <internal:SimpleXML> public method getName ] {

  - Parameters [0] {
  }
  - Tentative return [ string ]
}

Method [ <internal:SimpleXML, prototype Stringable> public method __toString ] {

  - Parameters [0] {
  }
  - Return [ string ]
}

Method [ <internal:SimpleXML> public method __debugInfo ] {

  - Parameters [0] {
  }
  - Return [ ?array ]
}

Method [ <internal:SimpleXML, prototype Countable> public method count ] {

  - Parameters [0] {
  }
  - Tentative return [ int ]
}

Method [ <internal:SimpleXML, prototype Iterator> public method rewind ] {

  - Parameters [0] {
  }
  - Tentative return [ void ]
}

Method [ <internal:SimpleXML, prototype Iterator> public method valid ] {

  - Parameters [0] {
  }
  - Tentative return [ bool ]
}

Method [ <internal:SimpleXML, prototype Iterator> public method current ] {

  - Parameters [0] {
  }
  - Tentative return [ SimpleXMLElement ]
}

Method [ <internal:SimpleXML, prototype Iterator> public method key ] {

  - Parameters [0] {
  }
  - Tentative return [ string ]
}

Method [ <internal:SimpleXML, prototype Iterator> public method next ] {

  - Parameters [0] {
  }
  - Tentative return [ void ]
}

Method [ <internal:SimpleXML, prototype RecursiveIterator> public method hasChildren ] {

  - Parameters [0] {
  }
  - Tentative return [ bool ]
}

Method [ <internal:SimpleXML, prototype RecursiveIterator> public method getChildren ] {

  - Parameters [0] {
  }
  - Tentative return [ ?SimpleXMLElement ]
}

-- SimpleXMLIterator adds nothing
parent=SimpleXMLElement own=0 interfaces=Iterator,Traversable,RecursiveIterator,Countable,Stringable
-- what an instance is NOT
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
Exception: Serialization of 'SimpleXMLElement' is not allowed
-- the class name travels
SxSurfaceKid SxSurfaceKid SxSurfaceKid SxSurfaceKid SxSurfaceKid
TypeError: simplexml_load_string(): Argument #2 ($class_name) must be a class name derived from SimpleXMLElement or null, NoSuchClassAtAll given
TypeError: simplexml_load_string(): Argument #2 ($class_name) must be a class name derived from SimpleXMLElement or null, stdClass given
