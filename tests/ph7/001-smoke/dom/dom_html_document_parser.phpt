--TEST--
Dom\HTMLDocument's two parsing producers build the HTML5 tree, not libxml's
--FILE--
<?php
/* Every diagnostic is printed through a handler so the case says WHAT was
 * reported without saying where from: the tree these two doors build is what
 * is under test, and a source path is not part of it. */
set_error_handler(function($n,$s){ echo "  ! $s\n"; return true; });

function h5show($l,$fn){ try { $v=$fn(); printf("%-30s => %s\n",$l,var_export($v,true)); }
  catch (\Throwable $e) { printf("%-30s !! %s: %s\n",$l,get_class($e),$e->getMessage()); } }

/* The shapes libxml's HTML parser cannot build: an implied `<head>` for a
 * document that states none, and an implied `<tbody>` around a bare row. */
foreach ([
	'bare'      => '<p>hi',
	'doctype'   => '<!DOCTYPE html><p>hi',
	'empty'     => '',
	'text'      => 'just text',
	'table'     => '<!DOCTYPE html><table><tr><td>x',
	'rowgroup'  => '<!DOCTYPE html><table><thead><tr><th>h</table>',
	'cells'     => '<table><tr><td>a<td>b<tr><td>c</table>',
	'comment'   => '<!DOCTYPE html><!-- c --><p>x',
	'meta'      => '<!DOCTYPE html><meta charset="utf-8"><p>x',
	'head'      => '<title>T</title><p>b',
	'headlate'  => '<body><p>a</p><link rel=x>',
	'nested'    => '<div><p>a<p>b</div>',
	'li'        => '<ul><li>a<li>b</ul>',
	'void'      => '<p>a<br>b<hr>c',
	'rawtext'   => '<style>a > b { }</style><p>x',
	'rcdata'    => '<title>a &amp; b &lt; c</title>',
	'script'    => '<script>if (a<b) { }</script><p>x',
	'attrs'     => '<p id=one CLASS="two" data-x data-x=dup>t',
	'entity'    => '<p>caf&eacute; &amp; &#233; &#x41; &nosuch;',
	'afterbody' => '<p>x</p></body><!--c-->',
	'bogus'     => '<!bogus><p>x',
	'pi'        => '<?pi><p>x',
] as $l => $src) {
	h5show($l, fn()=>Dom\HTMLDocument::createFromString($src)->saveHtml());
}

/* The four option bits that are spellable, and the refusal that names them. */
h5show('NOIMPLIED',  fn()=>Dom\HTMLDocument::createFromString('<p>hi',LIBXML_HTML_NOIMPLIED)->saveHtml());
h5show('NOERROR',    fn()=>Dom\HTMLDocument::createFromString('<p>hi',LIBXML_NOERROR)->saveHtml());
h5show('COMPACT',    fn()=>Dom\HTMLDocument::createFromString('<!DOCTYPE html><p>x',LIBXML_COMPACT)->saveHtml());
h5show('NO_DEFAULT_NS', fn()=>Dom\HTMLDocument::createFromString('<!DOCTYPE html><p>x',Dom\HTML_NO_DEFAULT_NS)->documentElement->namespaceURI);
h5show('default ns', fn()=>Dom\HTMLDocument::createFromString('<!DOCTYPE html><p>x')->documentElement->namespaceURI);
h5show('NODEFDTD',   fn()=>Dom\HTMLDocument::createFromString('<p>x',LIBXML_HTML_NODEFDTD)->saveHtml());
h5show('negative',   fn()=>Dom\HTMLDocument::createFromString('<p>x',-1)->saveHtml());

/* The document a parse produces answers the same three questions the two
 * builders' documents do. */
$d = Dom\HTMLDocument::createFromString('<!DOCTYPE html><p>x');
h5show('class',        fn()=>get_class($d));
h5show('documentURI',  fn()=>$d->documentURI);
h5show('characterSet', fn()=>$d->characterSet);
h5show('doctype',      fn()=>get_class($d->doctype));
h5show('doctype name', fn()=>$d->doctype->name);

/* The encoding argument is screened against the same label table
 * `createEmpty()` screens its own against. */
foreach (['UTF-8','bogus',"utf\0-8"] as $e) {
	h5show('enc '.var_export($e,true),
		fn()=>Dom\HTMLDocument::createFromString('<p>x',0,$e)->characterSet);
}

/* The file door: the same tree, php's own stream refusal for a path that will
 * not open, and the one ValueError on this class that names no method. */
