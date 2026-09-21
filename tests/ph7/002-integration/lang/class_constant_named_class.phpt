--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`class` is the one reserved word a class constant may not be named
--FILE--
<?php
class CcncHolder
{
    const class = 4;
}
--EXPECTF--
PHP Fatal error:  A class constant must not be called 'class'; it is reserved for class name fetching in %s on line %d%A
--CLEAN--
<?php
