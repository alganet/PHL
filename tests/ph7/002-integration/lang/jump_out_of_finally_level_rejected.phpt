--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break 2 out of a finally nested inside a catch is rejected — levels and nesting do not exempt it
--FILE--
<?php
foreach ([1, 2] as $a) {
    foreach ([1, 2] as $b) {
        try { throw new Exception("x"); }
        catch (Exception $e) {
            try { echo "i;"; }
            finally { break 2; }
        }
    }
}
echo "|end\n";
?>
--EXPECTF--
%Ajump out of a finally block is disallowed%A
--CLEAN--
<?php
