--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The order the two DOM class trees are declared in, and each element's implements list
--FILE--
<?php
/* get_declared_classes() and get_declared_interfaces() answer in REGISTRATION
 * order, so the order an extension declares its names in is observable. php's
 * dom extension interleaves the 2004 tree with php 8.4's namespaced one --
 * each old class next to its new counterpart -- rather than finishing one tree
 * before starting the other, and its enum is the first name of all. The
 * `Dom\DOMException` alias sits immediately after the class it renames.
 *
 * ReflectionClass::getInterfaceNames() is the second face: it answers in
 * DECLARATION order, which is not one rule for the whole tree. Dom\Element
 * declares ParentNode before ChildNode; Dom\HTMLElement, which extends it,
 * reads the two the other way round. */
$names = static function (array $a): void {
    foreach ($a as $n) {
        if (strcasecmp($n, 'DomainException') === 0) {
            continue;
        }
        if (stripos($n, 'Dom\\') === 0 || stripos($n, 'DOM') === 0) {
            echo $n, "\n";
        }
    }
};
echo "== interfaces ==\n";
$names(get_declared_interfaces());
echo "== classes ==\n";
$names(get_declared_classes());
echo "== implements ==\n";
foreach (['DOMElement', 'Dom\\Element', 'Dom\\HTMLElement', 'DOMDocument', 'Dom\\Document'] as $n) {
    echo $n, ': ', implode(', ', (new ReflectionClass($n))->getInterfaceNames()), "\n";
}
--EXPECT--
== interfaces ==
DOMParentNode
Dom\ParentNode
DOMChildNode
Dom\ChildNode
== classes ==
Dom\AdjacentPosition
DOMException
dom\domexception
DOMImplementation
Dom\Implementation
DOMNode
Dom\Node
DOMNameSpaceNode
Dom\NamespaceInfo
DOMDocumentFragment
Dom\DocumentFragment
Dom\Document
DOMDocument
Dom\HTMLDocument
Dom\XMLDocument
DOMNodeList
Dom\NodeList
DOMNamedNodeMap
Dom\NamedNodeMap
Dom\DtdNamedNodeMap
Dom\HTMLCollection
DOMCharacterData
Dom\CharacterData
DOMAttr
Dom\Attr
DOMElement
Dom\Element
Dom\HTMLElement
DOMText
Dom\Text
DOMComment
Dom\Comment
DOMCdataSection
Dom\CDATASection
DOMDocumentType
Dom\DocumentType
DOMNotation
Dom\Notation
DOMEntity
Dom\Entity
DOMEntityReference
Dom\EntityReference
DOMProcessingInstruction
Dom\ProcessingInstruction
DOMXPath
Dom\XPath
Dom\TokenList
== implements ==
DOMElement: DOMParentNode, DOMChildNode
Dom\Element: Dom\ParentNode, Dom\ChildNode
Dom\HTMLElement: Dom\ChildNode, Dom\ParentNode
DOMDocument: DOMParentNode
Dom\Document: Dom\ParentNode
