--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DIVERGENCE: get_defined_constants(true) groups the engine's own constants under one "Core" bucket — PHL has no extension partition (php's half is the _zend twin)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// php sorts its constants across ~28 extension buckets (Core, standard, date,
// pcre, json, hash, mbstring, SPL, …). PHL has no extension partition — the same
// limitation ReflectionFunction::getExtensionName() records by answering "Core"
// for every function — so it answers the two buckets it CAN tell apart: the
// engine's own under "Core", and everything a script defined under "user".
// The VALUES and the "user" bucket are php-exact; only the bucket SET differs.
$gdcc = get_defined_constants(true);
var_dump(array_keys($gdcc));
var_dump(isset($gdcc['Core']['PHP_EOL']), isset($gdcc['Core']['SORT_STRING']));
// A category with nothing in it is omitted, php's rule: no user constants yet.
var_dump(isset($gdcc['user']));
define('Gdcc_one', 1);
$gdcc = get_defined_constants(true);
var_dump(array_keys($gdcc), $gdcc['user']);
?>
--EXPECT--
array(1) {
  [0]=>
  string(4) "Core"
}
bool(true)
bool(true)
bool(false)
array(2) {
  [0]=>
  string(4) "Core"
  [1]=>
  string(4) "user"
}
array(1) {
  ["Gdcc_one"]=>
  int(1)
}
--CLEAN--
<?php
unset($gdcc);
