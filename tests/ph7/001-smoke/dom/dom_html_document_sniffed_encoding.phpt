--TEST--
A parse with no override sniffs its encoding from a BOM or a meta prescan
--FILE--
<?php
$enc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov)->charset;
$show = function ($l, $v) { printf("%-34s => %s\n", $l, $v); };

/* A byte-order mark decides alone and is not part of the document. */
$d = Dom\HTMLDocument::createFromString("\xEF\xBB\xBF<p>x</p>", LIBXML_NOERROR);
$show('utf-8 bom charset', $d->charset);
$show('utf-8 bom text', bin2hex($d->body->textContent));
$show('utf-8 bom saveHtml', bin2hex($d->saveHtml()));
$show('bom beats meta', $enc("\xEF\xBB\xBF<meta charset=koi8-r>"));
$show('second bom is text', bin2hex(Dom\HTMLDocument::createFromString("\xEF\xBB\xBF\xEF\xBB\xBFx", LIBXML_NOERROR)->body->textContent));
$show('utf-16be bom', $enc("\xFE\xFF\0<\0p\0>"));
$show('utf-16le bom', $enc("\xFF\xFE<\0p\0>\0"));
$show('short bom', $enc("\xEF\xBB") . ' ' . $enc("\xFE"));
$show('override beats bom', $enc("\xFF\xFE<\0p\0>\0", 'koi8-r'));
$show('override beats meta', $enc("<meta charset=koi8-r>", 'utf-8'));

/* The meta prescan: the NAME of the label, whatever the label's spelling. */
$cases = [
	'plain' => '<p>x</p>',
	'charset' => '<meta charset=koi8-r><p>x</p>',
	'charset quoted' => '<meta charset="KOI8-R">',
	'charset single-quoted' => "<meta charset='koi8-r'>",
	'charset spaced' => '<meta charset = "  koi8-r  ">',
	'charset label' => '<meta charset=latin1>',
	'charset alias' => '<meta charset=cp866>',
	'charset utf-16 stays' => '<meta charset=utf-16>',
	'charset x-user-defined stays' => '<meta charset=x-user-defined>',
	'charset replacement' => '<meta charset=hz-gb-2312>',
	'charset unknown' => '<meta charset=bogus>',
	'charset empty then next' => '<meta charset=""><meta charset=koi8-r>',
	'charset in body' => '<body><p>x</p><meta charset=koi8-r>',
	'charset in script' => '<script>"<meta charset=koi8-r>"</script>',
	'charset in title' => '<title><meta charset=koi8-r></title>',
	'first meta wins' => '<meta charset=koi8-r><meta charset=windows-1251>',
	'http-equiv' => '<meta http-equiv="Content-Type" content="text/html; charset=koi8-r">',
	'http-equiv reversed' => '<meta content="text/html; charset=koi8-r" http-equiv=content-type>',
	'http-equiv any case' => '<META HTTP-EQUIV=content-type CONTENT="text/html;CHARSET=\'windows-1251\'">',
	'http-equiv other' => '<meta http-equiv=refresh content="charset=koi8-r">',
	'content alone' => '<meta content="text/html; charset=koi8-r">',
	'content unquoted' => '<meta http-equiv=content-type content=charset=koi8-r>',
	'content charset spaced' => '<meta http-equiv=content-type content="charset = \'koi8-r\'">',
	'content charset to semicolon' => '<meta http-equiv=content-type content="charset=koi8-r;x">',
	'content charset in a word' => '<meta http-equiv=content-type content="xcharset=koi8-r">',
	'content unclosed quote' => '<meta http-equiv=content-type content="charset=\'koi8-r">',
	'content meets a quote' => '<meta http-equiv=content-type content="charset=koi8-r\'">',
	'content second charset' => '<meta http-equiv=content-type content="charset=bogus; charset=koi8-r">',
	'content first then charset' => '<meta content="charset=windows-1251" charset=koi8-r>',
	'charset first then content' => '<meta charset=koi8-r content="charset=windows-1251" http-equiv=content-type>',
	'duplicate charset' => '<meta charset=koi8-r charset=windows-1251>',
	'duplicate http-equiv' => '<meta http-equiv=x http-equiv=content-type content="charset=koi8-r">',
	'seven letters compare' => '<meta charsets=koi8-r>',
	'unquoted value to space' => '<meta charset=koi8-r foo>',
	'unquoted value keeps slash' => '<meta charset=koi8-r/>',
	'slash after meta' => '<meta/charset=koi8-r>',
	'newline after meta' => "<meta\ncharset=koi8-r>",
	'form feed after meta' => "<meta\fcharset=koi8-r>",
	'vertical tab after meta' => "<meta\x0bcharset=koi8-r>",
	'nothing after meta' => '<metacharset=koi8-r>',
	'bare meta eats next tag' => '<meta><meta charset=koi8-r>',
	'unclosed at end' => '<meta charset="koi8-r"',
	'unquoted unclosed at end' => '<meta charset=koi8-r',
	'comment' => '<!-- <meta charset=koi8-r> --><meta charset=windows-1251>',
	'comment closes at first' => '<!--><meta charset=koi8-r>',
	'comment never closes' => '<!-- <meta charset=koi8-r>',
	'bogus comment' => '<!-x><meta charset=koi8-r>',
	'processing instruction' => '<?php <meta charset=koi8-r> ?><meta charset=windows-1251>',
	'end tag attributes' => '</p charset=koi8-r>',
	'other tag attributes' => '<p charset=koi8-r>',
	'meta inside an attribute' => '<a b="<meta charset=koi8-r>">',
	'greater-than inside an attribute' => '<a b=">"><meta charset=koi8-r>',
];
foreach ($cases as $l => $src) $show($l, $enc($src));

