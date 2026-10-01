--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version') && PHP_VERSION_ID < 80511) echo 'skip php before 8.5.11 creates no attribute through attributes()'; ?>
--TEST--
ext/simplexml: a write through attributes() creates the attribute it names
--DESCRIPTION--
php has one handler for a property write and a dimension write, and on an
attribute list both name an ATTRIBUTE. Up to 8.5.10 that handler took the
list's first attribute for the element, so a name the element did not have yet
was dropped in silence (and an element with no attributes at all took no write).
8.5.11 writes to the element: the attribute is created, by either door, and it
carries no namespace even when the list is filtered to one.
--FILE--
<?php
foreach ([
    'dim, beside one'  => ['<r a="1"/>', function ($x) { $x->attributes()['new'] = 'v'; }],
    'dim, first one'   => ['<r/>',       function ($x) { $x->attributes()['created'] = 'yes'; }],
    'prop, beside one' => ['<r a="1"/>', function ($x) { $x->attributes()->other = 2; }],
    'prop, first one'  => ['<r/>',       function ($x) { $x->attributes()->q = 'v'; }],
    'dim, existing'    => ['<r a="1"/>', function ($x) { $x->attributes()['a'] = '2'; }],
    'held list'        => ['<r a="1"/>', function ($x) { $l = $x->attributes(); $l->b = 'x'; $l['c'] = 'y'; }],
    'ns-filtered list' => ['<r xmlns:p="urn:p" p:a="1"/>',
                           function ($x) { $x->attributes('urn:p')['b'] = 'v'; $x->attributes('urn:p')->a = 'Z'; }],
    'escaped'          => ['<r/>',       function ($x) { $x->attributes()->q = 'a&b<c>'; }],
] as $label => [$xml, $op]) {
    $x = simplexml_load_string($xml);
    $op($x);
    printf("%-17s %s\n", $label, trim(substr($x->asXML(), strlen('<?xml version="1.0"?>'))));
}
?>
--EXPECT--
dim, beside one   <r a="1" new="v"/>
dim, first one    <r created="yes"/>
prop, beside one  <r a="1" other="2"/>
prop, first one   <r q="v"/>
dim, existing     <r a="2"/>
held list         <r a="1" b="x" c="y"/>
ns-filtered list  <r xmlns:p="urn:p" p:a="Z" b="v"/>
escaped           <r q="a&amp;b&lt;c&gt;"/>
