--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An HTML document answers for its own head, body and title
--FILE--
<?php
// `head` and `body` are questions about the DOCUMENT ELEMENT, not searches:
// both answer null unless the root is an `html` element in the HTML namespace,
// and then it is the first CHILD of that root -- also in the HTML namespace --
// whose name matches. `head` matches `head`; `body` matches `body` OR
// `frameset`, so a document whose root child is a frameset answers it through
// `body` -- the write face covers that shape, which this parser does not build.
//
// `title` is the other shape: the first `title` element anywhere in the
// document in TREE ORDER -- so one that sits in the body is the document's
// title -- read as the CHILD TEXT CONTENT of that element with ASCII
// whitespace stripped and collapsed. An SVG root takes a narrower rule: a
// direct `title` child of the root, in the SVG namespace.
//
// All three are declared on the abstract base, so an XML document is asked the
// same way rather than refusing.
function htmlDocDoors($label, $src, $xml = false) {
    $d = $xml ? Dom\XMLDocument::createFromString($src, LIBXML_NOERROR)
              : Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR);
    printf("%-22s head=%-16s body=%-16s title=%s\n", $label,
        $d->head === null ? 'null' : get_class($d->head) . '/' . $d->head->localName,
        $d->body === null ? 'null' : get_class($d->body) . '/' . $d->body->localName,
        var_export($d->title, true));
}

htmlDocDoors('plain', '<!DOCTYPE html><html><head><title>t</title></head><body>x</body></html>');
htmlDocDoors('two heads', '<html><head id=1></head><head id=2></head><body></body></html>');
htmlDocDoors('title in body', '<html><head></head><body><title>bodytitle</title></body></html>');
htmlDocDoors('title whitespace', "<html><head><title>  a \t\n b  </title></head><body></body></html>");
htmlDocDoors('xml, no html root', '<r><head/><body/></r>', true);
htmlDocDoors('xml, xhtml root', '<html xmlns="http://www.w3.org/1999/xhtml"><head/><body/></html>', true);
htmlDocDoors('xml, svg root', '<svg xmlns="http://www.w3.org/2000/svg"><title>svgt</title></svg>', true);

// A document with no root at all has none of the three.
$e = Dom\HTMLDocument::createEmpty();
var_dump($e->head, $e->body, $e->title);

// ...and the producer that writes a root writes all three.
$i = (new Dom\Implementation)->createHTMLDocument('Hi');
printf("produced: %s %s %s\n", get_class($i->head), get_class($i->body),
    var_export($i->title, true));

// The child text content of a title is its TEXT children only: an element
// child contributes nothing to it.
$b = Dom\HTMLDocument::createFromString('<html><head></head><body></body></html>', LIBXML_NOERROR);
$t = $b->createElement('title');
$t->appendChild($b->createTextNode('o'));
$t->appendChild($b->createElement('b'))->appendChild($b->createTextNode('u'));
$t->appendChild($b->createTextNode('ter'));
$b->head->appendChild($t);
var_dump($b->title);
?>
--EXPECT--
plain                  head=Dom\HTMLElement/head body=Dom\HTMLElement/body title='t'
two heads              head=Dom\HTMLElement/head body=Dom\HTMLElement/body title=''
title in body          head=Dom\HTMLElement/head body=Dom\HTMLElement/body title='bodytitle'
title whitespace       head=Dom\HTMLElement/head body=Dom\HTMLElement/body title='a b'
xml, no html root      head=null             body=null             title=''
xml, xhtml root        head=Dom\HTMLElement/head body=Dom\HTMLElement/body title=''
xml, svg root          head=null             body=null             title='svgt'
NULL
NULL
string(0) ""
produced: Dom\HTMLElement Dom\HTMLElement 'Hi'
string(4) "oter"
