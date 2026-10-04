--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\XPath: the namespaced tree's evaluator, its Dom\NodeList results and its two own refusals
--FILE--
<?php
$mxpR = new ReflectionClass('Dom\XPath');
printf("final=%d parent=%s\n", $mxpR->isFinal() ? 1 : 0,
	var_export($mxpR->getParentClass() ?: null, true));
foreach ($mxpR->getProperties() as $mxpP) {
	printf("PROP %s %s mods=%d virtual=%d\n", $mxpP->getType(), $mxpP->getName(),
		$mxpP->getModifiers(), $mxpP->isVirtual() ? 1 : 0);
}
foreach ($mxpR->getMethods() as $mxpM) {
	$mxpA = [];
	foreach ($mxpM->getParameters() as $mxpQ) {
		$mxpS = ($mxpQ->getType() ?: '') . ' $' . $mxpQ->getName();
		if ($mxpQ->isDefaultValueAvailable()) {
			$mxpS .= ' = ' . var_export($mxpQ->getDefaultValue(), true);
		}
		$mxpA[] = $mxpS;
	}
	printf("METH %s%s(%s): %s\n", $mxpM->isStatic() ? 'static ' : '', $mxpM->getName(),
		implode(', ', $mxpA), $mxpM->getReturnType() ?: '-');
}

$mxpD = Dom\XMLDocument::createFromString('<r xmlns:a="urn:a"><a:x id="i">t</a:x><y/></r>');
$mxp = new Dom\XPath($mxpD);
var_dump(get_class($mxp->document), $mxp->registerNodeNamespaces);

// a result carries the family's own collection and the family's own wrappers
$mxpL = $mxp->query('//y');
var_dump(get_class($mxpL), $mxpL->length, get_class($mxpL->item(0)));
var_dump($mxp->evaluate('count(//*)'), get_class($mxp->evaluate('//y')),
	$mxp->evaluate('string(//y/preceding-sibling::*)'));
var_dump($mxp->registerNamespace('a', 'urn:a'), $mxp->query('//a:x')->length);

// query() promises a Dom\NodeList and evaluate()'s union has no false in it,
// so an expression that does not evaluate raises where the 2004 pair answers false
try { @$mxp->query('///'); } catch (\Throwable $e) {
	printf("BADQ %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage());
}
try { @$mxp->evaluate('///'); } catch (\Throwable $e) {
	printf("BADE %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage());
}

// the two trees never meet
try { new Dom\XPath(new DOMDocument()); } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}
try { $mxp->query('//y', new DOMDocument()); } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}
var_dump($mxp instanceof DOMXPath, Dom\XPath::quote("a'b"));

// the namespace:: axis is refused per RESULT, not per expression: an axis step
// that selects nothing still evaluates
try { $mxp->query('//*/namespace::*'); } catch (\Throwable $e) {
	printf("AXIS %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage());
}
try { $mxp->evaluate('//*/namespace::*'); } catch (\Throwable $e) {
	printf("AXIS2 %s(%d): %s\n", get_class($e), $e->getCode(), $e->getMessage());
}
var_dump($mxp->query('//zzz/namespace::*')->length);

try { clone $mxp; echo "clone ok\n"; } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}
try { echo serialize($mxp), "\n"; } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}

$mxp->registerNodeNamespaces = false;
var_dump($mxp->registerNodeNamespaces);
try { $mxp->registerNodeNamespaces = new stdClass; } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}
try { $mxp->document = $mxpD; } catch (\Throwable $e) {
	echo get_class($e), ": ", $e->getMessage(), "\n";
}

// every diagnostic names the class the call was made on
try { $mxp->registerPhpFunctionNS('http://php.net/xpath', 'f', 'strlen'); }
catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $mxp->registerPhpFunctionNS('urn:z', '1bad', 'strlen'); }
catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $mxp->registerPhpFunctions('nosuchfn__'); }
catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $mxp->registerPhpFunctions(['a' => 'nosuch__']); }
catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

$mxp2 = new Dom\XPath($mxpD);
$mxp2->registerNamespace('php', 'http://php.net/xpath');
$mxp2->registerPhpFunctions();
var_dump($mxp2->evaluate('php:function("strtoupper", string(//y/preceding-sibling::*))'));
?>
--EXPECT--
final=1 parent=NULL
PROP Dom\Document document mods=513 virtual=1
PROP bool registerNodeNamespaces mods=513 virtual=1
METH __construct(Dom\Document $document, bool $registerNodeNS = true): -
METH evaluate(string $expression, ?Dom\Node $contextNode = NULL, bool $registerNodeNS = true): Dom\NodeList|string|float|bool|null
METH query(string $expression, ?Dom\Node $contextNode = NULL, bool $registerNodeNS = true): Dom\NodeList
METH registerNamespace(string $prefix, string $namespace): bool
METH registerPhpFunctions(array|string|null $restrict = NULL): void
METH registerPhpFunctionNS(string $namespaceURI, string $name, callable $callable): void
METH static quote(string $str): string
string(15) "Dom\XMLDocument"
bool(true)
string(12) "Dom\NodeList"
int(1)
string(11) "Dom\Element"
float(3)
string(12) "Dom\NodeList"
string(1) "t"
bool(true)
int(1)
BADQ Error(0): Could not evaluate XPath expression
BADE Error(0): Could not evaluate XPath expression
TypeError: Dom\XPath::__construct(): Argument #1 ($document) must be of type Dom\Document, DOMDocument given
TypeError: Dom\XPath::query(): Argument #2 ($contextNode) must be of type ?Dom\Node, DOMDocument given
bool(false)
string(5) ""a'b""
AXIS DOMException(9): The namespace axis is not well-defined in the living DOM specification. Use Dom\Element::getInScopeNamespaces() or Dom\Element::getDescendantNamespaces() instead.
AXIS2 DOMException(9): The namespace axis is not well-defined in the living DOM specification. Use Dom\Element::getInScopeNamespaces() or Dom\Element::getDescendantNamespaces() instead.
int(0)
Error: Trying to clone an uncloneable object of class Dom\XPath
Exception: Serialization of 'Dom\XPath' is not allowed
bool(false)
TypeError: Cannot assign stdClass to property Dom\XPath::$registerNodeNamespaces of type bool
Error: Cannot modify readonly property Dom\XPath::$document
ValueError: Dom\XPath::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "http://php.net/xpath" because it is reserved by PHP
ValueError: Dom\XPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name
TypeError: Dom\XPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, function "nosuchfn__" not found or invalid function name
TypeError: Dom\XPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array with valid callbacks as values, function "nosuch__" not found or invalid function name
string(1) "T"
