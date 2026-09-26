--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath php-function bridge: re-entrancy, a table rewritten from inside its own callback
--FILE--
<?php
// re-entrancy: a callback that evaluates on the SAME DOMXPath, and one that
// rewrites the very registration table its own entry lives in
$d = new DOMDocument;
$d->loadXML('<r><a>1</a><a>2</a><b>x</b></r>');
$xp = new DOMXPath($d);
$xp->registerNamespace('php', 'http://php.net/xpath');
$xp->registerPhpFunctions();
function re_inner($nodes) { global $xp; return (string)$xp->evaluate('count(//a)') . ':' . count($nodes); }
var_dump($xp->evaluate('php:function("re_inner", //a)'));
function re_rewrite($s) { global $xr; $xr->registerPhpFunctions(['re_rewrite' => 'strtoupper']); return "first:$s"; }
$xr = new DOMXPath($d);
$xr->registerNamespace('php', 'http://php.net/xpath');
$xr->registerPhpFunctions(['re_rewrite']);
var_dump($xr->evaluate('php:function("re_rewrite","ab")'));
var_dump($xr->evaluate('php:function("re_rewrite","ab")'));
// a __toString that throws, as a callback's return value
class ReBad { public function __toString(): string { throw new RuntimeException('str'); } }
function re_bad() { return new ReBad; }
$xb = new DOMXPath($d); $xb->registerNamespace('php','http://php.net/xpath'); $xb->registerPhpFunctions();
try { var_dump($xb->evaluate('php:function("re_bad")')); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
// a callback used inside a predicate over many nodes
function re_len($s) { return strlen($s); }
$xl = new DOMXPath($d); $xl->registerNamespace('php','http://php.net/xpath'); $xl->registerPhpFunctions();
var_dump($xl->query('//a[php:functionString("re_len", string(.)) = "1"]')->length);
// deep: nested own-URI functions
$xn = new DOMXPath($d);
$xn->registerNamespace('my', 'urn:my');
$xn->registerPhpFunctionNS('urn:my', 'outer', function ($s) { global $xn; return $xn->evaluate('my:inner("' . $s . '")'); });
$xn->registerPhpFunctionNS('urn:my', 'inner', fn($s) => "in($s)");
var_dump($xn->evaluate('my:outer("z")'));
--EXPECT--
string(3) "2:2"
string(8) "first:ab"
string(2) "AB"
TypeError: Only objects that are instances of DOM nodes can be converted to an XPath expression
int(2)
string(5) "in(z)"
