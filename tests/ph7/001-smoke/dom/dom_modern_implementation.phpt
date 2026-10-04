--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Implementation makes a DOCTYPE and a namespaced document under its own rules
--FILE--
<?php
// php 8.4's factory is NOT the 2004 one under a new name: hasFeature() is gone,
// every argument that defaulted there is required here, and both doors that
// remain refuse different things.
$i = new Dom\Implementation();
foreach (['createDocumentType', 'createDocument', 'hasFeature'] as $n) {
    printf("%-18s %s\n", $n, var_export(method_exists($i, $n), true));
}

// A document hands out ONE implementation and keeps it, where the 2004 document
// mints a fresh one on every read. Two documents still answer two.
$d1 = Dom\XMLDocument::createFromString('<r/>');
$d2 = Dom\XMLDocument::createFromString('<r/>');
$o  = new DOMDocument();
var_dump($d1->implementation === $d1->implementation);
var_dump($d1->implementation === $d2->implementation);
var_dump($o->implementation === $o->implementation);
var_dump(get_class($d1->implementation));

// The doctype name is a QName and nothing else -- the 2004 door takes any
// non-empty byte string at all, so `p:q:r` and `1bad` are doctypes only there.
foreach (['html', 'a:b', '_', 'a-b', '1bad', 'a b', 'p:q:r', ':r', 'r:', '',
          'xmlns', 'xml:y'] as $n) {
    try {
        $t = $i->createDocumentType($n, '', '');
        printf("%-8s => %s name=%s\n", var_export($n, true), get_class($t), $t->name);
    } catch (Throwable $e) {
        printf("%-8s => %s(%d) %s\n", var_export($n, true), get_class($e),
            $e->getCode(), $e->getMessage());
    }
}

// A fresh doctype belongs to no document at all.
$dt = $i->createDocumentType('html', '-//W3C//DTD HTML 4.01//EN', 'sys.dtd');
var_dump($dt->name, $dt->publicId, $dt->systemId, $dt->ownerDocument, $dt->nodeType);

// The root name runs the namespaced ELEMENT grammar: a spelling failure is the
// Invalid Character Error and every rule ABOUT a namespace is the Namespace
// Error. The 2004 door answers 14 for both, and silently DROPS a prefix that
// names nothing rather than refusing it.
foreach ([[null, 'r'], ['', 'r'], ['http://u', 'r'], [null, 'p:r'],
          ['http://u', 'p:r'], [null, '1bad'], ['http://u', 'a:b:c'],
          [null, ':r'], [null, 'xmlns'], ['http://www.w3.org/2000/xmlns/', 'xmlns'],
          [null, 'xml:y'], ['http://www.w3.org/XML/1998/namespace', 'xml:y']] as [$u, $n]) {
    $lbl = sprintf('%-34s %-8s', var_export($u, true), var_export($n, true));
    try {
        $d = $i->createDocument($u, $n);
        echo $lbl, ' => ', str_replace("\n", ' ', $d->saveXml()), "\n";
    } catch (Throwable $e) {
        printf("%s => %s(%d) %s\n", $lbl, get_class($e), $e->getCode(), $e->getMessage());
    }
}

// An EMPTY name is a document with no root at all, which is what makes the
// three-argument call carrying only a doctype meaningful.
$e = $i->createDocument(null, '');
var_dump($e->documentElement, $e->childElementCount);
echo json_encode($e->saveXml()), "\n";

// Made from nowhere, so php's WHATWG spelling for exactly that.
$d = $i->createDocument('http://x', 'x:r');
var_dump(get_class($d), $d->URL, $d->documentURI, $d->xmlVersion, $d->characterSet);

// The doctype MOVES rather than being refused: the 2004 door raises the Wrong
// Document Error for a doctype that already seeded one, and this one takes it
// out of the first document and gives it to the second.
$a = $i->createDocument(null, 'r1', $dt);
$b = $i->createDocument(null, 'r2', $dt);
echo 'a: ', str_replace("\n", ' ', $a->saveXml()), "\n";
echo 'b: ', str_replace("\n", ' ', $b->saveXml()), "\n";
var_dump($dt->ownerDocument === $a, $dt->ownerDocument === $b);
var_dump($a->doctype, $b->doctype === $dt);

// The two trees do not mix: the modern door states ?Dom\DocumentType.
try {
    $i->createDocument(null, 'r', (new DOMImplementation)->createDocumentType('html'));
} catch (Throwable $t) {
    echo get_class($t), ': ', $t->getMessage(), "\n";
}
try {
    $i->createDocumentType('html');
} catch (Throwable $t) {
    echo get_class($t), ': ', $t->getMessage(), "\n";
}
?>
--EXPECT--
createDocumentType true
createDocument     true
hasFeature         false
bool(true)
bool(false)
bool(false)
string(18) "Dom\Implementation"
'html'   => Dom\DocumentType name=html
'a:b'    => Dom\DocumentType name=a:b
'_'      => Dom\DocumentType name=_
'a-b'    => Dom\DocumentType name=a-b
'1bad'   => DOMException(14) Namespace Error
'a b'    => DOMException(14) Namespace Error
'p:q:r'  => DOMException(14) Namespace Error
':r'     => DOMException(14) Namespace Error
'r:'     => DOMException(14) Namespace Error
''       => DOMException(14) Namespace Error
'xmlns'  => Dom\DocumentType name=xmlns
'xml:y'  => Dom\DocumentType name=xml:y
string(4) "html"
string(25) "-//W3C//DTD HTML 4.01//EN"
string(7) "sys.dtd"
NULL
int(10)
NULL                               'r'      => <?xml version="1.0" encoding="UTF-8"?> <r/>
''                                 'r'      => <?xml version="1.0" encoding="UTF-8"?> <r/>
'http://u'                         'r'      => <?xml version="1.0" encoding="UTF-8"?> <r xmlns="http://u"/>
NULL                               'p:r'    => DOMException(14) Namespace Error
'http://u'                         'p:r'    => <?xml version="1.0" encoding="UTF-8"?> <p:r xmlns:p="http://u"/>
NULL                               '1bad'   => DOMException(5) Invalid Character Error
'http://u'                         'a:b:c'  => DOMException(5) Invalid Character Error
NULL                               ':r'     => DOMException(5) Invalid Character Error
NULL                               'xmlns'  => DOMException(14) Namespace Error
'http://www.w3.org/2000/xmlns/'    'xmlns'  => <?xml version="1.0" encoding="UTF-8"?> <xmlns xmlns="http://www.w3.org/2000/xmlns/"/>
NULL                               'xml:y'  => DOMException(14) Namespace Error
'http://www.w3.org/XML/1998/namespace' 'xml:y'  => <?xml version="1.0" encoding="UTF-8"?> <xml:y/>
NULL
int(0)
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
string(15) "Dom\XMLDocument"
string(11) "about:blank"
string(11) "about:blank"
string(3) "1.0"
string(5) "UTF-8"
a: <?xml version="1.0" encoding="UTF-8"?> <r1/>
b: <?xml version="1.0" encoding="UTF-8"?> <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "sys.dtd"> <r2/>
bool(false)
bool(true)
NULL
bool(true)
TypeError: Dom\Implementation::createDocument(): Argument #3 ($doctype) must be of type ?Dom\DocumentType, DOMDocumentType given
ArgumentCountError: Dom\Implementation::createDocumentType() expects exactly 3 arguments, 1 given
