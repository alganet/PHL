--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath::quote: the two wrappings and the concat() split
--FILE--
<?php
// no single quote: single-quoted
var_dump(DOMXPath::quote('plain'));
var_dump(DOMXPath::quote(''));
var_dump(DOMXPath::quote('say "hi"'));
// a single quote but no double: double-quoted
var_dump(DOMXPath::quote("it's"));
var_dump(DOMXPath::quote("'"));
// both kinds: concat(), each run wrapped in the quote it does not contain
var_dump(DOMXPath::quote("both ' and \""));
var_dump(DOMXPath::quote("a'b\"c'd\"e"));
var_dump(DOMXPath::quote("'\"'"));
var_dump(DOMXPath::quote('""\'\''));
var_dump(DOMXPath::quote("\"'"));
var_dump(DOMXPath::quote("'\""));
// static, and reachable through an instance
$d = new DOMDocument;
$d->loadXML('<r><a>it\'s</a><a>say "hi"</a><a>both \' and "</a></r>');
$xp = new DOMXPath($d);
var_dump($xp->quote("inst'ance"));
// ZPP
var_dump(DOMXPath::quote(42), DOMXPath::quote(1.5), DOMXPath::quote(true));
try { DOMXPath::quote(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { DOMXPath::quote([]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// the point of it: every value rides a predicate as itself
foreach (["it's", 'say "hi"', 'both \' and "', 'nope'] as $needle) {
  $q = DOMXPath::quote($needle);
  var_dump($q, $xp->evaluate("count(//a[string(.) = $q])"));
}
--EXPECT--
string(7) "'plain'"
string(2) "''"
string(10) "'say "hi"'"
string(6) ""it's""
string(3) ""'""
string(25) "concat("both ' and ",'"')"
string(28) "concat("a'b",'"c',"'d",'"e')"
string(19) "concat("'",'"',"'")"
string(17) "concat('""',"''")"
string(15) "concat('"',"'")"
string(15) "concat("'",'"')"
string(11) ""inst'ance""
string(4) "'42'"
string(5) "'1.5'"
string(3) "'1'"
DOMXPath::quote() expects exactly 1 argument, 0 given
DOMXPath::quote(): Argument #1 ($str) must be of type string, array given
string(6) ""it's""
float(1)
string(10) "'say "hi"'"
float(1)
string(25) "concat("both ' and ",'"')"
float(1)
string(6) "'nope'"
float(0)
