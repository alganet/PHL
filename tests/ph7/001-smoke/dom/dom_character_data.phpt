--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMCharacterData append/substring/insert/delete/replaceData, DOMText::splitText, and the CHARACTER-counted length + wholeText
--FILE--
<?php
$mk = function ($s = 'abcdef') {
    $d = new DOMDocument;
    $d->loadXML('<r/>');
    $t = $d->createTextNode($s);
    $d->documentElement->appendChild($t);
    return [$d, $t];
};

/* length and every offset are UTF-8 CHARACTERS, not bytes */
$d = new DOMDocument;
$d->loadXML('<r>áé漢字<!--çé--><![CDATA[ãõ]]></r>');
foreach ($d->documentElement->childNodes as $n) {
    echo get_class($n), ' len=', $n->length, ' bytes=', strlen($n->data), "\n";
}
$t = $d->documentElement->firstChild;
var_dump($t->substringData(0, 1), $t->substringData(1, 2), $t->substringData(2, 99));

/* wholeText is the whole RUN of adjacent text/CDATA siblings */
var_dump($t->wholeText);
$d->documentElement->appendChild($d->createTextNode('AA'));
$d->documentElement->appendChild($d->createTextNode('BB'));
var_dump($d->documentElement->childNodes->item(3)->wholeText, $t->wholeText);

