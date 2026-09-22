--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the two declarations need not sit together: one per branch of a method is the same fatal
--FILE--
<?php
class LrCls {
    function m($x) {
        if ($x) {
            done:
            return "a";
        }
        while ($x) {
            done:
            break;
        }
        return "b";
    }
}
echo (new LrCls())->m(1), "\n";
?>
--EXPECTF--
%ALabel 'done' already defined in %s on line 9%A
--CLEAN--
<?php
