--TEST--
Dom\HTMLDocument decodes its source under its encoding and re-encodes at saveHtml()
--EXTENSIONS--
dom
--FILE--
<?php
// The bytes of a Dom\HTMLDocument: decoded under the document's encoding on
// the way in, re-encoded under it by saveHtml() on the way out. Every row is
// php 8.5's answer; the name each label resolves to is tested elsewhere.
set_error_handler(function ($no, $msg) { echo "  ! $msg\n"; return true; });
$u16le = fn(array $cps) => pack('v*', ...$cps);
$show = fn($k, $v) => printf("%-22s %s\n", $k, $v);
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$text = fn($d) => bin2hex($d->body->textContent);
$html = fn($d) => bin2hex($d->saveHtml());
$P = [0x3C,0x70,0x3E]; $Pc = [0x3C,0x2F,0x70,0x3E];   // <p> and </p>

// windows-1252: every high byte, the five C1 holes included
$t = [];
for ($b = 0x80; $b < 0x100; $b++) $t[] = $text($doc("<p>" . chr($b) . "</p>", 'windows-1252'));
$show('1252 decode', implode(' ', $t));
$d = $doc("<p>caf\xE9 \x80</p>", 'latin1');
$show('latin1 label', $d->charset . ' ' . $text($d) . ' ' . $html($d));
$d->body->firstChild->textContent = "x\u{4E2D}\u{20AC}\u{FFFD}y\u{1F600}";
$show('1252 encode', $html($d));
$show('1252 encode node', bin2hex($d->saveHtml($d->body->firstChild)));
$show('1252 innerHTML', bin2hex($d->body->firstChild->innerHTML));
// saveXml() is not measured on a legacy-encoded document: its encoder is the
// platform's iconv behind libxml, and php answers per build (glibc writes
// character references, macOS drops and mangles the unencodable run); the
// C1 controls are bytes under saveHtml for the same reason
$d->saveXml();
$d->body->firstChild->textContent = "\u{81}\u{8D}\u{8F}\u{90}\u{9D}";
$show('1252 encode c1', $html($d));
$d->body->firstChild->textContent = "bad\xFFutf";
$show('1252 raw tree byte', $html($d));
$d = Dom\HTMLDocument::createEmpty('latin1'); $d->append($d->createElement('p'));
$d->documentElement->textContent = "\u{e9}\u{4E2D}";
$show('createEmpty latin1', $d->charset . ' ' . $html($d));

// UTF-8: a malformed sequence is one U+FFFD per maximal subpart
foreach (["\x80", "\xC0\x80", "\xC2", "\xE2\x82", "\xE2\x82x", "\xED\xA0\x80", "\xF4\x90\x80\x80",
          "\xF8\x88\x80\x80\x80", "\xF0\x9F\x98", "\xE0\x80\x80", "\xC1\xBF", "\xF0\x9F\x98\x80",
          "\xE2\x28\xA1", "\xF0\x28\x8C\xBC", "\xF0\x90\x28\xBC"] as $c) {
	$show('utf-8 ' . bin2hex($c), $text($doc("<p>a{$c}b</p>")));
}
$show('utf-8 eof', $text($doc("<p>a</p>\xE2\x82")));
$d = $doc("<p>a</p>"); $d->body->firstChild->textContent = "bad\xFFutf";
$show('utf-8 raw tree byte', $html($d));
$show('utf-8 bom override', $html($doc("\xEF\xBB\xBF<p>a</p>", 'utf-8')));
$show('1252 bom override', $html($doc("\xEF\xBB\xBF<p>a</p>", 'windows-1252')));

