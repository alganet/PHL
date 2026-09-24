--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php spreads get_defined_constants(true) across its EXTENSION buckets (zend half of the twin pair — PHL has one "Core", see get_defined_constants_categorize.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// php sorts its constants across many extension buckets. What is asserted here
// is only what does not drift with the build: that there is MORE than one, that
// "Core" is the first, and that the user bucket behaves the same way in both.
$gdcc = get_defined_constants(true);
var_dump(count(array_keys($gdcc)) > 1);
var_dump(array_key_first($gdcc));
var_dump(isset($gdcc['Core']['PHP_EOL']), isset($gdcc['standard']['SORT_STRING']));
var_dump(isset($gdcc['user']));
define('Gdcc_one', 1);
$gdcc = get_defined_constants(true);
var_dump(array_key_last($gdcc), $gdcc['user']);
?>
--EXPECT--
bool(true)
string(4) "Core"
bool(true)
bool(true)
bool(false)
string(4) "user"
array(1) {
  ["Gdcc_one"]=>
  int(1)
}
--CLEAN--
<?php
unset($gdcc);
