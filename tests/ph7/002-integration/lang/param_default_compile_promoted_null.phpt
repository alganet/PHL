--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Promoted parameter: a null default for a non-nullable type is refused, where a plain parameter would be implicitly nullable
--FILE--
<?php
echo "never printed\n";
class PdcpnHolder {
    function __construct(public ?int $a = null, public mixed $b = null, public int $c = 1 ? null : 2) {}
}
--EXPECTF--
PHP Fatal error:  Cannot use null as default value for parameter $c of type int in %s on line 4%A
--CLEAN--
<?php
