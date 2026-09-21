--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize() validates $options and enforces max_depth
--FILE--
<?php
// $options used to be ignored wholesale, so a mistyped option did nothing at all.
// php validates the map BEFORE parsing a byte of $data.
$bad = [
    ["allowed_classes" => 1],
    ["allowed_classes" => "foo"],
    ["allowed_classes" => 1.0],
    ["allowed_classes" => null],
    ["allowed_classes" => new stdClass],
    ["allowed_classes" => [1]],
    ["allowed_classes" => ["A", 2]],
    ["max_depth" => "x"],
    ["max_depth" => 1.5],
    ["max_depth" => 2.0],
    ["max_depth" => null],
    ["max_depth" => []],
    ["max_depth" => -1],
];
foreach ($bad as $opt) {
    try {
        unserialize("i:1;", $opt);
    } catch (TypeError|ValueError $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

// The shapes php accepts stay accepted, and an unknown key is ignored silently.
foreach ([["allowed_classes" => true], ["allowed_classes" => false],
          ["allowed_classes" => []], ["allowed_classes" => ["A"]],
          ["max_depth" => 0], ["max_depth" => PHP_INT_MAX],
          ["bogus" => 1], []] as $opt) {
    var_dump(unserialize("i:1;", $opt));
}

// max_depth counts nested containers; 0 means unlimited, like the ini setting.
// PH7 ignored it entirely.
$flat   = 'a:1:{i:0;i:1;}';
$two    = 'a:1:{i:0;a:1:{i:0;i:1;}}';
$three  = 'a:1:{i:0;a:1:{i:0;a:1:{i:0;i:1;}}}';
set_error_handler(function ($n, $s) { echo "W: $s\n"; return true; });
foreach ([0, 1, 2, 3] as $d) {
    foreach ([$flat, $two, $three] as $i => $s) {
        $r = unserialize($s, ["max_depth" => $d]); // warnings print before the echo
        echo $d, '/', $i, ' => ', var_export($r !== false, true), "\n";
    }
}

// A failed parse now says WHERE it gave up -- PH7 failed silently, so a corrupt
// payload was indistinguishable from a serialized `false`.
foreach (["garbage", "i:1", 's:5:"ab";', 's:1:"ab";', "a:1:{i:0;", "a:2:{i:0;i:1;}",
          "i:1;xx"] as $s) {
    var_dump(unserialize($s));
}
restore_error_handler();
?>
--EXPECT--
TypeError: unserialize(): Option "allowed_classes" must be of type array|bool, int given
TypeError: unserialize(): Option "allowed_classes" must be of type array|bool, string given
TypeError: unserialize(): Option "allowed_classes" must be of type array|bool, float given
TypeError: unserialize(): Option "allowed_classes" must be of type array|bool, null given
TypeError: unserialize(): Option "allowed_classes" must be of type array|bool, stdClass given
TypeError: unserialize(): Option "allowed_classes" must be an array of class names, int given
TypeError: unserialize(): Option "allowed_classes" must be an array of class names, int given
TypeError: unserialize(): Option "max_depth" must be of type int, string given
TypeError: unserialize(): Option "max_depth" must be of type int, float given
TypeError: unserialize(): Option "max_depth" must be of type int, float given
TypeError: unserialize(): Option "max_depth" must be of type int, null given
TypeError: unserialize(): Option "max_depth" must be of type int, array given
ValueError: unserialize(): Option "max_depth" must be greater than or equal to 0
int(1)
int(1)
int(1)
int(1)
int(1)
int(1)
int(1)
int(1)
0/0 => true
0/1 => true
0/2 => true
1/0 => true
W: unserialize(): Maximum depth of 1 exceeded. The depth limit can be changed using the max_depth unserialize() option or the unserialize_max_depth ini setting
W: unserialize(): Error at offset 14 of 24 bytes
1/1 => false
W: unserialize(): Maximum depth of 1 exceeded. The depth limit can be changed using the max_depth unserialize() option or the unserialize_max_depth ini setting
W: unserialize(): Error at offset 14 of 34 bytes
1/2 => false
2/0 => true
2/1 => true
W: unserialize(): Maximum depth of 2 exceeded. The depth limit can be changed using the max_depth unserialize() option or the unserialize_max_depth ini setting
W: unserialize(): Error at offset 23 of 34 bytes
2/2 => false
3/0 => true
3/1 => true
3/2 => true
W: unserialize(): Error at offset 0 of 7 bytes
bool(false)
W: unserialize(): Error at offset 0 of 3 bytes
bool(false)
W: unserialize(): Error at offset 2 of 9 bytes
bool(false)
W: unserialize(): Error at offset 6 of 9 bytes
bool(false)
W: unserialize(): Error at offset 9 of 9 bytes
bool(false)
W: unserialize(): Unexpected end of serialized data
W: unserialize(): Error at offset 13 of 14 bytes
bool(false)
W: unserialize(): Extra data starting at offset 4 of 6 bytes
int(1)
--CLEAN--
<?php