/* Only the first 1024 bytes are prescanned, and a tag cut by the limit is cut. */
foreach ([1003, 1004] as $o) $show("meta at $o", $enc(str_repeat(' ', $o) . '<meta charset=koi8-r>'));
$show('meta after 1024', $enc(str_repeat('<!--a-->', 130) . '<meta charset=koi8-r>'));

/* The file producer sniffs the same way, and the three names agree. */
$f = tempnam(sys_get_temp_dir(), 'phl');
file_put_contents($f, "\xEF\xBB\xBF<meta charset=koi8-r><p>x</p>");
$d = Dom\HTMLDocument::createFromFile($f, LIBXML_NOERROR);
$show('file bom', $d->charset . ' ' . bin2hex($d->body->textContent));
file_put_contents($f, '<meta http-equiv=content-type content="text/html; charset=windows-1251"><p>x</p>');
$d = Dom\HTMLDocument::createFromFile($f, LIBXML_NOERROR);
$show('file meta', "$d->charset $d->characterSet $d->inputEncoding");
unlink($f);
--EXPECT--
utf-8 bom charset                  => UTF-8
utf-8 bom text                     => 78
utf-8 bom saveHtml                 => 3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e783c2f703e3c2f626f64793e3c2f68746d6c3e
bom beats meta                     => UTF-8
second bom is text                 => efbbbf78
utf-16be bom                       => UTF-16BE
utf-16le bom                       => UTF-16LE
short bom                          => UTF-8 UTF-8
override beats bom                 => KOI8-R
override beats meta                => UTF-8
plain                              => UTF-8
charset                            => KOI8-R
charset quoted                     => KOI8-R
charset single-quoted              => KOI8-R
charset spaced                     => KOI8-R
charset label                      => windows-1252
charset alias                      => IBM866
charset utf-16 stays               => UTF-16LE
charset x-user-defined stays       => x-user-defined
charset replacement                => replacement
charset unknown                    => UTF-8
charset empty then next            => UTF-8
charset in body                    => KOI8-R
charset in script                  => KOI8-R
charset in title                   => KOI8-R
first meta wins                    => KOI8-R
http-equiv                         => KOI8-R
http-equiv reversed                => KOI8-R
http-equiv any case                => windows-1251
http-equiv other                   => UTF-8
content alone                      => UTF-8
content unquoted                   => KOI8-R
content charset spaced             => KOI8-R
content charset to semicolon       => KOI8-R
content charset in a word          => KOI8-R
content unclosed quote             => UTF-8
content meets a quote              => UTF-8
content second charset             => UTF-8
content first then charset         => windows-1251
charset first then content         => KOI8-R
duplicate charset                  => KOI8-R
duplicate http-equiv               => UTF-8
seven letters compare              => KOI8-R
unquoted value to space            => KOI8-R
unquoted value keeps slash         => UTF-8
slash after meta                   => KOI8-R
newline after meta                 => KOI8-R
form feed after meta               => KOI8-R
vertical tab after meta            => UTF-8
nothing after meta                 => UTF-8
bare meta eats next tag            => UTF-8
unclosed at end                    => KOI8-R
unquoted unclosed at end           => UTF-8
comment                            => windows-1251
comment closes at first            => KOI8-R
comment never closes               => UTF-8
bogus comment                      => KOI8-R
processing instruction             => windows-1251
end tag attributes                 => UTF-8
other tag attributes               => UTF-8
meta inside an attribute           => UTF-8
greater-than inside an attribute   => KOI8-R
meta at 1003                       => KOI8-R
meta at 1004                       => UTF-8
meta after 1024                    => UTF-8
file bom                           => UTF-8 78
file meta                          => windows-1251 windows-1251 windows-1251
