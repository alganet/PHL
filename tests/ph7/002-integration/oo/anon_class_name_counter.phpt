--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class name: the whole thing, counter included
--FILE--
<?php
/* One counter for the whole request, bumped in COMPILE order — which is what
 * separates two anonymous classes written on the SAME line, where the file and
 * the line cannot. It runs in its own process here because the number is a
 * property of the process, not of the site. */
$show = static fn(object $o): string =>
    str_replace([__FILE__, "\0"], ['FILE', '<NUL>'], get_class($o));

$a = new class {}; $b = new class {};
echo $show($a), "\n";
echo $show($b), "\n";

// eval() names its origin the way every other diagnostic in it does.
eval('$e = new class {};');
echo str_replace("\0", '<NUL>', str_replace(__FILE__, 'FILE', get_class($e))), "\n";

// A site compiled once is one class however often it runs.
$mk = static fn() => new class {};
echo $show($mk()), "\n";
echo $show($mk()), "\n";
?>
--EXPECT--
class@anonymous<NUL>FILE:9$0
class@anonymous<NUL>FILE:9$1
class@anonymous<NUL>FILE(14) : eval()'d code:1$3
class@anonymous<NUL>FILE:18$2
class@anonymous<NUL>FILE:18$2
--CLEAN--
<?php
