--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A document validates against a schema, a RelaxNG grammar or its own DTD
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
$dom_val_dir = sys_get_temp_dir() . '/phl_dom_val_' . getmypid();
@mkdir($dom_val_dir);
$dom_val_xsd = '<?xml version="1.0"?><xs:schema '
    . 'xmlns:xs="http://www.w3.org/2001/XMLSchema">'
    . '<xs:element name="r" type="xs:string"/></xs:schema>';
$dom_val_rng = '<?xml version="1.0"?><element name="r" '
    . 'xmlns="http://relaxng.org/ns/structure/1.0"><text/></element>';
file_put_contents("$dom_val_dir/s.xsd", $dom_val_xsd);
file_put_contents("$dom_val_dir/r.rng", $dom_val_rng);
file_put_contents("$dom_val_dir/bad.xsd", 'not a schema');
file_put_contents("$dom_val_dir/d.dtd", '<!ELEMENT r (#PCDATA)>');

$dom_val_doc = static function (string $xml): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};
// Two channels are read here and the directory is normalised out of both: the
// schema's own diagnostics through libxml's error queue (which is where php
// puts them once internal errors are on, and the only place their wording is
// the same on both engines), and the extension's own last word through the
// error handler.
$dom_val_said = '';
set_error_handler(static function (int $n, string $s) use (&$dom_val_said): bool {
    $dom_val_said = $s;
    return true;
});
$dom_val_run = static function (callable $fn) use (&$dom_val_said, $dom_val_dir): string {
    $prior = libxml_use_internal_errors(true);
    libxml_clear_errors();
    $dom_val_said = '';
    try {
        $answer = var_export($fn(), true);
    } catch (Throwable $ex) {
        $answer = get_class($ex) . ': ' . $ex->getMessage();
    }
    $first = trim(libxml_get_errors()[0]->message ?? '');
    libxml_clear_errors();
    libxml_use_internal_errors($prior);
    foreach ([$first, $dom_val_said] as $extra) {
        if ($extra === '') {
            continue;
        }
        // A message naming the FILE is libxml's own I/O wording and differs
        // between its versions ("failed to load external entity" against
        // "failed to load ...: No such file or directory"), so only the fact
        // that it arrived is pinned.
        $answer .= ' | ' . (str_contains($extra, $dom_val_dir)
            ? '(path diagnostic)' : $extra);
    }
    return $answer;
};

$ok = $dom_val_doc('<r>ok</r>');
$bad = $dom_val_doc('<q>no</q>');
foreach ([
    'schema file ok'       => fn() => $ok->schemaValidate("$dom_val_dir/s.xsd"),
    'schema file flags'    => fn() => $ok->schemaValidate("$dom_val_dir/s.xsd", LIBXML_SCHEMA_CREATE),
    'schema file bad doc'  => fn() => $bad->schemaValidate("$dom_val_dir/s.xsd"),
    'schema file missing'  => fn() => $ok->schemaValidate("$dom_val_dir/nope.xsd"),
    'schema file junk'     => fn() => $ok->schemaValidate("$dom_val_dir/bad.xsd"),
    'schema file empty'    => fn() => $ok->schemaValidate(''),
    'schema file nul'      => fn() => $ok->schemaValidate("a\0b"),
    'schema source ok'     => fn() => $ok->schemaValidateSource($GLOBALS['dom_val_xsd']),
    'schema source empty'  => fn() => $ok->schemaValidateSource(''),
    'relaxng file ok'      => fn() => $ok->relaxNGValidate("$dom_val_dir/r.rng"),
    'relaxng file bad'     => fn() => $bad->relaxNGValidate("$dom_val_dir/r.rng"),
    'relaxng file missing' => fn() => $ok->relaxNGValidate("$dom_val_dir/nope.rng"),
    'relaxng file empty'   => fn() => $ok->relaxNGValidate(''),
    'relaxng source ok'    => fn() => $ok->relaxNGValidateSource($GLOBALS['dom_val_rng']),
    'relaxng source junk'  => fn() => $ok->relaxNGValidateSource('nope'),
    'relaxng source empty' => fn() => $ok->relaxNGValidateSource(''),
] as $label => $case) {
    printf("%-21s %s\n", $label, $dom_val_run($case));
}

