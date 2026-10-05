--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: a null default on a non-nullable type names the nullable spelling
--FILE--
<?php
echo "never printed\n";
class TpdcnHolder {
    public int|string $x = null;
}
--EXPECTF--
PHP Fatal error:  Default value for property of type string|int may not be null. Use the nullable type string|int|null to allow null default value in %s on line 4%A
--CLEAN--
<?php
