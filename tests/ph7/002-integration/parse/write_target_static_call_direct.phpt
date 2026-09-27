--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static call is a "method return value" too
--FILE--
<?php
class WriteTargetStaticDirect { static function m() { return [1,2]; } }
WriteTargetStaticDirect::m()++;
?>
--EXPECTF--
%ACan't use method return value in write context%A