// validate() asks the document's OWN DTD, and is the one of the five that
// takes no argument at all.
printf("%-21s %s\n", 'dtd ok', $dom_val_run(fn() =>
    $dom_val_doc('<!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><r>x</r>')->validate()));
printf("%-21s %s\n", 'dtd bad content', $dom_val_run(fn() =>
    $dom_val_doc('<!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><r><k/></r>')->validate()));
printf("%-21s %s\n", 'dtd wrong root', $dom_val_run(fn() =>
    $dom_val_doc('<!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><q>x</q>')->validate()));
printf("%-21s %s\n", 'dtd absent', $dom_val_run(fn() =>
    $dom_val_doc('<r>x</r>')->validate()));

// xinclude answers the COUNT of substitutions, -1 when one failed, and FALSE
// when there was nothing to do -- which a caller has to screen for separately.
$dom_val_inc = 'phl_dom_val_inc_' . getmypid() . '.xml';
file_put_contents($dom_val_inc, '<inc>included</inc>');
$xi = $dom_val_doc('<r xmlns:xi="http://www.w3.org/2001/XInclude">'
    . '<xi:include href="' . $dom_val_inc . '"/></r>');
printf("%-21s %s\n", 'xinclude', $dom_val_run(fn() => $xi->xinclude()));
printf("%-21s %s\n", 'xinclude result', $xi->documentElement->firstChild->textContent);
$xibad = $dom_val_doc('<r xmlns:xi="http://www.w3.org/2001/XInclude">'
    . '<xi:include href="phl_dom_val_nothing_here.xml"/></r>');
printf("%-21s %s\n", 'xinclude failed', var_export($xibad->xinclude(), true));
printf("%-21s %s\n", 'xinclude nothing', var_export($ok->xinclude(), true));
restore_error_handler();

// php's own signatures, which is what an option word being declared on one
// pair and not the other is.
foreach (['schemaValidate', 'schemaValidateSource', 'relaxNGValidate',
          'relaxNGValidateSource', 'validate', 'xinclude'] as $name) {
    $m = new ReflectionMethod('DOMDocument', $name);
    $ps = [];
    foreach ($m->getParameters() as $p) {
        $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName()
            . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
    }
    printf("%s(%s): %s\n", $name, implode(', ', $ps),
        (string)($m->getTentativeReturnType() ?? $m->getReturnType() ?? '-'));
}

@unlink($dom_val_inc);
foreach (['s.xsd', 'r.rng', 'bad.xsd', 'd.dtd'] as $f) {
    @unlink("$dom_val_dir/$f");
}
@rmdir($dom_val_dir);
?>
--EXPECT--
schema file ok        true
schema file flags     true
schema file bad doc   false | Element 'q': No matching global declaration available for the validation root.
schema file missing   false | (path diagnostic) | DOMDocument::schemaValidate(): Invalid Schema
schema file junk      false | Start tag expected, '<' not found | DOMDocument::schemaValidate(): Invalid Schema
schema file empty     ValueError: DOMDocument::schemaValidate(): Argument #1 ($filename) must not be empty
schema file nul       ValueError: DOMDocument::schemaValidate(): Argument #1 ($filename) must not contain any null bytes
schema source ok      true
schema source empty   ValueError: DOMDocument::schemaValidateSource(): Argument #1 ($source) must not be empty
relaxng file ok       true
relaxng file bad      false | Expecting element r, got q
relaxng file missing  false | (path diagnostic) | DOMDocument::relaxNGValidate(): Invalid RelaxNG
relaxng file empty    ValueError: DOMDocument::relaxNGValidate(): Argument #1 ($filename) must not be empty
relaxng source ok     true
relaxng source junk   false | Start tag expected, '<' not found | DOMDocument::relaxNGValidateSource(): Invalid RelaxNG
relaxng source empty  ValueError: DOMDocument::relaxNGValidateSource(): Argument #1 ($source) must not be empty
dtd ok                true
dtd bad content       false | Element r was declared #PCDATA but contains non text nodes
dtd wrong root        false | root and DTD name do not match 'q' and 'r'
dtd absent            false | no DTD found!
xinclude              1
xinclude result       included
xinclude failed       -1
xinclude nothing      false
schemaValidate(string $filename, int $flags = 0): bool
schemaValidateSource(string $source, int $flags = 0): bool
relaxNGValidate(string $filename): bool
relaxNGValidateSource(string $source): bool
validate(): bool
xinclude(int $options = 0): int|false
