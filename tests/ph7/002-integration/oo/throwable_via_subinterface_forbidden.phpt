--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: user class cannot reach Throwable via a sub-interface
--FILE--
<?php
interface ThSub extends Throwable {}
class ThViaSub implements ThSub {}
?>
--EXPECTF--
%s Fatal error:  Class ThViaSub cannot implement interface Throwable, extend Exception or Error instead%A
--CLEAN--
<?php
