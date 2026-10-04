--TEST--
Dom\HTMLDocument decodes and re-encodes EUC-KR
--EXTENSIONS--
dom
mbstring
--FILE--
<?php
// EUC-KR through a Dom\HTMLDocument, both directions. Every row is php 8.5's
// answer. Of php's CJK names this is the only one that is a plain table of
// two-byte cells: GBK's decoder is gb18030's and reads four-byte sequences,
// Big5 reaches past the BMP, EUC-JP has a second code set and ISO-2022-JP has
// shift states.
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$dec = fn($bytes) => bin2hex($doc("<p>$bytes</p>", 'EUC-KR')->getElementsByTagName('p')[0]->textContent);

echo "== the cell space, one digest per lead ==\n";
foreach (range(0x81, 0xFE) as $L) {
	$src = '';
	for ($b = 0x40; $b <= 0xFE; $b++) { $src .= chr($L) . chr($b) . ','; }
	printf("%02X %s\n", $L, md5($doc("<p>$src</p>", 'EUC-KR')->getElementsByTagName('p')[0]->textContent));
}

echo "== every single byte ==\n";
$o = '';
for ($b = 1; $b < 0x100; $b++) { $o .= $dec(chr($b)) . '|'; }
echo md5($o), "\n";

echo "== a broken pair, and the byte after it ==\n";
foreach (["\xA4\x40", "\x80\x41", "\x81\x40", "\xFE\xFE", "\xA1\x7F", "\xA1\xFF", "\xA1\x20",
          "\x81", "\x81\x81", "\xFF\x41", "\xB0\xA1\x81"] as $m) {
	printf("%-12s %s\n", bin2hex($m), $dec($m));
}

echo "== the encoder over the whole BMP ==\n";
$d = Dom\HTMLDocument::createEmpty('EUC-KR');
$txt = '';
for ($cp = 0x20; $cp <= 0xFFFF; $cp++) {
	if ($cp < 0xD800 || $cp > 0xDFFF) { $txt .= mb_chr($cp, 'UTF-8') . ','; }
}
echo md5($d->saveHtml($d->createComment($txt))), "\n";
foreach ([0x80, 0xA9, 0x20AC, 0x4E00, 0xAC00, 0xD7A3, 0x3000, 0xFF21, 0xE9, 0x1F600] as $cp) {
	$c = $d->createComment(mb_chr($cp, 'UTF-8'));
	printf("U+%04X %s\n", $cp, bin2hex(substr($d->saveHtml($c), 4, -3)));
}

