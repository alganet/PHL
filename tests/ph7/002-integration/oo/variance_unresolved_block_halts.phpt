--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unresolved override refused inside a block stops the script
--DESCRIPTION--
A class declared inside an if is declared when the block runs; the refusal of
its unresolved pair there is a fatal, so nothing after it runs.
--FILE--
<?php
class VuBhParent { function f(VuBhA $a) {} }
echo "before\n";
if (true) {
    class VuBhKid extends VuBhParent { function f(VuBhB $a) {} }
    echo "unreached\n";
}
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Could not check compatibility between VuBhKid::f(VuBhB $a) and VuBhParent::f(VuBhA $a), because class VuBhA is not available in %s on line 5
Stack trace:
#0 {main}
