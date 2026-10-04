--TEST--
The XML serializer of php 8.4 namespaced tree: empty elements, escapes, encoding
--FILE--
<?php
$H = 'http://www.w3.org/1999/xhtml';
$S = 'http://www.w3.org/2000/svg';

echo "-- an empty element is three answers, and the namespace picks which\n";
$d = Dom\XMLDocument::createFromString(
	'<r xmlns="' . $H . '"><br/><div/><img src="a"/><p/><span></span><template/></r>');
echo $d->saveXml(), "\n";
echo $d->documentElement->outerHTML, "\n";
echo $d->documentElement->innerHTML, "\n";

echo "\n-- the void names, which take a space before the slash\n";
$v = ['area','base','basefont','bgsound','br','col','embed','frame','hr','img',
      'input','keygen','link','menuitem','meta','param','source','track','wbr'];
$d = Dom\XMLDocument::createEmpty();
$r = $d->appendChild($d->createElementNS($H, 'r'));
foreach ($v as $n) {
	$r->appendChild($d->createElementNS($H, $n));
}
echo $d->saveXml($r), "\n";

echo "\n-- and a name that is not void takes a real end tag\n";
$d = Dom\XMLDocument::createEmpty();
$r = $d->appendChild($d->createElementNS($H, 'r'));
foreach (['div','p','span','td','table','a','menu','item'] as $n) {
	$r->appendChild($d->createElementNS($H, $n));
}
echo $d->saveXml($r), "\n";

echo "\n-- any other namespace, and none, keep libxml's shape\n";
$d = Dom\XMLDocument::createEmpty();
$r = $d->appendChild($d->createElementNS($S, 'svg'));
$r->appendChild($d->createElementNS($S, 'path'));
$r->appendChild($d->createElementNS($S, 'br'));
$r->appendChild($d->createElement('br'));
echo $d->saveXml($r), "\n";
echo Dom\XMLDocument::createFromString('<r><a/><b></b></r>')->saveXml(), "\n";

echo "\n-- LIBXML_NOEMPTYTAG writes the end tag everywhere, void names included\n";
$d = Dom\XMLDocument::createFromString(
	'<r xmlns="' . $H . '"><br/><div/><q xmlns="urn:a"/></r>');
echo $d->saveXml(null, LIBXML_NOEMPTYTAG), "\n";
echo $d->saveXml($d->documentElement, LIBXML_NOEMPTYTAG), "\n";

echo "\n-- text escapes three bytes, an attribute value escapes seven\n";
$d = Dom\XMLDocument::createEmpty();
$r = $d->appendChild($d->createElement('r'));
$r->setAttribute('a', "L1\rL2\nL3\tL4\"L5&L6<L7>L8'L9");
$r->appendChild($d->createTextNode("T1\rT2\nT3\tT4\"T5&T6<T7>T8'T9"));
echo bin2hex($d->saveXml($r)), "\n";
echo $d->saveXml($r->getAttributeNode('a')), "\n";

echo "\n-- a comment, a CDATA section and an instruction are raw\n";
$d = Dom\XMLDocument::createEmpty();
$r = $d->appendChild($d->createElement('r'));
$r->appendChild($d->createComment("M1<M2&M3"));
$r->appendChild($d->createCDATASection("C1<C2&C3"));
$r->appendChild($d->createProcessingInstruction('pi', "P1<P2&P3"));
echo $d->saveXml($r), "\n";

echo "\n-- a node's dump goes out in the encoding the document declares\n";
$d = Dom\XMLDocument::createEmpty('1.0', 'ISO-8859-1');
$r = $d->appendChild($d->createElementNS($H, 'r'));
$r->appendChild($d->createElementNS($H, 'br'));
$r->appendChild($d->createElementNS($H, 'div'));
$r->appendChild($d->createTextNode("\u{00E9}"));
echo bin2hex($d->saveXml()), "\n";
echo bin2hex($d->saveXml($r)), "\n";
echo bin2hex($r->outerHTML), "\n";

echo "\n-- formatting lays out elements, and stops at the first text child\n";
$d = Dom\XMLDocument::createFromString('<r><a><b/></a><c>x</c><d><e/></d></r>');
$d->formatOutput = true;
echo $d->saveXml(), "\n";
$d = Dom\XMLDocument::createFromString('<r xmlns="' . $H . '"><div/><br/><p><span/></p></r>');
$d->formatOutput = true;
echo $d->saveXml(), "\n";

echo "\n-- a template writes its content, and asks its emptiness of its children\n";
$d = Dom\XMLDocument::createFromString('<r xmlns="' . $H . '"><template><i>x</i></template></r>');
echo $d->saveXml(), "\n";
$d->formatOutput = true;
echo $d->saveXml(), "\n";
$h = Dom\HTMLDocument::createFromString(
	'<!DOCTYPE html><body><template><i>y</i></template>', LIBXML_NOERROR);
