--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOM property is answered by the class's HANDLER, before any magic accessor
--DESCRIPTION--
ext/dom has no __get/__set/__isset anywhere: every property is a C handler on the
class, and php's object handlers consult that table BEFORE the standard path -- so a
name the table carries is the handler's and only a name it does not carry falls through
to a subclass's magic accessor. PHL routed the whole surface through the magic trio,
which had the order exactly backwards: a subclass that wrote __get without delegating
silently replaced the entire DOM property surface for its instances, and every DOM class
carried three methods php does not. The write half is the same rule at the point the
value exists -- a plain store, a compound assign and a `??=` all reach the handler --
and a writable name on an object with no node behind it is php's Invalid State Error
rather than a write nobody notices.
--FILE--
<?php
// Not one DOM class declares a magic property accessor.
$phlPropMagic = [];
foreach (['DOMNode', 'DOMDocument', 'DOMElement', 'DOMAttr', 'DOMText', 'DOMCharacterData',
          'DOMProcessingInstruction', 'DOMDocumentFragment', 'DOMDocumentType', 'DOMEntity',
          'DOMNotation', 'DOMNodeList', 'DOMNamedNodeMap', 'DOMNameSpaceNode',
          'DOMXPath'] as $phlPropCls) {
    foreach (get_class_methods($phlPropCls) as $phlPropM) {
        if (in_array(strtolower($phlPropM), ['__get', '__set', '__isset', '__unset'], true)) {
            $phlPropMagic[] = $phlPropCls . '::' . $phlPropM;
        }
    }
}
echo 'magic accessors: ', $phlPropMagic ? implode(',', $phlPropMagic) : '(none)', "\n";

class PhlPropElement extends DOMElement
{
    public function __get($name) { return "mine:$name"; }
    public function __set($name, $value) { echo "  [__set $name]\n"; }
    public function __isset($name) { echo "  [__isset $name]\n"; return true; }
}
$phlPropDoc = new DOMDocument();
$phlPropDoc->registerNodeClass('DOMElement', 'PhlPropElement');
$phlPropDoc->loadXML('<r a="1">t</r>');
$phlPropEl = $phlPropDoc->documentElement;

// A name the handler table carries never reaches the accessor; one it does not, does.
echo 'read declared=', var_export($phlPropEl->tagName, true),
     ' read absent=', var_export($phlPropEl->nope, true), "\n";
$phlPropIsDecl = isset($phlPropEl->tagName);
$phlPropIsAbs = isset($phlPropEl->nope);
echo 'isset declared=', var_export($phlPropIsDecl, true),
     ' isset absent=', var_export($phlPropIsAbs, true), "\n";
try {
    $phlPropEl->tagName = 'q';
    echo "write declared: no refusal\n";
} catch (Throwable $e) {
    echo 'write declared=', get_class($e), ': ', $e->getMessage(), "\n";
}
$phlPropEl->nodeValue = 'through the handler';
echo 'write writable=', var_export($phlPropEl->nodeValue, true), "\n";
$phlPropEl->nope = 'q';

// Every write shape ends at the same handler.
$phlPropPlain = new DOMDocument();
$phlPropPlain->loadXML('<r>t</r>');
$phlPropPlain->version = '1.1';
$phlPropPlain->version .= '-x';
$phlPropPlain->encoding ??= 'UTF-8';
echo 'store+concat=', var_export($phlPropPlain->version, true),
     ' coalesce=', var_export($phlPropPlain->encoding, true), "\n";

// A writable name on an object libxml gave no node: php's handler reaches its
// null pointer and answers Invalid State -- but the declared type has its say first.
$phlPropBare = new DOMNode();
foreach ([['nodeValue', 'x'], ['nodeValue', []], ['nodeName', 'x']] as [$phlPropN, $phlPropV]) {
    try {
        $phlPropBare->$phlPropN = $phlPropV;
        echo "nodeless $phlPropN: no refusal\n";
    } catch (Throwable $e) {
        echo "nodeless $phlPropN=", get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n";
    }
}
--EXPECT--
magic accessors: (none)
read declared='r' read absent='mine:nope'
  [__isset nope]
isset declared=true isset absent=true
write declared=Error: Cannot modify readonly property PhlPropElement::$tagName
write writable='through the handler'
  [__set nope]
store+concat='1.1-x' coalesce='UTF-8'
nodeless nodeValue=DOMException(11): Invalid State Error
nodeless nodeValue=TypeError(0): Cannot assign array to property DOMNode::$nodeValue of type ?string
nodeless nodeName=Error(0): Cannot modify readonly property DOMNode::$nodeName
