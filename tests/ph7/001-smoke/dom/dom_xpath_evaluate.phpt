--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath evaluate: the four result kinds, and a scalar query() answers the EMPTY list
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r><a>1</a><a>two</a><b>3.5</b></r>');
$xp = new DOMXPath($d);
// the four result kinds
var_dump($xp->evaluate('count(//a)'));
var_dump($xp->evaluate('string(//b)'));
var_dump($xp->evaluate('boolean(//a)'));
var_dump($xp->evaluate('boolean(//nope)'));
var_dump($xp->evaluate('concat("a","b")'));
var_dump($xp->evaluate('1 div 0'));
var_dump($xp->evaluate('0 div 0'));
var_dump($xp->evaluate('number(//a[2])'));
var_dump($xp->evaluate('string(//nope)'));
$r = $xp->evaluate('//a');
var_dump(get_class($r), $r->length);
$t = $xp->evaluate('/r/a[1]/text()');
var_dump(get_class($t), $t->item(0)->data);
foreach ($xp->evaluate('//a') as $i => $n) { echo $i, ':', $n->textContent, ' '; }
echo "\n";
// a scalar-result query() answers php's EMPTY list, not false
$q = $xp->query('count(//a)');
var_dump(get_class($q), $q->length, $q->item(0));
var_dump($xp->query('string(//b)')->length);
// invalid expressions answer false from both (captured; count unchanged after)
$prev = libxml_use_internal_errors(true);
var_dump($xp->evaluate('///'));
var_dump($xp->evaluate(''));
var_dump($xp->evaluate('bogus-fn()'));
var_dump($xp->query('///'));
var_dump($xp->query(''));
libxml_use_internal_errors($prev);
libxml_clear_errors();
// context node + the registered/document namespaces ride evaluate too
$dns = new DOMDocument;
$dns->loadXML('<r xmlns:p="urn:p"><p:a>x</p:a><p:a>y</p:a></r>');
$xe = new DOMXPath($dns, false);
var_dump($xe->evaluate('count(//p:a)', null, true));
$xe2 = new DOMXPath($dns);
var_dump($xe2->evaluate('count(//p:a)'));
$xe3 = new DOMXPath($dns);
$xe3->registerNamespace('w', 'urn:p');
var_dump($xe3->evaluate('count(//w:a)', null, false));
// relative expression against an explicit context node
$first = $xe2->query('//p:a')->item(0);
var_dump($xe2->evaluate('string(.)', $first));
var_dump($xe2->evaluate('count(following-sibling::*)', $first));
// wrong-document context refuses with php's plain Error
$other = new DOMDocument;
$other->loadXML('<z><y/></z>');
try { $xp->evaluate('.', $other->documentElement); } catch (Error $e) { echo $e->getMessage(), "\n"; }
// ZPP
try { $xp->evaluate(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { $xp->evaluate([]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { $xp->evaluate('//a', 'nope'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
--EXPECT--
float(2)
string(3) "3.5"
bool(true)
bool(false)
string(2) "ab"
float(INF)
float(NAN)
float(NAN)
string(0) ""
string(11) "DOMNodeList"
int(2)
string(11) "DOMNodeList"
string(1) "1"
0:1 1:two 
string(11) "DOMNodeList"
int(0)
NULL
int(0)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
float(2)
float(2)
float(2)
string(1) "x"
float(1)
Node from wrong document
DOMXPath::evaluate() expects at least 1 argument, 0 given
DOMXPath::evaluate(): Argument #1 ($expression) must be of type string, array given
DOMXPath::evaluate(): Argument #2 ($contextNode) must be of type ?DOMNode, string given
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