echo $h->saveXml($h->getElementsByTagName('template')[0]), "\n";

echo "\n-- the prolog, a trailing comment and a doctype keep their own joins\n";
echo Dom\XMLDocument::createFromString(
	'<?xml version="1.0"?><?pi a?><!--c--><r/><!--tail-->')->saveXml(), "\n";
echo Dom\XMLDocument::createFromString(
	'<!DOCTYPE r PUBLIC "p" "s"><r xmlns="' . $H . '"><br/></r>')->saveXml(), "\n";
echo Dom\XMLDocument::createFromString('<r/>')->saveXml(null, LIBXML_NOXMLDECL), "\n";
--EXPECT--
-- an empty element is three answers, and the namespace picks which
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="http://www.w3.org/1999/xhtml"><br /><div></div><img src="a" /><p></p><span></span><template></template></r>
<r xmlns="http://www.w3.org/1999/xhtml"><br /><div></div><img src="a" /><p></p><span></span><template></template></r>
<br xmlns="http://www.w3.org/1999/xhtml" /><div xmlns="http://www.w3.org/1999/xhtml"></div><img xmlns="http://www.w3.org/1999/xhtml" src="a" /><p xmlns="http://www.w3.org/1999/xhtml"></p><span xmlns="http://www.w3.org/1999/xhtml"></span><template xmlns="http://www.w3.org/1999/xhtml"></template>

-- the void names, which take a space before the slash
<r xmlns="http://www.w3.org/1999/xhtml"><area /><base /><basefont /><bgsound /><br /><col /><embed /><frame /><hr /><img /><input /><keygen /><link /><menuitem /><meta /><param /><source /><track /><wbr /></r>

-- and a name that is not void takes a real end tag
<r xmlns="http://www.w3.org/1999/xhtml"><div></div><p></p><span></span><td></td><table></table><a></a><menu></menu><item></item></r>

-- any other namespace, and none, keep libxml's shape
<svg xmlns="http://www.w3.org/2000/svg"><path/><br/><br xmlns=""/></svg>
<?xml version="1.0" encoding="UTF-8"?>
<r><a/><b/></r>

-- LIBXML_NOEMPTYTAG writes the end tag everywhere, void names included
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="http://www.w3.org/1999/xhtml"><br></br><div></div><q xmlns="urn:a"></q></r>
<r xmlns="http://www.w3.org/1999/xhtml"><br></br><div></div><q xmlns="urn:a"></q></r>

-- text escapes three bytes, an attribute value escapes seven
3c7220613d224c31262331333b4c32262331303b4c332623393b4c342671756f743b4c3526616d703b4c36266c743b4c372667743b4c38274c39223e54310d54320a543309543422543526616d703b5436266c743b54372667743b54382754393c2f723e
a="L1&#13;L2&#10;L3&#9;L4&quot;L5&amp;L6&lt;L7&gt;L8'L9"

-- a comment, a CDATA section and an instruction are raw
<r><!--M1<M2&M3--><![CDATA[C1<C2&C3]]><?pi P1<P2&P3?></r>

-- a node's dump goes out in the encoding the document declares
3c3f786d6c2076657273696f6e3d22312e302220656e636f64696e673d2249534f2d383835392d31223f3e0a3c7220786d6c6e733d22687474703a2f2f7777772e77332e6f72672f313939392f7868746d6c223e3c6272202f3e3c6469763e3c2f6469763ee93c2f723e
3c7220786d6c6e733d22687474703a2f2f7777772e77332e6f72672f313939392f7868746d6c223e3c6272202f3e3c6469763e3c2f6469763ee93c2f723e
3c7220786d6c6e733d22687474703a2f2f7777772e77332e6f72672f313939392f7868746d6c223e3c6272202f3e3c6469763e3c2f6469763ec3a93c2f723e

-- formatting lays out elements, and stops at the first text child
<?xml version="1.0" encoding="UTF-8"?>
<r>
  <a>
    <b/>
  </a>
  <c>x</c>
  <d>
    <e/>
  </d>
</r>
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="http://www.w3.org/1999/xhtml">
  <div></div>
  <br />
  <p>
    <span></span>
  </p>
</r>

-- a template writes its content, and asks its emptiness of its children
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="http://www.w3.org/1999/xhtml"><template></template></r>
<?xml version="1.0" encoding="UTF-8"?>
<r xmlns="http://www.w3.org/1999/xhtml">
  <template>
  </template>
</r>
<template xmlns="http://www.w3.org/1999/xhtml"><i>y</i></template>

-- the prolog, a trailing comment and a doctype keep their own joins
<?xml version="1.0" encoding="UTF-8"?>
<?pi a?><!--c--><r/><!--tail-->
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE r PUBLIC "p" "s">
<r xmlns="http://www.w3.org/1999/xhtml"><br /></r>
<r/>
