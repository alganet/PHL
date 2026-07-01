--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A yield inside a generator's finally suspends and resumes correctly (ROOT C Face 1)
--FILE--
<?php
function g(){
    try { echo "try\n"; }
    finally { yield 1; yield 2; }
}
foreach (g() as $v) { echo "got $v\n"; }
?>
--EXPECT--
try
got 1
got 2
--CLEAN--
<?php
