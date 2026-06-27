--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() into a finished generator propagates the exception; getReturn() still works
--FILE--
<?php
function g(){ yield 1; return "R"; }
$g = g();
$g->current();
$g->next();   // run to completion
echo "valid=".var_export($g->valid(),true)."\n";
try { $g->throw(new Exception("late")); }
catch (Exception $e) { echo "caught:".$e->getMessage()."\n"; }
echo "getReturn=".$g->getReturn()."\n";
?>
--EXPECT--
valid=false
caught:late
getReturn=R
--CLEAN--
<?php
