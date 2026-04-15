--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: namespaced user class still forbidden from direct implement
--FILE--
<?php
namespace ThNs;
use Throwable;
class MyThrow implements Throwable {}
?>
--EXPECTF--
%s Fatal error:  Class ThNs\MyThrow cannot implement interface Throwable, extend Exception or Error instead%A
--CLEAN--
<?php
