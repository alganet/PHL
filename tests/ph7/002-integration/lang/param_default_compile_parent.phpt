--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a method names self and parent by class in the refusal
--FILE--
<?php
echo "never printed\n";
class PdcpBase {}
class PdcpChild extends PdcpBase {
    function m(?parent $p = []) {}
}
--EXPECTF--
PHP Fatal error:  Cannot use array as default value for parameter $p of type ?PdcpBase in %s on line 5%A
--CLEAN--
<?php
