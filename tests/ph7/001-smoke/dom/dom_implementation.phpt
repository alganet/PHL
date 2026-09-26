--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMImplementation makes a DOCTYPE and a document with a namespaced root
--FILE--
<?php
$i = new DOMImplementation;

// php's feature table is two rows wide and the version is compared as a
// STRING: only "1.0", "2.0" and "" are versions at all, and of those Core
// answers for "1.0" alone where XML answers for every one.
foreach ([['Core', '1.0'], ['core', '1.0'], ['XML', '1.0'], ['xml', '1.0'],
          ['Core', '2.0'], ['Core', ''], ['XML', ''], ['', '1.0'],
          ['HTML', '1.0'], ['Core', '1'], ['XML', '2.0']] as [$f, $v]) {
    printf("hasFeature(%-6s,%-5s) => %s\n", var_export($f, true),
        var_export($v, true), var_export($i->hasFeature($f, $v), true));
}

// The doctype name is not checked at ALL beyond being non-empty: nothing has
// parsed it, it is the bytes the <!DOCTYPE ...> line will carry.
foreach (['html', 'a:b', '1bad', 'a b', 'x:', '::', 'p:q:r', ''] as $name) {
    try {
        $dt = $i->createDocumentType($name);
        printf("createDocumentType(%-7s) => %s name=%s\n", var_export($name, true),
            get_class($dt), var_export($dt->name, true));
    } catch (Throwable $ex) {
        printf("createDocumentType(%-7s) => %s: %s\n", var_export($name, true),
            get_class($ex), $ex->getMessage());
    }
}
$dt = $i->createDocumentType('html', '-//P//EN', 's.dtd');
printf("dt: pub=%s sys=%s owner=%s parent=%s subset=%s connected=%s type=%d\n",
    var_export($dt->publicId, true), var_export($dt->systemId, true),
    var_export($dt->ownerDocument, true), var_export($dt->parentNode, true),
    var_export($dt->internalSubset, true), var_export($dt->isConnected, true),
    $dt->nodeType);

// An empty qualified name is a document with no root at all -- which is what
// makes the doctype-only call meaningful -- and the namespace decides what
// becomes of the name's PREFIX: declared under a URI, dropped without one.
foreach ([[null, null], [null, 'root'], ['urn:x', 'root'], ['urn:x', 'p:root'],
          ['', 'p:root'], ['urn:x', ''], [null, '1bad'], [null, 'a:b:c'],
          ['urn:x', ':a'], ['urn:x', 'a:']] as [$ns, $qn]) {
    try {
        $doc = $qn === null ? $i->createDocument() : $i->createDocument($ns, $qn);
        printf("createDocument(%-8s,%-8s) => %s\n", var_export($ns, true),
            var_export($qn, true), var_export(str_replace("\n", '|', $doc->saveXML()), true));
    } catch (Throwable $ex) {
        printf("createDocument(%-8s,%-8s) => %s(%d): %s\n", var_export($ns, true),
            var_export($qn, true), get_class($ex), $ex->getCode(), $ex->getMessage());
    }
}

// A doctype seeds ONE document: it is adopted, and the second call refuses.
$dt2 = $i->createDocumentType('root', 'p', 's');
$doc = $i->createDocument(null, 'root', $dt2);
printf("seeded: %s | doctype is the same object=%s | owner=%s | connected=%s\n",
    var_export(str_replace("\n", '|', $doc->saveXML()), true),
    var_export($doc->doctype === $dt2, true),
    var_export($dt2->ownerDocument === $doc, true),
    var_export($dt2->isConnected, true));
try {
    $i->createDocument(null, 'root', $dt2);
} catch (Throwable $ex) {
    printf("reused doctype: %s(%d): %s\n", get_class($ex), $ex->getCode(), $ex->getMessage());
}
printf("doctype only: %s\n", var_export(str_replace("\n", '|',
    $i->createDocument(null, '', $i->createDocumentType('z'))->saveXML()), true));