// UTF-16, both orders, by BOM and by override
$src = $u16le([...$P, 0x68, 0xE9, 0xD83D, 0xDE00, ...$Pc]);
$d = $doc("\xFF\xFE" . $src);
$show('utf-16le', $d->charset . ' ' . $text($d) . ' ' . $html($d));
$show('utf-16le node', bin2hex($d->saveHtml($d->body->firstChild)));
$show('utf-16le innerHTML', bin2hex($d->body->firstChild->innerHTML));
$d->body->firstChild->textContent = "bad\xFFutf\u{1F600}";
$show('utf-16le raw byte', $html($d));
$be = fn($le) => implode('', array_map('strrev', str_split($le, 2)));
$d = $doc("\xFE\xFF" . $be($src));
$show('utf-16be', $d->charset . ' ' . $text($d) . ' ' . $html($d));
$show('utf-16 lone high', $text($doc("\xFF\xFE" . $u16le([...$P, 0x61, 0xD800, 0x62, ...$Pc]) . "A")));
$show('utf-16 lone low', $text($doc("\xFF\xFE" . $u16le([...$P, 0x61, 0xDC00, 0x62, ...$Pc]))));
$show('utf-16 high then bmp', $text($doc("\xFF\xFE" . $u16le([...$P, 0x61, 0xD800, 0x41, 0x62, ...$Pc]))));
$d = $doc("\xFF\xFE" . $u16le([...$P, 0x61, ...$Pc]), 'UTF-16LE');
$show('utf-16 bom override', $text($d) . ' ' . bin2hex($d->saveHtml($d->head)));
$d = $doc("<meta charset=utf-16><p>ab</p>");
$show('meta utf-16', $d->charset . ' ' . $text($d) . ' ' . $html($d));
$d = Dom\HTMLDocument::createEmpty('utf-16'); $d->append($d->createElement('p'));
$d->documentElement->textContent = "\u{e9}";
$show('createEmpty utf-16', $d->charset . ' ' . $html($d));

// x-user-defined: the high half is U+F780..U+F7FF
$d = $doc("<p>a\x80\xFF</p>", 'x-user-defined');
$show('x-user-defined', $d->charset . ' ' . $text($d) . ' ' . $html($d));
$d->body->firstChild->textContent = "\u{F780}\u{4E2D}z";
$show('x-user-defined enc', $html($d));

