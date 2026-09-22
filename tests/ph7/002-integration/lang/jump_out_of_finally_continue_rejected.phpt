--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
continue out of a finally is a compile-time fatal, as php says
--FILE--
<?php
foreach ([1, 2] as $v) {
    try { echo "t$v;"; }
    finally { continue; }
}
echo "|end\n";
?>
--EXPECTF--
%Ajump out of a finally block is disallowed%A
--CLEAN--
<?php
