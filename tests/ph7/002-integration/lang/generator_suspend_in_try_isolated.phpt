--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator suspended inside a try does not corrupt the caller's exception handling
--FILE--
<?php
function gen() {
    try { yield 1; yield 2; }
    catch (\Throwable $e) { echo "GEN WRONGLY CAUGHT: ", $e->getMessage(), "\n"; }
}
$g = gen();
echo $g->current(), "\n";   // advances into the try, suspends at 'yield 1'
try { throw new Exception("caller-exc"); }
catch (Exception $e) { echo "caller caught: ", $e->getMessage(), "\n"; }
echo "done\n";
?>
--EXPECT--
1
caller caught: caller-exc
done
--CLEAN--
<?php
