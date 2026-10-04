--TEST--
Dom\HTMLDocument's two producers build an empty document and the HTML5 skeleton
--FILE--
<?php
function sh($l,$fn){ try { $v=$fn(); printf("%-32s => %s\n",$l,var_export($v,true)); }
  catch (\Throwable $e) { printf("%-32s !! %s: %s\n",$l,get_class($e),$e->getMessage()); } }

$d = Dom\HTMLDocument::createEmpty();
sh('class',            fn()=>get_class($d));
sh('childNodes',       fn()=>$d->childNodes->length);
sh('documentElement',  fn()=>$d->documentElement === null ? 'null' : 'node');
sh('charset',          fn()=>$d->charset);
sh('characterSet',     fn()=>$d->characterSet);
sh('inputEncoding',    fn()=>$d->inputEncoding);
sh('documentURI',      fn()=>$d->documentURI);
sh('saveHtml()',       fn()=>$d->saveHtml());

/* The encoding is a WHATWG LABEL, kept as the caller spelled it. */
foreach (['ISO-8859-1','utf8','l1','macintosh','UtF-8'] as $e) {
	sh("createEmpty('$e')", fn()=>Dom\HTMLDocument::createEmpty($e)->charset);
}
/* ...and a converter that is not a label is refused, as is an empty one. */
foreach (['','bogus','UCS-4','csUnicode',' utf-8 '] as $e) {
	sh('createEmpty '.var_export($e,true), fn()=>Dom\HTMLDocument::createEmpty($e)->charset);
}
sh('createEmpty(nul byte)', fn()=>Dom\HTMLDocument::createEmpty("UTF-8\0x")->charset);

/* An HTML document states none of the three XML declaration properties. */
foreach (['xmlVersion','xmlEncoding','xmlStandalone'] as $p) {
	printf("%-32s => %s\n","isset \$d->$p",var_export(isset($d->$p),true));
}
/* ...and has no CDATA sections to make. */
sh('createCDATASection', fn()=>$d->createCDATASection('x'));

echo "\n";
$impl = $d->implementation;
sh('impl class',        fn()=>get_class($impl));
sh('createHTMLDocument',fn()=>$impl->createHTMLDocument()->saveHtml());
sh('...with a title',   fn()=>$impl->createHTMLDocument('T')->saveHtml());
sh('...with an empty one', fn()=>$impl->createHTMLDocument('')->saveHtml());
sh('...title is escaped', fn()=>$impl->createHTMLDocument('<&>')->saveHtml());
sh('cHD charset',       fn()=>$impl->createHTMLDocument()->charset);
sh('cHD documentURI',   fn()=>$impl->createHTMLDocument()->documentURI);
sh('cHD doctype name',  fn()=>$impl->createHTMLDocument()->doctype->name);
sh('cHD root namespace',fn()=>$impl->createHTMLDocument()->documentElement->namespaceURI);
sh('called statically', fn()=>Dom\Implementation::createHTMLDocument('x'));

/* A node of another document is the only refusal the writers have. */
$o = Dom\HTMLDocument::createEmpty();
sh('saveHtml(foreign node)', fn()=>$d->saveHtml($o->createElement('b')));
sh('saveHtml(null) is whole', fn()=>$d->saveHtml(null) === $d->saveHtml());
?>
--EXPECT--
class                            => 'Dom\\HTMLDocument'
childNodes                       => 0
documentElement                  => 'null'
charset                          => 'UTF-8'
characterSet                     => 'UTF-8'
inputEncoding                    => 'UTF-8'
documentURI                      => 'about:blank'
saveHtml()                       => ''
createEmpty('ISO-8859-1')        => 'ISO-8859-1'
createEmpty('utf8')              => 'utf8'
createEmpty('l1')                => 'l1'
createEmpty('macintosh')         => 'macintosh'
createEmpty('UtF-8')             => 'UtF-8'
createEmpty ''                   !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must be a valid document encoding
createEmpty 'bogus'              !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must be a valid document encoding
createEmpty 'UCS-4'              !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must be a valid document encoding
createEmpty 'csUnicode'          !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must be a valid document encoding
createEmpty ' utf-8 '            !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must be a valid document encoding
createEmpty(nul byte)            !! ValueError: Dom\HTMLDocument::createEmpty(): Argument #1 ($encoding) must not contain any null bytes
isset $d->xmlVersion             => false
isset $d->xmlEncoding            => false
isset $d->xmlStandalone          => false
createCDATASection               !! DOMException: This operation is not supported for HTML documents

impl class                       => 'Dom\\Implementation'
createHTMLDocument               => '<!DOCTYPE html><html><head></head><body></body></html>'
...with a title                  => '<!DOCTYPE html><html><head><title>T</title></head><body></body></html>'
...with an empty one             => '<!DOCTYPE html><html><head><title></title></head><body></body></html>'
...title is escaped              => '<!DOCTYPE html><html><head><title>&lt;&amp;&gt;</title></head><body></body></html>'
cHD charset                      => 'UTF-8'
cHD documentURI                  => 'about:blank'
cHD doctype name                 => 'html'
cHD root namespace               => 'http://www.w3.org/1999/xhtml'
called statically                !! Error: Non-static method Dom\Implementation::createHTMLDocument() cannot be called statically
saveHtml(foreign node)           !! DOMException: Wrong Document Error
saveHtml(null) is whole          => true
