--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator with a declared return type completes without a spurious TypeError (regression: body-completion OP_DONE must not enforce the call-site type)
--FILE--
<?php
function g(): Generator {
    yield 1;
}
foreach (g() as $v) {
    echo "v=", $v, "\n";
}
function gi(): iterable {
    yield 2;
}
foreach (gi() as $v) {
    echo "v=", $v, "\n";
}
function gr(): Generator {
    yield 3;
    return "ret";
}
$gen = gr();
foreach ($gen as $v) {
    echo "v=", $v, "\n";
}
echo "ret=", $gen->getReturn(), "\n";
echo "done\n";
?>
--EXPECT--
v=1
v=2
v=3
ret=ret
done
--CLEAN--
<?php
