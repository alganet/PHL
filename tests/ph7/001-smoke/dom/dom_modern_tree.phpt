--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php 8.4's Dom\ node tree: the family a document makes, and Dom\XMLDocument's three producers
--FILE--
<?php
$dom_mt_show = static function (string $n): void {
    $r = new ReflectionClass($n);
    $p = $r->getParentClass();
    $i = $r->getInterfaceNames();
    sort($i);
    echo $r->getName(),
         ' abstract=', var_export($r->isAbstract(), true),
         ' final=', var_export($r->isFinal(), true),
         ' instantiable=', var_export($r->isInstantiable(), true),
         ' parent=', $p ? $p->getName() : '-',
         ' implements=', $i ? implode(',', $i) : '-', "\n";
};
foreach ([
    'Dom\Node', 'Dom\CharacterData', 'Dom\Attr', 'Dom\Element', 'Dom\HTMLElement',
    'Dom\Text', 'Dom\CDATASection', 'Dom\Comment', 'Dom\ProcessingInstruction',
    'Dom\DocumentFragment', 'Dom\DocumentType', 'Dom\Entity', 'Dom\EntityReference',
    'Dom\Notation', 'Dom\Document', 'Dom\XMLDocument', 'Dom\HTMLDocument',
] as $dom_mt_name) {
    $dom_mt_show($dom_mt_name);
}
echo "-- the two trees never meet\n";
var_dump(
    is_a('Dom\Element', 'DOMElement', true),
    is_a('DOMElement', 'Dom\Element', true),
    is_a('Dom\Node', 'DOMNode', true),
    is_a('Dom\ProcessingInstruction', 'Dom\CharacterData', true),
    is_a('DOMProcessingInstruction', 'DOMCharacterData', true)
);
echo "-- the constants are Dom\\Node's\n";
var_dump(Dom\Element::DOCUMENT_POSITION_CONTAINED_BY, Dom\Node::DOCUMENT_POSITION_PRECEDING);

echo "-- createEmpty\n";
$dom_mt_empty = Dom\XMLDocument::createEmpty();
var_dump(
    get_class($dom_mt_empty),
    $dom_mt_empty->xmlVersion, $dom_mt_empty->xmlEncoding,
    $dom_mt_empty->URL, $dom_mt_empty->documentURI,
    $dom_mt_empty->characterSet, $dom_mt_empty->charset, $dom_mt_empty->inputEncoding,
    $dom_mt_empty->documentElement, $dom_mt_empty->doctype,
    $dom_mt_empty->childElementCount, $dom_mt_empty->xmlStandalone,
    $dom_mt_empty->formatOutput, $dom_mt_empty->nodeValue, $dom_mt_empty->textContent
);
var_dump(get_class(Dom\XMLDocument::createEmpty('1.1', 'ISO-8859-1')),
    Dom\XMLDocument::createEmpty('1.1', 'ISO-8859-1')->xmlVersion,
    Dom\XMLDocument::createEmpty('1.1', 'ISO-8859-1')->xmlEncoding);

echo "-- createFromString: every node comes out of the new tree\n";
$dom_mt_doc = Dom\XMLDocument::createFromString(
    '<!DOCTYPE r><r a="1" id="i"><c/>t<!--x--><![CDATA[z]]><?pi v?><p:q xmlns:p="urn:x"/></r>');
$dom_mt_root = $dom_mt_doc->documentElement;
foreach ([$dom_mt_doc, $dom_mt_doc->doctype, $dom_mt_root, $dom_mt_root->attributes->item(0)] as $dom_mt_n) {
    echo get_class($dom_mt_n), ' ', $dom_mt_n->nodeType, ' ', $dom_mt_n->nodeName, "\n";
}
foreach ($dom_mt_root->childNodes as $dom_mt_kid) {
    echo '  ', get_class($dom_mt_kid), ' ', $dom_mt_kid->nodeType, ' ', $dom_mt_kid->nodeName, "\n";
}
echo "-- identity, ownership and the family of what a reader hands back\n";
var_dump(
    $dom_mt_doc->documentElement === $dom_mt_root,
    $dom_mt_root->ownerDocument === $dom_mt_doc,
    $dom_mt_root->parentNode === $dom_mt_doc,
    $dom_mt_root->firstChild->parentElement === $dom_mt_root,
    get_class($dom_mt_root->childNodes),
    get_class($dom_mt_root->attributes),
    get_class($dom_mt_doc->doctype->entities),
    $dom_mt_root->isConnected,
    $dom_mt_root->childElementCount,
    get_class($dom_mt_root->firstElementChild),
    get_class($dom_mt_root->lastElementChild)
);
echo "-- the readers the new tree words differently\n";
var_dump(
    $dom_mt_root->nodeValue, $dom_mt_root->textContent,
    $dom_mt_root->prefix, $dom_mt_root->namespaceURI, $dom_mt_root->localName,
    $dom_mt_root->tagName, $dom_mt_root->id, $dom_mt_root->className,
    $dom_mt_root->lastElementChild->prefix,
    $dom_mt_root->lastElementChild->namespaceURI,
    $dom_mt_doc->doctype->textContent, $dom_mt_doc->doctype->nodeValue,
    $dom_mt_doc->doctype->name, $dom_mt_doc->doctype->publicId
);
$dom_mt_text = $dom_mt_root->childNodes->item(1);
var_dump($dom_mt_text->data, $dom_mt_text->length, $dom_mt_text->wholeText,
    $dom_mt_text->nodeValue, get_class($dom_mt_text->previousElementSibling),
    $dom_mt_text->nextElementSibling === null);

