--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ignore_user_abort answers the value it held before the call
--FILE--
<?php
$iuaStart = ini_get('ignore_user_abort');
var_dump($iuaStart);
var_dump(ignore_user_abort(true), ignore_user_abort(), ini_get('ignore_user_abort'));
var_dump(ignore_user_abort(null), ignore_user_abort('x'));
var_dump(ini_set('ignore_user_abort', '0'), ignore_user_abort());
var_dump(ignore_user_abort(false), ini_get('ignore_user_abort'));
try { ignore_user_abort([]); } catch (Throwable $iuaErr) { echo get_class($iuaErr), ": ", $iuaErr->getMessage(), "\n"; }
try { ignore_user_abort(1, 2); } catch (Throwable $iuaErr) { echo get_class($iuaErr), ": ", $iuaErr->getMessage(), "\n"; }
ini_set('ignore_user_abort', $iuaStart);
?>
--EXPECT--
string(1) "0"
int(0)
int(1)
string(1) "1"
int(1)
int(1)
string(1) "1"
int(0)
int(0)
string(1) "0"
TypeError: ignore_user_abort(): Argument #1 ($enable) must be of type ?bool, array given
ArgumentCountError: ignore_user_abort() expects at most 1 argument, 2 given
--CLEAN--
<?php
unset($iuaStart, $iuaErr);
