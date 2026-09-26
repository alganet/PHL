--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocumentFragment: appendXML and the splice its three insertion points do
--FILE--
<?php
// A fragment is not linked, it is emptied: its children move into the target.
$diag = [];
set_error_handler(function ($no, $str) use (&$diag) {
    // libxml's own parse diagnostic is printed in php's three-line context form
    // and in one line here (PLAN 7.4); record only that one was raised.
    // (one marker per KIND: php raises three lines per libxml error, this
    // engine one, and how many is not the point of the test)
    $diag[str_contains($str, 'appendXML') ? 'appendXML: parse error' : $str] = true;
    return true;
});
$mk = function () { $d = new DOMDocument; $d->loadXML('<r><k>t</k></r>'); return $d; };
$show = function ($label, $d) use (&$diag) {
    printf("%-26s %-42s %s\n", $label, str_replace("\n", '', $d->saveXML()), implode(' | ', array_keys($diag)));
    $diag = [];
};
$d = $mk();
$f = $d->createDocumentFragment();
var_dump(get_class($f), $f->nodeName, $f->nodeType, $f->nodeValue, $f->textContent, $f->childNodes->length);
$a = $d->createElement('a');
$f->appendChild($a);
$f->appendChild($d->createTextNode('tx'));
$ret = $d->documentElement->appendChild($f);
printf("appendChild ret=%s(%s) first=%d frag=%d\n", get_class($ret), $ret->nodeName,
    (int) ($ret === $a), $f->childNodes->length);
$show('after append', $d);
// insertBefore takes the same route and answers the same first node.
$f2 = $d->createDocumentFragment();
$f2->appendChild($d->createElement('z'));
$r2 = $d->documentElement->insertBefore($f2, $d->documentElement->firstChild);
printf("insertBefore ret=%s(%s)\n", get_class($r2), $r2->nodeName);
$show('after insertBefore', $d);
// replaceChild answers the node it replaced, as it always does.
$f3 = $d->createDocumentFragment();
$f3->appendChild($d->createElement('q'));
$r3 = $d->documentElement->replaceChild($f3, $d->documentElement->firstChild);
printf("replaceChild ret=%s(%s)\n", get_class($r3), $r3->nodeName);
$show('after replaceChild', $d);
// An EMPTY fragment is a warning and false for two of the three -- and for
// replaceChild it simply removes the old child.
$d = $mk();
var_dump($d->documentElement->appendChild($d->createDocumentFragment()));
$show('empty append', $d);
var_dump($d->documentElement->insertBefore($d->createDocumentFragment(), $d->documentElement->firstChild));
$show('empty insertBefore', $d);
$gone = $d->documentElement->replaceChild($d->createDocumentFragment(), $d->documentElement->firstChild);
printf("empty replaceChild ret=%s(%s)\n", get_class($gone), $gone->nodeName);
$show('empty replaceChild', $d);
// appendXML parses a WELL-BALANCED chunk: no single root, bare text allowed.
// (the EMPTY chunk is deliberately not here: libxml 2.9 refuses it and 2.13
// accepts it, and php answers whichever its own libxml does -- PLAN 7.4.)
foreach (['<a/><b/>', 'bare text', '<a>x</a>tail', '<!--c-->', '<a>'] as $chunk) {
    $d = $mk();
    $f = $d->createDocumentFragment();
    $ok = $f->appendXML($chunk);
    printf("%-14s %s children=%d %-18s %s\n", var_export($chunk, true), var_export($ok, true),
        $f->childNodes->length, str_replace("\n", '', $d->saveXML($f)), implode(' | ', array_keys($diag)));
    $diag = [];
}
--EXPECT--
string(19) "DOMDocumentFragment"
string(18) "#document-fragment"
int(11)
NULL
string(0) ""
int(0)
appendChild ret=DOMElement(a) first=1 frag=0
after append               <?xml version="1.0"?><r><k>t</k><a/>tx</r> 
insertBefore ret=DOMElement(z)
after insertBefore         <?xml version="1.0"?><r><z/><k>t</k><a/>tx</r> 
replaceChild ret=DOMElement(z)
after replaceChild         <?xml version="1.0"?><r><q/><k>t</k><a/>tx</r> 
bool(false)
empty append               <?xml version="1.0"?><r><k>t</k></r>       DOMNode::appendChild(): Document Fragment is empty
bool(false)
empty insertBefore         <?xml version="1.0"?><r><k>t</k></r>       DOMNode::insertBefore(): Document Fragment is empty
empty replaceChild ret=DOMElement(k)
empty replaceChild         <?xml version="1.0"?><r/>                  
'<a/><b/>'     true children=2 <a/><b/>           
'bare text'    true children=1 bare text          
'<a>x</a>tail' true children=2 <a>x</a>tail       
'<!--c-->'     true children=1 <!--c-->           
'<a>'          false children=0                    appendXML: parse error
