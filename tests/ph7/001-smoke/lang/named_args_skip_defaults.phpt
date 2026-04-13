--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: skipping parameters with defaults
--FILE--
<?php
function nasdf($a, $b = "B", $c = "C", $d = "D") {
    echo "a=$a b=$b c=$c d=$d\n";
}
nasdf(a: "X");
nasdf(a: "X", d: "Y");
nasdf(a: "X", c: "Z");
nasdf("A", d: "DD");
?>
--EXPECT--
a=X b=B c=C d=D
a=X b=B c=C d=Y
a=X b=B c=Z d=D
a=A b=B c=C d=DD
--CLEAN--
<?php
