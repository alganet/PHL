--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMDocument parser directives: validateOnParse, resolveExternals, substituteEntities, recover
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
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });
$dtd = sys_get_temp_dir() . '/phl_dom_flags.dtd';
file_put_contents($dtd, "<!ELEMENT r (a)>\n<!ELEMENT a EMPTY>\n<!ATTLIST r ex CDATA \"fromdtd\">\n<!ENTITY ee \"EXPANDED\">\n");
$ext = '<!DOCTYPE r SYSTEM "' . $dtd . '">';
// (A document REFERENCING the DTD's entity is deliberately not here: what an
// undefined entity reference is -- a libxml warning or an error, dropped from
// the tree or kept -- is the libxml VERSION's answer, not php's. PLAN 7.4.)
$docs = [
    'external ok'    => $ext . '<r><a/></r>',
    'external bad'   => $ext . '<r><b/></r>',
    'internal'       => '<!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>',
    'internal bad'   => '<!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>',
];
$flags = ['validateOnParse', 'resolveExternals', 'substituteEntities', 'recover'];
// Every directive is false by default and every one of them ADDS to the parse.
foreach ($docs as $label => $src) {
    foreach ($flags as $f) {
        foreach ([false, true] as $v) {
            $d = new DOMDocument;
            $d->$f = $v;
            $ok = $d->loadXML($src);
            printf("%-13s %-19s %d -> %s %s\n", $label, $f, (int)$v, var_export($ok, true),
                str_replace([$dtd, "\n"], ['DTD', ''], (string)@$d->saveXML()));
        }
    }
}
// A malformed document is kept only when it is being recovered from -- through
// the property or through the option, which are the same switch. (How MANY
// diagnostics one broken document produces is libxml's version's answer, so the
// queue takes them here and only the result is pinned.)
$broken = '<r><a></r>';
libxml_use_internal_errors(true);
foreach ([['prop', true, 0], ['option', false, LIBXML_RECOVER], ['neither', false, 0]] as [$how, $prop, $opt]) {
    $d = new DOMDocument;
    $d->recover = $prop;
    printf("%-8s -> %s %s\n", $how, var_export($d->loadXML($broken, $opt), true),
        str_replace("\n", '', (string)@$d->saveXML()));
}
libxml_clear_errors();
libxml_use_internal_errors(false);
// ...and a recovering parse reports at E_WARNING however error_reporting is set:
// a damaged document does not come back in silence. The mask a handler sees
// DURING the parse is the evidence -- E_WARNING is forced in and nothing else is,
// so a libxml warning (an E_NOTICE) stays suppressed.
restore_error_handler();
$masks = [];
set_error_handler(function ($no, $str) use (&$masks) { $masks[] = error_reporting(); return true; });
foreach ([true, false] as $rec) {
    $masks = [];
    $old = error_reporting(0);
    $d = new DOMDocument;
    $d->recover = $rec;
    $d->loadXML($broken);
    error_reporting($old);
    echo 'recover=', (int)$rec, ' reporting during parse=', implode(',', array_unique($masks)), "\n";
}
restore_error_handler();
set_error_handler(function ($no, $str) { echo "diag $no: ", str_replace("\n", '', $str), "\n"; return true; });

// The directives ride a clone.
$d = new DOMDocument;
$d->loadXML('<r/>');
foreach (['validateOnParse', 'resolveExternals', 'substituteEntities', 'recover', 'formatOutput'] as $f) {
    $d->$f = true;
}
$d->preserveWhiteSpace = false;
$c = $d->cloneNode(true);
$row = [];
foreach (['validateOnParse', 'resolveExternals', 'substituteEntities', 'recover', 'formatOutput', 'preserveWhiteSpace'] as $f) {
    $row[] = $f . '=' . (int)$c->$f;
}
echo implode(' ', $row), "\n";
var_dump(LIBXML_RECOVER);
@unlink($dtd);
--EXPECT--
external ok   validateOnParse     0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   validateOnParse     1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   resolveExternals    0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   resolveExternals    1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r ex="fromdtd"><a/></r>
external ok   substituteEntities  0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   substituteEntities  1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   recover             0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external ok   recover             1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><a/></r>
external bad  validateOnParse     0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
diag 2: DOMDocument::loadXML(): No declaration for element b in Entity, line: 1
diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
external bad  validateOnParse     1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
external bad  resolveExternals    0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
external bad  resolveExternals    1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r ex="fromdtd"><b/></r>
external bad  substituteEntities  0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
external bad  substituteEntities  1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
external bad  recover             0 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
external bad  recover             1 -> true <?xml version="1.0"?><!DOCTYPE r SYSTEM "DTD"><r><b/></r>
internal      validateOnParse     0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
diag 2: DOMDocument::loadXML(): No declaration for element a in Entity, line: 1
diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (a CDATA) in Entity, line: 1
internal      validateOnParse     1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
internal      resolveExternals    0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
internal      resolveExternals    1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r ix="iv"><a/>&ie;</r>
internal      substituteEntities  0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
internal      substituteEntities  1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>IV</r>
internal      recover             0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
internal      recover             1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)><!ATTLIST r ix CDATA "iv"><!ENTITY ie "IV">]><r><a/>&ie;</r>
internal bad  validateOnParse     0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
diag 2: DOMDocument::loadXML(): No declaration for element b in Entity, line: 1
diag 2: DOMDocument::loadXML(): Element r content does not follow the DTD, expecting (a), got (b) in Entity, line: 1
internal bad  validateOnParse     1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  resolveExternals    0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  resolveExternals    1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  substituteEntities  0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  substituteEntities  1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  recover             0 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
internal bad  recover             1 -> true <?xml version="1.0"?><!DOCTYPE r [<!ELEMENT r (a)>]><r><b/></r>
prop     -> true <?xml version="1.0"?><r><a/></r>
option   -> true <?xml version="1.0"?><r><a/></r>
neither  -> false <?xml version="1.0"?>
recover=1 reporting during parse=2
recover=0 reporting during parse=0
validateOnParse=1 resolveExternals=1 substituteEntities=1 recover=1 formatOutput=1 preserveWhiteSpace=0
int(1)
