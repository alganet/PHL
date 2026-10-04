--TEST--
Dom\HTMLDocument's replacement encoding decodes to nothing and encodes to nothing
--EXTENSIONS--
dom
--FILE--
<?php
// `replacement` is the encoding standard's mitigation, not a converter: a
// source served under any of its six labels decodes to the empty string, so
// the tree is the bare html/head/body the builder mints from nothing, and the
// dump encodes to nothing however much the program appended afterwards.
// Every row is php 8.5's answer.
set_error_handler(function ($no, $msg) { echo "  ! $msg\n"; return true; });
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);

// Every label of the run resolves to the one name, at the override door.
foreach (['replacement','csiso2022kr','hz-gb-2312','iso-2022-cn','iso-2022-cn-ext','iso-2022-kr'] as $label) {
	$d = $doc("<p>hello\xA1\xA2</p>", $label);
	printf("%-16s charset=%s save=%s body=%s kids=%d\n", $label, $d->charset,
		bin2hex($d->saveHtml()), bin2hex($d->body->textContent), $d->body->childNodes->length);
}

// The source makes no difference: nothing survives the decode.
foreach (['', 'abc', "<p>\xA1\xA2\xB0</p>", "\x1b\x24\x29\x43abc", "<!--c--><p>x</p>"] as $i => $src) {
	$d = $doc($src, 'replacement');
	printf("src%d tree=%s html=%s\n", $i, bin2hex($d->saveHtml()),
		$d->documentElement->nodeName . '/' . $d->head->nodeName . '/' . $d->body->nodeName);
}

// The encode face is its own: an appended subtree is in the tree and out of
// every dump the document's encoding governs.
$d = $doc("<p>x</p>", 'replacement');
$d->body->appendChild($d->createTextNode("abc\u{20AC}"));
echo "appended body kids=", $d->body->childNodes->length,
	" text=", bin2hex($d->body->textContent), "\n";
echo "saveHtml=[", bin2hex($d->saveHtml()), "] node=[", bin2hex($d->saveHtml($d->body)), "]\n";
// innerHTML is UTF-8 whatever the document says, so it still reads the append.
echo "innerHTML=[", bin2hex($d->body->innerHTML), "]\n";
$f = tempnam(sys_get_temp_dir(), 'phlrep');
var_dump($d->saveHtmlFile($f));
echo "file=[", bin2hex(file_get_contents($f)), "]\n";
unlink($f);
// The second door: a meta naming one of the labels decides the same way.
$d = $doc("<meta charset=\"iso-2022-kr\"><p>hi\xA1</p>");
echo "sniff charset=", $d->charset, " save=[", bin2hex($d->saveHtml()),
	"] kids=", $d->body->childNodes->length, "\n";
--EXPECT--
replacement      charset=replacement save= body= kids=0
csiso2022kr      charset=replacement save= body= kids=0
hz-gb-2312       charset=replacement save= body= kids=0
iso-2022-cn      charset=replacement save= body= kids=0
iso-2022-cn-ext  charset=replacement save= body= kids=0
iso-2022-kr      charset=replacement save= body= kids=0
src0 tree= html=HTML/HEAD/BODY
src1 tree= html=HTML/HEAD/BODY
src2 tree= html=HTML/HEAD/BODY
src3 tree= html=HTML/HEAD/BODY
src4 tree= html=HTML/HEAD/BODY
appended body kids=1 text=616263e282ac
saveHtml=[] node=[]
innerHTML=[616263e282ac]
int(0)
file=[]
sniff charset=replacement save=[] kids=0
