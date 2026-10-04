--TEST--
Dom\HTMLDocument decodes and re-encodes EUC-JP
--EXTENSIONS--
dom
mbstring
--FILE--
<?php
// EUC-JP through a Dom\HTMLDocument, both directions. Every row is php 8.5's
// answer. EUC-JP is not one table: a byte stands alone, or opens a PAIR, or --
// 0x8F only -- opens a THREE-byte sequence naming a cell in a second code set,
// and php's two directions do not carry the same characters. It decodes all
// 6067 cells of that third set and encodes not one of them, writing `?`.
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$dec = fn($bytes) => bin2hex($doc("<p>$bytes</p>", 'EUC-JP')->getElementsByTagName('p')[0]->textContent);
$txt = fn($src) => $doc("<p>$src</p>", 'EUC-JP')->getElementsByTagName('p')[0]->textContent;

echo "== which bytes open a longer sequence, and how long ==\n";
$g = [];
for ($b = 0x80; $b <= 0xFF; $b++) {
	$n = mb_strlen($txt(chr($b) . "\xA1\xA1"), 'UTF-8');
	$g[$n][] = sprintf('%02X', $b);
}
ksort($g);
foreach ($g as $n => $l) { printf("%d char(s): %d bytes, first %s last %s\n", $n, count($l), $l[0], end($l)); }

echo "== the pair space, one digest per lead ==\n";
foreach (array_merge([0x8E], range(0xA1, 0xFE)) as $L) {
	$src = '';
	for ($b = 0x40; $b <= 0xFE; $b++) { $src .= chr($L) . chr($b) . ','; }
	printf("%02X %s\n", $L, md5($txt($src)));
}

echo "== the third code set behind 0x8F, one digest per row ==\n";
$o = '';
for ($L = 0x81; $L <= 0xFE; $L++) {
	$src = '';
	for ($b = 0x40; $b <= 0xFE; $b++) { $src .= "\x8F" . chr($L) . chr($b) . ','; }
	$o .= sprintf("%02X %s\n", $L, md5($txt($src)));
}
echo md5($o), "\n";
foreach ([[0xA2, 0xAF], [0xA1, 0xA1], [0xB0, 0xA1], [0xF3, 0xFE], [0xED, 0xE3]] as $c) {
	printf("8F%02X%02X %s\n", $c[0], $c[1], $dec("\x8F" . chr($c[0]) . chr($c[1])));
}

echo "== every single byte ==\n";
$o = '';
for ($b = 1; $b < 0x100; $b++) { $o .= $dec(chr($b)) . '|'; }
echo md5($o), "\n";

echo "== a sequence php does not read, and the bytes after it ==\n";
// An ASCII byte always ends the sequence and is read again; a byte the 0x8F
// lead does not accept in the MIDDLE ends it after two, leaving the third to
// open a character of its own -- which an accepted byte naming an empty row
// does not do.
foreach (["\x8F", "\x8FA", "\x8F\xA1", "\x8F\xA1A", "\x8F\x81\xA1\xA1", "\x8F\xA0\xA1\xA1",
          "\x8F\xA1\xA1\xA1", "\x8F\xFF\xA1\xA1", "\x8F\xA1\xFF\xA1\xA1", "\x8F\xA2\xFF",
          "\x8F\xA2\x3F", "\x8F\x81\x8E\xA1", "\x8E", "\x8E\x41", "\x8E\xFF", "\xA1",
          "\xA1\x41", "\xFF\xA1", "\x80\xA1\xA1", "\xA4\xA2\x8F"] as $m) {
	printf("%-12s %s\n", bin2hex($m), $dec($m));
}

echo "== the encoder over the whole BMP ==\n";
$d = Dom\HTMLDocument::createEmpty('EUC-JP');
$all = '';
for ($cp = 0x20; $cp <= 0xFFFF; $cp++) {
	if ($cp < 0xD800 || $cp > 0xDFFF) { $all .= mb_chr($cp, 'UTF-8') . ','; }
}
echo md5($d->saveHtml($d->createComment($all))), "\n";
foreach ([0x80, 0xA5, 0x203E, 0x5C, 0x7E, 0xFF61, 0xFF9F, 0x3000, 0x2252, 0x02D8, 0x4E00, 0xE9] as $cp) {
	$c = $d->createComment(mb_chr($cp, 'UTF-8'));
	printf("U+%04X %s\n", $cp, bin2hex(substr($d->saveHtml($c), 4, -3)));
}

