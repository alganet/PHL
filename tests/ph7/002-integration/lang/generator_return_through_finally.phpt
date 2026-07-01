--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A return inside a generator try/catch runs enclosing finallys before completing (ROOT C Face 1)
--FILE--
<?php
function g(){
    try {
        try { yield 1; return "R"; }
        finally { echo "inner\n"; }
    } finally { echo "outer\n"; }
}
$g = g();
echo $g->current(), "\n";
$g->next();
echo "ret=", $g->getReturn(), "\n";
?>
--EXPECT--
1
inner
outer
ret=R
--CLEAN--
<?php
