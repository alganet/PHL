--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator's finally runs when it returns from inside a try after a resume
--FILE--
<?php
function gen() {
    try { yield 1; return; }
    finally { echo "FIN\n"; }
}
$g = gen();
echo $g->current(), "\n";   // into the try, suspend
$g->next();                 // resume, hit 'return' inside the try -> finally runs
echo "after\n";
?>
--EXPECT--
1
FIN
after
--CLEAN--
<?php
