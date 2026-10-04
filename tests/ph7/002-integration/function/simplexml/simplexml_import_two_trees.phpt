--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/dom: the two import doors, one per class tree, and the latch between them
--FILE--
<?php
/* php 8.4 put a second DOM class tree over the same libxml nodes and gave the
 * simplexml bridge a second door rather than a flag. The two never meet: the
 * FIRST door to run over a document latches it, and the other then refuses that
 * whole document. */
function door(string $label, callable $f): void {
    try {
        $r = $f();
        printf("%-22s %s", $label, get_class($r));
        if (isset($r->ownerDocument)) { printf("   owner=%s", get_class($r->ownerDocument)); }
        echo "\n";
    } catch (Throwable $e) {
        printf("%-22s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

echo "-- an unlatched tree: whichever door runs first decides\n";
$a = simplexml_load_string('<r a="1"><c/></r>');
door('modern', fn() => Dom\import_simplexml($a));
door('then 2004', fn() => dom_import_simplexml($a));
$b = simplexml_load_string('<r a="1"><c/></r>');
door('2004', fn() => dom_import_simplexml($b));
door('then modern', fn() => Dom\import_simplexml($b));

echo "\n-- the latch is the TREE's: an attribute's import refuses the root\n";
$c = simplexml_load_string('<r a="1"><c/></r>');
door('attr modern', fn() => Dom\import_simplexml($c['a']));
door('root 2004', fn() => dom_import_simplexml($c));

echo "\n-- a document of php 8.4's tree arrives latched\n";
$m = Dom\XMLDocument::createFromString('<r a="1"/>');
$sm = simplexml_import_dom($m);
door('2004', fn() => dom_import_simplexml($sm));
door('modern', fn() => Dom\import_simplexml($sm));

echo "\n-- a 2004 document does NOT: it is what you get when nobody chose\n";
$d = new DOMDocument();
$d->loadXML('<r a="1"/>');
$sd = simplexml_import_dom($d);
door('modern', fn() => Dom\import_simplexml($sd));
door('then 2004', fn() => dom_import_simplexml($sd));

echo "\n-- the class is the door's choice, the owner is the cache's\n";
$e = new DOMDocument();
$e->loadXML('<r a="1"/>');
$se = simplexml_import_dom($e);
$mint = $e->documentElement;                 /* mint the 2004 wrapper first */
$hit = Dom\import_simplexml($se);            /* ...and the cache answers it */
var_dump(get_class($hit), $hit === $mint);
door('2004 after the hit', fn() => dom_import_simplexml($se));

echo "\n-- identity, ownership and the shared tree\n";
$f = simplexml_load_string('<r a="1"><c/></r>');
$e1 = Dom\import_simplexml($f);
$e2 = Dom\import_simplexml($f);
var_dump($e1 === $e2, get_class($e1->ownerDocument),
    $e1->ownerDocument->documentElement === $e1);
$e1->setAttribute('b', '2');
var_dump((string) $f['b']);
$at = Dom\import_simplexml($f['a']);
var_dump(get_class($at), get_class($at->ownerElement), $at->ownerElement === $e1);

echo "\n-- what the door itself refuses\n";
door('int', fn() => Dom\import_simplexml(1));
door('null', fn() => Dom\import_simplexml(null));
door('stdClass', fn() => Dom\import_simplexml(new stdClass()));
door('no argument', fn() => Dom\import_simplexml());

echo "\n-- the name folds and the extension is ext/dom, as the 2004 one's is\n";
var_dump(function_exists('Dom\import_simplexml'), function_exists('dom\IMPORT_SIMPLEXML'));
echo new ReflectionFunction('Dom\import_simplexml'), "\n";
?>
--EXPECT--
-- an unlatched tree: whichever door runs first decides
modern                 Dom\Element   owner=Dom\XMLDocument
then 2004              TypeError: dom_import_simplexml(): Argument #1 ($node) must not be already imported as a Dom\Node
2004                   DOMElement   owner=DOMDocument
then modern            TypeError: Dom\import_simplexml(): Argument #1 ($node) must not be already imported as a DOMNode

-- the latch is the TREE's: an attribute's import refuses the root
attr modern            Dom\Attr   owner=Dom\XMLDocument
root 2004              TypeError: dom_import_simplexml(): Argument #1 ($node) must not be already imported as a Dom\Node

-- a document of php 8.4's tree arrives latched
2004                   TypeError: dom_import_simplexml(): Argument #1 ($node) must not be already imported as a Dom\Node
modern                 Dom\Element   owner=Dom\XMLDocument

-- a 2004 document does NOT: it is what you get when nobody chose
modern                 Dom\Element   owner=DOMDocument
then 2004              TypeError: dom_import_simplexml(): Argument #1 ($node) must not be already imported as a Dom\Node

-- the class is the door's choice, the owner is the cache's
string(10) "DOMElement"
bool(true)
2004 after the hit     TypeError: dom_import_simplexml(): Argument #1 ($node) must not be already imported as a Dom\Node

-- identity, ownership and the shared tree
bool(true)
string(15) "Dom\XMLDocument"
bool(true)
string(1) "2"
string(8) "Dom\Attr"
string(11) "Dom\Element"
bool(true)

-- what the door itself refuses
int                    TypeError: Dom\import_simplexml(): Argument #1 ($node) must be of type object, int given
null                   TypeError: Dom\import_simplexml(): Argument #1 ($node) must be of type object, null given
stdClass               TypeError: Dom\import_simplexml(): Argument #1 ($node) is not a valid node type
no argument            ArgumentCountError: Dom\import_simplexml() expects exactly 1 argument, 0 given

-- the name folds and the extension is ext/dom, as the 2004 one's is
bool(true)
bool(true)
Function [ <internal:dom> function Dom\import_simplexml ] {

  - Parameters [1] {
    Parameter #0 [ <required> object $node ]
  }
  - Return [ Dom\Attr|Dom\Element ]
}
