--TEST--
Dom\HTMLDocument decodes and re-encodes GBK and gb18030
--EXTENSIONS--
dom
mbstring
--FILE--
<?php
// GBK and gb18030 through a Dom\HTMLDocument, both directions. Every row is
// php 8.5's answer. These two names share a DECODER and share no ENCODER, and
// both halves of that are measured here: a lead may open a PAIR or a FOUR-byte
// sequence, and only gb18030 ever writes the four-byte one.
$doc = fn($src, $ov = null) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, $ov);
$dec = fn($bytes, $L) => bin2hex($doc("<p>$bytes</p>", $L)->getElementsByTagName('p')[0]->textContent);
// The pointer of a four-byte sequence is its ordinal in the four byte windows:
// 0x81..0xFE, a digit, 0x81..0xFE, a digit.
$quad = fn($p) => chr(0x81 + intdiv($p, 12600)) . chr(0x30 + intdiv($p % 12600, 1260))
                . chr(0x81 + intdiv($p % 1260, 10)) . chr(0x30 + $p % 10);

echo "== the pair space, and the two labels decode it alike ==\n";
foreach (['GBK', 'gb18030'] as $L) {
	$o = '';
	for ($lead = 0x80; $lead <= 0xFF; $lead++) {
		$src = '';
		for ($b = 0x01; $b <= 0xFF; $b++) { $src .= chr($lead) . chr($b) . '|'; }
		$o .= $dec($src, $L) . "\n";
	}
	printf("%-8s %s\n", $L, md5($o));
}

echo "== four rows spelled out ==\n";
foreach ([0x81, 0xA1, 0xFD, 0xFE] as $lead) {
	$src = '';
	for ($b = 0x40; $b <= 0x4F; $b++) { $src .= chr($lead) . chr($b); }
	printf("%02X %s\n", $lead, $dec($src, 'GBK'));
}

echo "== the four-byte form, by pointer ==\n";
// 0 and 1 are U+0080 and U+0081; the run table's shape is what the boundaries
// measure; and a pointer that names nothing still spends all four bytes.
foreach ([0, 1, 7455, 7456, 39419, 39420, 189000, 1237575, 1237576, 1587599] as $p) {
	printf("%7d %-8s %s\n", $p, bin2hex($quad($p)), $dec($quad($p), 'gb18030'));
}

echo "== the framing's two failures, which spend a different number of bytes ==\n";
// Framed right but naming nothing, all four bytes go. Framed wrong, only the
// LEAD goes: the digit behind it is re-read as text and may open a pair of its
// own, which is not what the standard does with it.
foreach (["\x81", "\x81\x30", "\x81\x30\x81", "\x81\x30\x41\x42", "\x81\x30\x81\x41",
          "\x81\x30\x81\xA1", "\x81\x30\x81\x81\x40", "\x81\x39\x81\x39\x81\x40",
          "\x81\x7F\x42", "\x81\xFF\x42", "\x81\x20\x42", "\x81\x2C\x42",
          "\x80\x42", "\xFF\x42", "\xFE\x39\xFE\x39\x42", "\xA1\xA1\x81"] as $m) {
	printf("%-14s %s\n", bin2hex($m), $dec($m, 'gb18030'));
}

echo "== the encoders, which are NOT shared ==\n";
foreach (['GBK', 'gb18030'] as $L) {
	$d = Dom\HTMLDocument::createEmpty($L);
	$one = fn($cp) => bin2hex(substr($d->saveHtml($d->createComment(mb_chr($cp, 'UTF-8'))), 4, -3));
	// The euro is one byte under GBK and a pair under gb18030; the astral and
	// the excluded code points separate the four-byte encoder from `?`.
	printf("%-8s", $L);
	foreach ([0x4E00, 0x20AC, 0x0080, 0x0452, 0xE5E5, 0x10400, 0x10FFFF, 0x9FB4] as $cp) {
		printf(" %s", $one($cp));
	}
	echo "\n";
	$o = ''; $n4 = 0; $nq = 0;
	for ($base = 0x80; $base <= 0x10FFFF; $base += 0x8000) {
		$txt = '';
		for ($cp = $base; $cp < $base + 0x8000 && $cp <= 0x10FFFF; $cp++) {
			if ($cp < 0xD800 || $cp > 0xDFFF) { $txt .= mb_chr($cp, 'UTF-8') . ','; }
		}
		$out = substr($d->saveHtml($d->createComment($txt)), 4, -3);
		$o .= $out;
		foreach (explode(',', $out) as $g) {
			if (strlen($g) === 4) { $n4++; } elseif ($g === '?') { $nq++; }
		}
	}
	printf("%-8s %s four-byte=%d question=%d\n", $L, md5($o), $n4, $nq);
}

