--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fiber suspended inside a try does not corrupt the caller's exception handling
--FILE--
<?php
$f = new Fiber(function () {
    try { Fiber::suspend(1); }
    catch (\Throwable $e) { echo "FIBER WRONGLY CAUGHT\n"; }
});
$f->start();   // suspends inside the fiber's try
try { throw new Exception("caller-exc"); }
catch (Exception $e) { echo "caller caught: ", $e->getMessage(), "\n"; }
$f->resume();
echo "done\n";
?>
--EXPECT--
caller caught: caller-exc
done
--CLEAN--
<?php