$f = tempnam(sys_get_temp_dir(),'phl');
file_put_contents($f,'<!DOCTYPE html><table><tr><td>f');
h5show('file',         fn()=>Dom\HTMLDocument::createFromFile($f)->saveHtml());
unlink($f);
h5show('file missing', fn()=>Dom\HTMLDocument::createFromFile('/nonexistent-dir/x.html')->saveHtml());
h5show('file empty',   fn()=>Dom\HTMLDocument::createFromFile('')->saveHtml());
h5show('file nul',     fn()=>Dom\HTMLDocument::createFromFile("a\0b")->saveHtml());
restore_error_handler();
--EXPECT--
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
bare                           => '<html><head></head><body><p>hi</p></body></html>'
doctype                        => '<!DOCTYPE html><html><head></head><body><p>hi</p></body></html>'
empty                          => '<html><head></head><body></body></html>'
text                           => '<html><head></head><body>just text</body></html>'
table                          => '<!DOCTYPE html><html><head></head><body><table><tbody><tr><td>x</td></tr></tbody></table></body></html>'
rowgroup                       => '<!DOCTYPE html><html><head></head><body><table><thead><tr><th>h</th></tr></thead></table></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-6
cells                          => '<html><head></head><body><table><tbody><tr><td>a</td><td>b</td></tr><tr><td>c</td></tr></tbody></table></body></html>'
comment                        => '<!DOCTYPE html><!-- c --><html><head></head><body><p>x</p></body></html>'
meta                           => '<!DOCTYPE html><html><head><meta charset="utf-8"></head><body><p>x</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-6
head                           => '<html><head><title>T</title></head><body><p>b</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-5
headlate                       => '<html><head></head><body><p>a</p><link rel="x"></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-4
nested                         => '<html><head></head><body><div><p>a</p><p>b</p></div></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-3
li                             => '<html><head></head><body><ul><li>a</li><li>b</li></ul></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
void                           => '<html><head></head><body><p>a<br>b</p><hr>c</body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-6
rawtext                        => '<html><head><style>a > b { }</style></head><body><p>x</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-6
rcdata                         => '<html><head><title>a &amp; b &lt; c</title></head><body></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2-7
script                         => '<html><head><script>if (a<b) { }</script></head><body><p>x</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
attrs                          => '<html><head></head><body><p id="one" class="two" data-x="">t</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tokenizer error unknown-named-character-reference in Entity, line: 1, column: 43
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
entity                         => '<html><head></head><body><p>café &amp; é A &amp;nosuch;</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
afterbody                      => '<html><head></head><body><p>x</p></body><!--c--></html>'
  ! Dom\HTMLDocument::createFromString(): tokenizer error incorrectly-opened-comment in Entity, line: 1, column: 3
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 10
bogus                          => '<!--bogus--><html><head></head><body><p>x</p></body></html>'
  ! Dom\HTMLDocument::createFromString(): tokenizer error unexpected-question-mark-instead-of-tag-name in Entity, line: 1, column: 2
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 7
pi                             => '<!--?pi--><html><head></head><body><p>x</p></body></html>'
NOIMPLIED                      => '<p>hi</p>'
NOERROR                        => '<html><head></head><body><p>hi</p></body></html>'
COMPACT                        => '<!DOCTYPE html><html><head></head><body><p>x</p></body></html>'
NO_DEFAULT_NS                  => NULL
default ns                     => 'http://www.w3.org/1999/xhtml'
NODEFDTD                       !! ValueError: Dom\HTMLDocument::createFromString(): Argument #2 ($options) contains invalid flags (allowed flags: LIBXML_NOERROR, LIBXML_COMPACT, LIBXML_HTML_NOIMPLIED, Dom\HTML_NO_DEFAULT_NS)
negative                       !! ValueError: Dom\HTMLDocument::createFromString(): Argument #2 ($options) contains invalid flags (allowed flags: LIBXML_NOERROR, LIBXML_COMPACT, LIBXML_HTML_NOIMPLIED, Dom\HTML_NO_DEFAULT_NS)
class                          => 'Dom\\HTMLDocument'
documentURI                    => 'about:blank'
characterSet                   => 'UTF-8'
doctype                        => 'Dom\\DocumentType'
doctype name                   => 'html'
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
enc 'UTF-8'                    => 'UTF-8'
enc 'bogus'                    !! ValueError: Dom\HTMLDocument::createFromString(): Argument #3 ($overrideEncoding) must be a valid document encoding
enc 'utf' . "\0" . '-8'        !! ValueError: Dom\HTMLDocument::createFromString(): Argument #3 ($overrideEncoding) must not contain any null bytes
file                           => '<!DOCTYPE html><html><head></head><body><table><tbody><tr><td>f</td></tr></tbody></table></body></html>'
  ! Dom\HTMLDocument::createFromFile(/nonexistent-dir/x.html): Failed to open stream: No such file or directory
file missing                   !! Exception: Cannot open file '/nonexistent-dir/x.html'
file empty                     !! ValueError: Path must not be empty
file nul                       !! ValueError: Dom\HTMLDocument::createFromFile(): Argument #1 ($path) must not contain any null bytes
