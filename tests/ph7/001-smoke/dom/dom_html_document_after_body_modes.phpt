--TEST--
`</body>` only switches mode, and `</p>` opens the paragraph it cannot close
--FILE--
<?php
/* Two rules about what a document does after it stops saying anything.
 *
 * `</body>` and `</html>` do not POP the body: they only move the insertion
 * mode on, so the body -- and whatever the source left open inside it -- is
 * still where the whitespace and the stray content that follow land.  That is
 * why a file ending `</body>\n</html>` keeps its last newline INSIDE the body,
 * and why a `<div>` written after the close is still the body's child.  The
 * two modes past the body differ in one thing only: a comment written before
 * `</html>` belongs to the html element and one written after it belongs to
 * the document, beside the root.
 *
 * And `</p>` is `</br>`'s twin, the other end tag that OPENS an element.  With
 * no `<p>` in BUTTON scope it mints an empty one -- carrying none of the
 * attributes it was written with -- and closes it at once.  Button scope is
 * the ordinary scope question with `<button>` added to what stops it, so a
 * `</p>` inside a button mints a second paragraph rather than closing the one
 * outside, and a `<div>` inside a button does not close it either.  The modes
 * around the head answer end tags for themselves, so a `</p>` that never
 * reaches the body mints nothing at all. */
$cases = [
	'body newline'     => "<html><body>x</body>\n</html>",
	'body space tag'   => '<body>x</body>  <p>y',
	'body then text'   => '<body>x</body>zz',
	'body then div'    => '<body>x</body><div>d</div>',
	'body then body'   => '<body>x</body></body>y',
	'html then text'   => '<body>x</body></html>y',
	'html then space'  => "<html><body>x</body></html>\n",
	'open p kept'      => '<body><p>x</body> ',
	'open table kept'  => "<body><table></body>\n",
	'comment body'     => '<body>x</body><!--c-->',
	'comment html'     => '<body>x</body></html><!--c-->',
	'comment both'     => '<body>x</body><!--a--></html><!--b-->',
	'close p alone'    => '<body>a</p>b',
	'close p attrs'    => '<body></p class="q">x',
	'close p twice'    => '<body></p></p>x',
	'close p in b'     => '<body><b></p>x',
	'close p button'   => '<body><button></p>x</button>',
	'close p in p'     => '<body><p>a</p>b',
	'close p nested'   => '<body><p><button></p>x',
	'close p object'   => '<body><p><object></p>x',
	'close p cell'     => '<body><table><td></p>x',
	'close p select'   => '<body><select></p>x',
	'close p li'       => '<body><p><li>a</p>x',
	'close p head'     => '<head></p>x',
	'close p ahead'    => '<head></head></p>x',
	'close p alone 2'  => '</p>x',
	'close p frameset' => '<frameset></p>',
	'div in button'    => '<body><p><button><div>b',
	'h1 in button'     => '<body><p><button><h1>b',
];
foreach( $cases as $zWhat => $zSrc ){
	$oDoc = \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR);
	printf("%-17s %s\n", $zWhat, str_replace("\n", '\n', $oDoc->saveHtml()));
}
--EXPECT--
body newline      <html><head></head><body>x\n</body></html>
body space tag    <html><head></head><body>x  <p>y</p></body></html>
body then text    <html><head></head><body>xzz</body></html>
body then div     <html><head></head><body>x<div>d</div></body></html>
body then body    <html><head></head><body>xy</body></html>
html then text    <html><head></head><body>xy</body></html>
html then space   <html><head></head><body>x\n</body></html>
open p kept       <html><head></head><body><p>x </p></body></html>
open table kept   <html><head></head><body><table>\n</table></body></html>
comment body      <html><head></head><body>x</body><!--c--></html>
comment html      <html><head></head><body>x</body></html><!--c-->
comment both      <html><head></head><body>x</body><!--a--></html><!--b-->
close p alone     <html><head></head><body>a<p></p>b</body></html>
close p attrs     <html><head></head><body><p></p>x</body></html>
close p twice     <html><head></head><body><p></p><p></p>x</body></html>
close p in b      <html><head></head><body><b><p></p>x</b></body></html>
close p button    <html><head></head><body><button><p></p>x</button></body></html>
close p in p      <html><head></head><body><p>a</p>b</body></html>
close p nested    <html><head></head><body><p><button><p></p>x</button></p></body></html>
close p object    <html><head></head><body><p><object><p></p>x</object></p></body></html>
close p cell      <html><head></head><body><table><tbody><tr><td><p></p>x</td></tr></tbody></table></body></html>
close p select    <html><head></head><body><select><p></p>x</select></body></html>
close p li        <html><head></head><body><p></p><li>a<p></p>x</li></body></html>
close p head      <html><head></head><body>x</body></html>
close p ahead     <html><head></head><body>x</body></html>
close p alone 2   <html><head></head><body>x</body></html>
close p frameset  <html><head></head><frameset></frameset></html>
div in button     <html><head></head><body><p><button><div>b</div></button></p></body></html>
h1 in button      <html><head></head><body><p><button><h1>b</h1></button></p></body></html>
