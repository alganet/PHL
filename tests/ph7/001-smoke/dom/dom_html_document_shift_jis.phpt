--TEST--
Dom\HTMLDocument decodes and re-encodes Shift_JIS
--EXTENSIONS--
dom
mbstring
--FILE--
<?php
// Shift_JIS through a Dom\HTMLDocument, both directions. Every row is php
// 8.5's answer, and php's two directions do not carry the same characters:
// its lead set is 0x81..0x9F, 0xE0 and 0xFC only, so most of the second-level
// kanji cannot be read at all, while the encoder writes those same cells
// perfectly well.
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$dec = fn($bytes) => bin2hex($doc("<p>$bytes</p>", 'Shift_JIS')->getElementsByTagName('p')[0]->textContent);

echo "== the cell space, one digest per lead ==\n";
foreach (array_merge(range(0x81, 0x9F), range(0xE0, 0xFC)) as $L) {
	$src = '';
	for ($b = 0x40; $b <= 0xFC; $b++) {
		if ($b !== 0x7F) { $src .= chr($L) . chr($b) . ','; }
	}
	printf("%02X %s\n", $L, md5($doc("<p>$src</p>", 'Shift_JIS')->getElementsByTagName('p')[0]->textContent));
}

echo "== every single byte ==\n";
$o = '';
for ($b = 1; $b < 0x100; $b++) { $o .= $dec(chr($b)) . '|'; }
echo md5($o), "\n";

echo "== a cell php does not read, and the byte after it ==\n";
foreach (["\xE1\x4B", "\xE1\xA1", "\xE1\x80", "\xE1\x81z", "\xE1\xFD", "\x88\x4B", "\x88\x80",
          "\x81\x20", "\x81", "\xA0", "\xFD", "\x82\xA0\x81"] as $m) {
	printf("%-12s %s\n", bin2hex($m), $dec($m));
}

echo "== the encoder over the whole BMP ==\n";
$d = Dom\HTMLDocument::createEmpty('Shift_JIS');
$txt = '';
for ($cp = 0x20; $cp <= 0xFFFF; $cp++) {
	if ($cp < 0xD800 || $cp > 0xDFFF) { $txt .= mb_chr($cp, 'UTF-8') . ','; }
}
echo md5($d->saveHtml($d->createComment($txt))), "\n";
foreach ([0x80, 0xA5, 0x203E, 0x5C, 0x7E, 0xFF61, 0xFF9F, 0x3000, 0x2252, 0x9ADC, 0xE9] as $cp) {
	$c = $d->createComment(mb_chr($cp, 'UTF-8'));
	printf("U+%04X %s\n", $cp, bin2hex(substr($d->saveHtml($c), 4, -3)));
}

