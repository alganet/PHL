--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An HTML document's body and title are written, and its factory mints HTML elements
--FILE--
<?php
// Two of the document's three own doors write. `head` states no writer and
// lands on the readonly Error; `body` takes a `body` or a `frameset` element
// and nothing else; `title` takes a string and puts it in the title element,
// making one inside `head` when the document has none.
//
// The element FACTORY is what makes the body write usable: on an HTML document
// `createElement()` mints in the HTML NAMESPACE, so a made element is the same
// class as a parsed one and passes the `?Dom\HTMLElement` the property
// declares. An XML document's factory mints no namespace, and the name is
// never split on a colon for this.
function htmlDocFixture($src = '<html><head></head><body id=old>a</body></html>') {
    return Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR);
}
function htmlDocCase(string $label, callable $fn) {
    echo $label, ': ';
    try { $fn(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "-- the factory\n";
$h = htmlDocFixture();
foreach (['div', 'x:y'] as $n) {
    $e = $h->createElement($n);
    printf("  html %-4s %-16s ns=%s local=%s\n", $n, get_class($e),
        var_export($e->namespaceURI, true), $e->localName);
}
$x = Dom\XMLDocument::createEmpty();
printf("  xml  div  %-16s ns=%s\n", get_class($x->createElement('div')),
    var_export($x->createElement('div')->namespaceURI, true));
$o = new DOMDocument();
printf("  2004 div  %-16s ns=%s\n", get_class($o->createElement('div')),
    var_export($o->createElement('div')->namespaceURI, true));

echo "-- body\n";
htmlDocCase('replace', function () { $d = htmlDocFixture(); $b = $d->createElement('body');
    $b->setAttribute('id', 'new'); $d->body = $b; echo $d->saveHtml(), "\n"; });
htmlDocCase('frameset', function () { $d = htmlDocFixture(); $d->body = $d->createElement('frameset');
    echo $d->saveHtml(), "\n"; });
htmlDocCase('itself', function () { $d = htmlDocFixture(); $d->body = $d->body; echo $d->saveHtml(), "\n"; });
htmlDocCase('append', function () { $d = htmlDocFixture('<html><head></head></html>');
    $b = $d->createElement('body'); $b->setAttribute('id', 'n'); $d->body = $b;
    echo $d->saveHtml(), "\n"; });
htmlDocCase('cross document', function () { $d = htmlDocFixture(); $s = htmlDocFixture();
    $b = $s->createElement('body'); $b->setAttribute('id', 'oth'); $d->body = $b;
    echo $d->saveHtml(), ' | source ', $s->saveHtml(), "\n"; });
htmlDocCase('no root', function () { $d = Dom\HTMLDocument::createEmpty();
    $d->body = $d->createElement('body'); echo $d->saveHtml(), "\n"; });
htmlDocCase('a div', function () { $d = htmlDocFixture(); $d->body = $d->createElement('div'); });
htmlDocCase('null', function () { $d = htmlDocFixture(); $d->body = null; });
htmlDocCase('a string', function () { $d = htmlDocFixture(); $d->body = 'x'; });
htmlDocCase('svg body', function () { $d = htmlDocFixture();
    $d->body = $d->createElementNS('http://www.w3.org/2000/svg', 'body'); });
htmlDocCase('head', function () { $d = htmlDocFixture(); $d->head = $d->createElement('head'); });

echo "-- title\n";
htmlDocCase('makes one', function () { $d = htmlDocFixture(); $d->title = 'Zed'; echo $d->saveHtml(), "\n"; });
htmlDocCase('replaces', function () { $d = htmlDocFixture('<html><head><title>a<b>c</b></title></head><body></body></html>');
    $d->title = 'Zed'; echo $d->saveHtml(), "\n"; });
htmlDocCase('raw on write', function () { $d = htmlDocFixture(); $d->title = "  a   b  ";
    echo var_export($d->title, true), ' ', $d->saveHtml(), "\n"; });
htmlDocCase('no root', function () { $d = Dom\HTMLDocument::createEmpty(); $d->title = 'Z';
    echo '[', $d->saveHtml(), "]\n"; });
htmlDocCase('not a string', function () { $d = htmlDocFixture(); $d->title = null; });
htmlDocCase('xml root', function () { $d = Dom\XMLDocument::createFromString('<r/>');
    $d->title = 'Z'; echo $d->saveXml(), "\n"; });
htmlDocCase('svg root', function () { $d = Dom\XMLDocument::createFromString('<svg xmlns="http://www.w3.org/2000/svg"><g/></svg>');
    $d->title = 'Z'; echo $d->saveXml(), "\n"; });
htmlDocCase('svg root again', function () { $d = Dom\XMLDocument::createFromString('<svg xmlns="http://www.w3.org/2000/svg"><title>o</title><g/></svg>');
    $d->title = 'Z'; echo $d->saveXml(), "\n"; });
?>
--EXPECT--
-- the factory
  html div  Dom\HTMLElement  ns='http://www.w3.org/1999/xhtml' local=div
  html x:y  Dom\HTMLElement  ns='http://www.w3.org/1999/xhtml' local=x:y
  xml  div  Dom\Element      ns=NULL
  2004 div  DOMElement       ns=NULL
-- body
replace: <html><head></head><body id="new"></body></html>
frameset: <html><head></head><frameset></frameset></html>
itself: <html><head></head><body id="old">a</body></html>
append: <html><head></head><body id="n"></body></html>
cross document: <html><head></head><body id="oth"></body></html> | source <html><head></head><body id="old">a</body></html>
no root: DOMException: A body can only be set if there is a document element
a div: DOMException: The new body must either be a body or a frameset tag
null: DOMException: The new body must either be a body or a frameset tag
a string: TypeError: Cannot assign string to property Dom\Document::$body of type ?Dom\HTMLElement
svg body: TypeError: Cannot assign Dom\Element to property Dom\Document::$body of type ?Dom\HTMLElement
head: Error: Cannot modify readonly property Dom\HTMLDocument::$head
-- title
makes one: <html><head><title>Zed</title></head><body id="old">a</body></html>
replaces: <html><head><title>Zed</title></head><body></body></html>
raw on write: 'a b' <html><head><title>  a   b  </title></head><body id="old">a</body></html>
no root: []
not a string: TypeError: Cannot assign null to property Dom\Document::$title of type string
xml root: <?xml version="1.0" encoding="UTF-8"?>
<r/>
svg root: <?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"><title>Z</title><g/></svg>
svg root again: <?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"><title>Z</title><g/></svg>
