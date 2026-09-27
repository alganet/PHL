--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write INTO a method call's result says "method"
--FILE--
<?php
class WriteTargetMethodDirect { function m() { return [1,2]; } }
$o = new WriteTargetMethodDirect;
$o->m() = 5;
?>
--EXPECTF--
%ACan't use method return value in write context%A
