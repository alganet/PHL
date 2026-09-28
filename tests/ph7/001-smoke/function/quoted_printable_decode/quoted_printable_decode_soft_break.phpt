--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_decode drops a soft line break, spaces before it included
--FILE--
<?php
foreach (["a=\r\nb", "a=\nb", "a=\rb", "a= \r\nb", "a=\t\r\nb", "a= ", "a=\r\n\r\nb"] as $qpDecSoft) {
    printf("%-16s => %s\n", bin2hex($qpDecSoft), bin2hex(quoted_printable_decode($qpDecSoft)));
}
?>
--EXPECT--
613d0d0a62       => 6162
613d0a62         => 6162
613d0d62         => 6162
613d200d0a62     => 6162
613d090d0a62     => 6162
613d20           => 61
613d0d0a0d0a62   => 610d0a62
--CLEAN--
<?php
unset($qpDecSoft);
