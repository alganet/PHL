--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A nullsafe METHOD call is a method return value, not a chain
--FILE--
<?php
// php asks the call question BEFORE the nullsafe one.
class WtNullsafeMethod { function m() { return 1; } }
$o = new WtNullsafeMethod;
$o?->m()++;
?>
--EXPECTF--
%ACan't use method return value in write context%A
