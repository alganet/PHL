--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constant visibility enforcement (private/protected forbidden from outside)
--FILE--
<?php
class Restricted {
    public const PUB = "public";
    protected const PROT = "protected";
}

echo Restricted::PUB . "\n";
echo Restricted::PROT . "\n";
echo "should not reach here\n";
?>
--EXPECTF--
public
%s Fatal error:  Uncaught Error: Cannot access protected constant Restricted::PROT in %s
--CLEAN--
<?php

