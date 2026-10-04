--TEST--
A `<plaintext>` takes the rest of the document as one text node
--FILE--
<?php
/* `<plaintext>` switches the tokenizer into a state it never leaves: there is
 * no end tag, no markup and no character reference after it, so every
 * remaining byte of the source -- `</plaintext>` and `</html>` included -- is
 * that one element's text.  Only a NUL is not itself, and becomes U+FFFD.  In
 * the body it is an ordinary start tag otherwise: it closes an open `<p>`,
 * clears the frameset flag, leaves a table the way any other content does, and
 * inside `<svg>` it is an SVG element that arms nothing. */
$cases = [
	'basic'        => '<html><body><plaintext>a<b>c</plaintext>d',
	'markup'       => '<plaintext><p>x</p><div>y',
	'refs'         => '<plaintext>a&amp;b&#65;c',
	'nul'          => "<plaintext>a\0b",
	'in-head'      => '<html><head><plaintext>a<b>c',
	'in-p'         => '<html><body><p>q<plaintext>a</p>b',
	'in-table'     => '<html><body><table><plaintext>a</table>b',
	'in-select'    => '<html><body><select><plaintext>a</select>b',
	'closes-p'     => '<html><body><p>q</p><plaintext>a',
	'upper'        => '<PLAINTEXT>a<B>c',
	'self-close'   => '<plaintext/>a<b>c',
	'attrs'        => '<plaintext id=x>a<b>c',
	'end-tag'      => '</plaintext>a<b>c',
	'empty'        => '<html><body><plaintext>',
	'in-svg'       => '<html><body><svg><plaintext>a<b>c',
	'frameset-ok'  => '<html><plaintext></plaintext><frameset><frame>',
];
foreach( $cases as $zName => $zSrc ){
	printf("%-13s %s\n", $zName, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
?>
--EXPECT--
basic         <html><head></head><body><plaintext>a<b>c</plaintext>d</plaintext></body></html>
markup        <html><head></head><body><plaintext><p>x</p><div>y</plaintext></body></html>
refs          <html><head></head><body><plaintext>a&amp;b&#65;c</plaintext></body></html>
nul           <html><head></head><body><plaintext>a�b</plaintext></body></html>
in-head       <html><head></head><body><plaintext>a<b>c</plaintext></body></html>
in-p          <html><head></head><body><p>q</p><plaintext>a</p>b</plaintext></body></html>
in-table      <html><head></head><body><plaintext>a</table>b</plaintext><table></table></body></html>
in-select     <html><head></head><body><select><plaintext>a</select>b</plaintext></select></body></html>
closes-p      <html><head></head><body><p>q</p><plaintext>a</plaintext></body></html>
upper         <html><head></head><body><plaintext>a<B>c</plaintext></body></html>
self-close    <html><head></head><body><plaintext>a<b>c</plaintext></body></html>
attrs         <html><head></head><body><plaintext id="x">a<b>c</plaintext></body></html>
end-tag       <html><head></head><body>a<b>c</b></body></html>
empty         <html><head></head><body><plaintext></plaintext></body></html>
in-svg        <html><head></head><body><svg><plaintext>a</plaintext></svg><b>c</b></body></html>
frameset-ok   <html><head></head><body><plaintext></plaintext><frameset><frame></plaintext></body></html>
