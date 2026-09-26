--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset() on a property/element of a temporary receiver is silent
--FILE--
<?php
// isset() over a property or element of a TEMPORARY receiver: php's isset() is
// a language construct and says nothing at all about it.
class IssetTempBox { public $id = 3; public $nil = null; }
function isset_temp_box() { return new IssetTempBox; }
function isset_temp_arr() { return ['k' => 1, 'n' => null]; }
var_dump(isset(isset_temp_box()->id), isset(isset_temp_box()->nil), isset(isset_temp_box()->nope));
var_dump(isset((new IssetTempBox)->id), isset((new IssetTempBox)->nope));
var_dump(isset(isset_temp_arr()['k']), isset(isset_temp_arr()['n']), isset(isset_temp_arr()['z']));
$kept = new IssetTempBox;
$arr = ['k' => 1, 'n' => null];
var_dump(isset($kept->id), isset($kept->nil), isset($arr['k']), isset($arr['n']));
$s = 'abc';
var_dump(isset($s[1]), isset($s[9]));
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