echo "== a whole document round trip ==\n";
$d = $doc("<!doctype html><title>\xC7\xD1\xB1\xB9\xBE\xEE</title><p>\xB0\xA1\xB3\xAA\xB4\xD9</p>", 'EUC-KR');
echo $d->charset, "\n";
echo bin2hex($d->getElementsByTagName('title')[0]->textContent), "\n";
echo bin2hex($d->saveHtml()), "\n";
// saveXml() is deliberately NOT measured here: it goes through libxml's own
// encoders, and EUC-KR is outside the set libxml builds in, so the Windows
// build -- which has no iconv to fall back on -- cannot spell the document at
// all and warns instead. That is libxml's build talking, not this engine.
?>
--EXPECT--
== the cell space, one digest per lead ==
81 6b490f26f7f6e64adbcae301094cad00
82 5c676f267bec98e193d512693c09fdfa
83 7d1e1fc0e9b5a2bc916dc5c8da939f6f
84 e467c8da504c886fcc38c5b48f2f60e4
85 57ea249ff70156b93526eb2aaed96e92
86 88249e873ea157082d585a9e0fbda302
87 1ca863e6bdea9118b1c47cb61fead337
88 f8b607f102cd493910bf55606169ce0e
89 6b818ab5f383a177ea4b136e8fc336cb
8A 5d414799c75139582dfdf408dc198e95
8B d32388b97c391f138e9eca4e2c9f1c21
8C d890337ec580df20fb013da030534a86
8D f16ccac5e747ad8ac015f7b174c99edd
8E 73e3367d24d8604b128da5a51529b6d9
8F 2f1e7dd7620e03d0070e406008a3ccab
90 f4247148a11786888b40539f3e020837
91 69476267d93aa134f00f45a986cbe810
92 408d73e587248f8e83c989093940283a
93 539cb63d7dfd32e2a0e745d9c06a3f84
94 08efd8c521016d1da23d1068b790e112
95 96c8d57093540cef7a1fa941d317a833
96 09cae2aac7ab5f2c080d6de78eaa0c87
97 92df5553f81a4b7313b6117b6cd58109
98 8054837644250755cbd0875b69acdeb8
99 8efc96ba8938c4fbc53ff3b720d933b3
9A b65c3d4bfb46d769f6e73f71d22684ba
9B 33cbc99fd15fa069cefa0d90a19e37fb
9C e439fddd09eb68572f86cb9fb5265c62
9D 63e3c31321899f2218a14648681ce32a
9E 70476e42bf4f280a547d743d0408b00d
9F 15633ec77015d77e65b3d31dd1e6befb
A0 c6001b0a1bda0b931f866932d1aa9168
A1 d5cdd311e1db736738c196e3e437e362
A2 ffe314a33db66b0bdb3f79b3c20adfe0
A3 d1dfddef72b2b0701c045415d47c6f04
A4 804a7b17402ebf2ab466663b5feae7a6
A5 5dc2b0d15f3fcee70fd1e96ebde53fb1
A6 e2d2eab9d2c621854c946d33a3fc9bb8
A7 de06bd7d0a32aa6f907147fa9f74faf8
A8 7830eb2c976cc98a9db6756832ff76aa
A9 48f1ba5314e16f4601dd168b67bbe460
AA d2fdb4eeecd4462e9c892a66514904c4
AB c2c1072a1aa61d83f9c109272cf7804b
AC 1776705fd56ef316309a80cd427196c4
AD 419e9f378a56221e80d8fc9f80d3ea32
AE e2a54630fa8f5c8473b6645d8822e448
AF f0f9d22150d363680fbbbec943481269
B0 509a56a644fcf61518658a0e49ee2052
B1 ae5f80264e151c8a5fd635c91acfff15
B2 6d3de58e3176850dbf3a5e9ed47182c5
B3 00a6858c6be7a29f97b066d9326b9335
B4 229fc92ed7e173bb317879bfe0fa668e
B5 93cbac934e7b177bd4afb350d5edb85d
B6 c58a23ffcb315c55b83a5a42f602aeea
B7 f8508875645f8c0b12564a70b28a697d
B8 14d13ef3abefae073c33daefef35cf1a
B9 4cd247450fef6f6a9d3c3e1c7bba991e
BA 108a36564faa21658062f959c1078d80
BB 7c2a0438efb47a052f7be23c1f7ed26b
BC b22ae06e56dbbc6459a75af3ab16089e
BD 35f97d5222750b8c6580a1d800de3f1a
BE 35633465fff313dfd6eeccae7138d79c
BF 9b085d7cbf614552bfcdb9cb0b63dd70
C0 7e9989c8ff170a9a033546fe487f53ba
C1 38c1dcf36c7731a611460c46c569d5ce
C2 8b925f6d56d1ba97232d7b34e1024be5
C3 2738d3ec9b09d91495ea8e27d9b0dd29
C4 8c36f8c8387d241c2165bca59a4ae417
C5 bb8b18e80d789a7b64d52b717529e482
C6 7766956b399230cb13d34015cac700b5
C7 347a9c88f48825b5317530b599d30731
C8 22617f32cd027310ffb137b712858c85
C9 66188bfc8ea063d54eb74e7c80f69de1
CA d6011617e76c65ce161bb23c295f9011
CB 10479a775f183b0d35b54268acf1a940
CC bf5d1c19606e3447929efc054e588463
CD 1fbc5bae5ce133d97edcaa8377c96bb4
CE 0f13e987926444ff9dfa6621e5ddaf35
CF 8289e46ffe38820c78ceeff06a5a7936
D0 7b2a40f954bf651476c5d92e47d1eaec
D1 834983cb276b20e886055b3980cb954f
D2 58cd42386e159463bbd83eb8715c2c8a
D3 f6952bf4e7cb52008f1bae3b93065a95
D4 9c8a3457e58225d3415214eca76fd982
D5 57a7ca5f3e19848d66524cb0c92b7f09
D6 73c44b418566dcfe639ed2cf2a2cac09
D7 9d35762bc05e15bd26aa411611c716c9
D8 c5d2be341c4b5bb8b1cb43fa0a776b97
D9 8675bf6394de3eb6ab0b9f4b62143b82
DA 17ed8e93994b3eb34895cd49397b1551
DB ec36b2fa70caf8fdd45047a066d863ee
DC 62eb2f084f2abf76ec8c451a48c79cd9
DD bbcff496473901f55e274fdb69fb4ce0
DE 78bb957bcfd97556ea93b6e6d9e56ae1
DF 91da2c33a55a1c9897d4e1b7112cfad3
E0 4e3ad180136edd59ae8f8104f9e6a914
E1 4e8fbfb5daa3c2d575b8d106d90006a8
E2 33abe7e026b0a1234e3f7fb97119c5db
E3 9b769c24f06ddf6f7f78771c53420845
E4 ea083e98c1eff48fae4587f280318172
E5 ea5539b8acba52c8f73c30fa94e0bf7c
E6 03f97880ec2b5a52afdb6d0d35c6fe66
E7 372f0d8c25cd098204a7ac6e881249b9
E8 78be5ba3ef6075293abf6eb7e554ce7f
E9 ff75b9e3f1d893298653d23e7a78ef71
EA 1156e7c6f07d888871442751d243648f
EB dcecba2064010185ea3ab6765e25705d
EC e73bd66c696c5a25fb561f714386dae3
ED 7d00fb67764d96de873c73debb53e245
EE a73fe2921260985aecdb951a6ff4db13
EF 9866e9b6ff3151ef320d7a45d1eeaa8e
F0 ee916838fd8887d2eb3c9770e5d68eef
F1 b6c27917c2b047cca5e659bdb3d36bbc
F2 bb8d217e92f80f0bbbaa26df11a77fd9
F3 1aceaa8419fca1ff44bef91ae0f49b49
F4 21b6d770816acc4e319656362064eb83
F5 0aeea442e144c57186ba28301fb5fd69
F6 e2efab3fc51d71a37e186df27476ad55
F7 5011249c41219c8639d16ecc18fdaf27
F8 3424e49a1bfa75a0faa5c88eb08582e3
F9 5f8bef9a1a6593046c085269f4486f26
FA 04edebd585c72a53b0a90f8de0f873fe
FB f327e2317ff9c71345a7ba246e3ca987
FC 5ea18679b1b553624d3e3bfdc0aaac72
FD eaf68cb98972fb32f01dca04ada7f99f
FE 66188bfc8ea063d54eb74e7c80f69de1
== every single byte ==
52d13cb91a61c8a31198db1df06700be
== a broken pair, and the byte after it ==
a440         efbfbd40
8041         efbfbd41
8140         efbfbd40
fefe         efbfbd
a17f         efbfbd7f
a1ff         efbfbd
a120         efbfbd20
81           efbfbd
8181         eab196
ff41         efbfbd41
b0a181       eab080efbfbd
== the encoder over the whole BMP ==
cd841e088cf8ab51d5ed4967db9f627c
U+0080 3f
U+00A9 3f
U+20AC a2e6
U+4E00 ece9
U+AC00 b0a1
U+D7A3 c652
U+3000 a1a1
U+FF21 a3c1
U+00E9 3f
U+1F600 3f
== a whole document round trip ==
EUC-KR
ed959ceab5adec96b4
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653ec7d1b1b9beee3c2f7469746c653e3c2f686561643e3c626f64793e3c703eb0a1b3aab4d93c2f703e3c2f626f64793e3c2f68746d6c3e
