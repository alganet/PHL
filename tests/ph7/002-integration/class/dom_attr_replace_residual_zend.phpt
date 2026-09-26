--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: appendChild's attribute replacement FREES the displaced node -- a held wrapper reads Invalid State; an attribute reference splices into libxml's property chain (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the displaced attribute's wrapper is DEAD: php freed the node under it */
$d = new DOMDocument;
$d->loadXML('<r k="old"/>');
$held = $d->documentElement->getAttributeNode('k');
$a = $d->createAttribute('k');
$a->value = 'new';
$d->documentElement->appendChild($a);
echo $d->saveXML($d->documentElement), "\n";
try { $held->value; } catch (DOMException $e) { echo get_class($e), '(', $e->getCode(), ') ', $e->getMessage(), "\n"; }
try { $held->cloneNode(); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* a non-attribute argument against an ATTRIBUTE reference goes to
 * xmlAddPrevSibling, which splices it into the PROPERTY chain: the bytes and
 * the child list read as though nothing happened, but the argument's
 * parentNode answers the receiver */
$x = new DOMDocument;
$x->loadXML('<r><c b="2">t</c></r>');
$c = $x->documentElement->firstChild;
$e = $x->createElement('nE');
$ret = $c->insertBefore($e, $c->getAttributeNode('b'));
echo get_class($ret), "\n";
echo $x->saveXML($x->documentElement), "\n";
var_dump($c->childNodes->length, $e->parentNode === $c);
--EXPECT--
<r k="new"/>
DOMException(11) Invalid State Error
Error: Couldn't fetch DOMAttr
DOMElement
<r><c b="2">t</c></r>
int(1)
bool(true)
--CLEAN--
<?php
