--TEST--
An HTML document parses <svg> and <math> into their own namespaces
--FILE--
<?php
/* Inside a `<svg>` or a `<math>` the HTML rules stop: the elements carry a
 * namespace of their own, their names keep the case the vocabulary spells
 * them with rather than folding, `/>` closes an element, and a handful of
 * HTML tags abandon the subtree instead of nesting in it. */

function h5tree($src,$opt = LIBXML_NOERROR){
	$doc = \Dom\HTMLDocument::createFromString($src,$opt);
	$walk = function($node,$depth) use (&$walk) {
		foreach( $node->childNodes as $child ){
			if( $child->nodeType === XML_ELEMENT_NODE ){
				printf("%s<%s> %s\n",str_repeat('  ',$depth),$child->nodeName,
					$child->namespaceURI === null ? 'no-namespace'
						: substr($child->namespaceURI,strrpos($child->namespaceURI,'/') + 1));
				$walk($child,$depth + 1);
			}elseif( $child->nodeType === XML_TEXT_NODE ){
				printf("%s#text %s\n",str_repeat('  ',$depth),
					var_export($child->textContent,true));
			}
		}
	};
	$walk($doc->body ?? $doc,0);
}

foreach ([
	'svg'        => '<body><svg><circle/></svg>',
	'math'       => '<body><math><mi>x</mi></math>',
	'case'       => '<body><svg><CLIPPATH><feGaussianBlur/><textpath/><unknownThing/></clippath></svg>',
	'selfclose'  => '<body><svg><circle/>t</svg>after',
	'endtagcase' => '<body><svg><g></G></svg>',
	'breakout'   => '<body><svg><circle><p>x</p></svg>',
	'foreignobj' => '<body><svg><foreignObject><b>a<i>b</foreignObject>c</svg>',
	'mathtext'   => '<body><math><mi><b>x</b></mi></math>',
	'annotation' => '<body><math><annotation-xml encoding="text/html"><b>x</b></annotation-xml></math>',
	'nested'     => '<body><svg><foreignObject><math><mi>i</mi></math></foreignObject></svg>',
	'cdata'      => '<body><svg><![CDATA[raw]]></svg>',
	'htmlcdata'  => '<body><![CDATA[raw]]><p>x',
] as $label => $src ){
	echo "== $label\n";
	h5tree($src);
}

/* The attributes of a foreign start tag: the SVG table restores the case the
 * tokenizer folded away, `definitionurl` becomes MathML's own spelling, and
 * the `xlink:`/`xml:` names are namespaces rather than colons in a name. */
echo "== attributes\n";
$svg = \Dom\HTMLDocument::createFromString(
	'<body><svg viewBox="0 0 1 1" ATTRIBUTENAME="x" xlink:href="u" xml:lang="en" foo:bar="b" CamelCase="y"/>',
	LIBXML_NOERROR)->getElementsByTagName('svg')[0];
foreach( ['viewBox','attributeName','foo:bar','camelcase'] as $name ){
	printf("%-16s => %s\n",$name,var_export($svg->getAttribute($name),true));
}
foreach( [['http://www.w3.org/1999/xlink','href'],
          ['http://www.w3.org/XML/1998/namespace','lang']] as $pair ){
	$attr = $svg->getAttributeNodeNS($pair[0],$pair[1]);
	printf("%-16s => %s\n",$pair[1],
		$attr === null ? 'absent' : $attr->nodeName.'='.$attr->value);
}
$math = \Dom\HTMLDocument::createFromString('<body><math definitionurl="u"/>',
	LIBXML_NOERROR)->getElementsByTagName('math')[0];
printf("%-16s => %s\n",'definitionURL',var_export($math->getAttribute('definitionURL'),true));

/* The option that asks for an unnamespaced tree takes the foreign elements
 * with it, and the tree is still the one the foreign rules built. */
echo "== no-default-ns\n";
h5tree('<body><svg><CLIPPATH/><circle><p>x</p></svg>',
	LIBXML_NOERROR | \Dom\HTML_NO_DEFAULT_NS);

echo "== serialized\n";
echo \Dom\HTMLDocument::createFromString('<body><svg viewBox="1"><circle cx="1"/></svg>',
	LIBXML_NOERROR)->saveHtml(),"\n";
?>
--EXPECT--
== svg
<svg> svg
  <circle> svg
== math
<math> MathML
  <mi> MathML
    #text 'x'
== case
<svg> svg
  <clipPath> svg
    <feGaussianBlur> svg
    <textPath> svg
    <unknownthing> svg
== selfclose
<svg> svg
  <circle> svg
  #text 't'
#text 'after'
== endtagcase
<svg> svg
  <g> svg
== breakout
<svg> svg
  <circle> svg
<P> xhtml
  #text 'x'
== foreignobj
<svg> svg
  <foreignObject> svg
    <B> xhtml
      #text 'a'
      <I> xhtml
        #text 'bc'
== mathtext
<math> MathML
  <mi> MathML
    <B> xhtml
      #text 'x'
== annotation
<math> MathML
  <annotation-xml> MathML
    <B> xhtml
      #text 'x'
== nested
<svg> svg
  <foreignObject> svg
    <math> MathML
      <mi> MathML
        #text 'i'
== cdata
<svg> svg
  #text 'raw'
== htmlcdata
<P> xhtml
  #text 'x'
== attributes
viewBox          => '0 0 1 1'
attributeName    => 'x'
foo:bar          => 'b'
camelcase        => 'y'
href             => xlink:href=u
lang             => xml:lang=en
definitionURL    => 'u'
== no-default-ns
<html> no-namespace
  <head> no-namespace
  <body> no-namespace
    <svg> no-namespace
      <clipPath> no-namespace
      <circle> no-namespace
    <p> no-namespace
      #text 'x'
== serialized
<html><head></head><body><svg viewBox="1"><circle cx="1"></circle></svg></body></html>
