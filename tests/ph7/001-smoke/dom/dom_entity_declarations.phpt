--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DTD's entity declarations are DOMEntity, keyed in a named map
--FILE--
<?php
$dom_ent_doc = static function (): DOMDocument {
    $d = new DOMDocument;
    $d->substituteEntities = false;
    $d->loadXML('<!DOCTYPE r ['
        . '<!ENTITY up PUBLIC "-//PUB//EN" "u.xml" NDATA nn>'
        . '<!ENTITY us SYSTEM "s.xml" NDATA nn>'
        . '<!ENTITY ip "internal">'
        . '<!ENTITY ep SYSTEM "e.xml">'
        . '<!ENTITY % pe "<!ELEMENT r EMPTY>">'
        . '<!NOTATION nn SYSTEM "v">'
        . ']><r>&ip;</r>');
    return $d;
};
$d = $dom_ent_doc();
$dt = $d->doctype;
$map = $dt->entities;

// A PARAMETER entity is a child of the DTD like every other declaration and is
// NOT in this map: php reads libxml's `entities` table, not the child list.
printf("%s len=%d count=%d countable=%s children=%d\n", get_class($map),
    $map->length, $map->count(), var_export($map instanceof Countable, true),
    $dt->childNodes->length);
$seen = [];
foreach ($map as $name => $ent) {
    $seen[] = $name . ':' . get_class($ent);
}
sort($seen);   // the map's own order is libxml's hash order
echo implode(' ', $seen), "\n";

// The DOM's "for an UNPARSED entity" rule, which php follows to the letter:
// the three identifiers are null for everything that is not NDATA-declared,
// so an external but PARSED entity reads null from all three.
// (`textContent` is deliberately not asked here: libxml 2.9 answers "" for an
// internal entity's replacement text where 2.13 answers the text itself, so
// the cell has no cross-version answer to pin.)
foreach (['up', 'us', 'ip', 'ep'] as $name) {
    $e = $map->getNamedItem($name);
    printf("%-3s type=%d name=%s pub=%-13s sys=%-9s not=%s value=%s\n",
        $name, $e->nodeType, var_export($e->nodeName, true),
        var_export($e->publicId, true), var_export($e->systemId, true),
        var_export($e->notationName, true), var_export($e->nodeValue, true));
}

// It hangs off the doctype, and an internal entity's replacement text is its
// child once something has referenced it.
$ip = $map->getNamedItem('ip');
var_dump($ip->parentNode === $dt, $ip->ownerDocument === $d, $ip->isConnected,
    $ip->childNodes->length, $ip->firstChild->data, $ip->hasChildNodes(),
    $ip->localName, $ip->prefix, $ip->namespaceURI, $ip->attributes,
    $ip->getNodePath(), get_parent_class('DOMEntity'));

// The map is a fresh object per read and the ENTRIES are the identity-cached
// wrappers -- which is also what an entity REFERENCE answers as its child.
var_dump($dt->entities === $dt->entities, $map->item(0) === $map->item(0),
    $d->documentElement->firstChild->firstChild === $ip,
    $map->getNamedItem('pe'), $map->getNamedItem('zz'), $map->item(99),
    get_class($map->getNamedItemNS(null, 'ip')),
    get_class($map->getNamedItemNS('urn:x', 'ip')));

// php 8.4 deprecated three of the six, and the notice fires on a read and on
// an isset() alike -- not on a WRITE, where the readonly refusal answers first
// and says nothing about deprecation.
set_error_handler(static function (int $n, string $s): bool {
    printf("  [E%d] %s\n", $n, $s);
    return true;
});
foreach (['actualEncoding', 'encoding', 'version'] as $prop) {
    var_dump($ip->$prop);
}
var_dump(isset($ip->encoding));
restore_error_handler();
foreach (['publicId', 'systemId', 'notationName', 'actualEncoding', 'encoding',
          'version'] as $prop) {
    try {
        $ip->$prop = 'x';
    } catch (Throwable $ex) {
        printf("%-14s %s: %s\n", $prop, get_class($ex), $ex->getMessage());
    }
}

// The declarations are the DTD's: php refuses to give one children, copying
// one answers false, and a doctype that declares nothing has an empty map.
try {
    $ip->appendChild($d->createElement('x'));
} catch (Throwable $ex) {
    printf("appendChild %s(%d): %s\n", get_class($ex), $ex->getCode(), $ex->getMessage());
}
$empty = new DOMDocument;
$empty->loadXML('<!DOCTYPE r><r/>');
var_dump($ip->cloneNode(true), get_class(clone $ip), $ip->isEqualNode($ip),
    str_replace("\n", '', $d->saveXML($ip)), $empty->doctype->entities->length);
?>
--EXPECT--
DOMNamedNodeMap len=4 count=4 countable=true children=5
ep:DOMEntity ip:DOMEntity up:DOMEntity us:DOMEntity
up  type=17 name='up' pub='-//PUB//EN'  sys='u.xml'   not='nn' value=NULL
us  type=17 name='us' pub=NULL          sys='s.xml'   not='nn' value=NULL
ip  type=17 name='ip' pub=NULL          sys=NULL      not=NULL value=NULL
ep  type=17 name='ep' pub=NULL          sys=NULL      not=NULL value=NULL
bool(true)
bool(true)
bool(true)
int(1)
string(8) "internal"
bool(true)
NULL
string(0) ""
NULL
NULL
NULL
string(7) "DOMNode"
bool(false)
bool(true)
bool(true)
NULL
NULL
NULL
string(9) "DOMEntity"
string(9) "DOMEntity"
  [E8192] Property DOMEntity::$actualEncoding is deprecated
NULL
  [E8192] Property DOMEntity::$encoding is deprecated
NULL
  [E8192] Property DOMEntity::$version is deprecated
NULL
  [E8192] Property DOMEntity::$encoding is deprecated
bool(false)
publicId       Error: Cannot modify readonly property DOMEntity::$publicId
systemId       Error: Cannot modify readonly property DOMEntity::$systemId
notationName   Error: Cannot modify readonly property DOMEntity::$notationName
actualEncoding Error: Cannot modify readonly property DOMEntity::$actualEncoding
encoding       Error: Cannot modify readonly property DOMEntity::$encoding
version        Error: Cannot modify readonly property DOMEntity::$version
appendChild DOMException(7): No Modification Allowed Error
bool(false)
string(9) "DOMEntity"
bool(true)
string(23) "<!ENTITY ip "internal">"
int(0)
