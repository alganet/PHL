--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/simplexml: writes, xpath, asXML and the ext/dom doors
--FILE--
<?php
/* The write half, the two doors into ext/dom, and what each refusal says. */
/* A write is not a call, so php attributes its warnings to the CALLER's scope --
 * here a closure, whose php name carries this file's PATH. The handler keeps the
 * message and drops the position, which is what makes the expectation portable. */
$sxSaid = [];
set_error_handler(function ($no, $msg) use (&$sxSaid) {
    $sxSaid[] = $no . ':' . preg_replace('/\{closure:.*?\}/', '{closure}', $msg);
    return true;
});
function sxRun(string $label, callable $op): void {
    global $sxSaid;
    $sxSaid = [];
    $x = simplexml_load_string('<r a="1"><c>one</c><c>two</c><d>dee</d></r>');
    $out = 'ok';
    ob_start();
    try { $op($x); } catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    $said = trim(ob_get_clean()) . implode(' ', $sxSaid);
    printf("%-22s %-24s %s%s\n", $label, $out, str_replace("\n", '', $x->asXML()),
        $said === '' ? '' : '  ## ' . $said);
}

echo "-- property writes\n";
sxRun('new name',      function ($x) { $x->fresh = 'v'; });
sxRun('existing',      function ($x) { $x->d = 'changed'; });
sxRun('a set of two',  function ($x) { $x->c = 'changed'; });
sxRun('nested create', function ($x) { $x->a->b = 'v'; });
sxRun('int',           function ($x) { $x->d = 42; });
sxRun('float',         function ($x) { $x->d = 1.5; });
sxRun('true',          function ($x) { $x->d = true; });
sxRun('false',         function ($x) { $x->d = false; });
sxRun('null',          function ($x) { $x->d = null; });
sxRun('another node',  function ($x) { $x->d = $x->c; });
sxRun('array',         function ($x) { $x->d = [1, 2]; });
sxRun('plain object',  function ($x) { $x->d = new stdClass(); });
sxRun('escaped',       function ($x) { $x->d = 'a&b<c>'; });
sxRun('through children', function ($x) { $x->children()->d = 'v'; });
sxRun('on an attr list',  function ($x) { $x->attributes()->a = 'v'; });

echo "-- dimension writes\n";
sxRun('attr set',      function ($x) { $x['a'] = '9'; });
sxRun('attr create',   function ($x) { $x['z'] = '9'; });
sxRun('attr escaped',  function ($x) { $x['z'] = 'a&b<c>'; });
sxRun('nth element',   function ($x) { $x->c[1] = 'changed'; });
sxRun('past the end',  function ($x) { $x->c[5] = 'changed'; });
sxRun('negative',      function ($x) { $x->c[-1] = 'changed'; });
sxRun('append',        function ($x) { $x->c[] = 'nine'; });
sxRun('append to node',function ($x) { $x[] = 'nine'; });
sxRun('own text',      function ($x) { $x[0] = 'Z'; });
sxRun('past own text', function ($x) { $x[1] = 'Z'; });
sxRun('attr of a set', function ($x) { $x->c['q'] = 'Z'; });
sxRun('attr of a miss',function ($x) { $x->nope['q'] = 'Z'; });
sxRun('attr list set', function ($x) { $x->attributes()['a'] = 'Z'; });
sxRun('compound',      function ($x) { $x['a'] .= '!'; });

echo "-- unset\n";
sxRun('every match',   function ($x) { unset($x->c); });
sxRun('one of a set',  function ($x) { unset($x->c[0]); });
sxRun('an attribute',  function ($x) { unset($x['a']); });
sxRun('a miss',        function ($x) { unset($x->nope); unset($x['nope']); });
sxRun('through children', function ($x) { unset($x->children()->d); });

echo "-- addChild / addAttribute\n";
sxRun('addChild',      function ($x) { var_dump($x->addChild('n', 'v')->getName()); });
sxRun('addChild empty',function ($x) { $x->addChild(''); });
sxRun('addChild ns',   function ($x) { $x->addChild('q:n', 'v', 'urn:q'); });
sxRun('addChild on attrs', function ($x) { $x->attributes()->addChild('n', 'v'); });
sxRun('addChild on miss',  function ($x) { $x->nope->addChild('n', 'v'); });
sxRun('addAttribute',  function ($x) { var_dump($x->addAttribute('n', 'a&b')); });
sxRun('addAttribute dup',  function ($x) { $x->addAttribute('a', 'v'); });
sxRun('addAttribute ns',   function ($x) { $x->addAttribute('q:n', 'v', 'urn:q'); });
sxRun('addAttribute miss', function ($x) { $x->nope->addAttribute('n', 'v'); });

