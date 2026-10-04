--TEST--
Dom\HTMLDocument decodes and re-encodes ISO-2022-JP, escapes and all
--EXTENSIONS--
dom
--FILE--
<?php
// ISO-2022-JP is the one encoding of the forty that is not a table lookup: an
// escape sequence selects a code set and every byte after it is read in that
// set until the next escape, so both faces carry a state. Every row here is
// php 8.5's answer.
$doc = fn($src) => Dom\HTMLDocument::createFromString($src, LIBXML_NOERROR, 'ISO-2022-JP');
// A <style> element is RAWTEXT, so a decoded byte reaches textContent without
// the tokenizer taking `<` or `&` off it -- which matters here, because a
// refused escape hands its own bytes back as text.
$dec = function (string $bytes) use ($doc) {
	$d = $doc("<!doctype html><style>$bytes</style>");
	$t = $d->getElementsByTagName('style')[0]->textContent;
	$o = [];
	for ($i = 0, $n = mb_strlen($t, 'UTF-8'); $i < $n; $i++) {
		$o[] = sprintf('%04X', mb_ord(mb_substr($t, $i, 1, 'UTF-8'), 'UTF-8'));
	}
	return implode(' ', $o);
};
$enc = function (string $utf8) {
	$d = Dom\HTMLDocument::createEmpty('ISO-2022-JP');
	$s = $d->saveHtml($d->createComment($utf8));
	$o = '';
	for ($i = 0; $i < strlen($s); $i++) {
		$o .= ($s[$i] >= ' ' && $s[$i] <= '~') ? $s[$i] : sprintf('<%02X>', ord($s[$i]));
	}
	return $o;
};
echo "== the five escapes, and the two bytes behind each ==\n";
foreach (['(B' => 'ASCII', '(J' => 'Roman', '(I' => 'katakana',
          '$@' => 'pairs', '$B' => 'pairs'] as $e => $name) {
	printf("  ESC %s %-8s %s\n", $e, $name, $dec("\x1b$e\x21\x21\x1b(B"));
}
echo "== and three that name no set: the escape is one replacement and its\n";
echo "   own bytes are then read as text ==\n";
foreach (['$(D', '(A', '.F'] as $e) {
	printf("  ESC %-4s %s\n", $e, $dec("\x1b$e\x21\x21\x1b(B"));
}
echo "== the ASCII set, the two bytes Roman moves, and the katakana run ==\n";
foreach (['(B' => [0x41, 0x5C, 0x7E, 0x0E, 0x0F, 0x80, 0xFF],
          '(J' => [0x41, 0x5C, 0x7E, 0x0E, 0x80],
          '(I' => [0x20, 0x21, 0x30, 0x5F, 0x60, 0x80]] as $e => $bs) {
	$o = [];
	foreach ($bs as $b) { $o[] = sprintf('%02X=%s', $b, $dec("\x1b$e" . chr($b) . "\x1b(B")); }
	printf("  ESC %s  %s\n", $e, implode('  ', $o));
}
echo "== the pair set: the lead window, an undefined cell, and a byte that is\n";
echo "   no lead at all ==\n";
foreach ([[0x21, 0x21], [0x21, 0x7E], [0x7E, 0x7E], [0x23, 0x21], [0x21, 0x20],
          [0x21, 0xFF], [0x20, 0x21], [0x7F, 0x21]] as [$l, $t]) {
	printf("  %02X %02X -> %s\n", $l, $t, $dec("\x1b\$B" . chr($l) . chr($t) . "\x1b(B"));
}
echo "== php's output flag: an escape with no character before it is one\n";
echo "   replacement, and still selects its set ==\n";
foreach (['(B(B' => "\x1b(B\x1b(B\x21", '(Ja(B' => "\x1b(Ja\x1b(B\x21",
          '$B$B' => "\x1b\$B\x1b\$B\x21\x21\x1b(B",
          'lead ESC' => "\x1b\$B\x21\x1b(B\x21",
          'lead EOF' => "\x1b\$B\x21"] as $name => $bytes) {
	printf("  %-9s %s\n", $name, $dec($bytes));
}
echo "== the encoder shifts in, shifts back out, and ends in the ASCII set ==\n";
foreach (["a\u{3000}b", "\u{3000}\u{3001}", "\u{00A5}", "a\u{00A5}b", "\u{203E}",
          "\u{00A5}\\", "\u{00A5}~", "\u{3000}\u{00A5}", "\u{00A5}\u{3000}",
          "\u{FF61}", "\u{FF71}", "\u{2212}", "\u{FF0D}", "\u{2603}a",
          "\u{3000}\u{2603}a", "\u{FFFD}", "\u{1F600}"] as $s) {
	printf("  %-14s %s\n", bin2hex($s), $enc($s));
}
echo "== and the three bytes it refuses, which it refuses only once it is\n";
echo "   already in the ASCII or Roman set ==\n";
foreach (["a\u{000E}b", "a\u{000F}b", "a\u{001B}b", "\u{00A5}\u{001B}",
          "\u{3000}\u{001B}z"] as $s) {
	printf("  %-14s %s\n", bin2hex($s), $enc($s));
}
echo "== a document round trip, through both the tree and saveHtml() ==\n";
$d = $doc("<p>\x1b\$B\x24\x22\x24\x24\x1b(Babc</p>");
printf("  charset  %s\n", $d->charset);
printf("  text     %s\n", bin2hex($d->getElementsByTagName('p')[0]->textContent));
printf("  saveHtml %s\n", bin2hex($d->saveHtml($d->getElementsByTagName('p')[0])));
$d = Dom\HTMLDocument::createFromString('<!doctype html><p>x</p>', 0, 'ISO-2022-JP');
$d->getElementsByTagName('p')[0]->textContent = "a\u{00A5}b\u{3000}c";
printf("  assigned %s\n", bin2hex($d->saveHtml()));
// innerHTML is UTF-8 whatever the document says, the way it is for every other
// non-UTF-8 name.
printf("  innerHTML %s\n", bin2hex($d->getElementsByTagName('p')[0]->innerHTML));
?>
--EXPECT--
== the five escapes, and the two bytes behind each ==
  ESC (B ASCII    0021 0021
  ESC (J Roman    0021 0021
  ESC (I katakana FF61 FF61
  ESC $@ pairs    3000
  ESC $B pairs    3000
== and three that name no set: the escape is one replacement and its
   own bytes are then read as text ==
  ESC $(D  FFFD 0024 0028 0044 0021 0021
  ESC (A   FFFD 0028 0041 0021 0021
  ESC .F   FFFD 002E 0046 0021 0021
== the ASCII set, the two bytes Roman moves, and the katakana run ==
  ESC (B  41=0041  5C=005C  7E=007E  0E=FFFD  0F=FFFD  80=FFFD  FF=FFFD
  ESC (J  41=0041  5C=00A5  7E=203E  0E=FFFD  80=FFFD
  ESC (I  20=FFFD  21=FF61  30=FF70  5F=FF9F  60=FFFD  80=FFFD
== the pair set: the lead window, an undefined cell, and a byte that is
   no lead at all ==
  21 21 -> 3000
  21 7E -> 25C7
  7E 7E -> FFFD
  23 21 -> FFFD
  21 20 -> FFFD
  21 FF -> FFFD
  20 21 -> FFFD FFFD
  7F 21 -> FFFD FFFD
== php's output flag: an escape with no character before it is one
   replacement, and still selects its set ==
  (B(B      FFFD 0021
  (Ja(B     0061 0021
  $B$B      FFFD 3000
  lead ESC  FFFD 0021
  lead EOF  30FC FFFD FFFD 8DC2 FFFD
== the encoder shifts in, shifts back out, and ends in the ASCII set ==
  61e3808062     <!--a<1B>$B!!<1B>(Bb-->
  e38080e38081   <!--<1B>$B!!!"<1B>(B-->
  c2a5           <!--<1B>(J\--><1B>(B
  61c2a562       <!--a<1B>(J\b--><1B>(B
  e280be         <!--<1B>(J~--><1B>(B
  c2a55c         <!--<1B>(J\<1B>(B\-->
  c2a57e         <!--<1B>(J\<1B>(B~-->
  e38080c2a5     <!--<1B>$B!!<1B>(J\--><1B>(B
  c2a5e38080     <!--<1B>(J\<1B>$B!!<1B>(B-->
  efbda1         <!--<1B>$B!#<1B>(B-->
  efbdb1         <!--<1B>$B%"<1B>(B-->
  e28892         <!--<1B>$B!]<1B>(B-->
  efbc8d         <!--<1B>$B!]<1B>(B-->
  e2988361       <!--?a-->
  e38080e2988361 <!--<1B>$B!!?<1B>(Ba-->
  efbfbd         <!--?-->
  f09f9880       <!--?-->
== and the three bytes it refuses, which it refuses only once it is
   already in the ASCII or Roman set ==
  610e62         <!--a?b-->
  610f62         <!--a?b-->
  611b62         <!--a?b-->
  c2a51b         <!--<1B>(J\?--><1B>(B
  e380801b7a     <!--<1B>$B!!<1B>(B<1B>z-->
== a document round trip, through both the tree and saveHtml() ==
  charset  ISO-2022-JP
  text     e38182e38184616263
  saveHtml 3c703e1b2442242224241b28426162633c2f703e
  assigned 3c21444f43545950452068746d6c3e3c68746d6c3e3c686561643e3c2f686561643e3c626f64793e3c703e611b284a5c621b244221211b2842633c2f703e3c2f626f64793e3c2f68746d6c3e
  innerHTML 61c2a562e3808063
