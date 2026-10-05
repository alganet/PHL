--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a refused bool default is named bool, on the line of the parameter's type
--FILE--
<?php
echo "never printed\n";
function pdcb(
    $a,
    false
        $flag = 2 > 1
) {}
--EXPECTF--
PHP Fatal error:  Cannot use bool as default value for parameter $flag of type false in %s on line 5%A
--CLEAN--
<?php