restore_error_handler();
echo "-- xpath\n";
$q = simplexml_load_string('<r a="1" xmlns:m="urn:m" m:b="2"><!--c--><?pi z?>text<e>E</e><e>F</e></r>');
$q->registerXPathNamespace('z', 'urn:m');
foreach (['//e', '//e[2]', '//@a', '//@m:b', '//z:b', '//text()', '//comment()',
          '//processing-instruction()', '/', '//nope', 'count(//e)', 'string(//e)',
          '1=1', '///'] as $expr) {
    $r = @$q->xpath($expr);
    printf("%-28s %s\n", $expr, is_array($r)
        ? count($r) . ':' . implode(',', array_map(fn($n) => $n->getName() . '=' . trim((string) $n), $r))
        : var_export($r, true));
}
var_dump($q->attributes()->xpath('//e'), $q->nope->xpath('//e'));
var_dump($q->registerXPathNamespace('k', 'urn:k'));

echo "-- asXML\n";
$s = simplexml_load_string('<?xml version="1.0" encoding="UTF-8"?><r a="1"><c>one</c></r>');
var_dump($s->asXML(), $s->c->asXML(), $s['a']->asXML(), $s->children()->asXML(),
         $s->attributes()->asXML(), $s->nope->asXML(), $s->saveXML() === $s->asXML());
$file = tempnam(sys_get_temp_dir(), 'sx');
var_dump($s->asXML($file));
echo file_get_contents($file);
unlink($file);

