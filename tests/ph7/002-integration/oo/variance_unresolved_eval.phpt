--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override left unresolved inside eval() is refused with the eval frame on the trace
--DESCRIPTION--
The pair is settled where the eval'd declaration runs, so the fatal names
the eval'd unit and php's trace carries the eval() activation.
--FILE--
<?php
echo "before\n";
eval('class VuEvParent { function f(VuEvNowhereA $a) {} }
class VuEvKid extends VuEvParent { function f(VuEvNowhereB $a) {} }');
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Could not check compatibility between VuEvKid::f(VuEvNowhereB $a) and VuEvParent::f(VuEvNowhereA $a), because class VuEvNowhereA is not available in %s(3) : eval()'d code on line 2
Stack trace:
#0 %s(3): eval()
#1 {main}%A