echo "== a whole document round trip ==\n";
// The 0x8F cell in the title is the one php reads and cannot write back.
$d = $doc("<!doctype html><title>\xC6\xFC\xCB\xDC\xB8\xEC\x8F\xA2\xAF</title>"
        . "<p>\xA4\xA2\xA4\xA4\xA4\xA6\x8E\xB1</p>", 'EUC-JP');
echo $d->charset, "\n";
echo bin2hex($d->getElementsByTagName('title')[0]->textContent), "\n";
echo bin2hex($d->saveHtml()), "\n";
// saveXml() is deliberately NOT measured here, for the reason the Shift_JIS
// case states: EUC-JP is outside the set libxml builds in, and the Windows
// build has no iconv to fall back on.
?>
--EXPECT--
== which bytes open a longer sequence, and how long ==
1 char(s): 1 bytes, first 8F last 8F
2 char(s): 127 bytes, first 80 last FF
== the pair space, one digest per lead ==
8E 9db7c67442cecb35de1898406ba23c7e
A1 5de3d60708be8de4602c6f9a0d07aec8
A2 38345496ad4fb851175546d8f534b569
A3 0dede43df850c68ea8ce5adf53e5e799
A4 c3f194677f9150eddcb6c324b165a06a
A5 930993449d1b56df45764c5a8bace9db
A6 0503a4efafae9934cde000723d15e63f
A7 200d020087ef68ccdc56f2ddcc395bf1
A8 198646e5f9d86d4e2b730376f06e5de5
A9 66188bfc8ea063d54eb74e7c80f69de1
AA 66188bfc8ea063d54eb74e7c80f69de1
AB 66188bfc8ea063d54eb74e7c80f69de1
AC 66188bfc8ea063d54eb74e7c80f69de1
AD 82e7ea057e403b6912ea706fd180aee5
AE 66188bfc8ea063d54eb74e7c80f69de1
AF 66188bfc8ea063d54eb74e7c80f69de1
B0 17bcb73c3e25e0ac415ee71266c56393
B1 c31bc3a3c38cf9c245e8678d835d65f0
B2 161ab352624f0600d2080207ed98b39e
B3 176af33a6555d95e678177a50372b650
B4 da42709a8b84b93808a968d3b726b3a5
B5 58ac3035cebe1697cbe7a84f5c92e0a0
B6 3efd2a8d643339908b7d6dbf2faac736
B7 e55a485b51abc988f57c8f51a81fbf79
B8 658057f0e2c9cffa965d478a2e62448d
B9 469e2e954827119d0efad6f2ba76eb18
BA b5420d15a571f3122881948b9e11d4fb
BB 1ca9bc5e353d1930913badac39432f9e
BC fc88894cf4d3fcc64e0c62fa466ca4e3
BD 01b8ab98b67ac98e81dde635480ca4eb
BE b703e7dc4ffff18d6fcedf9c88f8e836
BF b2030cb1875482db9f33f9a3bcc6952e
C0 e7641ad5b724b12114c5ccbf77efecba
C1 8b4d0265a234198a7cb2bff1bbc24e22
C2 9e2bd118d307ec7a06ec6d52b871f9b3
C3 a870a6329f8b65e8cce28b89df316bf9
C4 c030db30a3e1a2cda62dd0909373ba2a
C5 a72ebdf3a25f5963e5ad7d59ec4570da
C6 3e1a7f8949ba457c17fa5ad533e5dd14
C7 23757cd4e05cd042bfd6f42a40f101d8
C8 edaad821e25861e6d816a1527b1e1f7f
C9 1156a21e768dc020e37bfc696d215e6f
CA b718b00724accb7e05b5246fa0bc469f
CB 484dd3108b040ef7a79727bde949daee
CC 3ecec0061d1443deebc8d00c07619773
CD a1fec21292f70d2bd1f8c70d582254bb
CE 4f3cfaa336a411d12395eb4c3cdba0df
CF 1c0848be7bef0404fd5b195fbfbfff37
D0 52dc05ace6c6eda41a0afd8b838d1be2
D1 7b91e2a4ba4cec5ddedff3f3cc5e761f
D2 fc3fa2d51b8ad439e89553eff4b3927c
D3 4e5b039df0fe1d9e71e189c5a683987a
D4 5b34ba77beeb63b9ce4f0594f6df73a3
D5 eb291eb1d644202dcd8ef874075d5ac7
D6 76a7f558cc2367cba36e227deb9331e9
D7 634971643c3e6e230651a817dc4451ab
D8 aad61aac715f3ac60550ff5e41622e3b
D9 a18236287c64950ff2f5fb9a7d434f6e
DA 64b63b24cf1257c3ca6bfa8376e0b08a
DB 0af997cde2a8ca0332d0a05f384c3743
DC 1fbc6aee190f855fec57cba2abcb69d5
DD 0495db8dbdb2b37a71de33a193a14cb6
DE 30283c55f8004c6a3bbdb9b7b8837446
DF 4c7b38de6b296db5ac987170c381f2ce
E0 068d8a964ea6e9f7fcbefc8093fd6eca
E1 fd5e02288b9f84a1045a861ab19f966c
E2 59601bebef59369a0b6a42248586fedf
E3 082150092ae6eb044674e829103885f9
E4 3dd8d3260ffb289108945adb01e3ed71
E5 47c4b06273f5a901003ea512b2164125
E6 7dd7b83c84eead482837a0c38d9ad62f
E7 fe1b11ebbfdf708a7ec5fff10bf3ba67
E8 7180d8031b147f664857aa6f71206a94
E9 043d9ccf5fe1e4406cbbe1ecd5db1af5
EA e8519f5473231de8cf98b7556a0b8d54
EB b2a2e12ccbd48970812241a022cee1ae
EC 5c0110b4b0dae5db59d12f6b3ec982b5
ED 8fee14f5d2f682daf1b3a5b61532e20f
EE 9c889dad1ed56a3548ada59cbc5b804d
EF 3cf72f896aef5dd68bb4f1745bce7212
F0 b73f28938cf9fc9444747e1902a9fe34
F1 33b7aed43547da22cfedeba3ceea1431
F2 46453b9a7dbdeb8527ca7ed60706cb07
F3 b22971fd9c0c784280cea1d36ad1c770
F4 8fbb7e8d97a526364d9e333a12023da7
F5 66188bfc8ea063d54eb74e7c80f69de1
F6 66188bfc8ea063d54eb74e7c80f69de1
F7 66188bfc8ea063d54eb74e7c80f69de1
F8 66188bfc8ea063d54eb74e7c80f69de1
F9 5a21354dbd7c7621cea9f25778d52f1e
FA ef98a9f239f956398084a69fab6778f1
FB 292c536b0e0fd61cf4c177e78bc8506a
FC e8ff87eb8183636513d657937bb560ec
FD 66188bfc8ea063d54eb74e7c80f69de1
FE 66188bfc8ea063d54eb74e7c80f69de1
== the third code set behind 0x8F, one digest per row ==
c735d122013c1dc85b722c5928677a70
8FA2AF cb98
8FA1A1 efbfbd
8FB0A1 e4b882
8FF3FE efbfbd
8FEDE3 e9bea5
== every single byte ==
52d13cb91a61c8a31198db1df06700be
== a sequence php does not read, and the bytes after it ==
8f           efbfbd
8f41         efbfbd41
8fa1         efbfbd
8fa141       efbfbd41
8f81a1a1     efbfbde38080
8fa0a1a1     efbfbde38080
8fa1a1a1     efbfbdefbfbd
8fffa1a1     efbfbde38080
8fa1ffa1a1   efbfbde38080
8fa2ff       efbfbd
8fa23f       efbfbd3f
8f818ea1     efbfbdefbda1
8e           efbfbd
8e41         efbfbd41
8eff         efbfbd
a1           efbfbd
a141         efbfbd41
ffa1         efbfbdefbfbd
80a1a1       efbfbde38080
a4a28f       e38182efbfbd
== the encoder over the whole BMP ==
9bd24a4f91e1d57f13d621b1da4b1772
U+0080 3f
U+00A5 5c
U+203E 7e
U+005C 5c
U+007E 7e
U+FF61 8ea1
U+FF9F 8edf
U+3000 a1a1
U+2252 a2e2
U+02D8 3f
U+4E00 b0ec
U+00E9 3f
== a whole document round trip ==
EUC-JP
e697a5e69cace8aa9ecb98
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653ec6fccbdcb8ec3f3c2f7469746c653e3c2f686561643e3c626f64793e3c703ea4a2a4a4a4a68eb13c2f703e3c2f626f64793e3c2f68746d6c3e
