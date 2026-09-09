--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique honors its sort flags (default SORT_STRING, numeric/regular/case/natural)
--FILE--
<?php
function arrayUniqueFlagCases(): array {
    $out = [];
    $out[] = json_encode(array_unique(["1", 1, "1.0", 1.0]));
    $out[] = json_encode(array_unique([1, "1", "1abc"], SORT_NUMERIC));
    $out[] = json_encode(array_unique([1, "1", "1abc"], SORT_STRING));
    $out[] = json_encode(array_unique([1, "1", 1.0, true], SORT_REGULAR));
    $out[] = json_encode(array_unique(["FILE1", "file1", "File2"], SORT_STRING | SORT_FLAG_CASE));
    $out[] = json_encode(array_unique(["x1", "x01", "x1"], SORT_NATURAL));
    $out[] = json_encode(array_unique(["10", "9", "10", "9.0"], SORT_NUMERIC));
    return $out;
}
echo implode("\n", arrayUniqueFlagCases()), "\n";
--EXPECT--
{"0":"1","2":"1.0"}
[1]
{"0":1,"2":"1abc"}
[1]
{"0":"FILE1","2":"File2"}
["x1","x01"]
["10","9"]
--CLEAN--
<?php
