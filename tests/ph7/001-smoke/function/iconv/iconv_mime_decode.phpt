--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_mime_decode() — the scanner, and what an encoded word that is not one becomes
--FILE--
<?php
/* iconv_mime_decode() reads RFC 2047 encoded words out of a header. What makes
 * it a SCANNER rather than a matcher is what it does with the shapes the RFC
 * does not allow: a word that turns out not to be one is re-emitted as the raw
 * text it was, and the whitespace BETWEEN two words disappears while the same
 * whitespace beside plain text stays. */
$icvDW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($icvDW);
$icvD = function ($s, $mode = 0, $enc = null) {
    $r = iconv_mime_decode($s, $mode, $enc);
    echo $r === false ? "false" : "'" . str_replace(["\r", "\n"], ['\r', '\n'], $r) . "'", "\n";
};
$icvD("Subject: =?UTF-8?B?UHLDvGZ1bmc=?=");
$icvD("Subject: =?ISO-8859-1?Q?Pr=FCfung?=");
$icvD("=?UTF-8?B?UHLDvGZ1bmc=?=");
/* Adjacent words JOIN; a word beside plain text does not. */
$icvD("=?UTF-8?B?YWI=?= =?UTF-8?B?Y2Q=?=");
$icvD("=?UTF-8?B?YWI=?=  \t =?UTF-8?B?Y2Q=?=");
$icvD("=?UTF-8?B?YWI=?= plain =?UTF-8?B?Y2Q=?=");
$icvD("=?UTF-8?B?YWI=?=x=?UTF-8?B?Y2Q=?=");
/* A folded header is one field; an unfolded newline ends it. */
$icvD("Subject: abc\r\n def");
$icvD("Subject: abc\n\tdef");
$icvD("Subject: abc\r\ndef");
$icvD("Subject: abc\rdef");
/* Q's '_' is a space; B's alphabet skips everything outside it, padding
 * included, so ":!!!=ZZ" still decodes its "ZZ". */
$icvD("=?UTF-8?Q?a_b?=");
$icvD("=?UTF-8?Q?a=20b?=");
$icvD("=?UTF-8?B?!!!?=");
$icvD("=?ASCII?B?:!!!=ZZ?=");
$icvD("=?UTF-8?B??=");
$icvD("=?UTF-8?Q??=");
/* A language tag after the charset is dismissed. */
$icvD("=?UTF-8*en?B?YWI=?=");
/* Every way a word can fail to be one, at each $mode. STRICT (1) refuses the
 * non-RFC shapes php otherwise takes; CONTINUE_ON_ERROR (2) turns a refusal
 * into "hand the raw word over and carry on". */
foreach ([0, 1, 2, 3] as $icvM) {
    echo "-- mode $icvM\n";
    foreach (["=?NOPE?B?YWI=?=", "x =?NOPE?B?YWI=?= y", "=?UTF-8?X?abc?=", "a =? b",
              "=?UTF-8?B?YWI=?", "=?UTF-8?B?YWI", "=?UTF-8?", "=?UTF-8", "=?", "=",
              "=?UTF-8?B?YWI=?=x", "plain text only", ""] as $icvC) {
        echo "   ", str_pad(var_export($icvC, true), 22), " ";
        $icvD($icvC, $icvM);
    }
}
/* The output charset is the third argument, and it decides what can come out. */
$icvD("=?UTF-8?B?UHLDvGZ1bmc=?=", 0, "ISO-8859-1");
$icvD("=?UTF-8?Q?=E2=82=AC?=", 0, "ISO-8859-1");
$icvD("=?ISO-8859-1?Q?=FF?=", 0, "UTF-8");
$icvD("=?ASCII?B?YWI=?=", 0, "ASCII");
/* And the two ways the call itself is refused. */
$icvD("abc", 0, "NOPE");
$icvD("abc", 0, str_repeat("X", 64));
restore_error_handler();
?>
--EXPECT--
'Subject: Prüfung'
'Subject: Prüfung'
'Prüfung'
'abcd'
'abcd'
'ab plain cd'
'abxcd'
'Subject: abc def'
'Subject: abc def'
'Subject: abc'
'Subject: abc\rdef'
'a b'
'a b'
''
'e'
''
''
'ab'
-- mode 0
   '=?NOPE?B?YWI=?='        W: iconv_mime_decode(): Wrong encoding, conversion from "???" to "UTF-8" is not allowed
false
   'x =?NOPE?B?YWI=?= y'    W: iconv_mime_decode(): Wrong encoding, conversion from "???" to "UTF-8" is not allowed
false
   '=?UTF-8?X?abc?='        W: iconv_mime_decode(): Malformed string
false
   'a =? b'                 W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI=?'        W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI'          W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?'               W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8'                W: iconv_mime_decode(): Malformed string
false
   '=?'                     W: iconv_mime_decode(): Malformed string
false
   '='                      W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI=?=x'    'abx'
   'plain text only'      'plain text only'
   ''                     ''
-- mode 1
   '=?NOPE?B?YWI=?='        W: iconv_mime_decode(): Wrong encoding, conversion from "???" to "UTF-8" is not allowed
false
   'x =?NOPE?B?YWI=?= y'    W: iconv_mime_decode(): Wrong encoding, conversion from "???" to "UTF-8" is not allowed
false
   '=?UTF-8?X?abc?='        W: iconv_mime_decode(): Malformed string
false
   'a =? b'                 W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI=?'        W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI'          W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?'               W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8'                W: iconv_mime_decode(): Malformed string
false
   '=?'                     W: iconv_mime_decode(): Malformed string
false
   '='                      W: iconv_mime_decode(): Malformed string
false
   '=?UTF-8?B?YWI=?=x'    '=?UTF-8?B?YWI=?=x'
   'plain text only'      'plain text only'
   ''                     ''
-- mode 2
   '=?NOPE?B?YWI=?='      '=?NOPE?B?YWI=?='
   'x =?NOPE?B?YWI=?= y'  'x =?NOPE?B?YWI=?= y'
   '=?UTF-8?X?abc?='      '=?UTF-8?X?abc?='
   'a =? b'               'a '
   '=?UTF-8?B?YWI=?'      ''
   '=?UTF-8?B?YWI'        ''
   '=?UTF-8?'             ''
   '=?UTF-8'              ''
   '=?'                   ''
   '='                    '='
   '=?UTF-8?B?YWI=?=x'    'abx'
   'plain text only'      'plain text only'
   ''                     ''
-- mode 3
   '=?NOPE?B?YWI=?='      '=?NOPE?B?YWI=?='
   'x =?NOPE?B?YWI=?= y'  'x =?NOPE?B?YWI=?= y'
   '=?UTF-8?X?abc?='      '=?UTF-8?X?abc?='
   'a =? b'               'a '
   '=?UTF-8?B?YWI=?'      ''
   '=?UTF-8?B?YWI'        ''
   '=?UTF-8?'             ''
   '=?UTF-8'              ''
   '=?'                   ''
   '='                    '='
   '=?UTF-8?B?YWI=?=x'    '=?UTF-8?B?YWI=?=x'
   'plain text only'      'plain text only'
   ''                     ''
'Pr�fung'
  W: iconv_mime_decode(): Detected an illegal character in input string
false
'ÿ'
'ab'
  W: iconv_mime_decode(): Wrong encoding, conversion from "???" to "NOPE" is not allowed
false
  W: iconv_mime_decode(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