/* substringData bounds */
foreach ([[0,3],[2,3],[0,0],[3,10],[6,1],[5,0],[7,1],[-1,2],[2,-1]] as $p) {
    [$dd, $tt] = $mk();
    try { var_dump($tt->substringData($p[0], $p[1])); }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* appendData takes raw bytes: no entity parsing */
[$dd, $tt] = $mk();
var_dump($tt->appendData('&amp;'), $tt->data);
echo $dd->saveXML($dd->documentElement), "\n";

/* insertData / deleteData / replaceData */
foreach ([[0,'X'],[3,'X'],[6,'X'],[2,''],[7,'X'],[-1,'X']] as $p) {
    [$dd, $tt] = $mk();
    try { $tt->insertData($p[0], $p[1]); echo 'ins ', $p[0], ' => ', $tt->data, "\n"; }
    catch (Throwable $e) { echo 'ins ', $p[0], ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach ([[0,3],[2,3],[3,10],[6,1],[0,0],[7,1],[-1,2],[2,-1]] as $p) {
    [$dd, $tt] = $mk();
    try { $tt->deleteData($p[0], $p[1]); echo 'del ', $p[0], ',', $p[1], ' => ', $tt->data, "\n"; }
    catch (Throwable $e) { echo 'del ', $p[0], ',', $p[1], ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach ([[0,3,'X'],[2,3,'XY'],[3,10,'Z'],[6,1,'Q'],[0,0,'A'],[7,1,'Q'],[2,-1,'Q']] as $p) {
    [$dd, $tt] = $mk();
    try { $tt->replaceData($p[0], $p[1], $p[2]); echo 'rep ', $p[0], ',', $p[1], ' => ', $tt->data, "\n"; }
    catch (Throwable $e) { echo 'rep ', $p[0], ',', $p[1], ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
/* the family is inherited by comments and CDATA sections */
$c = new DOMDocument;
$c->loadXML('<r><!--comment--><![CDATA[cdata]]></r>');
$cm = $c->documentElement->firstChild;
$cd = $c->documentElement->lastChild;
var_dump($cm->substringData(0, 3), $cm->appendData('!!'), $cm->data);
var_dump($cd->replaceData(0, 2, 'ZZ'), $cd->data);
echo $c->saveXML($c->documentElement), "\n";

/* splitText: the receiver keeps the head, a second node takes the tail */
foreach ([0, 1, 3, 6, 7] as $o) {
    $s = new DOMDocument;
    $s->loadXML('<r>abcdef<k/></r>');
    $st = $s->documentElement->firstChild;
    $new = $st->splitText($o);
    echo 'split ', $o, ' => ', ($new === false ? 'false' : get_class($new) . '(' . $new->data . ')'),
        ' head=', var_export($st->data, true),
        ' next=', ($st->nextSibling ? get_class($st->nextSibling) . '(' . $st->nextSibling->nodeValue . ')' : '-'),
        ' xml=', $s->saveXML($s->documentElement), "\n";
}
$s = new DOMDocument;
$s->loadXML('<r>áé漢字</r>');
$st = $s->documentElement->firstChild;
$new = $st->splitText(2);
var_dump($st->data, $new->data);
/* a CDATA section splits into a TEXT node */
$s2 = new DOMDocument;
$s2->loadXML('<r><![CDATA[abcdef]]></r>');
$new2 = $s2->documentElement->firstChild->splitText(2);
var_dump(get_class($new2), $s2->saveXML($s2->documentElement));
/* detached, and the negative refusal */
$s3 = new DOMDocument;
$s3->loadXML('<r/>');
$det = $s3->createTextNode('abcdef');
$new3 = $det->splitText(2);
var_dump($det->data, $new3->data, $new3->parentNode);
try { $det->splitText(-1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* isWhitespaceInElementContent / isElementContentWhitespace */
$w = new DOMDocument;
$w->loadXML("<r>  <k/>x<![CDATA[ ]]></r>");
foreach ($w->documentElement->childNodes as $n) {
    if ($n instanceof DOMText) {
        var_dump(get_class($n), $n->isElementContentWhitespace(), $n->isWhitespaceInElementContent());
    }
}
--EXPECT--
DOMText len=4 bytes=10
DOMComment len=2 bytes=4
DOMCdataSection len=2 bytes=4
string(2) "á"
string(5) "é漢"
string(6) "漢字"
string(10) "áé漢字"
string(8) "ãõAABB"
string(10) "áé漢字"
string(3) "abc"
string(3) "cde"
string(0) ""
string(3) "def"
string(0) ""
string(0) ""
DOMException: Index Size Error
DOMException: Index Size Error
DOMException: Index Size Error
bool(true)
string(11) "abcdef&amp;"
<r>abcdef&amp;amp;</r>
ins 0 => Xabcdef
ins 3 => abcXdef
ins 6 => abcdefX
ins 2 => abcdef
ins 7 => DOMException: Index Size Error
ins -1 => DOMException: Index Size Error
del 0,3 => def
del 2,3 => abf
del 3,10 => abc
del 6,1 => abcdef
del 0,0 => abcdef
del 7,1 => DOMException: Index Size Error
del -1,2 => DOMException: Index Size Error
del 2,-1 => DOMException: Index Size Error
rep 0,3 => Xdef
rep 2,3 => abXYf
rep 3,10 => abcZ
rep 6,1 => abcdefQ
rep 0,0 => Aabcdef
rep 7,1 => DOMException: Index Size Error
rep 2,-1 => DOMException: Index Size Error
string(3) "com"
bool(true)
string(9) "comment!!"
bool(true)
string(5) "ZZata"
<r><!--comment!!--><![CDATA[ZZata]]></r>
split 0 => DOMText(abcdef) head='' next=DOMText(abcdef) xml=<r>abcdef<k/></r>
split 1 => DOMText(bcdef) head='a' next=DOMText(bcdef) xml=<r>abcdef<k/></r>
split 3 => DOMText(def) head='abc' next=DOMText(def) xml=<r>abcdef<k/></r>
split 6 => DOMText() head='abcdef' next=DOMText() xml=<r>abcdef<k/></r>
split 7 => false head='abcdef' next=DOMElement() xml=<r>abcdef<k/></r>
string(4) "áé"
string(6) "漢字"
string(7) "DOMText"
string(25) "<r><![CDATA[ab]]>cdef</r>"
string(2) "ab"
string(4) "cdef"
NULL
ValueError: DOMText::splitText(): Argument #1 ($offset) must be greater than or equal to 0
string(7) "DOMText"
bool(true)
bool(true)
string(7) "DOMText"
bool(false)
bool(false)
string(15) "DOMCdataSection"
bool(true)
bool(true)
