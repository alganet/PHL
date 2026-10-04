--TEST--
Dom\HTMLDocument decodes and re-encodes Big5
--EXTENSIONS--
dom
mbstring
--FILE--
<?php
// Big5 through a Dom\HTMLDocument, both directions. Every row is php 8.5's
// answer. This index is neither of the other two double-byte shapes: 1713 of
// its cells stand for a supplementary code point and four for TWO code
// points, and its two directions disagree about them -- 1713 cells decode
// past the BMP where only 291 supplementary code points encode back.
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$dec = fn($bytes) => bin2hex($doc("<p>$bytes</p>", 'Big5')->getElementsByTagName('p')[0]->textContent);

echo "== the cell space, one digest per lead ==\n";
for ($L = 0x81; $L <= 0xFE; $L++) {
	$src = '';
	for ($b = 0x40; $b <= 0xFE; $b++) { $src .= chr($L) . chr($b) . ','; }
	printf("%02X %s\n", $L, md5($doc("<p>$src</p>", 'Big5')->getElementsByTagName('p')[0]->textContent));
}

echo "== every single byte ==\n";
$o = '';
for ($b = 1; $b < 0x100; $b++) { $o .= $dec(chr($b)) . '|'; }
echo md5($o), "\n";

echo "== a cell past the BMP, and the four spelled with two code points ==\n";
foreach (["\x87\x45", "\x87\x48", "\xFA\x5F", "\x88\x62", "\x88\x64", "\x88\xA3", "\x88\xA5"] as $m) {
	printf("%-6s %s\n", bin2hex($m), $dec($m));
}

echo "== bytes that name no character, and the byte after them ==\n";
foreach (["\x80", "\xFF", "\x81\x39", "\x81\xFF", "\xA4\x3F", "\xA4\x40z", "\x81z",
          "\x81\x80", "\xA4", "\xA4\x40\x81"] as $m) {
	printf("%-8s %s\n", bin2hex($m), $dec($m));
}

echo "== the encoder over the whole code space ==\n";
$d = Dom\HTMLDocument::createEmpty('Big5');
for ($base = 0x20; $base <= 0x10FFFF; $base += 0x8000) {
	$txt = '';
	for ($cp = $base; $cp < $base + 0x8000 && $cp <= 0x10FFFF; $cp++) {
		if ($cp < 0xD800 || $cp > 0xDFFF) { $txt .= mb_chr($cp, 'UTF-8') . ','; }
	}
	printf("%06X %s\n", $base, md5($d->saveHtml($d->createComment($txt))));
}
foreach ([0x80, 0xA5, 0x4E00, 0xF6B1, 0x27267, 0x2A351, 0x20000, 0xCA, 0x304, 0xEA, 0x30C] as $cp) {
	$c = $d->createComment(mb_chr($cp, 'UTF-8'));
	printf("U+%05X %s\n", $cp, bin2hex(substr($d->saveHtml($c), 4, -3)));
}
// The four two-code-point cells have no encoder: each half writes '?', so the
// pair writes "??" rather than going back to the cell it came from.
$c = $d->createComment("\u{00CA}\u{0304}\u{00EA}\u{030C}");
echo bin2hex(substr($d->saveHtml($c), 4, -3)), "\n";

