--TEST--
A frameset document is parsed into its own insertion modes
--FILE--
<?php
/* A `<frameset>` document has no body at all: the parser mints none, and one
 * written where a body already exists REPLACES it -- but only while the
 * frameset-ok flag still stands.  Any non-whitespace character, an explicit
 * `<body>`, and a closed list of start tags clear that flag, after which the
 * frameset is dropped and so is every `<frame>` under it.  Inside a frameset
 * only frames, nested framesets, `<noframes>`, comments and whitespace are
 * kept; past the outermost `</frameset>` the same holds, and past `</html>`
 * a comment belongs to the document rather than to the root. */
$cases = [
	/* The frameset document itself, in each of the ways it can be spelled. */
	'bare'           => '<frameset><frame></frameset>',
	'after-head'     => '<html><head></head><frameset><frame></frameset>',
	'attrs'          => '<html><frameset cols="1,2" rows=3><frame src=a name=n></frameset>',
	'nested'         => '<html><frameset><frameset><frame></frameset><frame></frameset>',
	/* What `in frameset` keeps, and what it drops. */
	'fs-text'        => '<html><frameset>abc<frame></frameset>',
	'fs-space'       => "<html><frameset>\n <frame></frameset>",
	'fs-mixed'       => "<html><frameset> a\tb <frame></frameset>",
	'fs-p'           => '<html><frameset><p>x</p><frame></frameset>',
	'fs-comment'     => '<html><frameset><!--c--><frame></frameset>',
	'fs-noframes'    => '<html><frameset><noframes>raw<p></noframes><frame></frameset>',
	'fs-body'        => '<html><frameset><body>x</body><frame></frameset>',
	'fs-meta'        => '<html><frameset><meta><frame></frameset>',
	'fs-end-extra'   => '<html><frameset><frame></frameset></frameset>',
	'fs-end-frame'   => '<html><frameset><frame></frame><frame></frameset>',
	'fs-eof-open'    => '<html><frameset><frame>',
	/* ...and the two modes after it. */
	'af-text'        => '<html><frameset><frame></frameset>tail',
	'af-space'       => "<html><frameset><frame></frameset> \n",
	'af-p'           => '<html><frameset><frame></frameset><p>x',
	'af-comment'     => '<html><frameset><frame></frameset><!--c-->',
	'af-noframes'    => '<html><frameset><frame></frameset><noframes>r</noframes>',
	'af-html-attr'   => '<html><frameset><frame></frameset><html lang=en>',
	'aaf-comment'    => '<html><frameset><frame></frameset></html><!--c-->',
	'aaf-p'          => '<html><frameset><frame></frameset></html><p>x',
	/* The frameset-ok flag.  Everything here is `<X><frameset><frame>`, and
	 * the question each row asks is whether the frameset survived. */
	'ok-space'       => "<html>  \n<frameset><frame></frameset>",
	'ok-p'           => '<html><p><frameset><frame></frameset>',
	'ok-b'           => '<html><b><frameset><frame></frameset>',
	'ok-form'        => '<html><form><frameset><frame></frameset>',
	'ok-h1'          => '<html><h1><frameset><frame></frameset>',
	'ok-ul'          => '<html><ul><frameset><frame></frameset>',
	'ok-nobr'        => '<html><nobr><frameset><frame></frameset>',
	'ok-option'      => '<html><option><frameset><frame></frameset>',
	'ok-caption'     => '<html><caption><frameset><frame></frameset>',
	'ok-meta'        => '<html><meta><frameset><frame></frameset>',
	'ok-script'      => '<html><script></script><frameset><frame></frameset>',
	'ok-comment'     => '<html><!--c--><frameset><frame></frameset>',
	'ok-noembed'     => '<html><noembed></noembed><frameset><frame></frameset>',
	'ok-input-hid'   => '<html><input type=hidden><frameset><frame></frameset>',
	'no-text'        => '<html>x<frameset><frame></frameset>',
	'no-body'        => '<html><body><frameset><frame></frameset>',
	'no-pre'         => '<html><pre><frameset><frame></frameset>',
	'no-listing'     => '<html><listing><frameset><frame></frameset>',
	'no-li'          => '<html><li><frameset><frame></frameset>',
	'no-dd'          => '<html><dd><frameset><frame></frameset>',
	'no-dt'          => '<html><dt><frameset><frame></frameset>',
	'no-button'      => '<html><button><frameset><frame></frameset>',
	'no-table'       => '<html><table><frameset><frame></frameset>',
	'no-hr'          => '<html><hr><frameset><frame></frameset>',
	'no-select'      => '<html><select><frameset><frame></frameset>',
	'no-input'       => '<html><input><frameset><frame></frameset>',
	/* php compares the type BYTEWISE, where the spec asks for an ASCII
	 * case-insensitive match, so a capitalised hidden is content. */
	'no-input-HID'   => '<html><input type=HiDdEn><frameset><frame></frameset>',
	'no-input-text'  => '<html><input type=text><frameset><frame></frameset>',
	'no-br'          => '<html><br><frameset><frame></frameset>',
	'no-img'         => '<html><img><frameset><frame></frameset>',
	'no-keygen'      => '<html><keygen><frameset><frame></frameset>',
	'no-embed'       => '<html><embed><frameset><frame></frameset>',
	'no-area'        => '<html><area><frameset><frame></frameset>',
	'no-wbr'         => '<html><wbr><frameset><frame></frameset>',
	'no-marquee'     => '<html><marquee><frameset><frame></frameset>',
	'no-object'      => '<html><object><frameset><frame></frameset>',
	'no-applet'      => '<html><applet><frameset><frame></frameset>',
	/* A frameset inside foreign content is an ordinary foreign element. */
	'svg'            => '<html><svg><frameset><frame></frameset>',
	'math'           => '<html><math><frameset><frame></frameset>',
	/* A `<frame>` outside a frameset is dropped wherever it is written. */
	'frame-in-body'  => '<html><body>x<frame>y',
	'frame-in-p'     => '<html><p>a<frame>b',
	'frame-in-table' => '<html><table><frame></table>',
];
foreach( $cases as $zName => $zSrc ){
	$oDoc = \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR);
	printf("%-15s %s\n", $zName, $oDoc->saveHtml());
}
/* The document's `body` property answers the frameset, and nothing it holds
 * is reachable as a body element. */