echo "== a whole document round trip ==\n";
foreach (['GBK', 'gb18030'] as $L) {
	$d = $doc("<!doctype html><title>\xD6\xD0\xCE\xC4</title><p>\x81\x30\x81\x31\xA1\xA1\x80</p>", $L);
	echo $d->charset, ' ', bin2hex($d->getElementsByTagName('title')[0]->textContent), "\n";
	echo bin2hex($d->saveHtml()), "\n";
}
// saveXml() is deliberately NOT measured: it goes through libxml's own
// encoders, neither name is in the set libxml builds in, and the Windows build
// has no iconv to fall back on -- so it cannot spell the document there
// whatever this engine does.
?>
--EXPECT--
== the pair space, and the two labels decode it alike ==
GBK      07e6bb7d910759071b69035eaf7b31a5
gb18030  07e6bb7d910759071b69035eaf7b31a5
== four rows spelled out ==
81 e4b882e4b884e4b885e4b886e4b88fe4b892e4b897e4b89fe4b8a0e4b8a1e4b8a3e4b8a6e4b8a9e4b8aee4b8afe4b8b1
A1 ee9386ee9387ee9388ee9389ee938aee938bee938cee938dee938eee938fee9390ee9391ee9392ee9393ee9394ee9395
FD e9bcb2e9bcb3e9bcb4e9bcb5e9bcb6e9bcb8e9bcbae9bcbce9bcbfe9bd80e9bd81e9bd82e9bd83e9bd85e9bd86e9bd87
FE efa88cefa88defa88eefa88fefa891efa893efa894efa898efa89fefa8a0efa8a1efa8a3efa8a4efa8a7efa8a8efa8a9
== the four-byte form, by pointer ==
      0 81308130 c280
      1 81308131 c281
   7455 8135f435 e1b8bd
   7456 8135f436 e1b8be
  39419 8431a439 efbfbd
  39420 8431a530 efbfbd
 189000 90308130 f0908080
1237575 e3329a35 f48fbfbf
1237576 e3329a36 efbfbd
1587599 fe39fe39 efbfbd
== the framing's two failures, which spend a different number of bytes ==
81             efbfbd
8130           efbfbd30
813081         efbfbd30efbfbd
81304142       efbfbd304142
81308141       efbfbd30e4b884
813081a1       efbfbd30e4bbad
8130818140     efbfbd30e4ba9640
813981398140   e2ba9be4b882
817f42         efbfbd7f42
81ff42         efbfbd42
812042         efbfbd2042
812c42         efbfbd2c42
8042           e282ac42
ff42           efbfbd42
fe39fe3942     efbfbd42
a1a181         e38080efbfbd
== the encoders, which are NOT shared ==
GBK      d2bb 80 3f 3f 3f 3f 3f fe59
GBK      f76844d1d8eb60122780767706e0c42f four-byte=0 question=1087997
gb18030  d2bb a2e3 81308130 8130d330 3f 9030e734 e3329a35 fe59
gb18030  a50b99b5177193b60fd86667d5c68424 four-byte=1087996 question=1
== a whole document round trip ==
GBK e4b8ade69687
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653ed6d0cec43c2f7469746c653e3c2f686561643e3c626f64793e3c703e3fa1a1803c2f703e3c2f626f64793e3c2f68746d6c3e
gb18030 e4b8ade69687
3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c7469746c653ed6d0cec43c2f7469746c653e3c2f686561643e3c626f64793e3c703e81308131a1a1a2e33c2f703e3c2f626f64793e3c2f68746d6c3e
