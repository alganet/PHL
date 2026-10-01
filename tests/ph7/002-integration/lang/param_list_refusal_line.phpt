--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter-list refusal is reported on the line the parameter starts on, not its dollar sign
--FILE--
<?php
function q(
  int
  ...$a,
  string
  $b
  = "x"
) {}
?>
--EXPECTF--
%s Fatal error:  Only the last parameter can be variadic in %s on line 5
--CLEAN--
<?php
