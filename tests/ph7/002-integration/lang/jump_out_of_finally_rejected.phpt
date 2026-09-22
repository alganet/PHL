--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break out of a finally is a compile-time fatal, as php says
--FILE--
<?php
foreach ([1, 2, 3] as $v) {
    try { echo "t$v;"; }
    finally { echo "f$v;"; break; }
}
echo "|end\n";
?>
--EXPECTF--
%Ajump out of a finally block is disallowed%A
--CLEAN--
<?php
