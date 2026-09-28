--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOM class DECLARES its virtual properties: Reflection, property_exists, isset and unset
--DESCRIPTION--
A virtual property is not an undeclared name the class happens to answer: php declares
every one of them, so Reflection lists it with its type and modifiers 513 (public plus
IS_VIRTUAL), property_exists() is true, isInitialized() is true and hasDefaultValue()
is false -- there is no slot to hold a default in. Reflection's getValue()/setValue()
reach the same handler `$o->p` reaches, which is why the write half refuses exactly
where the property has no writer; and unset() is php's `Cannot unset C::$p` rather than
the silent no-op a name the object merely lacks would take. PHL declared seven of
DOMDocument's forty as ordinary slots and answered the other thirty-three from a magic
accessor alone, so property_exists() was false for them and Reflection reported the
wrong set.
--FILE--
<?php
// The names ARE declared: Reflection lists them, property_exists() answers true,
// isVirtual() is true and hasDefaultValue() is false -- there is no slot to hold
// a default in. An internal class reports its parent's properties FIRST, which is
// its registration order rather than a rule of its own.
foreach (['DOMNodeList', 'DOMXPath', 'DOMText', 'DOMException'] as $cls) {
    $r = new ReflectionClass($cls);
    $rows = [];
    foreach ($r->getProperties() as $p) {
        $rows[] = $p->getDeclaringClass()->getName() . '::$' . $p->getName()
            . ' mods=' . $p->getModifiers()
            . ' type=' . ($p->hasType() ? (string) $p->getType() : '-')
            . ' virtual=' . var_export($p->isVirtual(), true)
            . ' default=' . var_export($p->hasDefaultValue(), true);
    }
    echo $cls, "\n  ", implode("\n  ", $rows), "\n";
}
$d = new DOMDocument();
$d->loadXML('<r a="1">t</r>');
$e = $d->documentElement;
echo 'exists(class)=', var_export(property_exists('DOMDocument', 'preserveWhiteSpace'), true),
     ' exists(object)=', var_export(property_exists($d, 'nodeName'), true),
     ' exists(absent)=', var_export(property_exists($d, 'nope'), true), "\n";
// Reflection reads and writes through the same handler `$o->p` reaches.
$p = new ReflectionProperty('DOMDocument', 'preserveWhiteSpace');
echo 'get=', var_export($p->getValue($d), true),
     ' init=', var_export($p->isInitialized($d), true), "\n";
$p->setValue($d, false);
echo 'after-set=', var_export($d->preserveWhiteSpace, true),
     ' cast=', json_encode((array) $d), "\n";
$q = new ReflectionProperty('DOMNode', 'nodeName');
echo 'node=', var_export($q->getValue($e), true), "\n";
try { $q->setValue($e, 'z'); } catch (Throwable $t) { echo 'set: ', get_class($t), ': ', $t->getMessage(), "\n"; }
// isset() goes to the has_property handler; unset() has nothing to remove.
echo 'isset=', var_export(isset($d->documentElement), true),
     ' isset-null=', var_export(isset($d->doctype), true), "\n";
foreach ([[$d, 'preserveWhiteSpace'], [$e, 'nodeName']] as [$obj, $name]) {
    try { unset($obj->$name); echo "unset $name: went through\n"; }
    catch (Throwable $t) { echo "unset $name: ", get_class($t), ': ', $t->getMessage(), "\n"; }
}
--EXPECT--
DOMNodeList
  DOMNodeList::$length mods=513 type=int virtual=true default=false
DOMXPath
  DOMXPath::$document mods=513 type=DOMDocument virtual=true default=false
  DOMXPath::$registerNodeNamespaces mods=513 type=bool virtual=true default=false
DOMText
  DOMNode::$nodeName mods=513 type=string virtual=true default=false
  DOMNode::$nodeValue mods=513 type=?string virtual=true default=false
  DOMNode::$nodeType mods=513 type=int virtual=true default=false
  DOMNode::$parentNode mods=513 type=?DOMNode virtual=true default=false
  DOMNode::$parentElement mods=513 type=?DOMElement virtual=true default=false
  DOMNode::$childNodes mods=513 type=DOMNodeList virtual=true default=false
  DOMNode::$firstChild mods=513 type=?DOMNode virtual=true default=false
  DOMNode::$lastChild mods=513 type=?DOMNode virtual=true default=false
  DOMNode::$previousSibling mods=513 type=?DOMNode virtual=true default=false
  DOMNode::$nextSibling mods=513 type=?DOMNode virtual=true default=false
  DOMNode::$attributes mods=513 type=?DOMNamedNodeMap virtual=true default=false
  DOMNode::$isConnected mods=513 type=bool virtual=true default=false
  DOMNode::$ownerDocument mods=513 type=?DOMDocument virtual=true default=false
  DOMNode::$namespaceURI mods=513 type=?string virtual=true default=false
  DOMNode::$prefix mods=513 type=string virtual=true default=false
  DOMNode::$localName mods=513 type=?string virtual=true default=false
  DOMNode::$baseURI mods=513 type=?string virtual=true default=false
  DOMNode::$textContent mods=513 type=string virtual=true default=false
  DOMCharacterData::$data mods=513 type=string virtual=true default=false
  DOMCharacterData::$length mods=513 type=int virtual=true default=false
  DOMCharacterData::$previousElementSibling mods=513 type=?DOMElement virtual=true default=false
  DOMCharacterData::$nextElementSibling mods=513 type=?DOMElement virtual=true default=false
  DOMText::$wholeText mods=513 type=string virtual=true default=false
DOMException
  Exception::$message mods=2 type=- virtual=false default=true
  Exception::$file mods=2 type=string virtual=false default=true
  Exception::$line mods=2 type=int virtual=false default=true
  DOMException::$code mods=1 type=- virtual=false default=true
exists(class)=true exists(object)=true exists(absent)=false
get=true init=true
after-set=false cast=[]
node='r'
set: Error: Cannot modify readonly property DOMElement::$nodeName
isset=true isset-null=false
unset preserveWhiteSpace: Error: Cannot unset DOMDocument::$preserveWhiteSpace
unset nodeName: Error: Cannot unset DOMElement::$nodeName
