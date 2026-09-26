--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_mime_encode() — the line budget, the two schemes and what each option is read as
--FILE--
<?php
/* iconv_mime_encode() builds one RFC 2047 header field. The line budget is a
 * single running counter that every piece of the header subtracts from, and
 * the two schemes spend it differently — base64 works out how many INPUT bytes
 * fit by arithmetic, quoted-printable converts a candidate run, PRICES it and
 * shrinks the run until the price fits. */
$icvEW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($icvEW);
$icvShow = function ($r) { echo $r === false ? "false" : str_replace(["\r", "\n"], ['\r', '\n'], $r), "\n"; };
$icvShow(iconv_mime_encode("Subject", "Prufung"));
$icvShow(iconv_mime_encode("Subject", "Pr\u{fc}fung"));
$icvShow(iconv_mime_encode("Subject", "Pr\u{fc}fung", ["scheme" => "Q"]));
/* The scheme is one LETTER, case-insensitively — and anything else leaves the
 * default where it was rather than failing. */
foreach (["B", "b", "Q", "q", "X", "", "Quoted"] as $icvS) {
    echo str_pad(var_export($icvS, true), 10), " ";
    $icvShow(iconv_mime_encode("S", "Pr\u{fc}f", ["scheme" => $icvS]));
}
/* A non-string option is not coerced, it is ignored. */
$icvShow(iconv_mime_encode("S", "Pr\u{fc}f", ["scheme" => 66]));
/* Both charsets, in both directions. */
$icvShow(iconv_mime_encode("To", "Pr\u{fc}fung", ["output-charset" => "ISO-8859-1"]));
$icvShow(iconv_mime_encode("To", "Pr\u{fc}fung", ["scheme" => "Q", "output-charset" => "ISO-8859-1"]));
$icvShow(iconv_mime_encode("S", "Pr\xFCf", ["input-charset" => "ISO-8859-1"]));
$icvShow(iconv_mime_encode("S", "Pr\xFCf", ["input-charset" => "ISO-8859-1", "output-charset" => "ISO-8859-1"]));
/* Wrapping: where the fold lands is the budget's answer, not the value's. */
$icvShow(iconv_mime_encode("Subject", str_repeat("Pr\u{fc}fung ", 8)));
$icvShow(iconv_mime_encode("Subject", str_repeat("Pr\u{fc}fung ", 8), ["scheme" => "Q"]));
$icvShow(iconv_mime_encode("Subject", "abc def ghi jkl mno pqr stu vwx yz0 123 456", ["scheme" => "Q", "line-length" => 30]));
$icvShow(iconv_mime_encode("Subject", "abc def ghi jkl mno pqr stu vwx yz0 123 456", ["scheme" => "B", "line-length" => 30]));
$icvShow(iconv_mime_encode("Subject", "abc def ghi jkl mno pqr stu vwx", ["scheme" => "Q", "line-length" => 30, "line-break-chars" => "\n"]));
$icvShow(iconv_mime_encode("Subject", "abc def ghi jkl mno pqr stu vwx", ["scheme" => "Q", "line-length" => 30, "line-break-chars" => ""]));
$icvShow(iconv_mime_encode("Subject", str_repeat("Pr\u{fc}fung ", 8), ["line-length" => 1000]));
/* A budget too small for the shortest possible word is refused, and so is a
 * field NAME that cannot fit — the check comes before any converter is opened,
 * which is why a bogus charset beside it is never reported. */
$icvShow(iconv_mime_encode("Subject", "Pr\u{fc}fung", ["line-length" => 10]));
$icvShow(iconv_mime_encode("Subject", "hi", ["line-length" => 0]));
$icvShow(iconv_mime_encode(str_repeat("N", 73), "hi"));
$icvShow(iconv_mime_encode(str_repeat("N", 74), "hi"));
$icvShow(iconv_mime_encode("S", "hi", ["line-length" => 4, "input-charset" => "NOPE"]));
/* An empty value is still a whole encoded word; a field name that is not ASCII
 * contributes nothing at all, while still costing its own length. */
