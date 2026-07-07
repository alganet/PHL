--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fiber abandoned while suspended inside a nested call is cleaned up without leak or crash (php-exact output; BYTECODE.md stage 4 VmFreeParkedSegment)
--FILE--
<?php
function inner() {
    $big = str_repeat("x", 1000);
    Fiber::suspend("deep");
    return strlen($big);
}
function outer() {
    $local = [1, 2, 3];
    return inner() + count($local);
}
for ($i = 0; $i < 100; $i++) {
    $f = new Fiber('outer');
    $v = $f->start();   // suspends deep inside inner()
    // never resumed: $f goes out of scope next iteration → segment freed
    unset($f);
}
echo "created and abandoned 100 deeply-suspended fibers\n";
echo "done\n";
?>
--EXPECT--
created and abandoned 100 deeply-suspended fibers
done
--CLEAN--
<?php
