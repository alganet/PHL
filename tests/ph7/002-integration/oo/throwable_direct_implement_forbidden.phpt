--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: user class cannot implement Throwable directly
--FILE--
<?php
class ThDirect implements Throwable {}
?>
--EXPECTF--
%s Fatal error:  Class ThDirect cannot implement interface Throwable, extend Exception or Error instead%A
--CLEAN--
<?php
