--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
in_array()/array_search() throw TypeError for a non-array $haystack (php-exact type name)
--FILE--
<?php
// in_array()/array_search() require an array $haystack; php throws a TypeError
// naming the given type (int/float/true/false/string/null) for a non-array.
function t($l, $fn) { try { var_dump($fn()); echo "$l => (no throw)\n"; } catch (Throwable $e) { echo "$l => ", $e->getMessage(), "\n"; } }
$vals = ['int' => 2, 'float' => 1.5, 'true' => true, 'false' => false, 'string' => 'x', 'null' => null];
foreach ($vals as $name => $v) {
    t("in_array $name", fn() => in_array(1, $v));
}
foreach ($vals as $name => $v) {
    t("array_search $name", fn() => array_search(1, $v));
}
// valid array lookups are unaffected
t('in_array hit',     fn() => in_array(2, [1, 2, 3]));
t('in_array miss',    fn() => in_array(9, [1, 2, 3]));
t('array_search hit', fn() => array_search(2, [1, 2, 3]));
t('array_search miss',fn() => array_search(9, [1, 2, 3]));
?>
--EXPECT--
in_array int => in_array(): Argument #2 ($haystack) must be of type array, int given
in_array float => in_array(): Argument #2 ($haystack) must be of type array, float given
in_array true => in_array(): Argument #2 ($haystack) must be of type array, true given
in_array false => in_array(): Argument #2 ($haystack) must be of type array, false given
in_array string => in_array(): Argument #2 ($haystack) must be of type array, string given
in_array null => in_array(): Argument #2 ($haystack) must be of type array, null given
array_search int => array_search(): Argument #2 ($haystack) must be of type array, int given
array_search float => array_search(): Argument #2 ($haystack) must be of type array, float given
array_search true => array_search(): Argument #2 ($haystack) must be of type array, true given
array_search false => array_search(): Argument #2 ($haystack) must be of type array, false given
array_search string => array_search(): Argument #2 ($haystack) must be of type array, string given
array_search null => array_search(): Argument #2 ($haystack) must be of type array, null given
bool(true)
in_array hit => (no throw)
bool(false)
in_array miss => (no throw)
int(1)
array_search hit => (no throw)
bool(false)
array_search miss => (no throw)
