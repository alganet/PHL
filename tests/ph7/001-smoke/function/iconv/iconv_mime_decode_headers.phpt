--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_mime_decode_headers() — one field at a time, and what ends the walk
--FILE--
<?php
/* iconv_mime_decode_headers() decodes one field at a time and splits each at
 * its FIRST ':'. A line with none is dropped; a name seen twice becomes a LIST;
 * and a field that decodes to NOTHING AT ALL ends the walk, which is how the
 * blank line between a header block and a body stops it. */
$icvHW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($icvHW);
$icvH = function ($s, $mode = 0, $enc = null) {
    $r = iconv_mime_decode_headers($s, $mode, $enc);
    if ($r === false) { echo "false\n"; return; }
    var_export($r);
    echo "\n";
};
$icvH("Subject: =?UTF-8?B?UHLDvGZ1bmc=?=\r\nTo: a@b\r\n");
$icvH("Set-Cookie: a\r\nSet-Cookie: b\r\n");
$icvH("Set-Cookie: a\r\nSet-Cookie: b\r\nSet-Cookie: c\r\n");
$icvH("A: 1\r\nA: 2\r\nB: 3\r\nA: 4\r\n");
/* A folded value is one field. */
$icvH("Subject: line1\r\n\tline2\r\nX: y\r\n");
/* Bare LF works, and so does a missing trailing newline. */
$icvH("A: 1\nB: 2\n");
$icvH("A: 1");
$icvH("A: 1\r\n");
/* A line with no colon contributes nothing but does not stop the walk. */
$icvH("garbage line\r\nX: y\r\n");
$icvH("A\r\n");
/* An empty NAME is a name; an empty VALUE is a value, and neither ends it. */
$icvH(": novalue\r\n");
$icvH("=?ASCII?B??=\nA: ");
/* The blank line does end it, so a body is never read. */
$icvH("A: 1\r\n\r\nBody: x\r\n");
/* No space after the colon is the same field. */
$icvH("A:1\r\n");
$icvH("");
/* And every $mode reaches the same walk. */
foreach ([1, 2, 3] as $icvM) {
    echo "-- mode $icvM\n";
    $icvH("Subject: =?UTF-8?B?UHLDvGZ1bmc=?=\r\nTo: a@b\r\n", $icvM);
    $icvH("A: =?NOPE?B?YWI=?=\r\nB: 2\r\n", $icvM);
}
$icvH("Subject: =?UTF-8?B?UHLDvGZ1bmc=?=\r\n", 0, "ISO-8859-1");
$icvH("A: 1\r\n", 0, "NOPE");
restore_error_handler();
?>
--EXPECT--
array (
  'Subject' => 'Prüfung',
  'To' => 'a@b',
)
array (
  'Set-Cookie' => 
  array (
    0 => 'a',
    1 => 'b',
  ),
)
array (
  'Set-Cookie' => 
  array (
    0 => 'a',
    1 => 'b',
    2 => 'c',
  ),
)
array (
  'A' => 
  array (
    0 => '1',
    1 => '2',
    2 => '4',
  ),
  'B' => '3',
)
array (
  'Subject' => 'line1 line2',
  'X' => 'y',
)
array (
  'A' => '1',
  'B' => '2',
)
array (
  'A' => '1',
)
array (
  'A' => '1',
)
array (
  'X' => 'y',
)
array (
)
array (
  '' => 'novalue',
)
array (
  'A' => '',
)
array (
  'A' => '1',
)
array (
  'A' => '1',
)
array (
)
-- mode 1
array (
  'Subject' => 'Prüfung',
  'To' => 'a@b',
)
  W: iconv_mime_decode_headers(): Wrong encoding, conversion from "???" to "UTF-8" is not allowed
false
-- mode 2
array (
  'Subject' => 'Prüfung',
  'To' => 'a@b',
)
array (
  'A' => '=?NOPE?B?YWI=?=',
  'B' => '2',
)
-- mode 3
array (
  'Subject' => 'Prüfung',
  'To' => 'a@b',
)
array (
  'A' => '=?NOPE?B?YWI=?=',
  'B' => '2',
)
array (
  'Subject' => 'Pr�fung',
)
  W: iconv_mime_decode_headers(): Wrong encoding, conversion from "???" to "NOPE" is not allowed
false
