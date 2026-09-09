--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SORT_* flag constants match php and drive numeric/string/natural/case-insensitive sorts
--FILE--
<?php
function sortFlagCases(): array {
    $out = [];
    $out[] = implode(",", [SORT_REGULAR, SORT_NUMERIC, SORT_STRING, SORT_DESC, SORT_ASC, SORT_LOCALE_STRING, SORT_NATURAL, SORT_FLAG_CASE]);
    $a = [3, 1, 2, 10, 20]; sort($a, SORT_STRING); $out[] = json_encode($a);
    $a = [3, 1, 2, 10, 20]; sort($a, SORT_NUMERIC); $out[] = json_encode($a);
    $a = ["img10", "img1", "img2", "img12"]; sort($a, SORT_NATURAL); $out[] = json_encode($a);
    $a = ["IMG10", "img1", "Img2"]; sort($a, SORT_NATURAL | SORT_FLAG_CASE); $out[] = json_encode($a);
    $a = ["Banana", "apple", "Cherry"]; sort($a, SORT_STRING | SORT_FLAG_CASE); $out[] = json_encode($a);
    $a = ["b" => 10, "a" => 2, "c" => 1]; arsort($a, SORT_NUMERIC); $out[] = json_encode($a);
    $a = ["x2", "x10", "x1"]; asort($a, SORT_NATURAL); $out[] = json_encode($a);
    $a = [3, 1, 2, 10]; rsort($a, SORT_STRING); $out[] = json_encode($a);
    return $out;
}
echo implode("\n", sortFlagCases()), "\n";
--EXPECT--
0,1,2,3,4,5,6,8
[1,10,2,20,3]
[1,2,3,10,20]
["img1","img2","img10","img12"]
["img1","Img2","IMG10"]
["apple","Banana","Cherry"]
{"b":10,"a":2,"c":1}
{"2":"x1","0":"x2","1":"x10"}
[3,2,10,1]
--CLEAN--
<?php
