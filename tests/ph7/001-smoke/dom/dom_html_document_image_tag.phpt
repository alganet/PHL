--TEST--
An `<image>` start tag is an `<img>`
--FILE--
<?php
/* `<image>` names no element: the start tag is renamed to `img`, so what lands
 * is a VOID element carrying the token's attributes rather than an open one
 * swallowing the rest of the body.  The rename is an HTML rule -- an `<image>`
 * inside `<svg>` or `<math>` is a foreign element and keeps its name -- and php
 * reaches it by re-running the renamed token, which a table's foster path has
 * no re-run to hand, so an `<image>` written straight into a table is dropped
 * where the `<img>` it would have become is fostered out. */
$cases = [
	'plain'      => '<html><body><image>after',
	'attrs'      => '<html><body><image src=x alt=y>',
	'upper'      => '<html><body><IMaGe src=x>',
	'in-head'    => '<html><head><image src=x>',
	'implied'    => '<image>a',
	'end-tag'    => '<html><body><img></image>z',
	'end-only'   => '<html><body></image>z',
	'self-close' => '<html><body><image/>z',
	'in-table'   => '<html><body><table><image></table>',
	'in-select'  => '<html><body><select><image>a</select>',
	'frameset'   => '<html><body><image><frameset><frame>',
	'in-svg'     => '<html><body><svg><image href=x></svg>',
	'in-math'    => '<html><body><math><image></math>',
	'in-p'       => '<html><body><p>a<image>b</p>',
];
foreach( $cases as $zName => $zSrc ){
	printf("%-11s %s\n", $zName, \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR)->saveHtml());
}
?>
--EXPECT--
plain       <html><head></head><body><img>after</body></html>
attrs       <html><head></head><body><img src="x" alt="y"></body></html>
upper       <html><head></head><body><img src="x"></body></html>
in-head     <html><head></head><body><img src="x"></body></html>
implied     <html><head></head><body><img>a</body></html>
end-tag     <html><head></head><body><img>z</body></html>
end-only    <html><head></head><body>z</body></html>
self-close  <html><head></head><body><img>z</body></html>
in-table    <html><head></head><body><table></table></body></html>
in-select   <html><head></head><body><select><img>a</select></body></html>
frameset    <html><head></head><body><img></body></html>
in-svg      <html><head></head><body><svg><image href="x"></image></svg></body></html>
in-math     <html><head></head><body><math><image></image></math></body></html>
in-p        <html><head></head><body><p>a<img>b</p></body></html>