echo "-- the two doors into ext/dom\n";
$d = dom_import_simplexml($s->c);
printf("%s %s owner=%s\n", get_class($d), $d->tagName, get_class($d->ownerDocument));
var_dump($d === dom_import_simplexml($s->c));
var_dump(get_class(dom_import_simplexml($s['a'])));
$d->setAttribute('added', '1');
echo $s->c->asXML(), "\n";
$dom = new DOMDocument();
$dom->loadXML('<r><q>7</q></r>');
$back = simplexml_import_dom($dom);
printf("%s %s\n", $back->getName(), (string) $back->q);
$back->q = 'written';
echo $dom->saveXML();
var_dump(dom_import_simplexml($back) === $dom->documentElement);
var_dump(simplexml_import_dom($dom) === simplexml_import_dom($dom));
foreach ([fn() => dom_import_simplexml(new stdClass()), fn() => simplexml_import_dom(new stdClass()),
          fn() => simplexml_import_dom(new DOMDocument())] as $bad) {
    try { var_dump(@$bad()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "-- parse failures\n";
/* libxml's OWN diagnostic is what a parse failure prints here, and both its
 * wording and how many of them one bad document produces move between libxml
 * releases -- so what is pinned is php's own answer: the value, the
 * constructor's Exception, and that internal-error mode silences the printing
 * and fills the queue instead. */
foreach (['<r><', '', '<r>ok</r>'] as $src) {
    $loaded = @simplexml_load_string($src);
    /* Object HANDLES never match between engines: the answer is shown by shape. */
    var_dump($loaded === false ? false : (array) $loaded);
    try { @new SimpleXMLElement($src); echo "constructed\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
libxml_use_internal_errors(true);
var_dump(simplexml_load_string('<r><'));
$errs = libxml_get_errors();
printf("queued=%d first=%d last=%s\n", (int) (count($errs) > 0),
    count($errs) > 0 ? $errs[0]->level : -1,
    var_export(libxml_get_last_error() !== false, true));
libxml_clear_errors();
libxml_use_internal_errors(false);
var_dump(libxml_get_last_error());
var_dump((array) simplexml_load_string('<r><c><![CDATA[cd]]></c></r>', 'SimpleXMLElement', LIBXML_NOCDATA));
--EXPECT--
-- property writes
new name               ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><fresh>v</fresh></r>
existing               ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>changed</d></r>
a set of two           ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:{closure}(): Cannot assign to an array of nodes (duplicate subnodes or attr detected)
nested create          ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><a><b>v</b></a></r>
int                    ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>42</d></r>
float                  ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>1.5</d></r>
true                   ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>1</d></r>
false                  ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d/></r>
null                   ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d/></r>
another node           ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>one</d></r>
array                  TypeError: It's not possible to assign a complex type to properties, array given <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>
plain object           TypeError: It's not possible to assign a complex type to properties, stdClass given <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>
escaped                ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>a&amp;b&lt;c&gt;</d></r>
through children       ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>v</d></r>
on an attr list        ok                       <?xml version="1.0"?><r a="v"><c>one</c><c>two</c><d>dee</d></r>
-- dimension writes
attr set               ok                       <?xml version="1.0"?><r a="9"><c>one</c><c>two</c><d>dee</d></r>
attr create            ok                       <?xml version="1.0"?><r a="1" z="9"><c>one</c><c>two</c><d>dee</d></r>
attr escaped           ok                       <?xml version="1.0"?><r a="1" z="a&amp;b&lt;c&gt;"><c>one</c><c>two</c><d>dee</d></r>
nth element            ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>changed</c><d>dee</d></r>
past the end           ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><c>changed</c></r>  ## 2:{closure}(): Cannot add element c number 5 when only 2 such elements exist
negative               ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:{closure}(): Cannot add element c number -1 when only 2 such elements exist
append                 ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><c>nine</c></r>
append to node         ValueError: Cannot append to an attribute list <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>
own text               ok                       <?xml version="1.0"?><r a="1">Z</r>
past own text          ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:{closure}(): Cannot add element r number 1 when only 0 such elements exist
attr of a set          ok                       <?xml version="1.0"?><r a="1"><c q="Z">one</c><c>two</c><d>dee</d></r>
attr of a miss         ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><nope q="Z"/></r>
attr list set          ok                       <?xml version="1.0"?><r a="Z"><c>one</c><c>two</c><d>dee</d></r>
compound               ok                       <?xml version="1.0"?><r a="1!"><c>one</c><c>two</c><d>dee</d></r>
-- unset
every match            ok                       <?xml version="1.0"?><r a="1"><d>dee</d></r>
one of a set           ok                       <?xml version="1.0"?><r a="1"><c>two</c><d>dee</d></r>
an attribute           ok                       <?xml version="1.0"?><r><c>one</c><c>two</c><d>dee</d></r>
a miss                 ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>
through children       ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c></r>
-- addChild / addAttribute
addChild               ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><n>v</n></r>  ## string(1) "n"
addChild empty         ValueError: SimpleXMLElement::addChild(): Argument #1 ($qualifiedName) must not be empty <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>
addChild ns            ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d><q:n xmlns:q="urn:q">v</q:n></r>
addChild on attrs      ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:SimpleXMLElement::addChild(): Cannot add element to attributes
addChild on miss       ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:SimpleXMLElement::addChild(): Cannot add child. Parent is not a permanent member of the XML tree
addAttribute           ok                       <?xml version="1.0"?><r a="1" n="a&amp;b"><c>one</c><c>two</c><d>dee</d></r>  ## NULL
addAttribute dup       ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:SimpleXMLElement::addAttribute(): Attribute already exists
addAttribute ns        ok                       <?xml version="1.0"?><r xmlns:q="urn:q" a="1" q:n="v"><c>one</c><c>two</c><d>dee</d></r>
addAttribute miss      ok                       <?xml version="1.0"?><r a="1"><c>one</c><c>two</c><d>dee</d></r>  ## 2:SimpleXMLElement::addAttribute(): Unable to locate parent Element
-- xpath
//e                          2:e=E,e=F
//e[2]                       1:e=F
//@a                         1:a=1
//@m:b                       1:b=2
//z:b                        0:
//text()                     3:r=text,e=E,e=F
//comment()                  1:comment=c
//processing-instruction()   1:pi=z
/                            0:
//nope                       0:
count(//e)                   false
string(//e)                  false
1=1                          false
///                          false
NULL
NULL
bool(true)
-- asXML
string(63) "<?xml version="1.0" encoding="UTF-8"?>
<r a="1"><c>one</c></r>
"
string(10) "<c>one</c>"
string(6) " a="1""
string(10) "<c>one</c>"
string(6) " a="1""
bool(false)
bool(true)
bool(true)
<?xml version="1.0" encoding="UTF-8"?>
<r a="1"><c>one</c></r>
-- the two doors into ext/dom
DOMElement c owner=DOMDocument
bool(true)
string(7) "DOMAttr"
<c added="1">one</c>
r 7
<?xml version="1.0"?>
<r><q>written</q></r>
bool(true)
bool(false)
TypeError: dom_import_simplexml(): Argument #1 ($node) is not a valid node type
TypeError: simplexml_import_dom(): Argument #1 ($node) must be a valid XML node
NULL
-- parse failures
bool(false)
Exception: String could not be parsed as XML
bool(false)
Exception: String could not be parsed as XML
array(1) {
  [0]=>
  string(2) "ok"
}
constructed
bool(false)
queued=1 first=3 last=true
bool(false)
array(1) {
  ["c"]=>
  string(2) "cd"
}
