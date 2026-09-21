--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgets never returns more than length-1 bytes, even when a line spans read chunks
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgb');
// Lines longer and shorter than the (length-1) cap, plus bare newlines, so a
// capped read leaves a partial line that a later read extends past the cap.
file_put_contents($fn, "abcdefgh\nij\nklmnop");
$f = fopen($fn, 'r');
$out = '';
while (($chunk = fgets($f, 3)) !== false) {   // cap = 2 bytes
    $out .= '[' . str_replace("\n", '\n', $chunk) . ']';
}
echo $out, "\n";
fclose($f);

file_put_contents($fn, "aa\nbbbbbbbbbb\ncc");
$f = fopen($fn, 'r');
$max = 0;
while (($chunk = fgets($f, 3)) !== false) {
    $max = max($max, strlen($chunk));
}
echo "max_piece=", $max, "\n";   // must never exceed length-1 == 2
fclose($f);
unlink($fn);
?>
--EXPECT--
[ab][cd][ef][gh][\n][ij][\n][kl][mn][op]
max_piece=2
--CLEAN--
<?php
