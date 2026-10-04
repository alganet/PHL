--TEST--
A NUL is the replacement character in every state but the data one
--FILE--
<?php
/* The data state DROPS a NUL, which is where most documents meet one.  Every
 * other state replaces it with U+FFFD: the two text-only flavours, the comment
 * states, the attribute name and both of its value forms, and the tag and
 * doctype names -- where keeping the byte would have TRUNCATED the name at it,
 * because what libxml is handed is a C string. */
$aRaw = ['style','script','iframe','noembed','noframes','xmp','title','textarea','noscript','div'];
foreach( $aRaw as $zEl ){
	$zSrc = "<html><body><$zEl>a\0b</$zEl>";
	printf("%-9s %s\n", $zEl, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
$more = [
	'data'      => "<html><body>a\0b",
	'comment'   => "<html><body><!--a\0b-->",
	'attr'      => "<html><body><p id=\"a\0b\">",
	'tag-name'  => "<html><body><d\0iv>x",
	'style-run' => "<html><body><style>\0\0</style>",
	'rcdata-ref'=> "<html><head><title>a\0&amp;b</title>",
];
foreach( $more as $zName => $zSrc ){
	printf("%-11s %s\n", $zName, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
$last = [
	'attr-name' => "<html><body><p a\0b=v>",
	'doctype'   => "<!DOCTYPE h\0tml><html><body>x",
	'end-name'  => "<html><body><p>x</p\0>y",
	'bogus'     => "<html><body><?a\0b>",
];
foreach( $last as $zName => $zSrc ){
	printf("%-11s %s\n", $zName, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
?>
--EXPECT--
style     <html><head></head><body><style>a�b</style></body></html>
script    <html><head></head><body><script>a�b</script></body></html>
iframe    <html><head></head><body><iframe>a�b</iframe></body></html>
noembed   <html><head></head><body><noembed>a�b</noembed></body></html>
noframes  <html><head></head><body><noframes>a�b</noframes></body></html>
xmp       <html><head></head><body><xmp>a�b</xmp></body></html>
title     <html><head></head><body><title>a�b</title></body></html>
textarea  <html><head></head><body><textarea>a�b</textarea></body></html>
noscript  <html><head></head><body><noscript>ab</noscript></body></html>
div       <html><head></head><body><div>ab</div></body></html>
data        <html><head></head><body>ab</body></html>
comment     <html><head></head><body><!--a�b--></body></html>
attr        <html><head></head><body><p id="a�b"></p></body></html>
tag-name    <html><head></head><body><d�iv>x</d�iv></body></html>
style-run   <html><head></head><body><style>��</style></body></html>
rcdata-ref  <html><head><title>a�&amp;b</title></head><body></body></html>
attr-name   <html><head></head><body><p a�b="v"></p></body></html>
doctype     <!DOCTYPE h�tml><html><head></head><body>x</body></html>
end-name    <html><head></head><body><p>xy</p></body></html>
bogus       <html><head></head><body><!--?a�b--></body></html>
