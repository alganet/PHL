--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A yield inside a generator's catch suspends and resumes; a following finally still runs (ROOT C Face 1)
--FILE--
<?php
function g(){
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "caught\n"; yield 1; yield 2; echo "after\n"; }
    finally { echo "finally\n"; }
}
foreach (g() as $v) { echo "got $v\n"; }
?>
--EXPECT--
caught
got 1
got 2
after
finally
--CLEAN--
<?php
