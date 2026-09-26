--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing DOMNode::$prefix, and the class/id attributes behind two properties
--FILE--
<?php
$mk = function ($xml) { $d = new DOMDocument; $d->loadXML($xml); return $d; };
$show = function ($label, $d) { printf("%-30s %s\n", $label, str_replace("\n", '', $d->saveXML())); };
// A prefix write re-points the node at a declaration binding the SAME URI --
// reusing one that is already in scope, or declaring one where there is none.
foreach (['b', '', 'zz'] as $p) {
    $d = $mk('<a:r xmlns:a="urn:a" xmlns:b="urn:a" a:k="v"><a:c/></a:r>');
    $d->documentElement->prefix = $p;
    $show('elem prefix ' . var_export($p, true), $d);
}
// Only the node's OWN declarations count: an ancestor's is not reused, so
// re-prefixing a child grows a second one beside its parent's.
$d = $mk('<a:r xmlns:a="urn:a" xmlns:q="urn:a"><a:m><a:c/></a:m></a:r>');
$d->documentElement->firstChild->firstChild->prefix = 'q';
$show('ancestor decl not reused', $d);
// A prefix already bound HERE to another URI is libxml's refusal.
$d = $mk('<a:r xmlns:a="urn:a" xmlns:b="urn:other"/>');
try { $d->documentElement->prefix = 'b'; echo "reused\n"; }
catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
// An attribute's new declaration lands on its element.
$d = $mk('<r xmlns:a="urn:a" a:k="v"/>');
$d->documentElement->attributes->item(0)->prefix = 'q';
$show('attr prefix q', $d);
// A node with no namespace at all is left alone, and so is one that cannot
// carry a prefix.
$d = $mk('<r k="v">t</r>');
$d->documentElement->prefix = 'z';
$d->documentElement->firstChild->prefix = 'z';
$show('no-namespace prefix', $d);
// `xml` may only ever name its own namespace.
$d = $mk('<a:r xmlns:a="urn:a"/>');
try { $d->documentElement->prefix = 'xml'; echo "xml prefix ok\n"; }
catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
// className and id ARE the class and id attributes, both ways.
$d = $mk('<r class="one" id="two"/>');
$e = $d->documentElement;
var_dump($e->className, $e->id);
$e->className = 'three'; $e->id = 'four';
$show('class/id written', $d);
var_dump($e->getAttribute('class'), $e->getAttribute('id'));
$d = $mk('<r/>');
var_dump($d->documentElement->className, $d->documentElement->id);
--EXPECT--
elem prefix 'b'                <?xml version="1.0"?><b:r xmlns:a="urn:a" xmlns:b="urn:a" a:k="v"><a:c/></b:r>
elem prefix ''                 <?xml version="1.0"?><r xmlns:a="urn:a" xmlns:b="urn:a" xmlns="urn:a" a:k="v"><a:c/></r>
elem prefix 'zz'               <?xml version="1.0"?><zz:r xmlns:a="urn:a" xmlns:b="urn:a" xmlns:zz="urn:a" a:k="v"><a:c/></zz:r>
ancestor decl not reused       <?xml version="1.0"?><a:r xmlns:a="urn:a" xmlns:q="urn:a"><a:m><q:c xmlns:q="urn:a"/></a:m></a:r>
DOMException: Namespace Error
attr prefix q                  <?xml version="1.0"?><r xmlns:a="urn:a" xmlns:q="urn:a" q:k="v"/>
no-namespace prefix            <?xml version="1.0"?><r k="v">t</r>
DOMException: Namespace Error
string(3) "one"
string(3) "two"
class/id written               <?xml version="1.0"?><r class="three" id="four"/>
string(5) "three"
string(4) "four"
string(0) ""
string(0) ""