$icvShow(iconv_mime_encode("Subject", ""));
$icvShow(iconv_mime_encode("Subject", "", ["scheme" => "Q"]));
$icvShow(iconv_mime_encode("", "hi"));
$icvShow(iconv_mime_encode("S\u{fc}bject", "hi"));
/* Q escapes what its table says it must, including the space and '?'. */
$icvShow(iconv_mime_encode("Subject", "a=b_c?d e\tf", ["scheme" => "Q"]));
$icvShow(iconv_mime_encode("Subject", "hello world", ["scheme" => "Q"]));
/* A NUL is a byte like any other. */
$icvShow(iconv_mime_encode("S", "a\x00b"));
/* And the two ways it refuses. */
$icvShow(iconv_mime_encode("S", "hi", ["output-charset" => "NOPE"]));
$icvShow(iconv_mime_encode("S", "hi", ["input-charset" => "NOPE"]));
$icvShow(iconv_mime_encode("S", "hi", ["output-charset" => str_repeat("X", 64)]));
$icvShow(iconv_mime_encode("S", "\u{4e2d}", ["output-charset" => "ISO-8859-1"]));
$icvShow(iconv_mime_encode("S", "a\xFFb"));
restore_error_handler();
?>
--EXPECT--
Subject: =?UTF-8?B?UHJ1ZnVuZw==?=
Subject: =?UTF-8?B?UHLDvGZ1bmc=?=
Subject: =?UTF-8?Q?Pr=C3=BCfung?=
'B'        S: =?UTF-8?B?UHLDvGY=?=
'b'        S: =?UTF-8?B?UHLDvGY=?=
'Q'        S: =?UTF-8?Q?Pr=C3=BCf?=
'q'        S: =?UTF-8?Q?Pr=C3=BCf?=
'X'        S: =?UTF-8?B?UHLDvGY=?=
''         S: =?UTF-8?B?UHLDvGY=?=
'Quoted'   S: =?UTF-8?Q?Pr=C3=BCf?=
S: =?UTF-8?B?UHLDvGY=?=
To: =?ISO-8859-1?B?UHL8ZnVuZw==?=
To: =?ISO-8859-1?Q?Pr=FCfung?=
S: =?UTF-8?B?UHLDvGY=?=
S: =?ISO-8859-1?B?UHL8Zg==?=
Subject: =?UTF-8?B?UHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmc=?=\r\n =?UTF-8?B?IFByw7xmdW5nIFByw7xmdW5nIFByw7xmdW5nIFByw7xmdW5nIA==?=
Subject: =?UTF-8?Q?Pr=C3=BCfung=20Pr=C3=BCfung=20Pr=C3=BCfung=20Pr=C3=BCfu?=\r\n =?UTF-8?Q?ng=20Pr=C3=BCfung=20Pr=C3=BCfung=20Pr=C3=BCfung=20Pr=C3=BCfung?=\r\n =?UTF-8?Q?=20?=
Subject: =?UTF-8?Q?abc=20def?=\r\n =?UTF-8?Q?=20ghi=20jkl=20mn?=\r\n =?UTF-8?Q?o=20pqr=20stu=20v?=\r\n =?UTF-8?Q?wx=20yz0=20123=20?=\r\n =?UTF-8?Q?456?=
Subject: =?UTF-8?B?YWI=?=\r\n =?UTF-8?B?YyBkZWYgZ2g=?=\r\n =?UTF-8?B?aSBqa2wgbW4=?=\r\n =?UTF-8?B?byBwcXIgc3Q=?=\r\n =?UTF-8?B?dSB2d3ggeXo=?=\r\n =?UTF-8?B?MCAxMjMgNDU=?=\r\n =?UTF-8?B?Ng==?=
Subject: =?UTF-8?Q?abc=20def?=\n =?UTF-8?Q?=20ghi=20jkl=20mn?=\n =?UTF-8?Q?o=20pqr=20stu=20v?=\n =?UTF-8?Q?wx?=
Subject: =?UTF-8?Q?abc=20def?= =?UTF-8?Q?=20ghi=20jkl=20mn?= =?UTF-8?Q?o=20pqr=20stu=20v?= =?UTF-8?Q?wx?=
Subject: =?UTF-8?B?UHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcgUHLDvGZ1bmcg?=
  W: iconv_mime_encode(): Buffer length exceeded
false
  W: iconv_mime_encode(): Buffer length exceeded
false
NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN: \r\n =?UTF-8?B?aGk=?=
  W: iconv_mime_encode(): Buffer length exceeded
false
  W: iconv_mime_encode(): Buffer length exceeded
false
Subject: =?UTF-8?B??=
Subject: =?UTF-8?Q??=
: =?UTF-8?B?aGk=?=
: =?UTF-8?B?aGk=?=
Subject: =?UTF-8?Q?a=3Db=5Fc=3Fd=20e=09f?=
Subject: =?UTF-8?Q?hello=20world?=
S: =?UTF-8?B?YQBi?=
  W: iconv_mime_encode(): Wrong encoding, conversion from "UTF-8" to "NOPE" is not allowed
false
  W: iconv_mime_encode(): Wrong encoding, conversion from "NOPE" to "UTF-8" is not allowed
false
  W: iconv_mime_encode(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
  W: iconv_mime_encode(): Detected an illegal character in input string
false
  W: iconv_mime_encode(): Detected an illegal character in input string
false
