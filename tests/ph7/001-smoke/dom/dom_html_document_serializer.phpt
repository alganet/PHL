--TEST--
The HTML5 serializer of php 8.4 namespaced tree: void, raw, names and escapes
--FILE--
<?php
$H = 'http://www.w3.org/1999/xhtml';
$d = Dom\HTMLDocument::createEmpty();
function el($d,$ns,$n,$kid=null){ $e=$d->createElementNS($ns,$n); if($kid!==null) $e->appendChild($d->createTextNode($kid)); return $e; }

echo "-- text and attribute escaping (two tables, not one)\n";
$r = el($d,$H,'div');
$r->appendChild($d->createTextNode("a&b<c>d\"e'f\u{00A0}g"));
$r->setAttribute('x', "a&b<c>d\"e'f\u{00A0}g");
echo $d->saveHtml($r),"\n";
echo $d->saveHtml(el($d,$H,'i','')),"\n";

echo "\n-- the void elements, which open and never close\n";
foreach (['area','base','basefont','bgsound','br','col','embed','frame','hr','img',
          'input','keygen','link','meta','param','source','track','wbr'] as $v) {
	echo $d->saveHtml(el($d,$H,$v,'dropped')),"\n";
}
echo "-- and three that are not\n";
foreach (['p','span','textarea'] as $v) { echo $d->saveHtml(el($d,$H,$v)),"\n"; }

echo "\n-- the raw-text elements, whose children are not escaped\n";
foreach (['iframe','noembed','noframes','plaintext','script','style','xmp'] as $v) {
	echo $d->saveHtml(el($d,$H,$v,'a<b>&c')),"\n";
}
echo "-- and three the 2004 dumper also calls raw, which these rules do not\n";
foreach (['noscript','title','textarea'] as $v) {
	echo $d->saveHtml(el($d,$H,$v,'a<b>&c')),"\n";
}

echo "\n-- the tag name comes from the namespace, not from the prefix\n";
echo $d->saveHtml(el($d,$H,'p:div')),"\n";
echo $d->saveHtml(el($d,'http://www.w3.org/2000/svg','s:circle')),"\n";
echo $d->saveHtml(el($d,'http://www.w3.org/1998/Math/MathML','m:mi')),"\n";
echo $d->saveHtml(el($d,'urn:x','p:thing','t')),"\n";
echo $d->saveHtml(el($d,'urn:x','p:br')),"\n";
echo $d->saveHtml(el($d,'http://www.w3.org/2000/svg','s:script','a<b')),"\n";
echo $d->saveHtml($d->createElement('br')),"\n";

echo "\n-- three attribute names are re-spelled from the namespace\n";
$a = el($d,$H,'a');
$a->setAttributeNS('http://www.w3.org/XML/1998/namespace','foo:lang','en');
$a->setAttributeNS('http://www.w3.org/1999/xlink','q:href','u');
$a->setAttributeNS('urn:z','z:k','v');
$a->setAttribute('plain','p');
echo $d->saveHtml($a),"\n";

echo "\n-- the other node kinds\n";
echo $d->saveHtml($d->createComment('a<b>&c--')),"\n";
echo $d->saveHtml($d->createProcessingInstruction('t','d>e')),"\n";
echo $d->saveHtml($d->createProcessingInstruction('t','')),"\n";
echo $d->saveHtml($d->createTextNode('a<b>&')),"\n";
$g = $d->createDocumentFragment();
$g->appendChild(el($d,$H,'i'));
$g->appendChild($d->createTextNode('&'));
echo $d->saveHtml($g),"\n";
$q = el($d,$H,'i'); $q->setAttribute('a','b');
var_dump($d->saveHtml($q->getAttributeNode('a')));

echo "\n-- a doctype drops both identifiers here and keeps them under saveXml\n";
$h = $d->implementation->createHTMLDocument('x');
echo $h->saveHtml($h->doctype),"\n";
echo $h->saveHtml(),"\n";

echo "\n-- a template writes its content, which a hand-built one has none of\n";
$tp = el($d,$H,'template','appended');
printf("childNodes=%d %s\n",$tp->childNodes->length,$d->saveHtml($tp));

echo "\n-- the file writer answers the byte count and writes the same bytes\n";
$f = sys_get_temp_dir().'/phl_savehtml_'.getmypid().'.html';
var_dump($h->saveHtmlFile($f));
var_dump(file_get_contents($f) === $h->saveHtml());
@unlink($f);
?>
--EXPECT--
-- text and attribute escaping (two tables, not one)
<div x="a&amp;b<c>d&quot;e'f&nbsp;g">a&amp;b&lt;c&gt;d"e'f&nbsp;g</div>
<i></i>

-- the void elements, which open and never close
<area>
<base>
<basefont>
<bgsound>
<br>
<col>
<embed>
<frame>
<hr>
<img>
<input>
<keygen>
<link>
<meta>
<param>
<source>
<track>
<wbr>
-- and three that are not
<p></p>
<span></span>
<textarea></textarea>

-- the raw-text elements, whose children are not escaped
<iframe>a<b>&c</iframe>
<noembed>a<b>&c</noembed>
<noframes>a<b>&c</noframes>
<plaintext>a<b>&c</plaintext>
<script>a<b>&c</script>
<style>a<b>&c</style>
<xmp>a<b>&c</xmp>
-- and three the 2004 dumper also calls raw, which these rules do not
<noscript>a&lt;b&gt;&amp;c</noscript>
<title>a&lt;b&gt;&amp;c</title>
<textarea>a&lt;b&gt;&amp;c</textarea>

-- the tag name comes from the namespace, not from the prefix
<div></div>
<circle></circle>
<mi></mi>
<p:thing>t</p:thing>
<p:br></p:br>
<script>a&lt;b</script>
<br>

-- three attribute names are re-spelled from the namespace
<a xml:lang="en" xlink:href="u" z:k="v" plain="p"></a>

-- the other node kinds
<!--a<b>&c---->
<?t d>e>
<?t >
a&lt;b&gt;&amp;
<i></i>&amp;
string(0) ""

-- a doctype drops both identifiers here and keeps them under saveXml
<!DOCTYPE html>
<!DOCTYPE html><html><head><title>x</title></head><body></body></html>

-- a template writes its content, which a hand-built one has none of
childNodes=1 <template></template>

-- the file writer answers the byte count and writes the same bytes
int(70)
bool(true)