// a diagnostic's column counts decoded characters, not source bytes
$d = Dom\HTMLDocument::createFromString("<p>caf\xE9</p></b>", 0, 'windows-1252');
--EXPECT--
1252 decode            e282ac c281 e2809a c692 e2809e e280a6 e280a0 e280a1 cb86 e280b0 c5a0 e280b9 c592 c28d c5bd c28f c290 e28098 e28099 e2809c e2809d e280a2 e28093 e28094 cb9c e284a2 c5a1 e280ba c593 c29d c5be c5b8 c2a0 c2a1 c2a2 c2a3 c2a4 c2a5 c2a6 c2a7 c2a8 c2a9 c2aa c2ab c2ac c2ad c2ae c2af c2b0 c2b1 c2b2 c2b3 c2b4 c2b5 c2b6 c2b7 c2b8 c2b9 c2ba c2bb c2bc c2bd c2be c2bf c380 c381 c382 c383 c384 c385 c386 c387 c388 c389 c38a c38b c38c c38d c38e c38f c390 c391 c392 c393 c394 c395 c396 c397 c398 c399 c39a c39b c39c c39d c39e c39f c3a0 c3a1 c3a2 c3a3 c3a4 c3a5 c3a6 c3a7 c3a8 c3a9 c3aa c3ab c3ac c3ad c3ae c3af c3b0 c3b1 c3b2 c3b3 c3b4 c3b5 c3b6 c3b7 c3b8 c3b9 c3ba c3bb c3bc c3bd c3be c3bf
latin1 label           windows-1252 636166c3a920e282ac 3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e636166e920803c2f703e3c2f626f64793e3c2f68746d6c3e
1252 encode            3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e783f803f793f3c2f703e3c2f626f64793e3c2f68746d6c3e
1252 encode node       3c703e783f803f793f3c2f703e
1252 innerHTML         78e4b8ade282acefbfbd79f09f9880
1252 encode c1         3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e818d8f909d3c2f703e3c2f626f64793e3c2f68746d6c3e
1252 raw tree byte     3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e6261643f7574663c2f703e3c2f626f64793e3c2f68746d6c3e
createEmpty latin1     latin1 3c703ee93f3c2f703e
utf-8 80               61efbfbd62
utf-8 c080             61efbfbdefbfbd62
utf-8 c2               61efbfbd62
utf-8 e282             61efbfbd62
utf-8 e28278           61efbfbd7862
utf-8 eda080           61efbfbdefbfbdefbfbd62
utf-8 f4908080         61efbfbdefbfbdefbfbdefbfbd62
utf-8 f888808080       61efbfbdefbfbdefbfbdefbfbdefbfbd62
utf-8 f09f98           61efbfbd62
utf-8 e08080           61efbfbdefbfbdefbfbd62
utf-8 c1bf             61efbfbdefbfbd62
utf-8 f09f9880         61f09f988062
utf-8 e228a1           61efbfbd28efbfbd62
utf-8 f0288cbc         61efbfbd28efbfbdefbfbd62
utf-8 f09028bc         61efbfbd28efbfbd62
utf-8 eof              61efbfbd
utf-8 raw tree byte    3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e626164efbfbd7574663c2f703e3c2f626f64793e3c2f68746d6c3e
utf-8 bom override     3c68746d6c3e3c686561643e3c2f686561643e3c626f64793eefbbbf3c703e613c2f703e3c2f626f64793e3c2f68746d6c3e
1252 bom override      3c68746d6c3e3c686561643e3c2f686561643e3c626f64793eefbbbf3c703e613c2f703e3c2f626f64793e3c2f68746d6c3e
utf-16le               UTF-16LE 68c3a9f09f9880 3c00680074006d006c003e003c0068006500610064003e003c002f0068006500610064003e003c0062006f00640079003e003c0070003e006800e9003dd800de3c002f0070003e003c002f0062006f00640079003e003c002f00680074006d006c003e00
utf-16le node          3c0070003e006800e9003dd800de3c002f0070003e00
utf-16le innerHTML     68c3a9f09f9880
utf-16le raw byte      3c00680074006d006c003e003c0068006500610064003e003c002f0068006500610064003e003c0062006f00640079003e003c0070003e00620061006400fdff7500740066003dd800de3c002f0070003e003c002f0062006f00640079003e003c002f00680074006d006c003e00
utf-16be               UTF-16BE 68c3a9f09f9880 003c00680074006d006c003e003c0068006500610064003e003c002f0068006500610064003e003c0062006f00640079003e003c0070003e006800e9d83dde00003c002f0070003e003c002f0062006f00640079003e003c002f00680074006d006c003e
utf-16 lone high       61efbfbd62efbfbd
utf-16 lone low        61efbfbd62
utf-16 high then bmp   61efbfbd4162
utf-16 bom override    efbbbf61 3c0068006500610064003e003c002f0068006500610064003e00
meta utf-16            UTF-16LE e6b4bce791a5e281a1e6a1a3e789a1e695b3e3b5b4e791b5e2b5a6e398b1e3b0bee3b9b0e689a1e2bcbce3b9b0 3c00680074006d006c003e003c0068006500610064003e003c002f0068006500610064003e003c0062006f00640079003e003c6d65746120636861727365743d7574662d31363e3c703e61623c2f703e3c002f0062006f00640079003e003c002f00680074006d006c003e00
createEmpty utf-16     utf-16 3c0070003e00e9003c002f0070003e00
x-user-defined         x-user-defined 61ef9e80ef9fbf 3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e6180ff3c2f703e3c2f626f64793e3c2f68746d6c3e
x-user-defined enc     3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e803f7a3c2f703e3c2f626f64793e3c2f68746d6c3e
  ! Dom\HTMLDocument::createFromString(): tree error unexpected-token-in-initial-mode in Entity, line: 1, column: 2
