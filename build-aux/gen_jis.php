<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Generate src/ph7/builtin_jis.h -- the JIS X 0208 <-> Unicode mapping table.
 *
 * Usage:  XDEBUG_MODE=off php build-aux/gen_jis.php [<out-header>]
 *
 * The table is CUT FROM THE ORACLE, not transcribed from a standard: every one
 * of the 94x94 JIS X 0208 cells is handed to the php running this script, in
 * both of the faces php has for it -- `iconv()`, which is the C library's
 * table, and `mb_convert_encoding()`, which is mbstring's own -- and a cell is
 * written out only when the two AGREE. They agree everywhere: all 6879 assigned
 * cells, no cell where one face answers and the other does not. That is worth
 * knowing, because it is the reason one table can serve both extensions.
 *
 * The mapping is also a BIJECTION -- 6879 cells onto 6879 distinct code points
 * -- so the reverse direction is the forward table inverted rather than a
 * second sweep with its own duplicate-target rules. The inverse ships as an
 * index into the forward table, sorted by code point, so it costs two bytes a
 * row instead of four.
 *
 * The framing encodings are NOT here. ISO-2022-JP, EUC-JP and Shift_JIS each
 * put these same cells on the wire differently, and what is common to them --
 * this table, and the two JIS X 0201 sets -- is what this file holds. The X
 * 0201 sets are not tables at all and are asserted here rather than emitted:
 * the Roman set is ASCII with two cells moved (0x5C is U+00A5 and 0x7E is
 * U+203E), and the katakana set is the arithmetic 0xA1..0xDF -> U+FF61+n.
 */

$out = $argv[1] ?? (__DIR__ . '/../src/ph7/builtin_jis.h');

if (!function_exists('iconv') || !function_exists('mb_convert_encoding')) {
    fwrite(STDERR, "need both iconv and mbstring to cut this table\n");
    exit(1);
}

/* --- The sweep ------------------------------------------------------------
 * One cell at a time, wrapped in its ISO-2022-JP framing so that both faces
 * are asked the same question in a spelling they both take. A cell is assigned
 * only if both faces answer, and answer the same thing; mbstring reports an
 * unassigned cell as U+FFFD or '?' rather than by failing, so those are read
 * as "no answer" too.
 */
$fwd = [];      /* linear cell index -> code point */
$disagree = [];
for ($r = 0x21; $r <= 0x7E; $r++) {
    for ($c = 0x21; $c <= 0x7E; $c++) {
        $jis = "\x1b\$B" . chr($r) . chr($c) . "\x1b(B";
        $a = @iconv('ISO-2022-JP', 'UTF-8', $jis);
        $b = @mb_convert_encoding($jis, 'UTF-8', 'ISO-2022-JP');
        $ua = ($a === false || $a === '') ? null : mb_ord($a, 'UTF-8');
        $ub = ($b === false || $b === '' || $b === "\u{FFFD}" || $b === '?')
            ? null : mb_ord($b, 'UTF-8');
        if ($ua !== $ub) {
            $disagree[] = sprintf('%02X%02X: iconv=%s mbstring=%s', $r, $c,
                $ua === null ? '-' : sprintf('U+%04X', $ua),
                $ub === null ? '-' : sprintf('U+%04X', $ub));
            continue;
        }
        if ($ua === null) {
            continue;
        }
        if ($ua > 0xFFFF) {
            fwrite(STDERR, sprintf("cell %02X%02X is U+%04X, outside the BMP\n", $r, $c, $ua));
            exit(1);
        }
        $fwd[($r - 0x21) * 94 + ($c - 0x21)] = $ua;
    }
}
if ($disagree) {
    fwrite(STDERR, "php's two faces disagree on " . count($disagree) . " cells:\n  "
        . implode("\n  ", array_slice($disagree, 0, 20)) . "\n");
    exit(1);
}

