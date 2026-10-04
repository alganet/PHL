--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\DocumentFragment::appendXml is the 2004 door's body under the 8.4 spelling
--FILE--
<?php
// php 8.4 renamed the trailing `XML` to `Xml` and made the tentative return
// type real; the parse, the refusals and the empty-chunk `false` are the same
// body. libxml's own parse diagnostic is printed in php's three-line context
// form and in one line here, so count the raised warnings rather than word
// them -- that gap is the shared capture's, not this door's.
$warn = 0;
set_error_handler(function ($no, $str) use (&$warn) {
    $warn++;
    return true;
});

$doc = Dom\XMLDocument::createEmpty();

foreach ([
    'two roots' => '<a/><b>x</b>',
    'bare text' => 'hello',
    'mixed'     => 'lead<a/>tail',
    'empty'     => '',
    'unbalanced'=> '<a>',
    'undeclared entity' => '<a>&nope;</a>',
] as $label => $chunk) {
    $warn = 0;
    $frag = $doc->createDocumentFragment();
    $rc = $frag->appendXml($chunk);
    printf("%-18s rc=%s children=%d warned=%s xml=%s\n",
        $label, var_export($rc, true), $frag->childNodes->length,
        $warn > 0 ? 'yes' : 'no', $doc->saveXml($frag));
}

// A refused chunk leaves what an earlier accepted one appended.
$frag = $doc->createDocumentFragment();
var_dump($frag->appendXml('<keep/>'));
var_dump($frag->appendXml('<a>'));
echo $doc->saveXml($frag), "\n";

// The fragment still splices into the tree the way append() does.
$doc2 = Dom\XMLDocument::createFromString('<r><anchor/></r>');
$f2 = $doc2->createDocumentFragment();
$f2->appendXml('<one/><two/>');
$doc2->documentElement->appendChild($f2);
echo $doc2->saveXml($doc2->documentElement), "\n";
var_dump($f2->childNodes->length);

// The empty chunk is refused by the engine, not by libxml: 2.9 answers a
// nonzero code for it and 2.13 answers 0 with an empty node list, so both
// doors have to say `false` here without asking the library.
$old = new DOMDocument;
$old->loadXML('<r/>');
$of = $old->createDocumentFragment();
$warn = 0;
var_dump($of->appendXML(''), $of->childNodes->length, $warn);

restore_error_handler();

$m = new ReflectionMethod('Dom\DocumentFragment', 'appendXml');
echo $m->getDeclaringClass()->getName(), ' ',
     $m->getNumberOfParameters(), ' ',
     $m->getReturnType(), "\n";
var_dump(method_exists('Dom\DocumentFragment', 'appendXML'));
?>
--EXPECT--
two roots          rc=true children=2 warned=no xml=<a/><b>x</b>
bare text          rc=true children=1 warned=no xml=hello
mixed              rc=true children=3 warned=no xml=lead<a/>tail
empty              rc=false children=0 warned=no xml=
unbalanced         rc=false children=0 warned=yes xml=
undeclared entity  rc=false children=0 warned=yes xml=
bool(true)
bool(false)
<keep/>
<r><anchor/><one/><two/></r>
int(0)
bool(false)
int(0)
int(0)
Dom\DocumentFragment 1 bool
bool(true)
