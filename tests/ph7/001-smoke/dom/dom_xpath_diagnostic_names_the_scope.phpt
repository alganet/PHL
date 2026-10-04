--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMXPath diagnostics name the declaring scope, not the receiver's own class
--FILE--
<?php
namespace Dom;

// php names the scope a method is DECLARED on, so every subclass of the 2004
// class is still "DOMXPath" in its messages -- including one that happens to be
// declared inside a Dom\ namespace, which the namespaced class does not own.
class XpScopeProbe extends \DOMXPath {}

$xpsD = new \DOMDocument;
$xpsD->loadXML('<r/>');
$xps = new XpScopeProbe($xpsD);

try { $xps->registerPhpFunctions('nosuchfn__'); }
catch (\Throwable $e) { echo $e->getMessage(), "\n"; }
try { $xps->registerPhpFunctionNS('urn:z', '1bad', 'strlen'); }
catch (\Throwable $e) { echo $e->getMessage(), "\n"; }
try { $xps->registerNodeNamespaces = new \stdClass; }
catch (\Throwable $e) { echo $e->getMessage(), "\n"; }

// and the subclass keeps the 2004 answer shape: false, not a raised Error
var_dump(@$xps->query('///'));
var_dump(\get_class($xps->query('//r')));
?>
--EXPECT--
DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, function "nosuchfn__" not found or invalid function name
DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name
Cannot assign stdClass to property DOMXPath::$registerNodeNamespaces of type bool
bool(false)
string(11) "DOMNodeList"