echo "== a whole document round trip ==\n";
$d = $doc("<!doctype html><title>\x93\xFA\x96\x7B\x8C\xEA</title><p>\x82\xA0\x82\xA2\x82\xA4</p>", 'Shift_JIS');
echo $d->charset, "\n";
echo bin2hex($d->getElementsByTagName('title')[0]->textContent), "\n";
echo bin2hex($d->saveHtml()), "\n";
// saveXml() is deliberately NOT measured here: it goes through libxml's own
// encoders, and Shift_JIS is outside the set libxml builds in, so the Windows
// build -- which has no iconv to fall back on -- cannot spell the document at
// all and warns instead. That is libxml's build talking, not this engine.
?>
--EXPECT--
== the cell space, one digest per lead ==
81 531deb0b061368587e52825d94dcf4b7
82 deddc9829d21cde565134d8b4866a6bb
83 d2d4954d964d07f6a506c2ca40f0664f
84 991f5627bdc85527d9b6ce419f321b14
85 40a9ca1c21eeec223e9be3576e6a48f0
86 40a9ca1c21eeec223e9be3576e6a48f0
87 8c9807f113321b5ccd93be08e9702fc4
88 5c22fc4c4ce3568dda17c00c649599d3
89 37420db7a01e7b35cf09f15e26a4e458
8A b78139f57756715291590db8a4d16854
8B b5b9776b74c6095cfdac9f0f5c6b962e
8C e92d4dabcc97916b69056ad83beb2da9
8D 757ced4a3b404b17d3aad42ad9febc63
8E 904b326e76994a07baa050e455d8c039
8F 50c305a73f38ed1eface9a5d134ee004
90 dbedcc3147ccc09f46c1fcdbdfa03fae
91 407270e95ce0df073703b8532c98821a
92 2273a8c88331df57a57db6c54af9ae0e
93 47af9ba14b3cac32727c74239d16a06f
94 3229558ef38dfe1f7b18f84e9d1416f8
95 295d21aaabc7ed8dd2d098eb24e64bf9
96 f698a7c1d0bac9f7ec98682af6c2d8fe
97 c6bbc0f09e04cfc2f07e0d525ef66ca0
98 296f6427b616ef5d64f12b6ac87ca7f3
99 c8d71ecc3c40f5c4f3c4e52558c3366e
9A e7b06525bb2dd2891128fc77210662fa
9B fc4923f4a26b9e8dc5a872a32868756a
9C 9cc199fb08ce2787a80c1feeb4a9e52e
9D e5faf2f6d1fa739b01b2557681c73bc9
9E cd89cfaf5582a82bc1b066059bd65a41
9F 45b1d04c09f252cf3a8d9798a66a420a
E0 ef17752a1a25f5e24c0136d8d5eaf46e
E1 10aea934d2fdbf39e8bd8b92ae7642fd
E2 10aea934d2fdbf39e8bd8b92ae7642fd
E3 10aea934d2fdbf39e8bd8b92ae7642fd
E4 10aea934d2fdbf39e8bd8b92ae7642fd
E5 10aea934d2fdbf39e8bd8b92ae7642fd
E6 10aea934d2fdbf39e8bd8b92ae7642fd
E7 10aea934d2fdbf39e8bd8b92ae7642fd
E8 10aea934d2fdbf39e8bd8b92ae7642fd
E9 10aea934d2fdbf39e8bd8b92ae7642fd
EA 10aea934d2fdbf39e8bd8b92ae7642fd
EB 10aea934d2fdbf39e8bd8b92ae7642fd
EC 10aea934d2fdbf39e8bd8b92ae7642fd
ED 10aea934d2fdbf39e8bd8b92ae7642fd
EE 10aea934d2fdbf39e8bd8b92ae7642fd
EF 10aea934d2fdbf39e8bd8b92ae7642fd
F0 10aea934d2fdbf39e8bd8b92ae7642fd
F1 10aea934d2fdbf39e8bd8b92ae7642fd
F2 10aea934d2fdbf39e8bd8b92ae7642fd
F3 10aea934d2fdbf39e8bd8b92ae7642fd
F4 10aea934d2fdbf39e8bd8b92ae7642fd
F5 10aea934d2fdbf39e8bd8b92ae7642fd
F6 10aea934d2fdbf39e8bd8b92ae7642fd
F7 10aea934d2fdbf39e8bd8b92ae7642fd
F8 10aea934d2fdbf39e8bd8b92ae7642fd
F9 10aea934d2fdbf39e8bd8b92ae7642fd
FA 10aea934d2fdbf39e8bd8b92ae7642fd
FB 10aea934d2fdbf39e8bd8b92ae7642fd
FC 33846d3186c2361c3c00379be1511d5a
== every single byte ==
d2c3bda7ed4841eef62ccf6b418a4ba2
== a cell php does not read, and the byte after it ==
e14b         efbfbd4b
e1a1         efbfbdefbda1
e180         efbfbdc280
e1817a       efbfbde38091
e1fd         efbfbdefbfbd
884b         efbfbd4b
8880         efbfbd
8120         efbfbd20
81           efbfbd
a0           efbfbd
fd           efbfbd
82a081       e38182efbfbd
== the encoder over the whole BMP ==
186718d3e76a281a64a31ff5e5a06e95
U+0080 80
U+00A5 5c
U+203E 7e
U+005C 5c
U+007E 7e
U+FF61 a1
U+FF9F df
U+3000 8140
U+2252 81e0
U+9ADC eee1
U+00E9 3f
== a whole document round trip ==
Shift_JIS
e697a5e69cace8aa9e
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653e93fa967b8cea3c2f7469746c653e3c2f686561643e3c626f64793e3c703e82a082a282a43c2f703e3c2f626f64793e3c2f68746d6c3e
