--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
xml_parse_into_struct() answers php's exact row and index shapes, SKIP options, 255-level truncation profile and the warning-not-Error recursion refusal
--FILE--
<?php
// The two output shapes: $values rows (tag/type/level, attributes only when
// present, value merged across entity boundaries and CDATA), and $index
// listing every row number a tag appears at -- opens, cdatas and closes.
$xpi1 = xml_parser_create();
var_dump(xml_parse_into_struct($xpi1, '<mo c="1">lead<in>x&amp;y<![CDATA[q]]></in>mid<em/><deep><d2>z</d2></deep>tail</mo>', $v, $i));
print_r($v);
print_r($i);
// $index is optional; both arrays are RESET, stale contents and all.
$xpi2 = xml_parser_create();
$v2 = ["stale"]; 
var_dump(xml_parse_into_struct($xpi2, "<a></a>", $v2));
print_r($v2);
// SKIP_WHITE drops whitespace-only cdata rows; SKIP_TAGSTART strips the tag
// key (and the ltag the cdata rows are labeled with).
$xpi3 = xml_parser_create();
xml_parser_set_option($xpi3, XML_OPTION_SKIP_WHITE, 1);
xml_parser_set_option($xpi3, XML_OPTION_SKIP_TAGSTART, 3);
xml_parser_set_option($xpi3, XML_OPTION_CASE_FOLDING, 0);
xml_parse_into_struct($xpi3, "<ns:a>\n  <ns:b>x</ns:b>\n</ns:a>", $v3, $i3);
print_r($v3);
print_r($i3);
// A malformed document answers 0 and keeps the rows recorded so far.
$xpi4 = xml_parser_create();
var_dump(xml_parse_into_struct($xpi4, "<a><b></a>", $v4, $i4));
print_r($v4);
var_dump(xml_get_error_code($xpi4));
// The user's own handlers still fire while the rows are recorded.
$xpi5 = xml_parser_create();
xml_set_element_handler($xpi5, function ($p, $n, $a) { echo "S $n\n"; }, null);
xml_set_character_data_handler($xpi5, function ($p, $d) { echo "C"; var_export($d); echo "\n"; });
var_dump(xml_parse_into_struct($xpi5, "<a>t</a>", $v5));
print_r($v5);
// Depth is capped at 255: deeper opens are dropped (with one warning per
// element at depth 256), the level counter keeps counting, and the closes
// come back down with it -- php's exact truncation shape.
set_error_handler(function ($no, $msg) { echo "W[$no] $msg\n"; return true; });
$xpi6 = xml_parser_create();
$deep = str_repeat("<d>", 258) . "x" . str_repeat("</d>", 258);
var_dump(xml_parse_into_struct($xpi6, $deep, $v6, $i6));
restore_error_handler();
echo "rows=", count($v6), "\n";
foreach (array_slice($v6, 252, 6) as $r) {
    echo $r['type'], " L", $r['level'], isset($r['value']) ? " v=" . $r['value'] : "", "\n";
}
// Calling it from inside a handler is a warning + false, not an Error.
$xpi7 = xml_parser_create();
xml_set_element_handler($xpi7, function ($p, $n, $a) {
    set_error_handler(function ($no, $msg) { echo "W[$no] $msg\n"; return true; });
    var_dump(xml_parse_into_struct($p, "<x/>", $inner));
    restore_error_handler();
}, null);
var_dump(xml_parse($xpi7, "<a/>", true));
?>
--EXPECT--
int(1)
Array
(
    [0] => Array
        (
            [tag] => MO
            [type] => open
            [level] => 1
            [attributes] => Array
                (
                    [C] => 1
                )

            [value] => lead
        )

    [1] => Array
        (
            [tag] => IN
            [type] => complete
            [level] => 2
            [value] => x&yq
        )

    [2] => Array
        (
            [tag] => MO
            [value] => mid
            [type] => cdata
            [level] => 1
        )

    [3] => Array
        (
            [tag] => EM
            [type] => complete
            [level] => 2
        )

    [4] => Array
        (
            [tag] => DEEP
            [type] => open
            [level] => 2
        )

    [5] => Array
        (
            [tag] => D2
            [type] => complete
            [level] => 3
            [value] => z
        )

    [6] => Array
        (
            [tag] => DEEP
            [type] => close
            [level] => 2
        )

    [7] => Array
        (
            [tag] => MO
            [value] => tail
            [type] => cdata
            [level] => 1
        )

    [8] => Array
        (
            [tag] => MO
            [type] => close
            [level] => 1
        )

)
Array
(
    [MO] => Array
        (
            [0] => 0
            [1] => 2
            [2] => 7
            [3] => 8
        )

    [IN] => Array
        (
            [0] => 1
        )

    [EM] => Array
        (
            [0] => 3
        )

    [DEEP] => Array
        (
            [0] => 4
            [1] => 6
        )

    [D2] => Array
        (
            [0] => 5
        )

)
int(1)
Array
(
    [0] => Array
        (
            [tag] => A
            [type] => complete
            [level] => 1
        )

)
Array
(
    [0] => Array
        (
            [tag] => a
            [type] => open
            [level] => 1
        )

    [1] => Array
        (
            [tag] => b
            [type] => complete
            [level] => 2
            [value] => x
        )

    [2] => Array
        (
            [tag] => a
            [type] => close
            [level] => 1
        )

)
Array
(
    [a] => Array
        (
            [0] => 0
            [1] => 2
        )

    [b] => Array
        (
            [0] => 1
        )

)
int(0)
Array
(
    [0] => Array
        (
            [tag] => A
            [type] => open
            [level] => 1
        )

    [1] => Array
        (
            [tag] => B
            [type] => open
            [level] => 2
        )

)
int(76)
S A
C't'
int(1)
Array
(
    [0] => Array
        (
            [tag] => A
            [type] => complete
            [level] => 1
            [value] => t
        )

)
W[2] xml_parse_into_struct(): Maximum depth exceeded - Results truncated
int(1)
rows=512
open L253
open L254
complete L255 v=x
close L257
close L256
close L255
W[2] xml_parse_into_struct(): Parser must not be called recursively
bool(false)
int(1)
