--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sys_get_temp_dir() answers a directory with no trailing separator
--FILE--
<?php
$tmp = sys_get_temp_dir();
// php's answer never ends with a separator, on either platform: it is joined to
// "/name" by every caller, and a trailing one makes "...Temp\/name".
var_dump(is_string($tmp), $tmp !== '', substr($tmp, -1) !== '/' && substr($tmp, -1) !== '\\');
var_dump(is_dir($tmp));
// ...and a file created under it is reachable by the joined name.
$f = $tmp . '/phl_sys_temp_shape_' . getmypid() . '.txt';
var_dump(file_put_contents($f, 'x') === 1, file_get_contents($f) === 'x');
@unlink($f);
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
