--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOM object shows NOTHING to get_properties and the whole handler table to the debug one
--DESCRIPTION--
Every property php declares on a DOM class is VIRTUAL: a read_property handler over
libxml's state, with no slot on the object. So php keeps two tables for one of these
objects and every door reads exactly one of them. get_properties -- the (array) cast,
get_object_vars(), get_mangled_object_vars(), json_encode(), foreach, var_export() and
array_walk() -- sees the object's REAL table, which for anything but a subclass is
empty; get_debug_info -- print_r() and var_dump() -- sees the forty names the handler
table fabricates, own class first and the parent chain behind it, with an OBJECT value
replaced by the literal `(object value omitted)` rather than recursed into. PHL had
only the real table and declared seven of the forty as ordinary slots, so both halves
were wrong in opposite directions: the cast published seven keys php has none of and
the dump was missing thirty-three.
--FILE--
<?php
// php's get_properties for a DOM object is the object's own table, and there is
// none: every declared property is a handler over libxml's state.
$d = new DOMDocument();
echo "cast=", json_encode((array) $d), "\n";
echo "vars=", json_encode(get_object_vars($d)), "\n";
echo "json=", json_encode($d), "\n";
echo "mangled=", json_encode(get_mangled_object_vars($d)), "\n";
$seen = [];
foreach ($d as $k => $v) { $seen[] = $k; }
echo "foreach=", json_encode($seen), "\n";
var_export($d);
echo "\n";
$walked = [];
array_walk($d, function ($v, $k) use (&$walked) { $walked[] = $k; });
echo "walk=", json_encode($walked), "\n";
// ...while print_r/var_dump read the OTHER table, the one the debug handler
// fabricates: the forty names in php's own order, own class first.
print_r($d);
// The object id belongs to the run, not to the shape: this corpus shares one
// interpreter, so the dump is normalized before it is compared.
ob_start(); var_dump($d); echo preg_replace('/#\d+ \(/', '#N (', ob_get_clean());
// A subclass's real properties come first, under php's mangled keys, on BOTH.
class VpDoc extends DOMDocument { public $own = 1; private $hidden = 2; }
$s = new VpDoc();
echo "sub-cast=", json_encode(array_keys((array) $s)), "\n";
print_r($s);
--EXPECT--
cast=[]
vars=[]
json={}
mangled=[]
foreach=[]
\DOMDocument::__set_state(array(
))
walk=[]
DOMDocument Object
(
    [doctype] => 
    [implementation] => (object value omitted)
    [documentElement] => 
    [actualEncoding] => 
    [encoding] => 
    [xmlEncoding] => 
    [standalone] => 
    [xmlStandalone] => 
    [version] => 1.0
    [xmlVersion] => 1.0
    [strictErrorChecking] => 1
    [documentURI] => 
    [config] => 
    [formatOutput] => 
    [validateOnParse] => 
    [resolveExternals] => 
    [preserveWhiteSpace] => 1
    [recover] => 
    [substituteEntities] => 
    [firstElementChild] => 
    [lastElementChild] => 
    [childElementCount] => 0
    [nodeName] => #document
    [nodeValue] => 
    [nodeType] => 9
    [parentNode] => 
    [parentElement] => 
    [childNodes] => (object value omitted)
    [firstChild] => 
    [lastChild] => 
    [previousSibling] => 
    [nextSibling] => 
    [attributes] => 
    [isConnected] => 1
    [ownerDocument] => 
    [namespaceURI] => 
    [prefix] => 
    [localName] => 
    [baseURI] => 
    [textContent] => 
)
object(DOMDocument)#N (40) {
  ["doctype"]=>
  NULL
  ["implementation"]=>
  string(22) "(object value omitted)"
  ["documentElement"]=>
  NULL
  ["actualEncoding"]=>
  NULL
  ["encoding"]=>
  NULL
  ["xmlEncoding"]=>
  NULL
  ["standalone"]=>
  bool(false)
  ["xmlStandalone"]=>
  bool(false)
  ["version"]=>
  string(3) "1.0"
  ["xmlVersion"]=>
  string(3) "1.0"
  ["strictErrorChecking"]=>
  bool(true)
  ["documentURI"]=>
  NULL
  ["config"]=>
  NULL
  ["formatOutput"]=>
  bool(false)
  ["validateOnParse"]=>
  bool(false)
  ["resolveExternals"]=>
  bool(false)
  ["preserveWhiteSpace"]=>
  bool(true)
  ["recover"]=>
  bool(false)
  ["substituteEntities"]=>
  bool(false)
  ["firstElementChild"]=>
  NULL
  ["lastElementChild"]=>
  NULL
  ["childElementCount"]=>
  int(0)
  ["nodeName"]=>
  string(9) "#document"
  ["nodeValue"]=>
  NULL
  ["nodeType"]=>
  int(9)
  ["parentNode"]=>
  NULL
  ["parentElement"]=>
  NULL
  ["childNodes"]=>
  string(22) "(object value omitted)"
  ["firstChild"]=>
  NULL
  ["lastChild"]=>
  NULL
  ["previousSibling"]=>
  NULL
  ["nextSibling"]=>
  NULL
  ["attributes"]=>
  NULL
  ["isConnected"]=>
  bool(true)
  ["ownerDocument"]=>
  NULL
  ["namespaceURI"]=>
  NULL
  ["prefix"]=>
  string(0) ""
  ["localName"]=>
  NULL
  ["baseURI"]=>
  NULL
  ["textContent"]=>
  string(0) ""
}
sub-cast=["own","\u0000VpDoc\u0000hidden"]
VpDoc Object
(
    [own] => 1
    [hidden:VpDoc:private] => 2
    [doctype] => 
    [implementation] => (object value omitted)
    [documentElement] => 
    [actualEncoding] => 
    [encoding] => 
    [xmlEncoding] => 
    [standalone] => 
    [xmlStandalone] => 
    [version] => 1.0
    [xmlVersion] => 1.0
    [strictErrorChecking] => 1
    [documentURI] => 
    [config] => 
    [formatOutput] => 
    [validateOnParse] => 
    [resolveExternals] => 
    [preserveWhiteSpace] => 1
    [recover] => 
    [substituteEntities] => 
    [firstElementChild] => 
    [lastElementChild] => 
    [childElementCount] => 0
    [nodeName] => #document
    [nodeValue] => 
    [nodeType] => 9
    [parentNode] => 
    [parentElement] => 
    [childNodes] => (object value omitted)
    [firstChild] => 
    [lastChild] => 
    [previousSibling] => 
    [nextSibling] => 
    [attributes] => 
    [isConnected] => 1
    [ownerDocument] => 
    [namespaceURI] => 
    [prefix] => 
    [localName] => 
    [baseURI] => 
    [textContent] => 
)
