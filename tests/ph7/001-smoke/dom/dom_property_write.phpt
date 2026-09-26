--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing a DOM content property: nodeValue parses entities, textContent does not
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "warning: $str\n"; return true; });
$xml = '<r k="v">t1<c/><!--cm--><![CDATA[cd]]></r>';
$mk = function () use ($xml) { $d = new DOMDocument; $d->loadXML($xml); return $d; };
$show = function ($label, $d) { printf("%-34s %s\n", $label, str_replace("\n", '', $d->saveXML())); };
// nodeValue PARSES entity references on an element or an attribute; textContent
// stores the bytes. Both drop the node's children first. (A MALFORMED reference
// is deliberately not pinned here: libxml 2.9 refuses it and leaves the node
// empty, 2.13 drops the ampersand and says nothing -- PLAN 7.4.)
foreach (['nodeValue', 'textContent'] as $p) {
    foreach (['plain', 'a&amp;b', '<x/>', ''] as $v) {
        $d = $mk(); $d->documentElement->$p = $v;
        $show("elem $p " . var_export($v, true), $d);
    }
}
$d = $mk(); $d->documentElement->attributes->item(0)->value = 'a&amp;b';
$show('attr value entity', $d);
$d = $mk(); $d->documentElement->attributes->item(0)->textContent = 'a&amp;b';
$show('attr textContent literal', $d);
$d = $mk(); $d->documentElement->firstChild->data = 'a&b<x>';
$show('text data literal', $d);
$d = $mk(); $d->documentElement->childNodes->item(2)->data = 'a&b';
$show('comment data literal', $d);
$d = $mk(); $d->documentElement->childNodes->item(3)->data = 'a&b';
$show('cdata data literal', $d);
// An empty write still leaves one text child, which is what keeps <r></r> from
// collapsing to <r/>.
$d = $mk(); $d->documentElement->textContent = '';
printf("empty write: children=%d %s\n", $d->documentElement->childNodes->length,
    str_replace("\n", '', $d->saveXML()));
// A child a variable still holds survives its parent being overwritten, and
// comes away detached -- while a node UNDER one that also survives keeps the
// parent it had, which is php's rule for the subtree it drops.
$d = $mk(); $kept = $d->documentElement->firstChild;
$d->documentElement->textContent = 'new';
var_dump($kept->nodeValue, $kept->parentNode, $d->documentElement->textContent);
$d = new DOMDocument; $d->loadXML('<r><a><b>deep</b></a></r>');
$a = $d->documentElement->firstChild; $b = $a->firstChild;
$d->documentElement->textContent = 'flat';
printf("a: parent=%s first=%s | b: parent=%s text=%s | %s\n",
    $a->parentNode ? $a->parentNode->nodeName : 'NULL',
    $a->firstChild ? $a->firstChild->nodeName : 'NULL',
    $b->parentNode ? $b->parentNode->nodeName : 'NULL',
    $b->textContent, str_replace("\n", '', $d->saveXML()));
// Scalars coerce; the write goes through a copy, so the caller's own value is
// left alone.
$d = $mk(); $v = 5; $d->documentElement->nodeValue = $v;
var_dump($v, $d->documentElement->nodeValue);
$d = $mk(); $d->documentElement->nodeValue = true;  echo $d->documentElement->nodeValue, "\n";
$d = $mk(); $d->documentElement->nodeValue = 1.25;  echo $d->documentElement->nodeValue, "\n";
$d = $mk(); $d->documentElement->nodeValue = null;  var_dump($d->documentElement->nodeValue);
--EXPECT--
elem nodeValue 'plain'             <?xml version="1.0"?><r k="v">plain</r>
elem nodeValue 'a&amp;b'           <?xml version="1.0"?><r k="v">a&amp;b</r>
elem nodeValue '<x/>'              <?xml version="1.0"?><r k="v">&lt;x/&gt;</r>
elem nodeValue ''                  <?xml version="1.0"?><r k="v"></r>
elem textContent 'plain'           <?xml version="1.0"?><r k="v">plain</r>
elem textContent 'a&amp;b'         <?xml version="1.0"?><r k="v">a&amp;amp;b</r>
elem textContent '<x/>'            <?xml version="1.0"?><r k="v">&lt;x/&gt;</r>
elem textContent ''                <?xml version="1.0"?><r k="v"></r>
attr value entity                  <?xml version="1.0"?><r k="a&amp;b">t1<c/><!--cm--><![CDATA[cd]]></r>
attr textContent literal           <?xml version="1.0"?><r k="a&amp;amp;b">t1<c/><!--cm--><![CDATA[cd]]></r>
text data literal                  <?xml version="1.0"?><r k="v">a&amp;b&lt;x&gt;<c/><!--cm--><![CDATA[cd]]></r>
comment data literal               <?xml version="1.0"?><r k="v">t1<c/><!--a&b--><![CDATA[cd]]></r>
cdata data literal                 <?xml version="1.0"?><r k="v">t1<c/><!--cm--><![CDATA[a&b]]></r>
empty write: children=1 <?xml version="1.0"?><r k="v"></r>
string(2) "t1"
NULL
string(3) "new"
a: parent=NULL first=b | b: parent=a text=deep | <?xml version="1.0"?><r>flat</r>
int(5)
string(1) "5"
1
1.25
string(0) ""
