--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
how php's format scanner walks a format beside its input
--FILE--
<?php
/* php's format scanner walks the two strings TOGETHER and stops the moment the
 * INPUT runs out; what is left of the format is judged afterwards, where the
 * two reset specifiers and `+` are allowed to stand and the first specifier
 * that wants input is one refusal. Nothing aborts the walk: a literal that does
 * not match costs a refusal and steps over the input byte anyway, so the two
 * strings stay in step, while a SEPARATOR that does not match refuses and
 * leaves the byte where it is. A format space eats a whole run of blanks --
 * the two Unicode ones included -- and never refuses at all. */
date_default_timezone_set('UTC');
$ffScan = [
    ['Y-m-d', '2020-01-02'], ['q', 'q'], ['q', 'r'], ['qq', 'ab'], ['q-q', 'a-b'],
    ['--', 'ab'], ['-', 'a'], ['.', 'x'], ['#', 'x'], ['#', '-'], ['#', '('],
    [' ', 'x'], [' ', ''], ['  ', '   '], [' ', " \t  x"], [' ', "\xc2\xa0"],
    [' ', "\xe2\x80\xaf"],
    ['\\a', 'a'], ['\\a', 'b'], ['\\', ''], ['\\', 'a'], ['q\\', 'q'],
    ['*', ''], ['*', 'a'], ['*', 'abc'], ['*', 'abc def'], ['*', '(abc)def'],
    ['?', 'a'], ['?', ''],
    ['Y', '2020xx'], ['+Y', '2020xx'], ['Y+', '2020xx'], ['Yq', '2020'],
    ['Y!', '2020'], ['Y|', '2020'], ['Y+', '2020'], ['Y+q', '2020'],
    ['Y!q', '2020'], ["Y\x00m", '2020'], ["\x00", '2020'],
];
foreach ($ffScan as $ffCase) {
    $ffObj = DateTime::createFromFormat($ffCase[0], $ffCase[1], new DateTimeZone('UTC'));
    printf("%-8s %-16s %-6s %s\n",
        '[' . str_replace(["\t", "\0", "\xc2\xa0", "\xe2\x80\xaf"], ['\t', '\0', '<NB>', '<NN>'], $ffCase[0]) . ']',
        '[' . str_replace(["\t", "\0", "\xc2\xa0", "\xe2\x80\xaf"], ['\t', '\0', '<NB>', '<NN>'], $ffCase[1]) . ']',
        $ffObj === false ? 'FALSE' : 'OK',
        json_encode(DateTime::getLastErrors()));
}
?>
--EXPECT--
[Y-m-d]  [2020-01-02]     OK     false
[q]      [q]              OK     false
[q]      [r]              FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["The format separator does not match"]}
[qq]     [ab]             FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["The format separator does not match","The format separator does not match"]}
[q-q]    [a-b]            FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":{"0":"The format separator does not match","2":"The format separator does not match"}}
[--]     [ab]             FALSE  {"warning_count":0,"warnings":[],"error_count":3,"errors":["Trailing data"]}
[-]      [a]              FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["Trailing data"]}
[.]      [x]              FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["Trailing data"]}
[#]      [x]              FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["Trailing data"]}
[#]      [-]              OK     false
[#]      [(]              OK     false
[ ]      [x]              FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Trailing data"]}
[ ]      []               FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Not enough data available to satisfy format"]}
[  ]     [   ]            FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"3":"Not enough data available to satisfy format"}}
[ ]      [ \t  x]         FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Trailing data"}}
[ ]      [<NB>]           OK     false
[ ]      [<NN>]           OK     false
[\a]     [a]              OK     false
[\a]     [b]              FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["Trailing data"]}
[\]      []               FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Not enough data available to satisfy format"]}
[\]      [a]              FALSE  {"warning_count":0,"warnings":[],"error_count":2,"errors":["Trailing data"]}
[q\]     [q]              FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"1":"Not enough data available to satisfy format"}}
[*]      []               FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Not enough data available to satisfy format"]}
[*]      [a]              OK     false
[*]      [abc]            OK     false
[*]      [abc def]        FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"3":"Trailing data"}}
[*]      [(abc)def]       OK     false
[?]      [a]              OK     false
[?]      []               FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Not enough data available to satisfy format"]}
[Y]      [2020xx]         FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Trailing data"}}
[+Y]     [2020xx]         OK     {"warning_count":1,"warnings":{"4":"Trailing data"},"error_count":0,"errors":[]}
[Y+]     [2020xx]         OK     {"warning_count":1,"warnings":{"4":"Trailing data"},"error_count":0,"errors":[]}
[Yq]     [2020]           FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Not enough data available to satisfy format"}}
[Y!]     [2020]           OK     false
[Y|]     [2020]           OK     false
[Y+]     [2020]           OK     false
[Y+q]    [2020]           FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Not enough data available to satisfy format"}}
[Y!q]    [2020]           FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Not enough data available to satisfy format"}}
[Y\0m]   [2020]           OK     false
[\0]     [2020]           FALSE  {"warning_count":0,"warnings":[],"error_count":1,"errors":["Trailing data"]}
