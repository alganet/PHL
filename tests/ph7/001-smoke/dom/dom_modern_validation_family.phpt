--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced document validates against a schema, a grammar or its own DTD
--FILE--
<?php
$dom_mval_dir = sys_get_temp_dir() . '/phl_dom_mval_' . getmypid();
@mkdir($dom_mval_dir);
$dom_mval_xsd = '<?xml version="1.0"?><xs:schema '
    . 'xmlns:xs="http://www.w3.org/2001/XMLSchema">'
    . '<xs:element name="r" type="xs:string"/></xs:schema>';
$dom_mval_rng = '<?xml version="1.0"?><element name="r" '
    . 'xmlns="http://relaxng.org/ns/structure/1.0"><text/></element>';
file_put_contents("$dom_mval_dir/s.xsd", $dom_mval_xsd);
file_put_contents("$dom_mval_dir/r.rng", $dom_mval_rng);

$dom_mval_doc = static fn(string $xml): Dom\XMLDocument
    => Dom\XMLDocument::createFromString($xml, LIBXML_NOERROR | LIBXML_NOWARNING);

// The same two channels the 2004 family reads, and for the same reason: the
// schema's own diagnostics arrive through libxml's error queue, and the
// extension's last word through the error handler. The wording of a diagnostic
// naming a FILE is libxml's and differs between its versions, so only the fact
// that one arrived is pinned.
$dom_mval_said = '';
set_error_handler(static function (int $n, string $s) use (&$dom_mval_said): bool {
    $dom_mval_said = $s;
    return true;
});
$dom_mval_run = static function (callable $fn) use (&$dom_mval_said, $dom_mval_dir): string {
    $prior = libxml_use_internal_errors(true);
    libxml_clear_errors();
    $dom_mval_said = '';
    try {
        $answer = var_export($fn(), true);
    } catch (Throwable $ex) {
        $answer = get_class($ex) . '(' . $ex->getCode() . '): ' . $ex->getMessage();
    }
    $first = trim(libxml_get_errors()[0]->message ?? '');
    libxml_clear_errors();
    libxml_use_internal_errors($prior);
    foreach ([$first, $dom_mval_said] as $extra) {
        if ($extra === '') {
            continue;
        }
        $folded = str_contains($extra, $dom_mval_dir)
            || str_contains($extra, str_replace('\\', '/', $dom_mval_dir));
        $answer .= ' | ' . ($folded ? '(path diagnostic)' : $extra);
    }
    return $answer;
};

$ok = $dom_mval_doc('<r>ok</r>');
$bad = $dom_mval_doc('<q>no</q>');
foreach ([
    'schema file ok'       => fn() => $ok->schemaValidate("$dom_mval_dir/s.xsd"),
    'schema file flags'    => fn() => $ok->schemaValidate("$dom_mval_dir/s.xsd", LIBXML_SCHEMA_CREATE),
    'schema file bad doc'  => fn() => $bad->schemaValidate("$dom_mval_dir/s.xsd"),
    'schema file empty'    => fn() => $ok->schemaValidate(''),
    'schema file nul'      => fn() => $ok->schemaValidate("a\0b"),
    'schema source ok'     => fn() => $ok->schemaValidateSource($GLOBALS['dom_mval_xsd']),
    'schema source empty'  => fn() => $ok->schemaValidateSource(''),
    'relaxng file ok'      => fn() => $ok->relaxNgValidate("$dom_mval_dir/r.rng"),
    'relaxng file bad'     => fn() => $bad->relaxNgValidate("$dom_mval_dir/r.rng"),
    'relaxng file empty'   => fn() => $ok->relaxNgValidate(''),
    'relaxng source ok'    => fn() => $ok->relaxNgValidateSource($GLOBALS['dom_mval_rng']),
    'relaxng source empty' => fn() => $ok->relaxNgValidateSource(''),
] as $label => $case) {
    printf("%-21s %s\n", $label, $dom_mval_run($case));
}

// The DTD pass, which php declares on the FINAL document where it declares the
// schema four on the abstract one above it -- so these two diagnostics name a
// different class from the twelve above.
printf("%-21s %s\n", 'dtd ok', $dom_mval_run(fn() =>
    $dom_mval_doc('<!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><r>x</r>')->validate()));
