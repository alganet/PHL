--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_constants(true) buckets by EXTENSION, Core first and user last
--FILE--
<?php
// This used to be a twin pair: php spread its constants over its extension
// buckets while PHL had no extension partition and answered ONE "Core" holding
// all 1314. It has the partition now, so both engines are asserted here -- and
// only about what does not drift with the build.
$gdcc = get_defined_constants(true);
var_dump(count(array_keys($gdcc)) > 1);
var_dump(array_key_first($gdcc));
var_dump(isset($gdcc['Core']['PHP_EOL']), isset($gdcc['standard']['SORT_STRING']));
var_dump(isset($gdcc['date']['DATE_ATOM']), isset($gdcc['json']['JSON_PRETTY_PRINT']));
// Every constant lands in exactly one bucket, and none is lost on the way.
$gdccSeen = [];
foreach ($gdcc as $gdccBucket) { foreach ($gdccBucket as $gdccName => $gdccVal) { $gdccSeen[$gdccName] = true; } }
var_dump(count($gdccSeen) === count(get_defined_constants()));
// A category with nothing in it is omitted, php's rule: no user constants yet.
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
bool(true)
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
