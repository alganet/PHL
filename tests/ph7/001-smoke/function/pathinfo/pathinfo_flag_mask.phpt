--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pathinfo() $flags is a BITMASK: a combined mask answers the first requested component
--FILE--
<?php
// The components are powers of two (1/2/4/8), so they combine; anything other than
// PATHINFO_ALL answers with the FIRST component the mask actually produced, in the
// order dirname, basename, extension, filename.
$pifm_p = "/var/www/index.html";
var_dump(pathinfo($pifm_p, PATHINFO_DIRNAME));
var_dump(pathinfo($pifm_p, PATHINFO_BASENAME));
var_dump(pathinfo($pifm_p, PATHINFO_EXTENSION));
var_dump(pathinfo($pifm_p, PATHINFO_FILENAME));
// combined masks: first requested wins
var_dump(pathinfo($pifm_p, PATHINFO_DIRNAME | PATHINFO_BASENAME));
var_dump(pathinfo($pifm_p, PATHINFO_EXTENSION | PATHINFO_FILENAME));
var_dump(pathinfo($pifm_p, PATHINFO_BASENAME | PATHINFO_FILENAME));
// an unknown bit riding along with a known one answers as if only the known one
// was passed (99 == 1|2|32|64)
var_dump(pathinfo($pifm_p, 99));
// no known bit at all -> the empty string
var_dump(pathinfo($pifm_p, 0));
var_dump(pathinfo($pifm_p, 32));
// a component the path does not have is skipped, so the NEXT requested one answers
var_dump(pathinfo("/var/www/README", PATHINFO_EXTENSION | PATHINFO_FILENAME));
var_dump(pathinfo("/var/www/README", PATHINFO_EXTENSION));
// the full mask is the array form
var_dump(pathinfo($pifm_p) === pathinfo($pifm_p, PATHINFO_ALL));
var_dump(pathinfo($pifm_p, PATHINFO_ALL));

// EMPTY is not ABSENT: an emitted-but-empty component ends the search. php emits
// basename and filename whenever they are asked for, emits extension whenever the
// basename holds a dot (even a trailing one), and emits dirname only when non-empty.
foreach ([
    ["x.", PATHINFO_EXTENSION | PATHINFO_FILENAME],
    [".", PATHINFO_FILENAME],
    ["/x/.hidden", PATHINFO_FILENAME],
    [".bashrc", PATHINFO_EXTENSION],
    ["..", PATHINFO_EXTENSION | PATHINFO_FILENAME],
    ["file.txt", PATHINFO_DIRNAME],
    ["file.txt", PATHINFO_DIRNAME | PATHINFO_BASENAME],
    ["/var/www/", PATHINFO_DIRNAME | PATHINFO_BASENAME],
    ["/", PATHINFO_BASENAME],
    ["", PATHINFO_DIRNAME | PATHINFO_BASENAME],
] as [$pifm_path, $pifm_flag]) {
    echo str_pad(var_export($pifm_path, true), 14), $pifm_flag, " => ",
         var_export(pathinfo($pifm_path, $pifm_flag), true), "\n";
}
// A root-only path's dirname is the platform's OWN separator — both engines answer "\"
// on Windows (php's DEFAULT_SLASH) and "/" elsewhere — so normalise before printing.
echo "root dirname => ",
     var_export(strtr(pathinfo("/", PATHINFO_DIRNAME), "\\", "/"), true), "\n";
$pifm_root = pathinfo("/");
$pifm_root["dirname"] = strtr($pifm_root["dirname"], "\\", "/");
var_dump($pifm_root);
// the whole-array shape for the edge paths
var_dump(pathinfo(".bashrc"), pathinfo("/var/www/"), pathinfo(""));
// a mask wider than 32 bits still selects (4294967311 == 15 + 2**32) and must not
// fall back to the ARRAY form
var_dump(pathinfo("/a/b.txt", 4294967311));
?>
--EXPECT--
string(8) "/var/www"
string(10) "index.html"
string(4) "html"
string(5) "index"
string(8) "/var/www"
string(4) "html"
string(10) "index.html"
string(8) "/var/www"
string(0) ""
string(0) ""
string(6) "README"
string(0) ""
bool(true)
array(4) {
  ["dirname"]=>
  string(8) "/var/www"
  ["basename"]=>
  string(10) "index.html"
  ["extension"]=>
  string(4) "html"
  ["filename"]=>
  string(5) "index"
}
'x.'          12 => ''
'.'           8 => ''
'/x/.hidden'  8 => ''
'.bashrc'     4 => 'bashrc'
'..'          12 => ''
'file.txt'    1 => '.'
'file.txt'    3 => '.'
'/var/www/'   3 => '/var'
'/'           2 => ''
''            3 => ''
root dirname => '/'
array(3) {
  ["dirname"]=>
  string(1) "/"
  ["basename"]=>
  string(0) ""
  ["filename"]=>
  string(0) ""
}
array(4) {
  ["dirname"]=>
  string(1) "."
  ["basename"]=>
  string(7) ".bashrc"
  ["extension"]=>
  string(6) "bashrc"
  ["filename"]=>
  string(0) ""
}
array(3) {
  ["dirname"]=>
  string(4) "/var"
  ["basename"]=>
  string(3) "www"
  ["filename"]=>
  string(3) "www"
}
array(2) {
  ["basename"]=>
  string(0) ""
  ["filename"]=>
  string(0) ""
}
string(2) "/a"
--CLEAN--
<?php