printf("%-21s %s\n", 'dtd bad content', $dom_mval_run(fn() =>
    $dom_mval_doc('<!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><r><k/></r>')->validate()));
printf("%-21s %s\n", 'dtd absent', $dom_mval_run(fn() => $ok->validate()));

// XInclude, whose namespaced door declares a plain int and so has nowhere to
// put either of the 2004 answers: a failed substitution is a DOMException, and
// nothing to do is the count 0 rather than false.
// Both hrefs are absolute and live in the scratch directory, so nothing is
// written beside the corpus and the miss names a path the normaliser folds.
$dom_mval_href = str_replace('\\', '/', $dom_mval_dir);
file_put_contents("$dom_mval_dir/inc.xml", '<inc>included</inc>');
$xi = $dom_mval_doc('<r xmlns:xi="http://www.w3.org/2001/XInclude">'
    . '<xi:include href="' . $dom_mval_href . '/inc.xml"/></r>');
printf("%-21s %s\n", 'xinclude', $dom_mval_run(fn() => $xi->xinclude()));
printf("%-21s %s\n", 'xinclude result', $xi->documentElement->firstChild->textContent);
printf("%-21s %s\n", 'xinclude marks', (string)$xi->documentElement->childNodes->length);
$xibad = $dom_mval_doc('<r xmlns:xi="http://www.w3.org/2001/XInclude">'
    . '<xi:include href="' . $dom_mval_href . '/nothing_here.xml"/></r>');
printf("%-21s %s\n", 'xinclude failed', $dom_mval_run(fn() => $xibad->xinclude()));
printf("%-21s %s\n", 'xinclude nothing', $dom_mval_run(fn() => $ok->xinclude()));
restore_error_handler();

// Which class DECLARES each of the six, and under which signature: the name a
// native method answers under is the declaring class's and not the receiver's,
// and the schema pair states an option word the grammar pair does not.
foreach (['schemaValidate', 'schemaValidateSource', 'relaxNgValidate',
          'relaxNgValidateSource', 'validate', 'xinclude'] as $name) {
    $m = new ReflectionMethod('Dom\XMLDocument', $name);
    $ps = [];
    foreach ($m->getParameters() as $p) {
        $ps[] = ($p->getType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName()
            . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
    }
    printf("%s::%s(%s): %s\n", $m->getDeclaringClass()->getName(), $name, implode(', ', $ps),
        (string)($m->getTentativeReturnType() ?? $m->getReturnType() ?? '-'));
}

foreach (['s.xsd', 'r.rng', 'inc.xml'] as $f) {
    @unlink("$dom_mval_dir/$f");
}
@rmdir($dom_mval_dir);
?>
--EXPECT--
schema file ok        true
schema file flags     true
schema file bad doc   false | Element 'q': No matching global declaration available for the validation root.
schema file empty     ValueError(0): Dom\Document::schemaValidate(): Argument #1 ($filename) must not be empty
schema file nul       ValueError(0): Dom\Document::schemaValidate(): Argument #1 ($filename) must not contain any null bytes
schema source ok      true
schema source empty   ValueError(0): Dom\Document::schemaValidateSource(): Argument #1 ($source) must not be empty
relaxng file ok       true
relaxng file bad      false | Expecting element r, got q
relaxng file empty    ValueError(0): Dom\Document::relaxNgValidate(): Argument #1 ($filename) must not be empty
relaxng source ok     true
relaxng source empty  ValueError(0): Dom\Document::relaxNgValidateSource(): Argument #1 ($source) must not be empty
dtd ok                true
dtd bad content       false | Element r was declared #PCDATA but contains non text nodes
dtd absent            false | no DTD found!
xinclude              1
xinclude result       included
xinclude marks        1
xinclude failed       DOMException(13): Invalid Modification Error | (path diagnostic)
xinclude nothing      0
Dom\Document::schemaValidate(string $filename, int $flags = 0): bool
Dom\Document::schemaValidateSource(string $source, int $flags = 0): bool
Dom\Document::relaxNgValidate(string $filename): bool
Dom\Document::relaxNgValidateSource(string $source): bool
Dom\XMLDocument::validate(): bool
Dom\XMLDocument::xinclude(int $options = 0): int
