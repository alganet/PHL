--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath registerNamespace + registerNodeNS: returns, document prefixes, precedence, the live property default
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p"><p:a>one</p:a><b xmlns:q="urn:q"><q:c>two</q:c></b></r>');
// registration returns: only the EMPTY prefix refuses
$xp = new DOMXPath($d);
var_dump($xp->registerNamespace('x', 'urn:p'));
var_dump($xp->registerNamespace('x', 'urn:other'));
var_dump($xp->registerNamespace('', 'urn:e'));
var_dump($xp->registerNamespace('1bad', 'urn:e'));
var_dump($xp->registerNamespace('x', ''));
// a registered prefix resolves in a later query
$xa = new DOMXPath($d);
$xa->registerNamespace('z', 'urn:p');
var_dump($xa->query('//z:a')->length);
// document prefixes register from the CONTEXT node's scope (default: root element)
var_dump($xp->query('//p:a')->length);
$b = $d->getElementsByTagName('b')->item(0);
var_dump($xp->query('.//q:c', $b)->length);
$prev = libxml_use_internal_errors(true);
var_dump($xp->query('//q:c'));             // q is not in scope of the default context
var_dump($xp->query('//p:a', null, false)); // node registration switched off per call
var_dump($xp->query('//p:a', $d));          // an explicitly PASSED document node carries none
libxml_use_internal_errors($prev);
libxml_clear_errors();
// node declarations beat the registered table only while registration is ON
$xw = new DOMXPath($d);
$xw->registerNamespace('p', 'urn:WRONG');
var_dump($xw->query('//p:a')->length);
var_dump($xw->query('//p:a', null, false)->length);
// the constructor's second argument lands on the property, the live 3rd-arg default
$xf = new DOMXPath($d, false);
var_dump($xf->registerNodeNamespaces);
var_dump($xf->query('//p:a', null, true)->length);
$xf->registerNodeNamespaces = true;
var_dump($xf->query('//p:a')->length);
$xf->registerNodeNamespaces = 1;
var_dump($xf->registerNodeNamespaces);
try { $xf->registerNodeNamespaces = null; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { $xf->document = $d; } catch (Error $e) { echo $e->getMessage(), "\n"; }
var_dump(isset($xf->registerNodeNamespaces), isset($xf->document), isset($xf->nope));
try { $xp->registerNamespace('x'); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
int(1)
int(1)
int(1)
bool(false)
bool(false)
bool(false)
int(1)
int(0)
bool(false)
int(1)
int(1)
bool(true)
Cannot assign null to property DOMXPath::$registerNodeNamespaces of type bool
Cannot modify readonly property DOMXPath::$document
bool(true)
bool(true)
bool(false)
DOMXPath::registerNamespace() expects exactly 2 arguments, 1 given
--CLEAN--
<?php
libxml_use_internal_errors(false);
libxml_clear_errors();
