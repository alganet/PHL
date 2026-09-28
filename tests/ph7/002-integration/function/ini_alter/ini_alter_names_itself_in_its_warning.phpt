--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_alter blames itself, not ini_set, in a refused write
--FILE--
<?php
// Nothing is printed until the end: ANY output marks the headers as sent, and
// a session directive refuses on that ground before its own rule is reached.
$iniAlterLog = [];
set_error_handler(function ($n, $s) use (&$iniAlterLog) { $iniAlterLog[] = "E$n: $s"; return true; });
$iniAlterRet = [];
$iniAlterRet[] = ini_alter('session.name', '123');
$iniAlterRet[] = ini_alter('session.serialize_handler', 'nope');
$iniAlterRet[] = ini_set('session.name', '123');
restore_error_handler();
var_dump($iniAlterRet);
echo implode("\n", $iniAlterLog), "\n";
?>
--EXPECT--
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
E2: ini_alter(): session.name "123" must not be numeric, empty, contain null bytes or any of the following characters "=,;.[ \t\r\n\013\014"
E2: ini_alter(): Serialization handler "nope" cannot be found
E2: ini_set(): session.name "123" must not be numeric, empty, contain null bytes or any of the following characters "=,;.[ \t\r\n\013\014"
--CLEAN--
<?php
unset($iniAlterLog, $iniAlterRet);
