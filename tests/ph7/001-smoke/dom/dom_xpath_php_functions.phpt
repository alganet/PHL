--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath registerPhpFunctions + registerPhpFunctionNS: modes, conversions, screening
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r><a id="X">one</a><a id="y">TWO</a></r>');
// the php: pair exists on the context, but answers only after registration
$x0 = new DOMXPath($d);
$x0->registerNamespace('php', 'http://php.net/xpath');
try { $x0->evaluate('php:functionString("strtolower", "AB")'); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// ...and the prefix itself is nobody's job but the caller's
$x1 = new DOMXPath($d);
$x1->registerPhpFunctions();
$prev = libxml_use_internal_errors(true);
var_dump($x1->evaluate('php:function("strrev","abc")'));
libxml_use_internal_errors($prev);
libxml_clear_errors();
// unrestricted: any callable name, both spellings
$xp = new DOMXPath($d);
$xp->registerNamespace('php', 'http://php.net/xpath');
var_dump($xp->registerPhpFunctions());
var_dump($xp->evaluate('php:functionString("strtolower", string(//a[1]/@id))'));
var_dump($xp->evaluate('php:function("strrev", "abc")'));
var_dump($xp->query('//a[php:functionString("strlen", string(.)) = "3"]')->length);
// argument conversion: nodeset/string/number/boolean in, per spelling
function xpcb_dump(...$args) {
  foreach ($args as $i => $a) {
    if (is_array($a)) { echo "arg$i: array(", count($a), ")"; foreach ($a as $e) { echo " ", get_class($e); } echo "\n"; }
    else { echo "arg$i: ", gettype($a), " ", var_export($a, true), "\n"; }
  }
  return "ret";
}
var_dump($xp->evaluate('php:function("xpcb_dump", //a, string(//a[1]), 1+1, true(), false(), //nope)'));
var_dump($xp->evaluate('php:functionString("xpcb_dump", //a, true(), 3)'));
// return conversion: bool stays boolean, a node becomes a nodeset, the rest strings
function xpcb_int(){ return 7; }
function xpcb_bool(){ return true; }
function xpcb_null(){ return null; }
function xpcb_node(){ global $d; return $d->documentElement; }
function xpcb_obj(){ return new stdClass; }
function xpcb_throw(){ throw new RuntimeException('boom'); }
var_dump($xp->evaluate('php:function("xpcb_int")'));
var_dump($xp->evaluate('php:function("xpcb_int") + 1'));
var_dump($xp->evaluate('string(php:function("xpcb_bool"))'));
var_dump($xp->evaluate('php:function("xpcb_null")'));
$rn = $xp->evaluate('php:function("xpcb_node")');
var_dump(get_class($rn), $rn->length, $rn->item(0)->nodeName);
try { $xp->evaluate('php:function("xpcb_obj")'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { $xp->evaluate('php:function("xpcb_throw")'); } catch (RuntimeException $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// eval-time name screening
try { $xp->evaluate('php:function("xpcb_no_such_fn")'); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $xp->evaluate('php:function(123)'); } catch (TypeError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $xp->evaluate('php:function()'); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// restriction: names accumulate; a bare call lifts it; a later restrict RE-restricts
$xr = new DOMXPath($d);
$xr->registerNamespace('php', 'http://php.net/xpath');
$xr->registerPhpFunctions('strtolower');
$xr->registerPhpFunctions(['strrev', 'strlen']);
var_dump($xr->evaluate('php:function("strtolower","AB")'));
var_dump($xr->evaluate('php:function("strrev","ab")'));
try { $xr->evaluate('php:function("strtoupper","ab")'); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$xr->registerPhpFunctions();
var_dump($xr->evaluate('php:function("strtoupper","ab")'));
$xr->registerPhpFunctions('ucfirst');
try { $xr->evaluate('php:function("strtoupper","ab")'); } catch (Error $e) { echo $e->getMessage(), "\n"; }
var_dump($xr->evaluate('php:function("ucfirst","ab")'));
// a keyed restrict row is an ALIAS: the key answers, the value runs
$xk = new DOMXPath($d);
$xk->registerNamespace('php', 'http://php.net/xpath');
$xk->registerPhpFunctions(['shout' => 'strtoupper']);
var_dump($xk->evaluate('php:function("shout","ab")'));
try { $xk->evaluate('php:function("strtoupper","ab")'); } catch (Error $e) { echo $e->getMessage(), "\n"; }
// registration screening
try { $xk->registerPhpFunctions('xpcb_not_a_fn'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { $xk->registerPhpFunctions([1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
var_dump($xk->registerPhpFunctions(null));
// registerPhpFunctionNS: own URI, closures, replacement, screening
$xn = new DOMXPath($d);
$xn->registerNamespace('my', 'urn:my');
var_dump($xn->registerPhpFunctionNS('urn:my', 'lower', fn($s) => strtolower($s)));
var_dump($xn->evaluate('my:lower("ABC")'));
$xn->registerPhpFunctionNS('urn:my', 'lower', fn($s) => "second:$s");
var_dump($xn->evaluate('my:lower("ABC")'));
$xn->registerPhpFunctionNS('urn:my', 'count2', function(...$a){ return count($a[0]); });
var_dump($xn->evaluate('my:count2(//a)'));
// ...it does not open the php: door
$xn->registerNamespace('php', 'http://php.net/xpath');
try { $xn->evaluate('php:function("strrev","ab")'); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { $xn->registerPhpFunctionNS('http://php.net/xpath', 'x', 'strtolower'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { $xn->registerPhpFunctionNS('urn:my', '1a', 'strtolower'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { $xn->registerPhpFunctionNS('urn:my', 'a b', 'strtolower'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { $xn->registerPhpFunctionNS('urn:my', '', 'strtolower'); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { $xn->registerPhpFunctionNS('urn:my', 'x', 'xpcb_not_a_fn'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// the restriction moves BEFORE the screening: a refused row leaves the object
// restricted, with every row up to it registered
$xa = new DOMXPath($d);
$xa->registerNamespace('php', 'http://php.net/xpath');
try { $xa->registerPhpFunctions(['strrev', 'xpcb_not_a_fn', 'strtolower']); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
var_dump($xa->evaluate('php:function("strrev","AB")'));
try { $xa->evaluate('php:function("strtolower","AB")'); } catch (Error $e) { echo $e->getMessage(), "\n"; }
--EXPECT--
Error: No callbacks were registered
bool(false)
NULL
string(1) "x"
string(3) "cba"
int(2)
arg0: array(2) DOMElement DOMElement
arg1: string 'one'
arg2: double 2.0
arg3: boolean true
arg4: boolean false
arg5: array(0)
string(3) "ret"
arg0: string 'one'
arg1: boolean true
arg2: double 3.0
string(3) "ret"
string(1) "7"
float(8)
string(4) "true"
string(0) ""
string(11) "DOMNodeList"
int(1)
string(1) "r"
Only objects that are instances of DOM nodes can be converted to an XPath expression
RuntimeException: boom
Error: Invalid callback xpcb_no_such_fn, function "xpcb_no_such_fn" not found or invalid function name
TypeError: Handler name must be a string
Error: Function name must be passed as the first argument
string(2) "ab"
string(2) "ba"
Error: No callback handler "strtoupper" registered
string(2) "AB"
No callback handler "strtoupper" registered
string(2) "Ab"
string(2) "AB"
No callback handler "strtoupper" registered
DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, function "xpcb_not_a_fn" not found or invalid function name
DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array with valid callbacks as values, no array or string given
NULL
NULL
string(3) "abc"
string(10) "second:ABC"
string(1) "2"
No callbacks were registered
DOMXPath::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "http://php.net/xpath" because it is reserved by PHP
DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name
DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name
DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name
DOMXPath::registerPhpFunctionNS(): Argument #3 ($callable) must be a valid callback, function "xpcb_not_a_fn" not found or invalid function name
DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array with valid callbacks as values, function "xpcb_not_a_fn" not found or invalid function name
string(2) "BA"
No callback handler "strtolower" registered
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
