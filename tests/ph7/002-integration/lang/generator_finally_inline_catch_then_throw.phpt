--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator finally can swallow a locally-thrown exception via an inline catch and then throw a fresh one that supersedes and propagates
--FILE--
<?php
function g(){
    try {
        yield 1;
    } finally {
        try { throw new RuntimeException("inner"); }
        catch (RuntimeException $e) { echo "caught inner\n"; }
        throw new LogicException("from finally");
    }
}
try {
    foreach (g() as $v) { echo "got ", $v, "\n"; }
} catch (LogicException $e) {
    echo "caller: ", $e->getMessage(), "\n";
}
echo "done\n";
?>
--EXPECT--
got 1
caught inner
caller: from finally
done
--CLEAN--
<?php
