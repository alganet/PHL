--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php 8.4's Dom\ classes belong to ext/dom, on all three of the faces that say so
--FILE--
<?php
// The namespaced tree is ext/dom's, exactly as the 2004 one is: a name the
// extension partition has no row for falls back to Core, which is what all 26
// of these answered while only the DOM* spellings were listed.
//
// Only names BOTH engines register are pinned -- php also carries Dom\XPath,
// Dom\TokenList and Dom\NamespaceInfo, which this engine has yet to grow.
$dom_ep_names = [
    'Dom\\AdjacentPosition', 'DOMException', 'DOMParentNode', 'Dom\\ParentNode',
    'DOMChildNode', 'Dom\\ChildNode', 'DOMImplementation', 'Dom\\Implementation',
    'DOMNode', 'Dom\\Node', 'DOMNameSpaceNode', 'DOMDocumentFragment',
    'Dom\\DocumentFragment', 'Dom\\Document', 'DOMDocument', 'Dom\\HTMLDocument',
    'Dom\\XMLDocument', 'DOMNodeList', 'Dom\\NodeList', 'DOMNamedNodeMap',
    'Dom\\NamedNodeMap', 'Dom\\DtdNamedNodeMap', 'Dom\\HTMLCollection',
    'DOMCharacterData', 'Dom\\CharacterData', 'DOMAttr', 'Dom\\Attr',
    'DOMElement', 'Dom\\Element', 'Dom\\HTMLElement', 'DOMText', 'Dom\\Text',
    'DOMComment', 'Dom\\Comment', 'DOMCdataSection', 'Dom\\CDATASection',
    'DOMDocumentType', 'Dom\\DocumentType', 'DOMNotation', 'Dom\\Notation',
    'DOMEntity', 'Dom\\Entity', 'DOMEntityReference', 'Dom\\EntityReference',
    'DOMProcessingInstruction', 'Dom\\ProcessingInstruction', 'DOMXPath',
];

// Face one: the class says which extension it belongs to.
$dom_ep_bad = [];
foreach ($dom_ep_names as $dom_ep_n) {
    $dom_ep_e = (new ReflectionClass($dom_ep_n))->getExtensionName();
    if ($dom_ep_e !== 'dom') {
        $dom_ep_bad[] = $dom_ep_n . '=>' . var_export($dom_ep_e, true);
    }
}
echo 'classes=', count($dom_ep_names), ' notdom=', ($dom_ep_bad ? implode(' ', $dom_ep_bad) : 'none'), "\n";

// getExtension() is the same answer as an object.
$dom_ep_obj = (new ReflectionClass('Dom\\Element'))->getExtension();
echo 'getExtension=', get_class($dom_ep_obj), ':', $dom_ep_obj->getName(), "\n";

// Face two: the extension lists them back, in php's own registration order --
// which interleaves the two trees rather than grouping either one.
$dom_ep_listed = (new ReflectionExtension('dom'))->getClassNames();
$dom_ep_keep = array_values(array_intersect($dom_ep_listed, $dom_ep_names));
echo 'listed=', count($dom_ep_keep), "\n";
foreach ($dom_ep_keep as $dom_ep_i => $dom_ep_n) {
    printf("  %2d %s\n", $dom_ep_i, $dom_ep_n);
}
// The lower-case alias php registers for DOMException rides the same group.
echo 'alias=', var_export(in_array('dom\\domexception', $dom_ep_listed, true), true), "\n";

// Face three: the export prints the extension in its <internal:...> tag.
foreach (['Dom\\Element', 'DOMElement', 'Dom\\XMLDocument'] as $dom_ep_n) {
    $dom_ep_s = (string) new ReflectionClass($dom_ep_n);
    echo substr($dom_ep_s, 0, strpos($dom_ep_s, ' class ')), "\n";
}

// Core keeps none of them, and neither does libxml.
foreach (['Core', 'libxml'] as $dom_ep_other) {
    $dom_ep_hit = array_intersect((new ReflectionExtension($dom_ep_other))->getClassNames(), $dom_ep_names);
    echo $dom_ep_other, '=', count($dom_ep_hit), "\n";
}
?>
--EXPECT--
classes=47 notdom=none
getExtension=ReflectionExtension:dom
listed=47
   0 Dom\AdjacentPosition
   1 DOMException
   2 DOMParentNode
   3 Dom\ParentNode
   4 DOMChildNode
   5 Dom\ChildNode
   6 DOMImplementation
   7 Dom\Implementation
   8 DOMNode
   9 Dom\Node
  10 DOMNameSpaceNode
  11 DOMDocumentFragment
  12 Dom\DocumentFragment
  13 Dom\Document
  14 DOMDocument
  15 Dom\HTMLDocument
  16 Dom\XMLDocument
  17 DOMNodeList
  18 Dom\NodeList
  19 DOMNamedNodeMap
  20 Dom\NamedNodeMap
  21 Dom\DtdNamedNodeMap
  22 Dom\HTMLCollection
  23 DOMCharacterData
  24 Dom\CharacterData
  25 DOMAttr
  26 Dom\Attr
  27 DOMElement
  28 Dom\Element
  29 Dom\HTMLElement
  30 DOMText
  31 Dom\Text
  32 DOMComment
  33 Dom\Comment
  34 DOMCdataSection
  35 Dom\CDATASection
  36 DOMDocumentType
  37 Dom\DocumentType
  38 DOMNotation
  39 Dom\Notation
  40 DOMEntity
  41 Dom\Entity
  42 DOMEntityReference
  43 Dom\EntityReference
  44 DOMProcessingInstruction
  45 Dom\ProcessingInstruction
  46 DOMXPath
alias=true
Class [ <internal:dom>
Class [ <internal:dom>
Class [ <internal:dom> final
Core=0
libxml=0
