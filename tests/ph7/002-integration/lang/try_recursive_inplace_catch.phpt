--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An in-place catch at one recursion level must not steal the outer level's activation of the same lexical try (regression: outer finally ran early, outer catch orphaned)
--FILE--
<?php
function f($n) {
    try {
        if (!$n) { throw new Exception('a'); }
        f($n - 1);
        throw new Exception('b');
    } catch (Exception $e) {
        echo "caught:", $e->getMessage(), ":n=", $n, "\n";
    } finally {
        echo "fin:", $n, "\n";
    }
}
f(1);
f(2);
echo "end\n";
?>
--EXPECT--
caught:a:n=0
fin:0
caught:b:n=1
fin:1
caught:a:n=0
fin:0
caught:b:n=1
fin:1
caught:b:n=2
fin:2
end
--CLEAN--
<?php
