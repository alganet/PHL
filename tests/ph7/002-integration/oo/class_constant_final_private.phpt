--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A private class constant may not be final
--DESCRIPTION--
`final` says "no subclass may replace this", and a private constant is not
visible to one -- so php refuses the pair, naming the constant. Same shape as the
private-final METHOD rule one member over, except php makes this one a fatal
rather than a warning. PHL accepted it silently.
--FILE--
<?php
class CcfpC { final private const K = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Private constant CcfpC::K cannot be final as it is not visible to other classes %s
--CLEAN--
<?php
