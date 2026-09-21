--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Embedded builtins' TypeError uses php's ZPP value-name (int/float/true/false/null), not gettype()
--FILE--
<?php
// php's ZPP value-name in TypeError text is int/float/string/true/false/null,
// not gettype()'s integer/double/boolean. Covers the embedded builtins that
// assemble their own TypeError message.
function t($l, $fn) { try { $fn(); echo "$l => (no throw)\n"; } catch (Throwable $e) { echo "$l => ", $e->getMessage(), "\n"; } }
$vals = ['int' => 1, 'float' => 1.5, 'true' => true, 'false' => false, 'null' => null, 'string' => 'x'];
foreach ($vals as $name => $v) {
    t("array_merge_recursive $name", fn() => array_merge_recursive($v));
}
foreach ($vals as $name => $v) {
    t("array_unshift $name", function() use ($v) { array_unshift($v, 9); });
}
foreach ($vals as $name => $v) {
    t("http_build_query $name", fn() => http_build_query($v));
}
?>
--EXPECT--
array_merge_recursive int => array_merge_recursive(): Argument #1 must be of type array, int given
array_merge_recursive float => array_merge_recursive(): Argument #1 must be of type array, float given
array_merge_recursive true => array_merge_recursive(): Argument #1 must be of type array, true given
array_merge_recursive false => array_merge_recursive(): Argument #1 must be of type array, false given
array_merge_recursive null => array_merge_recursive(): Argument #1 must be of type array, null given
array_merge_recursive string => array_merge_recursive(): Argument #1 must be of type array, string given
array_unshift int => array_unshift(): Argument #1 ($array) must be of type array, int given
array_unshift float => array_unshift(): Argument #1 ($array) must be of type array, float given
array_unshift true => array_unshift(): Argument #1 ($array) must be of type array, true given
array_unshift false => array_unshift(): Argument #1 ($array) must be of type array, false given
array_unshift null => array_unshift(): Argument #1 ($array) must be of type array, null given
array_unshift string => array_unshift(): Argument #1 ($array) must be of type array, string given
http_build_query int => http_build_query(): Argument #1 ($data) must be of type array, int given
http_build_query float => http_build_query(): Argument #1 ($data) must be of type array, float given
http_build_query true => http_build_query(): Argument #1 ($data) must be of type array, true given
http_build_query false => http_build_query(): Argument #1 ($data) must be of type array, false given
http_build_query null => http_build_query(): Argument #1 ($data) must be of type array, null given
http_build_query string => http_build_query(): Argument #1 ($data) must be of type array, string given
