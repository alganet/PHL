--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument declaration state: version/encoding/standalone pairs, documentURI, the deprecated two
--SKIPIF--
<?php
// php's Windows build ships libxml 2.11, which spells a local path as a file:/
// URI (documentURI and every file diagnostic) and cannot load a backslashed
// absolute DTD path; PHL answers as php does on Linux and macOS. Only the
// oracle is skipped there -- PHL runs this test on every platform.
if (function_exists('zend_version') && PHP_OS_FAMILY === 'Windows') {
    echo 'skip the Windows oracle libxml spells local paths as file: URIs';
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "diag $no: $str\n"; return true; });
$names = [
    'version', 'xmlVersion', 'encoding', 'xmlEncoding', 'actualEncoding',
    'standalone', 'xmlStandalone', 'config',
];
$show = function ($label, $d) use ($names) {
    $out = [];
    foreach ($names as $n) {
        $out[] = $n . '=' . var_export($d->$n, true);
    }
    echo $label, "\n  ", implode(' ', $out), "\n";
};
// A fresh document carries the constructor's two arguments and nothing else.
$show('new DOMDocument', new DOMDocument);
$show("new DOMDocument('1.1','ISO-8859-1')", new DOMDocument('1.1', 'ISO-8859-1'));
// An EMPTY version argument is written as given: only an omitted one defaults.
$show("new DOMDocument('')", new DOMDocument(''));

// A parsed declaration is read back through both spellings of each name.
foreach ([
    '<r/>',
    '<?xml version="1.0"?><r/>',
    '<?xml version="1.0" encoding="US-ASCII" standalone="yes"?><r/>',
    '<?xml version="1.0" standalone="no"?><r/>',
] as $src) {
    $d = new DOMDocument;
    $d->loadXML($src);
    $show($src, $d);
}

// The URI of a document parsed from MEMORY is the working directory.
$d = new DOMDocument;
$d->loadXML('<r><a/></r>');
$cwd = getcwd();
$want = $cwd . (substr($cwd, -1) === '/' || substr($cwd, -1) === '\\' ? '' : '/');
var_dump($d->documentURI === $want, $d->baseURI === $want);
// ...and every node under it answers that URI as its base.
var_dump($d->documentElement->baseURI === $want,
    $d->documentElement->firstChild->baseURI === $want,
    $d->createElement('never-appended')->baseURI === $want);
// A document that was never parsed has no URI at all.
$fresh = new DOMDocument;
var_dump($fresh->documentURI, $fresh->baseURI);

// xml:base wins over the document's URI, nearest declaration first.
$b = new DOMDocument;
$b->loadXML('<r xml:base="http://e/x/"><a xml:base="deep/"><t>q</t></a><p/></r>');
foreach (['r' => $b->documentElement,
          'a' => $b->documentElement->firstChild,
          't' => $b->documentElement->firstChild->firstChild,
          'p' => $b->documentElement->lastChild] as $k => $n) {
    echo $k, ' base=', var_export($n->baseURI, true), "\n";
}

// isset() is php's own: a name that exists AND reads back non-null.
$e = new DOMDocument;
$e->loadXML('<?xml version="1.0" encoding="UTF-8"?><r/>');
foreach (['encoding', 'version', 'standalone', 'config'] as $n) {
    echo $n, ' isset=', var_export(isset($e->$n), true), "\n";
}
$plain = new DOMDocument;
$plain->loadXML('<r/>');
echo 'unset encoding isset=', var_export(isset($plain->encoding), true), "\n";
--EXPECT--
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
new DOMDocument
  version='1.0' xmlVersion='1.0' encoding=NULL xmlEncoding=NULL actualEncoding=NULL standalone=false xmlStandalone=false config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
new DOMDocument('1.1','ISO-8859-1')
  version='1.1' xmlVersion='1.1' encoding='ISO-8859-1' xmlEncoding='ISO-8859-1' actualEncoding='ISO-8859-1' standalone=false xmlStandalone=false config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
new DOMDocument('')
  version='' xmlVersion='' encoding=NULL xmlEncoding=NULL actualEncoding=NULL standalone=false xmlStandalone=false config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
<r/>
  version='1.0' xmlVersion='1.0' encoding=NULL xmlEncoding=NULL actualEncoding=NULL standalone=false xmlStandalone=false config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
<?xml version="1.0"?><r/>
  version='1.0' xmlVersion='1.0' encoding=NULL xmlEncoding=NULL actualEncoding=NULL standalone=false xmlStandalone=false config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
<?xml version="1.0" encoding="US-ASCII" standalone="yes"?><r/>
  version='1.0' xmlVersion='1.0' encoding='US-ASCII' xmlEncoding='US-ASCII' actualEncoding='US-ASCII' standalone=true xmlStandalone=true config=NULL
diag 8192: Property DOMDocument::$actualEncoding is deprecated
diag 8192: Property DOMDocument::$config is deprecated
<?xml version="1.0" standalone="no"?><r/>
  version='1.0' xmlVersion='1.0' encoding=NULL xmlEncoding=NULL actualEncoding=NULL standalone=false xmlStandalone=false config=NULL
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
NULL
NULL
r base='http://e/x/'
a base='http://e/x/deep/'
t base='http://e/x/deep/'
p base='http://e/x/'
encoding isset=true
version isset=true
standalone isset=true
config isset=diag 8192: Property DOMDocument::$config is deprecated
false
unset encoding isset=false