echo "-- createFromString and createFromFile refuse in three different ways\n";
try {
    Dom\XMLDocument::createFromString('');
} catch (Throwable $dom_mt_e) {
    echo get_class($dom_mt_e), '|', $dom_mt_e->getCode(), '|', $dom_mt_e->getMessage(), "\n";
}
try {
    @Dom\XMLDocument::createFromString('<r>');
} catch (Throwable $dom_mt_e) {
    echo get_class($dom_mt_e), '|', $dom_mt_e->getCode(), '|', $dom_mt_e->getMessage(), "\n";
}
try {
    @Dom\XMLDocument::createFromFile(__DIR__ . '/no-such-file-here.xml');
} catch (Throwable $dom_mt_e) {
    echo get_class($dom_mt_e), '|', $dom_mt_e->getCode(), '|', basename($dom_mt_e->getMessage()), "\n";
}
echo "-- createFromFile\n";
$dom_mt_dir = rtrim(sys_get_temp_dir(), '/\\') . '/phl_dom_modern';
@mkdir($dom_mt_dir);
$dom_mt_path = $dom_mt_dir . '/tree.xml';
file_put_contents($dom_mt_path, "<f><g/></f>\n");
$dom_mt_file = Dom\XMLDocument::createFromFile($dom_mt_path);
var_dump(get_class($dom_mt_file), $dom_mt_file->documentElement->tagName,
    get_class($dom_mt_file->documentElement->firstElementChild),
    basename($dom_mt_file->documentURI));
unlink($dom_mt_path);
rmdir($dom_mt_dir);
echo "-- an override encoding is libxml's parse encoding\n";
var_dump(Dom\XMLDocument::createFromString('<r/>', 0, 'ISO-8859-1')->xmlEncoding);
--EXPECT--
Dom\Node abstract=false final=false instantiable=false parent=- implements=-
Dom\CharacterData abstract=false final=false instantiable=false parent=Dom\Node implements=Dom\ChildNode
Dom\Attr abstract=false final=false instantiable=false parent=Dom\Node implements=-
Dom\Element abstract=false final=false instantiable=false parent=Dom\Node implements=Dom\ChildNode,Dom\ParentNode
Dom\HTMLElement abstract=false final=false instantiable=false parent=Dom\Element implements=Dom\ChildNode,Dom\ParentNode
Dom\Text abstract=false final=false instantiable=false parent=Dom\CharacterData implements=Dom\ChildNode
Dom\CDATASection abstract=false final=false instantiable=false parent=Dom\Text implements=Dom\ChildNode
Dom\Comment abstract=false final=false instantiable=false parent=Dom\CharacterData implements=Dom\ChildNode
Dom\ProcessingInstruction abstract=false final=false instantiable=false parent=Dom\CharacterData implements=Dom\ChildNode
Dom\DocumentFragment abstract=false final=false instantiable=false parent=Dom\Node implements=Dom\ParentNode
Dom\DocumentType abstract=false final=false instantiable=false parent=Dom\Node implements=Dom\ChildNode
Dom\Entity abstract=false final=false instantiable=false parent=Dom\Node implements=-
Dom\EntityReference abstract=false final=false instantiable=false parent=Dom\Node implements=-
Dom\Notation abstract=false final=false instantiable=false parent=Dom\Node implements=-
Dom\Document abstract=true final=false instantiable=false parent=Dom\Node implements=Dom\ParentNode
Dom\XMLDocument abstract=false final=true instantiable=false parent=Dom\Document implements=Dom\ParentNode
Dom\HTMLDocument abstract=false final=true instantiable=false parent=Dom\Document implements=Dom\ParentNode
-- the two trees never meet
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
-- the constants are Dom\Node's
int(16)
int(2)
-- createEmpty
string(15) "Dom\XMLDocument"
string(3) "1.0"
string(5) "UTF-8"
string(11) "about:blank"
string(11) "about:blank"
string(5) "UTF-8"
string(5) "UTF-8"
string(5) "UTF-8"
NULL
NULL
int(0)
bool(false)
bool(false)
NULL
NULL
string(15) "Dom\XMLDocument"
string(3) "1.1"
string(10) "ISO-8859-1"
-- createFromString: every node comes out of the new tree
Dom\XMLDocument 9 #document
Dom\DocumentType 10 r
Dom\Element 1 r
Dom\Attr 2 a
  Dom\Element 1 c
  Dom\Text 3 #text
  Dom\Comment 8 #comment
  Dom\CDATASection 4 #cdata-section
  Dom\ProcessingInstruction 7 pi
  Dom\Element 1 p:q
-- identity, ownership and the family of what a reader hands back
bool(true)
bool(true)
bool(true)
bool(true)
string(12) "Dom\NodeList"
string(16) "Dom\NamedNodeMap"
string(19) "Dom\DtdNamedNodeMap"
bool(true)
int(2)
string(11) "Dom\Element"
string(11) "Dom\Element"
-- the readers the new tree words differently
NULL
string(2) "tz"
NULL
NULL
string(1) "r"
string(1) "r"
string(1) "i"
string(0) ""
string(1) "p"
string(5) "urn:x"
NULL
NULL
string(1) "r"
string(0) ""
string(1) "t"
int(1)
string(1) "t"
string(1) "t"
string(11) "Dom\Element"
bool(false)
-- createFromString and createFromFile refuse in three different ways
ValueError|0|Dom\XMLDocument::createFromString(): Argument #1 ($source) must not be empty
DOMException|12|XML fragment is not well-formed
Exception|0|no-such-file-here.xml'
-- createFromFile
string(15) "Dom\XMLDocument"
string(1) "f"
string(11) "Dom\Element"
string(8) "tree.xml"
-- an override encoding is libxml's parse encoding
string(10) "ISO-8859-1"
