--TEST--
A `<noscript>` in the head parses in a mode of its own
--FILE--
<?php
/* This parser, like php's, runs with scripting DISABLED, so a `<noscript>`
 * holds markup rather than raw text.  In the head it gets an insertion mode of
 * its own: six head elements stay inside it, `<head>` and a nested `<noscript>`
 * are dropped, an `<html>` merges its attributes, whitespace and comments are
 * kept, and anything else -- a `<title>`, a `<div>`, any content at all --
 * closes it and is re-run in `in head`.  In the body it is simply an ordinary
 * element. */
$head = [
	'basefont','bgsound','link','meta','noframes','style',
	'base','script','template','title',
];
foreach( $head as $zEl ){
	$zSrc = "<html><head><noscript><$zEl>q</$zEl></noscript>r";
	printf("%-10s %s\n", $zEl, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
$cases = [
	'space'       => "<html><head><noscript>   </noscript>",
	'comment'     => '<html><head><noscript><!--c--></noscript>',
	'text'        => '<html><head><noscript>abc</noscript>',
	'div'         => '<html><head><noscript><div>d</div></noscript>',
	'br'          => '<html><head><noscript><br>a</noscript>',
	'nested'      => '<html><head><noscript><noscript>q</noscript>',
	'head'        => '<html><head><noscript><head>',
	'html-attr'   => '<html><head><noscript><html lang=en>',
	'end-head'    => '<html><head><noscript></head>x',
	'end-other'   => '<html><head><noscript></p>x',
	'eof'         => '<html><head><noscript><link>',
	'close'       => '<html><head><noscript></noscript><title>t</title>',
	'style-close' => '<html><head><noscript><style>i</style></noscript>',
	'implied'     => '<noscript><link></noscript>x',
	'in-body'     => '<html><body><noscript><meta charset=utf-8>x</noscript>y',
	'body-markup' => '<html><body><noscript><b>x</b></noscript>',
	'body-frame'  => '<html><body>t<noscript><frameset>',
];
foreach( $cases as $zName => $zSrc ){
	printf("%-11s %s\n", $zName, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
?>
--EXPECT--
basefont   <html><head><noscript><basefont></noscript></head><body>qr</body></html>
bgsound    <html><head><noscript><bgsound></noscript></head><body>qr</body></html>
link       <html><head><noscript><link></noscript></head><body>qr</body></html>
meta       <html><head><noscript><meta></noscript></head><body>qr</body></html>
noframes   <html><head><noscript><noframes>q</noframes></noscript></head><body>r</body></html>
style      <html><head><noscript><style>q</style></noscript></head><body>r</body></html>
base       <html><head><noscript></noscript><base></head><body>qr</body></html>
script     <html><head><noscript></noscript><script>q</script></head><body>r</body></html>
template   <html><head><noscript></noscript><template>q</template></head><body>r</body></html>
title      <html><head><noscript></noscript><title>q</title></head><body>r</body></html>
space       <html><head><noscript>   </noscript></head><body></body></html>
comment     <html><head><noscript><!--c--></noscript></head><body></body></html>
text        <html><head><noscript></noscript></head><body>abc</body></html>
div         <html><head><noscript></noscript></head><body><div>d</div></body></html>
br          <html><head><noscript></noscript></head><body><br>a</body></html>
nested      <html><head><noscript></noscript></head><body>q</body></html>
head        <html><head><noscript></noscript></head><body></body></html>
html-attr   <html lang="en"><head><noscript></noscript></head><body></body></html>
end-head    <html><head><noscript></noscript></head><body>x</body></html>
end-other   <html><head><noscript></noscript></head><body>x</body></html>
eof         <html><head><noscript><link></noscript></head><body></body></html>
close       <html><head><noscript></noscript><title>t</title></head><body></body></html>
style-close <html><head><noscript><style>i</style></noscript></head><body></body></html>
implied     <html><head><noscript><link></noscript></head><body>x</body></html>
in-body     <html><head></head><body><noscript><meta charset="utf-8">x</noscript>y</body></html>
body-markup <html><head></head><body><noscript><b>x</b></noscript></body></html>
body-frame  <html><head></head><body>t<noscript></noscript></body></html>
