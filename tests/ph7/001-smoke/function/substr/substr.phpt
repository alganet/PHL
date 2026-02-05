--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: substr basic behaviors
--FILE--
<?php
echo substr('abcdef',2) . "\n";      // from 2 -> 'cdef'
echo substr('abcdef', -1) . "\n";    // last char -> 'f'
echo substr('abcdef', 0, 2) . "\n"; // first 2 -> 'ab'
?>
--EXPECT--
cdef
f
ab
--CLEAN--
<?php

