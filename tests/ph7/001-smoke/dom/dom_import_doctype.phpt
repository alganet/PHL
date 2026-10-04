--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
importNode carries a DOCTYPE across, subset and all, at either depth
--FILE--
<?php
$dom_idt_src = static function (string $dt): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($dt . '<r/>');
    return $d;
};
$dom_idt_show = static function (object $d, $i): string {
    if (!is_object($i)) return var_export($i, true);
    return get_class($i)
        . ' name=' . $i->name
        . ' pub=' . var_export($i->publicId, true)
        . ' sys=' . var_export($i->systemId, true)
        . ' sub=' . var_export($i->internalSubset, true)
        . ' ents=' . $i->entities->length . ' nots=' . $i->notations->length
        . ' kids=' . $i->childNodes->length
        . ' owner=' . var_export($i->ownerDocument === $d, true)
        . ' parent=' . ($i->parentNode ? $i->parentNode->nodeName : 'NULL')
        . ' connected=' . var_export($i->isConnected, true)
        . ' type=' . $i->nodeType;
};

/* Every shape of DOCTYPE, at both depths. The internal subset is not a child in
 * the ordinary sense, so it rides along whatever $deep says -- while the
 * `internalSubset` property reads the RECEIVER document's subset rather than the
 * copy's own declarations, and so answers null on an orphan. */
$dom_idt_docs = [
    'public' => '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0//EN" "http://www.w3.org/TR/xhtml1.dtd">',
    'system' => '<!DOCTYPE root SYSTEM "urn:sys">',
    'bare'   => '<!DOCTYPE root>',
    'subset' => '<!DOCTYPE root [<!ENTITY e "v"><!ELEMENT r EMPTY><!NOTATION n SYSTEM "urn:n">]>',
];
foreach ($dom_idt_docs as $label => $decl) {
    foreach ([false, true] as $deep) {
        $s = $dom_idt_src($decl);
        $d = new DOMDocument;
        printf("%-7s deep=%d %s\n", $label, (int)$deep,
            $dom_idt_show($d, $d->importNode($s->doctype, $deep)));
    }
}

/* The copy serializes as the declaration it came from, and appending it makes
 * the receiver's own doctype -- at which point the subset reads back. */
$dom_idt_s = $dom_idt_src($dom_idt_docs['subset']);
$dom_idt_d = new DOMDocument;
$dom_idt_i = $dom_idt_d->importNode($dom_idt_s->doctype, false);
var_dump($dom_idt_d->saveXML($dom_idt_i));
var_dump($dom_idt_d->doctype);
$dom_idt_d->appendChild($dom_idt_i);
$dom_idt_d->appendChild($dom_idt_d->createElement('r'));
var_dump($dom_idt_d->doctype === $dom_idt_i, $dom_idt_i->internalSubset,
    $dom_idt_i->isConnected, $dom_idt_d->saveXML());

/* Two imports are two copies, and the source keeps its own. */
$dom_idt_d2 = new DOMDocument;
$dom_idt_a = $dom_idt_d2->importNode($dom_idt_s->doctype, true);
$dom_idt_b = $dom_idt_d2->importNode($dom_idt_s->doctype, true);
var_dump($dom_idt_a === $dom_idt_b, $dom_idt_a === $dom_idt_s->doctype,
    $dom_idt_s->doctype->ownerDocument === $dom_idt_s,
    $dom_idt_s->doctype->internalSubset,
    $dom_idt_s->saveXML());

/* A doctype already OF this document is answered unchanged, like any node. */
var_dump($dom_idt_d->importNode($dom_idt_d->doctype, true) === $dom_idt_i);

/* The 8.4 tree: its own doctype through importNode, and a 2004 one through the
 * single door that crosses trees. Both arrive wrapped by the RECEIVER's family. */
$dom_idt_m = Dom\XMLDocument::createFromString($dom_idt_docs['public'] . '<r/>');
$dom_idt_m2 = Dom\XMLDocument::createEmpty();
foreach ([false, true] as $deep) {
    $i = $dom_idt_m2->importNode($dom_idt_m->doctype, $deep);
    printf("modern  deep=%d %s\n", (int)$deep, $dom_idt_show($dom_idt_m2, $i));
}
$dom_idt_l = $dom_idt_m2->importLegacyNode($dom_idt_s->doctype, true);
printf("legacy-in %s\n", $dom_idt_show($dom_idt_m2, $dom_idt_l));
var_dump($dom_idt_l instanceof Dom\DocumentType, $dom_idt_l instanceof DOMDocumentType);
?>
--EXPECT--
public  deep=0 DOMDocumentType name=html pub='-//W3C//DTD XHTML 1.0//EN' sys='http://www.w3.org/TR/xhtml1.dtd' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
public  deep=1 DOMDocumentType name=html pub='-//W3C//DTD XHTML 1.0//EN' sys='http://www.w3.org/TR/xhtml1.dtd' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
system  deep=0 DOMDocumentType name=root pub='' sys='urn:sys' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
system  deep=1 DOMDocumentType name=root pub='' sys='urn:sys' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
bare    deep=0 DOMDocumentType name=root pub='' sys='' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
bare    deep=1 DOMDocumentType name=root pub='' sys='' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
subset  deep=0 DOMDocumentType name=root pub='' sys='' sub=NULL ents=1 nots=1 kids=2 owner=true parent=NULL connected=false type=10
subset  deep=1 DOMDocumentType name=root pub='' sys='' sub=NULL ents=1 nots=1 kids=2 owner=true parent=NULL connected=false type=10
string(54) "<!DOCTYPE root [
<!ENTITY e "v">
<!ELEMENT r EMPTY>
]>"
NULL
bool(true)
string(35) "<!ENTITY e "v">
<!ELEMENT r EMPTY>
"
bool(true)
string(112) "<?xml version="1.0"?>
<!DOCTYPE root [
<!NOTATION n SYSTEM "urn:n" >
<!ENTITY e "v">
<!ELEMENT r EMPTY>
]>
<r/>
"
bool(false)
bool(false)
bool(true)
string(35) "<!ENTITY e "v">
<!ELEMENT r EMPTY>
"
string(112) "<?xml version="1.0"?>
<!DOCTYPE root [
<!NOTATION n SYSTEM "urn:n" >
<!ENTITY e "v">
<!ELEMENT r EMPTY>
]>
<r/>
"
bool(true)
modern  deep=0 Dom\DocumentType name=html pub='-//W3C//DTD XHTML 1.0//EN' sys='http://www.w3.org/TR/xhtml1.dtd' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
modern  deep=1 Dom\DocumentType name=html pub='-//W3C//DTD XHTML 1.0//EN' sys='http://www.w3.org/TR/xhtml1.dtd' sub=NULL ents=0 nots=0 kids=0 owner=true parent=NULL connected=false type=10
legacy-in Dom\DocumentType name=root pub='' sys='' sub=NULL ents=1 nots=1 kids=2 owner=true parent=NULL connected=false type=10
bool(true)
bool(false)
