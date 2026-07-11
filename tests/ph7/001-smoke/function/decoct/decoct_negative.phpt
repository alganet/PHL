--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
decoct prints the full 64-bit two's-complement for negatives
--FILE--
<?php
echo decoct(-8) . "\n";           // 1777777777777777777770
echo decoct(PHP_INT_MIN) . "\n";  // 1000000000000000000000
echo decoct(8) . "\n";            // 10 (positive control)
?>
--EXPECT--
1777777777777777777770
1000000000000000000000
10
--CLEAN--
<?php