// A document answers a FRESH implementation on every read, and it is
// read-only.
$d = new DOMDocument;
var_dump(get_class($d->implementation), $d->implementation === $d->implementation,
    isset($d->implementation));
try {
    $d->implementation = 1;
} catch (Throwable $ex) {
    printf("%s: %s\n", get_class($ex), $ex->getMessage());
}

foreach (['createDocumentType', 'createDocument', 'hasFeature'] as $name) {
    $m = new ReflectionMethod('DOMImplementation', $name);
    $ps = [];
    foreach ($m->getParameters() as $p) {
        $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName()
            . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
    }
    printf("%s(%s): %s | static=%s\n", $name, implode(', ', $ps),
        (string)($m->getTentativeReturnType() ?? $m->getReturnType() ?? '-'),
        var_export($m->isStatic(), true));
}
?>
--EXPECT--
hasFeature('Core','1.0') => true
hasFeature('core','1.0') => true
hasFeature('XML' ,'1.0') => true
hasFeature('xml' ,'1.0') => true
hasFeature('Core','2.0') => false
hasFeature('Core',''   ) => false
hasFeature('XML' ,''   ) => true
hasFeature(''    ,'1.0') => false
hasFeature('HTML','1.0') => false
hasFeature('Core','1'  ) => false
hasFeature('XML' ,'2.0') => true
createDocumentType('html' ) => DOMDocumentType name='html'
createDocumentType('a:b'  ) => DOMDocumentType name='a:b'
createDocumentType('1bad' ) => DOMDocumentType name='1bad'
createDocumentType('a b'  ) => DOMDocumentType name='a b'
createDocumentType('x:'   ) => DOMDocumentType name='x:'
createDocumentType('::'   ) => DOMDocumentType name='::'
createDocumentType('p:q:r') => DOMDocumentType name='p:q:r'
createDocumentType(''     ) => ValueError: DOMImplementation::createDocumentType(): Argument #1 ($qualifiedName) must not be empty
dt: pub='-//P//EN' sys='s.dtd' owner=NULL parent=NULL subset=NULL connected=false type=10
createDocument(NULL    ,NULL    ) => '<?xml version="1.0"?>|'
createDocument(NULL    ,'root'  ) => '<?xml version="1.0"?>|<root/>|'
createDocument('urn:x' ,'root'  ) => '<?xml version="1.0"?>|<root xmlns="urn:x"/>|'
createDocument('urn:x' ,'p:root') => '<?xml version="1.0"?>|<p:root xmlns:p="urn:x"/>|'
createDocument(''      ,'p:root') => '<?xml version="1.0"?>|<root/>|'
createDocument('urn:x' ,''      ) => '<?xml version="1.0"?>|'
createDocument(NULL    ,'1bad'  ) => DOMException(14): Namespace Error
createDocument(NULL    ,'a:b:c' ) => DOMException(14): Namespace Error
createDocument('urn:x' ,':a'    ) => DOMException(14): Namespace Error
createDocument('urn:x' ,'a:'    ) => DOMException(14): Namespace Error
seeded: '<?xml version="1.0"?>|<!DOCTYPE root PUBLIC "p" "s">|<root/>|' | doctype is the same object=true | owner=true | connected=true
reused doctype: DOMException(4): Wrong Document Error
doctype only: '<?xml version="1.0"?>|<!DOCTYPE z>|'
string(17) "DOMImplementation"
bool(false)
bool(true)
Error: Cannot modify readonly property DOMDocument::$implementation
createDocumentType(string $qualifiedName, string $publicId = '', string $systemId = ''): - | static=false
createDocument(?string $namespace = NULL, string $qualifiedName = '', ?DOMDocumentType $doctype = NULL): DOMDocument | static=false
hasFeature(string $feature, string $version): bool | static=false