echo "== a whole document round trip ==\n";
$d = $doc("<!doctype html><title>\xA4\xA4\xA4\xE5</title><p>\xA4\x40\xA4\x41\x87\x45</p>", 'Big5');
echo $d->charset, "\n";
echo bin2hex($d->getElementsByTagName('title')[0]->textContent), "\n";
echo bin2hex($d->saveHtml()), "\n";
// saveXml() is deliberately NOT measured here: it goes through libxml's own
// encoders, Big5 is outside the set libxml builds in, and the Windows build
// has no iconv to fall back on -- so it cannot spell the document there
// whatever this engine does.
?>
--EXPECT--
== the cell space, one digest per lead ==
81 66188bfc8ea063d54eb74e7c80f69de1
82 66188bfc8ea063d54eb74e7c80f69de1
83 66188bfc8ea063d54eb74e7c80f69de1
84 66188bfc8ea063d54eb74e7c80f69de1
85 66188bfc8ea063d54eb74e7c80f69de1
86 66188bfc8ea063d54eb74e7c80f69de1
87 b54acc5cfe32c85e79d894bdd80d5237
88 1b49f89efa886fde4e98adc3d233eec0
89 3db6806fdbf02eec136f58f27cfa34e3
8A a2e19137a506e4b36ac877f7d03e38e0
8B 39e6ed72dc16ebf71d5ebf21757f7cf0
8C 3ad9177dbe94e3ed0e6ed6560bfebc7d
8D 7714da56940ddb82b3d88ca9062bbef0
8E e6069429dbc9a6486ad48a7cd5ba3a64
8F b6bd1b07db9a712de7ff05780c054cbf
90 8c1b96177873b66a1893f30b67d727d6
91 937ddbc8b77089c9b3fda98977bd6805
92 e542eff7596e5d5d3fd40ac666466617
93 6bad2233838f7119ab828cb653452582
94 de8698b129a2993b7d0430d2e4f66298
95 488ef6fac945b69ef52b21e47a9ed619
96 35c320ba47f1ed19bd6214c61ef365b8
97 ae022f6ebe0e40945086d8a2f0045f6b
98 742673b71df8ee014ef00f7bd5426da7
99 57f55f0b33b197e203879919c449c248
9A fee88854fd39300511c633864fb374e9
9B 8b7471091cc5f186a645c1cc22e01121
9C 3a6a9ed8ed69631d3397b5d50f064e42
9D 9e351e10c0ec0388dfd9f7ba4a17dd1e
9E c185ca762c09ed5189065e71194d0c23
9F 3e58365a04b87f7fa110650a4e0f0f5d
A0 6eb18b2616423353a4cb564c986e4910
A1 2e46de7b692ac2bab3aacc5db88d47fc
A2 cea288f9327b4b491e318c22549519d8
A3 ea56ccfa9fc58e3494c3dcec67b15671
A4 630d5dbaf29bbda50b66f4d2e341c002
A5 54640bc5e9c4f8189062c501ef44837a
A6 af5c00efa26a2dcd0043d16f50f97588
A7 2489768b29744c1ca5f6d763a73d48e5
A8 8d5d1b8d35922386ccdffcd234d427b9
A9 09572439260c66e8e3b0dd91be28959a
AA 2a06e6e8b82857a1766b396217820622
AB e95a9f450d8d755565773f3fe765ae8b
AC 22cafe54818b753cb9798d8e169ee7cb
AD 986aecd9d5e25a909d10e24776dde6d4
AE f19977906e4492f4a8cd50ea0b6ce6ee
AF 24e14327ab577fbde6ffaa66f16b1bfc
B0 02c9779fef941f4cbc8ff246b7d64745
B1 cbcc6b35c975a32ec16842777a0dd20c
B2 4a5d6e7946c9cb2d422ca33bb5dc5929
B3 bb06fd492206229a6635f7ca659eeb01
B4 01fc30814947ed8d6151510eafdb0cc0
B5 bed09082fce94406d13281d679c91ff0
B6 d290b4851b184f930d0d065848d556ee
B7 d53dbde4b708a741a34828371570a544
B8 6e3c3d2a05286bda3757df9c66357db7
B9 010ea93230731d1668df02d981b5c185
BA 0f82baf336c4fc9f8a195bb4021b9a68
BB af4fe02ac6e97333aba2ea31b57d334b
BC 95348bc1656f056a795daada666036fe
BD 2fc60d0d7e887203d50646010692c2c0
BE 2558fff70ba72ff9afb9fe63f3933315
BF 1784d49223d370d9a36ce59e1fe841a6
C0 cf16608089f2b55c7597b223a2932288
C1 de1af4bbdcaf0b83062c88958036ec0e
C2 3d78e4957971324d2e75975aaf4adac0
C3 38d4802fdbbb495f388d7639f2ad1598
C4 133b322d737596ea11062f2517c6168e
C5 ad14b0391af9324e1961ad2112a6fa1a
C6 9b3efa487247ab72693b8e92e984ad5b
C7 d76fe86631b886c5e73986255144e7aa
C8 6641dea55d0f8d7b6b1415ee5c719c74
C9 84085e7ca58c09cf4815651236606c8c
CA 40335f79a825f3e19ee35dcd6cf58aa6
CB 4e313a216473b4fe8e1d7129af487ba4
CC 8ef7955cc499aa052bbd57b39cd2edb8
CD 8be12ad89c246f9eba847e1a98655e89
CE 2c4b99a1b4377e36bf371e1c886e72fd
CF 4649296e54e622a712df2a507f78cc5e
D0 9535e0b10b2c2c8a632bf3fc2d269e28
D1 a66efb7bdec11d7ea23b51669597843f
D2 4ccd68e4f51b532f43c9a1dbba58b3ad
D3 e516dd75ba480e13728b7418f34c953f
D4 638c4e449fe17763b661e283e5942afe
D5 e975e5cfa49efb44ad1939dd27110ca3
D6 82648e5e135d27289af9e07d6328a711
D7 d5d08375f07c274fb46bf833d5a5233e
D8 d0fe39e66fc9f5540e52a5258bc8356a
D9 c1e3a6d97d867188e8c43a2b866b7aae
DA 2fc37b1df48b115756be0f592075266f
DB 52872a807d33488d70cbc4db89e06343
DC 7da0a193a4e0bef26be673545d6a2c3f
DD 391359a765021abbad6656c51d796728
DE 99654a0fc613d5f7b5bffcdb5059abd1
DF c8a3b95e9b52168ebc8c53055ac616ee
E0 41bdbd3e7e1663718944561ef18f9d3f
E1 2f37397cf0f6b91c50bac9c4eb409d4a
E2 0caef73873bc1903088afa77bc87fbf0
E3 aa5a07d12a9d65e75924239c5fe2fb0c
E4 74c98a7c7688f8c2d0d9f6ce6bc7c7bf
E5 4f5df3ada63e9d8b2cf6bd533e3d6e43
E6 30410d6002650552f72ef246e18d8879
E7 0d18fdfb49ca638d96e00a74aa942ba5
E8 9849dfc7379269bacaf00e8d6d122ff9
E9 51dec3e08c3d482392c82a5836b9479e
EA 3086db36fa46c912e1c91e2de35c6eef
EB 503e36b30ad326e19a3336730fff394c
EC f91b3599f2f81f75fdb83b017c9631d3
ED 47ba207e224c7e00e4ff3ce18a536c89
EE 49fde3774e5e318dd4f3556df4cb463d
EF da7b6bb111a65def6a0eb9345e12b5e3
F0 52f0585d8b89a4ba60c970419b2d2945
F1 2a43f43f6965ab990be183b409d55e49
F2 5d2d5ddfe76d12c39710bd6c15be7825
F3 2ec46446e8d58de79c5552ec5a6538bb
F4 0e508fac545e53439ab009278204be5b
F5 2f60e9b5ec077eee9b88d5f6b79e8e7b
F6 fc64c3a0718470995bba5a3aa37cf385
F7 641f6040898c11fc271277dd8531cef0
F8 58965ff710f426fb938b8ef375fea0de
F9 7c66f2b756375b171cdf10c2fd96a2d9
FA 10efd6f4d4990a104a1a06130fa959e5
FB 8a0b0a9ed8a5b13e357155559c45f702
FC fb55f1f93a90b73aaae1b1283d6c2870
FD 7d9956e1c9b68c0823fcfbda286dcff9
FE 88b5faf2e09ad947696fcd29ae96c6b9
== every single byte ==
52d13cb91a61c8a31198db1df06700be
== a cell past the BMP, and the four spelled with two code points ==
8745   f0a789a7
8748   f0a7b2b1
fa5f   e580a9
8862   c38acc84
8864   c38acc8c
88a3   c3aacc84
88a5   c3aacc8c
== bytes that name no character, and the byte after them ==
80       efbfbd
ff       efbfbd
8139     efbfbd39
81ff     efbfbd
a43f     efbfbd3f
a4407a   e4b8807a
817a     efbfbd7a
8180     efbfbd
a4       efbfbd
a44081   e4b880efbfbd
== the encoder over the whole code space ==
000020 4c9c3ade8441a47d99cdd0c7286c2513
008020 62184c4e830143e2ce1847e946812210
010020 888d4b4d9f668ba2bbfbd20d0b9f3645
018020 888d4b4d9f668ba2bbfbd20d0b9f3645
020020 6c235320ec6ccf426f4761b5882466d8
028020 01451ec82c45051027b8ac36f38996f2
030020 888d4b4d9f668ba2bbfbd20d0b9f3645
038020 888d4b4d9f668ba2bbfbd20d0b9f3645
040020 888d4b4d9f668ba2bbfbd20d0b9f3645
048020 888d4b4d9f668ba2bbfbd20d0b9f3645
050020 888d4b4d9f668ba2bbfbd20d0b9f3645
058020 888d4b4d9f668ba2bbfbd20d0b9f3645
060020 888d4b4d9f668ba2bbfbd20d0b9f3645
068020 888d4b4d9f668ba2bbfbd20d0b9f3645
070020 888d4b4d9f668ba2bbfbd20d0b9f3645
078020 888d4b4d9f668ba2bbfbd20d0b9f3645
080020 888d4b4d9f668ba2bbfbd20d0b9f3645
088020 888d4b4d9f668ba2bbfbd20d0b9f3645
090020 888d4b4d9f668ba2bbfbd20d0b9f3645
098020 888d4b4d9f668ba2bbfbd20d0b9f3645
0A0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0A8020 888d4b4d9f668ba2bbfbd20d0b9f3645
0B0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0B8020 888d4b4d9f668ba2bbfbd20d0b9f3645
0C0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0C8020 888d4b4d9f668ba2bbfbd20d0b9f3645
0D0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0D8020 888d4b4d9f668ba2bbfbd20d0b9f3645
0E0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0E8020 888d4b4d9f668ba2bbfbd20d0b9f3645
0F0020 888d4b4d9f668ba2bbfbd20d0b9f3645
0F8020 888d4b4d9f668ba2bbfbd20d0b9f3645
100020 888d4b4d9f668ba2bbfbd20d0b9f3645
108020 fe1ffa026ee422bf00addedaec92b8b3
U+00080 3f
U+000A5 3f
U+04E00 a440
U+0F6B1 3f
U+27267 3f
U+2A351 3f
U+20000 3f
U+000CA 3f
U+00304 3f
U+000EA 3f
U+0030C 3f
3f3f3f3f
== a whole document round trip ==
Big5
e4b8ade69687
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653ea4a4a4e53c2f7469746c653e3c2f686561643e3c626f64793e3c703ea440a4413f3c2f703e3c2f626f64793e3c2f68746d6c3e