/* --- The inverse ----------------------------------------------------------
 * A duplicate target would mean the reverse direction has to choose, and the
 * choice would have to come from the oracle rather than from here -- so a
 * duplicate is a hard stop, not a rule applied quietly.
 */
$rev = [];
foreach ($fwd as $k => $u) {
    if (isset($rev[$u])) {
        fwrite(STDERR, sprintf("U+%04X is the target of two cells (%d and %d)\n", $u, $rev[$u], $k));
        exit(1);
    }
    $rev[$u] = $k;
}
ksort($rev);

/* --- The two JIS X 0201 sets, asserted against the oracle ----------------- */
foreach ([0x5C => 0xA5, 0x7E => 0x203E] as $cell => $want) {
    $got = mb_ord(iconv('ISO-2022-JP', 'UTF-8', "\x1b(J" . chr($cell) . "\x1b(B"), 'UTF-8');
    if ($got !== $want) {
        fwrite(STDERR, sprintf("X0201 Roman %02X is U+%04X, not U+%04X\n", $cell, $got, $want));
        exit(1);
    }
}
for ($c = 0xA1; $c <= 0xDF; $c++) {
    $got = mb_ord(iconv('SJIS', 'UTF-8', chr($c)), 'UTF-8');
    if ($got !== 0xFF61 + ($c - 0xA1)) {
        fwrite(STDERR, sprintf("X0201 katakana %02X is U+%04X, not arithmetic\n", $c, $got));
        exit(1);
    }
}

/* --- Emit ----------------------------------------------------------------- */
$n = count($fwd);
$h = "/**\n"
   . " * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet\x40gmail.com>\n"
   . " * SPDX-License-Identifier: BSD-3-Clause\n"
   . " *\n"
   . " * The JIS X 0208 <-> Unicode mapping table. GENERATED -- do not hand-edit;\n"
   . " * re-cut it with `XDEBUG_MODE=off php build-aux/gen_jis.php`.\n"
   . " *\n"
   . " * aJisX0208Uni is indexed by (row-0x21)*94 + (cell-0x21) over the 94x94 code\n"
   . " * space, and holds the code point a cell stands for or 0 for an unassigned\n"
   . " * one. aJisX0208Rev is those same cells as indices into it, ordered by the\n"
   . " * code point they carry, which is what the reverse direction binary-searches.\n"
   . " *\n"
   . " * Both were cut from php itself -- from iconv() and mb_convert_encoding()\n"
   . " * together, agreeing cell for cell -- and the mapping is one-to-one, so the\n"
   . " * reverse is an inversion rather than a second table with its own rules.\n"
   . " */\n"
   . "#define PH7_JIS_X0208_ROWS   94\n"
   . "#define PH7_JIS_X0208_CELLS  " . (94 * 94) . "\n"
   . "#define PH7_JIS_X0208_COUNT  " . $n . "\n\n"
   . "static const sxu16 aJisX0208Uni[PH7_JIS_X0208_CELLS] = {\n";

$row = [];
for ($i = 0; $i < 94 * 94; $i++) {
    $row[] = sprintf('0x%04X', $fwd[$i] ?? 0);
    if (count($row) === 12) {
        $h .= "\t" . implode(',', $row) . ",\n";
        $row = [];
    }
}
if ($row) {
    $h .= "\t" . implode(',', $row) . ",\n";
}
$h .= "};\n\nstatic const sxu16 aJisX0208Rev[PH7_JIS_X0208_COUNT] = {\n";
$row = [];
foreach ($rev as $u => $k) {
    $row[] = sprintf('%4d', $k);
    if (count($row) === 16) {
        $h .= "\t" . implode(',', $row) . ",\n";
        $row = [];
    }
}
if ($row) {
    $h .= "\t" . implode(',', $row) . ",\n";
}
$h .= "};\n";

file_put_contents($out, $h);
fwrite(STDERR, sprintf("%s: %d assigned cells, both faces agreeing, bijective\n", $out, $n));
