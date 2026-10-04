--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An HTML document cases element and attribute names for the caller
--FILE--
<?php
// php's HTML document is the one document kind that folds names, in ASCII
// alone, across the whole qualified name -- and not all in one direction:
// the element factory and the by-NAME attribute doors fold DOWN, while
// nodeName and tagName read back UP. Every *NS twin folds nothing, and no
// other document kind folds at all.
$H = 'http://www.w3.org/1999/xhtml';

$h = Dom\HTMLDocument::createFromString('<p FOO="1">x</p>', LIBXML_NOERROR);
$p = $h->getElementsByTagName('p')->item(0);

echo "## a parsed element reads back UP\n";
printf("local=%s nodeName=%s tagName=%s\n", $p->localName, $p->nodeName, $p->tagName);

echo "## the factory folds DOWN, the *NS twin does not\n";
foreach (['P', 'MiXeD', 'h:P', "p\u{00c9}"] as $n) {
    $e = $h->createElement($n);
    printf("createElement(%-8s) local=%-8s nodeName=%s\n", $n, $e->localName, $e->nodeName);
}
$ns = $h->createElementNS($H, 'P');
printf("createElementNS('P')  local=%-8s nodeName=%s\n", $ns->localName, $ns->nodeName);

echo "## the by-name attribute doors fold DOWN\n";
var_dump($p->getAttribute('FOO'), $p->hasAttribute('FOO'), $p->getAttributeNode('FOO')->nodeName);
$p->setAttribute('BAR', '1');
$p->toggleAttribute('BAZ');
foreach ($p->attributes as $a) {
    echo "  attr ", $a->nodeName, "\n";
}
$p->removeAttribute('FOO');
var_dump($p->hasAttribute('foo'));

echo "## the element's namespace decides, not the document's kind alone\n";
$svg = $h->createElementNS('http://www.w3.org/2000/svg', 'rect');
$svg->setAttribute('VIEWBOX', '0');
printf("svg nodeName=%s attr=%s\n", $svg->nodeName, $svg->attributes->item(0)->nodeName);

echo "## and the document is asked at READ time\n";
$x = Dom\XMLDocument::createFromString('<r xmlns="' . $H . '"><q/></r>');
$q = $x->documentElement->firstChild;
printf("in the XML document: %s\n", $q->nodeName);
$h->adoptNode($q);
printf("adopted into the HTML one: %s\n", $q->nodeName);

echo "## no other document kind folds\n";
printf("XMLDocument: local=%s nodeName=%s\n",
    ($e = $x->createElement('MiXeD'))->localName, $e->nodeName);
$l = new DOMDocument();
@$l->loadHTML('<p>x</p>', LIBXML_NOERROR);
$le = $l->createElement('MiXeD');
$le->setAttribute('QUX', '1');
printf("loadHTML:    local=%s nodeName=%s attr=%s\n",
    $le->localName, $le->nodeName, $le->attributes->item(0)->nodeName);
?>
--EXPECT--
## a parsed element reads back UP
local=p nodeName=P tagName=P
## the factory folds DOWN, the *NS twin does not
createElement(P       ) local=p        nodeName=P
createElement(MiXeD   ) local=mixed    nodeName=MIXED
createElement(h:P     ) local=h:p      nodeName=H:P
createElement(pÉ     ) local=pÉ      nodeName=PÉ
createElementNS('P')  local=P        nodeName=P
## the by-name attribute doors fold DOWN
string(1) "1"
bool(true)
string(3) "foo"
  attr foo
  attr bar
  attr baz
bool(false)
## the element's namespace decides, not the document's kind alone
svg nodeName=rect attr=VIEWBOX
## and the document is asked at READ time
in the XML document: q
adopted into the HTML one: Q
## no other document kind folds
XMLDocument: local=MiXeD nodeName=MiXeD
loadHTML:    local=MiXeD nodeName=MiXeD attr=QUX
