--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM tree looks an element up by its QUALIFIED name
--FILE--
<?php
/* `getElementsByTagName` is the oldest lookup the DOM has and php 8.4's tree
 * asks it a DIFFERENT question: the 2004 document matches an element's LOCAL
 * name, so `a` answers `a`, `p:a` and `q:a` alike and `p:a` answers nothing at
 * all, while the namespaced one matches the QUALIFIED name and separates them.
 * `getElementsByTagNameNS` takes a local name on BOTH trees, and `*` is the
 * wildcard on both -- a `p:*` is a name like any other and matches nothing.
 *
 * The collection is live and typed by family: a `Dom\HTMLCollection` here, a
 * DOMNodeList there, off the one body. */
$xml = '<r xmlns:p="urn:u" xmlns:q="urn:q"><a id="i1"/><p:a/><q:a/><b/><p:b/></r>';
$names = ['a', 'p:a', 'q:a', '*', 'b', 'p:b', 'p:*', 'x'];

function row(string $tag, string $name, $list): void {
    $seen = [];
    foreach ($list as $el) { $seen[] = $el->nodeName; }
    printf("%-6s %-4s %-18s [%s]\n", $tag, $name, get_class($list), implode(',', $seen));
}

$legacy = new DOMDocument();
$legacy->loadXML($xml);
foreach ($names as $name) {
    row('legacy', $name, $legacy->documentElement->getElementsByTagName($name));
}
$modern = Dom\XMLDocument::createFromString($xml);
foreach ($names as $name) {
    row('modern', $name, $modern->documentElement->getElementsByTagName($name));
}

/* The namespace-aware half, whose three URI cases are asymmetric and unchanged
 * between the trees. */
foreach ([['urn:u', 'a'], ['*', 'a'], [null, 'a'], ['', 'a'], ['urn:u', '*']] as $q) {
    $list = $modern->documentElement->getElementsByTagNameNS($q[0], $q[1]);
    row('ns', var_export($q[0], true) . '/' . $q[1], $list);
}

/* Declared on the DOCUMENT as well as the element, and the document's walk
 * includes the root the element's excludes. */
row('doc', '*', $modern->getElementsByTagName('*'));

/* Live: a list built before the append sees the appended element. */
$live = $modern->documentElement->getElementsByTagName('z');
printf("live before=%d", $live->length);
$modern->documentElement->appendChild($modern->createElement('z'));
printf(" after=%d\n", $live->length);

/* `id` is not an ID attribute without a DTD to declare it, on either tree. */
var_dump($modern->getElementById('i1'));
$modern->documentElement->firstElementChild->setIdAttribute('id', true);
var_dump($modern->getElementById('i1')?->nodeName);
?>
--EXPECT--
legacy a    DOMNodeList        [a,p:a,q:a]
legacy p:a  DOMNodeList        []
legacy q:a  DOMNodeList        []
legacy *    DOMNodeList        [a,p:a,q:a,b,p:b]
legacy b    DOMNodeList        [b,p:b]
legacy p:b  DOMNodeList        []
legacy p:*  DOMNodeList        []
legacy x    DOMNodeList        []
modern a    Dom\HTMLCollection [a]
modern p:a  Dom\HTMLCollection [p:a]
modern q:a  Dom\HTMLCollection [q:a]
modern *    Dom\HTMLCollection [a,p:a,q:a,b,p:b]
modern b    Dom\HTMLCollection [b]
modern p:b  Dom\HTMLCollection [p:b]
modern p:*  Dom\HTMLCollection []
modern x    Dom\HTMLCollection []
ns     'urn:u'/a Dom\HTMLCollection [p:a]
ns     '*'/a Dom\HTMLCollection [a,p:a,q:a]
ns     NULL/a Dom\HTMLCollection [a]
ns     ''/a Dom\HTMLCollection [a]
ns     'urn:u'/* Dom\HTMLCollection [p:a,p:b]
doc    *    Dom\HTMLCollection [r,a,p:a,q:a,b,p:b]
live before=0 after=1
NULL
string(1) "a"
