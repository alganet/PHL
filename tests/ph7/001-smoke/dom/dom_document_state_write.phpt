--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing DOMDocument declaration state: coercions, the read-only four, the encoding ValueError
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "diag $no: $str\n"; return true; });
$mk = function () { $d = new DOMDocument; $d->loadXML('<r/>'); return $d; };
$one = function ($name, $value) use ($mk) {
    $d = $mk();
    $label = $name . ' = ' . (is_object($value) ? get_class($value) : var_export($value, true));
    try {
        $d->$name = $value;
        printf("%-34s -> %-14s %s\n", $label, var_export($d->$name, true),
            str_replace("\n", '', $d->saveXML()));
    } catch (\Throwable $e) {
        printf("%-34s -> %s: %s\n", $label, get_class($e), $e->getMessage());
    }
};
// The three writable strings coerce as php's `?string` does: null is the EMPTY
// write, an array or an object is the TypeError.
foreach (['version', 'xmlVersion', 'documentURI'] as $n) {
    foreach (['1.1', '', null, 3, true, [1], new stdClass] as $v) {
        $one($n, $v);
    }
}
// `encoding` is the same write behind a name check: a name libxml has no
// converter for is refused BEFORE anything is stored.
foreach (['UTF-8', 'ISO-8859-1', 'US-ASCII', 'x', null, 3, [1]] as $v) {
    $one('encoding', $v);
}
// The EMPTY name is the library's call, not php's: php asks libxml the same
// question, and libxml 2.9 has no converter for '' while 2.15 hands one back.
// So the row asserts only that the answer is one of those two -- refused, or
// stored as ''.
$d = $mk();
try {
    $d->encoding = '';
    $ok = $d->encoding === '';
} catch (ValueError $e) {
    $ok = $e->getMessage() === 'Invalid document encoding';
}
echo "encoding = '' is libxml's answer: ", var_export($ok, true), "\n";
// The bool pair writes a DECLARED answer either way -- false is standalone="no",
// not the absent attribute -- and is not nullable.
foreach (['standalone', 'xmlStandalone'] as $n) {
    foreach ([true, false, 1, 0, 'x', '', null, [1]] as $v) {
        $one($n, $v);
    }
}
// The read-only four: php's Error, and no deprecation notice on the way (the
// refusal comes before the property is ever read).
foreach (['actualEncoding', 'xmlEncoding', 'config'] as $n) {
    $one($n, 'zz');
}
// A write is visible through the OTHER spelling of the same slot.
$d = $mk();
$d->version = '1.1';
$d->encoding = 'ISO-8859-1';
$d->standalone = true;
echo $d->xmlVersion, ' ', $d->xmlEncoding, ' ', var_export($d->xmlStandalone, true), "\n";
echo str_replace("\n", '', $d->saveXML()), "\n";
// documentURI is what baseURI reads.
$d->documentURI = 'http://e/f/g.xml';
echo $d->baseURI, ' ', $d->documentElement->baseURI, "\n";
--EXPECT--
version = '1.1'                    -> '1.1'          <?xml version="1.1"?><r/>
version = ''                       -> ''             <?xml version=""?><r/>
version = NULL                     -> ''             <?xml version=""?><r/>
version = 3                        -> '3'            <?xml version="3"?><r/>
version = true                     -> '1'            <?xml version="1"?><r/>
version = array (
  0 => 1,
)      -> TypeError: Cannot assign array to property DOMDocument::$version of type ?string
version = stdClass                 -> TypeError: Cannot assign stdClass to property DOMDocument::$version of type ?string
xmlVersion = '1.1'                 -> '1.1'          <?xml version="1.1"?><r/>
xmlVersion = ''                    -> ''             <?xml version=""?><r/>
xmlVersion = NULL                  -> ''             <?xml version=""?><r/>
xmlVersion = 3                     -> '3'            <?xml version="3"?><r/>
xmlVersion = true                  -> '1'            <?xml version="1"?><r/>
xmlVersion = array (
  0 => 1,
)   -> TypeError: Cannot assign array to property DOMDocument::$xmlVersion of type ?string
xmlVersion = stdClass              -> TypeError: Cannot assign stdClass to property DOMDocument::$xmlVersion of type ?string
documentURI = '1.1'                -> '1.1'          <?xml version="1.0"?><r/>
documentURI = ''                   -> ''             <?xml version="1.0"?><r/>
documentURI = NULL                 -> ''             <?xml version="1.0"?><r/>
documentURI = 3                    -> '3'            <?xml version="1.0"?><r/>
documentURI = true                 -> '1'            <?xml version="1.0"?><r/>
documentURI = array (
  0 => 1,
)  -> TypeError: Cannot assign array to property DOMDocument::$documentURI of type ?string
documentURI = stdClass             -> TypeError: Cannot assign stdClass to property DOMDocument::$documentURI of type ?string
encoding = 'UTF-8'                 -> 'UTF-8'        <?xml version="1.0" encoding="UTF-8"?><r/>
encoding = 'ISO-8859-1'            -> 'ISO-8859-1'   <?xml version="1.0" encoding="ISO-8859-1"?><r/>
encoding = 'US-ASCII'              -> 'US-ASCII'     <?xml version="1.0" encoding="US-ASCII"?><r/>
encoding = 'x'                     -> ValueError: Invalid document encoding
encoding = NULL                    -> ValueError: Invalid document encoding
encoding = 3                       -> ValueError: Invalid document encoding
encoding = array (
  0 => 1,
)     -> TypeError: Cannot assign array to property DOMDocument::$encoding of type ?string
encoding = '' is libxml's answer: true
standalone = true                  -> true           <?xml version="1.0" standalone="yes"?><r/>
standalone = false                 -> false          <?xml version="1.0" standalone="no"?><r/>
standalone = 1                     -> true           <?xml version="1.0" standalone="yes"?><r/>
standalone = 0                     -> false          <?xml version="1.0" standalone="no"?><r/>
standalone = 'x'                   -> true           <?xml version="1.0" standalone="yes"?><r/>
standalone = ''                    -> false          <?xml version="1.0" standalone="no"?><r/>
standalone = NULL                  -> TypeError: Cannot assign null to property DOMDocument::$standalone of type bool
standalone = array (
  0 => 1,
)   -> TypeError: Cannot assign array to property DOMDocument::$standalone of type bool
xmlStandalone = true               -> true           <?xml version="1.0" standalone="yes"?><r/>
xmlStandalone = false              -> false          <?xml version="1.0" standalone="no"?><r/>
xmlStandalone = 1                  -> true           <?xml version="1.0" standalone="yes"?><r/>
xmlStandalone = 0                  -> false          <?xml version="1.0" standalone="no"?><r/>
xmlStandalone = 'x'                -> true           <?xml version="1.0" standalone="yes"?><r/>
xmlStandalone = ''                 -> false          <?xml version="1.0" standalone="no"?><r/>
xmlStandalone = NULL               -> TypeError: Cannot assign null to property DOMDocument::$xmlStandalone of type bool
xmlStandalone = array (
  0 => 1,
) -> TypeError: Cannot assign array to property DOMDocument::$xmlStandalone of type bool
actualEncoding = 'zz'              -> Error: Cannot modify readonly property DOMDocument::$actualEncoding
xmlEncoding = 'zz'                 -> Error: Cannot modify readonly property DOMDocument::$xmlEncoding
config = 'zz'                      -> Error: Cannot modify readonly property DOMDocument::$config
1.1 ISO-8859-1 true
<?xml version="1.1" encoding="ISO-8859-1" standalone="yes"?><r/>
http://e/f/g.xml http://e/f/g.xml