$oDoc = \Dom\HTMLDocument::createFromString('<frameset><frame></frameset>', LIBXML_NOERROR);
var_dump($oDoc->body->tagName);
var_dump($oDoc->getElementsByTagName('body')->length);
var_dump($oDoc->getElementsByTagName('frame')->length);
?>
--EXPECT--
bare            <html><head></head><frameset><frame></frameset></html>
after-head      <html><head></head><frameset><frame></frameset></html>
attrs           <html><head></head><frameset cols="1,2" rows="3"><frame src="a" name="n"></frameset></html>
nested          <html><head></head><frameset><frameset><frame></frameset><frame></frameset></html>
fs-text         <html><head></head><frameset><frame></frameset></html>
fs-space        <html><head></head><frameset>
 <frame></frameset></html>
fs-mixed        <html><head></head><frameset> 	 <frame></frameset></html>
fs-p            <html><head></head><frameset><frame></frameset></html>
fs-comment      <html><head></head><frameset><!--c--><frame></frameset></html>
fs-noframes     <html><head></head><frameset><noframes>raw<p></noframes><frame></frameset></html>
fs-body         <html><head></head><frameset><frame></frameset></html>
fs-meta         <html><head></head><frameset><frame></frameset></html>
fs-end-extra    <html><head></head><frameset><frame></frameset></html>
fs-end-frame    <html><head></head><frameset><frame><frame></frameset></html>
fs-eof-open     <html><head></head><frameset><frame></frameset></html>
af-text         <html><head></head><frameset><frame></frameset></html>
af-space        <html><head></head><frameset><frame></frameset> 
</html>
af-p            <html><head></head><frameset><frame></frameset></html>
af-comment      <html><head></head><frameset><frame></frameset><!--c--></html>
af-noframes     <html><head></head><frameset><frame></frameset><noframes>r</noframes></html>
af-html-attr    <html lang="en"><head></head><frameset><frame></frameset></html>
aaf-comment     <html><head></head><frameset><frame></frameset></html><!--c-->
aaf-p           <html><head></head><frameset><frame></frameset></html>
ok-space        <html><head></head><frameset><frame></frameset></html>
ok-p            <html><head></head><frameset><frame></frameset></html>
ok-b            <html><head></head><frameset><frame></frameset></html>
ok-form         <html><head></head><frameset><frame></frameset></html>
ok-h1           <html><head></head><frameset><frame></frameset></html>
ok-ul           <html><head></head><frameset><frame></frameset></html>
ok-nobr         <html><head></head><frameset><frame></frameset></html>
ok-option       <html><head></head><frameset><frame></frameset></html>
ok-caption      <html><head></head><frameset><frame></frameset></html>
ok-meta         <html><head><meta></head><frameset><frame></frameset></html>
ok-script       <html><head><script></script></head><frameset><frame></frameset></html>
ok-comment      <html><!--c--><head></head><frameset><frame></frameset></html>
ok-noembed      <html><head></head><frameset><frame></frameset></html>
ok-input-hid    <html><head></head><frameset><frame></frameset></html>
no-text         <html><head></head><body>x</body></html>
no-body         <html><head></head><body></body></html>
no-pre          <html><head></head><body><pre></pre></body></html>
no-listing      <html><head></head><body><listing></listing></body></html>
no-li           <html><head></head><body><li></li></body></html>
no-dd           <html><head></head><body><dd></dd></body></html>
no-dt           <html><head></head><body><dt></dt></body></html>
no-button       <html><head></head><body><button></button></body></html>
no-table        <html><head></head><body><table></table></body></html>
no-hr           <html><head></head><body><hr></body></html>
no-select       <html><head></head><body><select></select></body></html>
no-input        <html><head></head><body><input></body></html>
no-input-HID    <html><head></head><body><input type="HiDdEn"></body></html>
no-input-text   <html><head></head><body><input type="text"></body></html>
no-br           <html><head></head><body><br></body></html>
no-img          <html><head></head><body><img></body></html>
no-keygen       <html><head></head><body><keygen></body></html>
no-embed        <html><head></head><body><embed></body></html>
no-area         <html><head></head><body><area></body></html>
no-wbr          <html><head></head><body><wbr></body></html>
no-marquee      <html><head></head><body><marquee></marquee></body></html>
no-object       <html><head></head><body><object></object></body></html>
no-applet       <html><head></head><body><applet></applet></body></html>
svg             <html><head></head><body><svg><frameset><frame></frame></frameset></svg></body></html>
math            <html><head></head><body><math><frameset><frame></frame></frameset></math></body></html>
frame-in-body   <html><head></head><body>xy</body></html>
frame-in-p      <html><head></head><body><p>ab</p></body></html>
frame-in-table  <html><head></head><body><table></table></body></html>
string(8) "FRAMESET"
int(0)
int(1)
