--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_encode keeps a CRLF pair and escapes a space before CR
--FILE--
<?php
foreach (["a\r\nb", "a\rb", "a\nb", " \r\n", " \r", " \n", 'trailing ', "trailing\t"] as $qpEncEol) {
    printf("%-12s => %s\n", bin2hex($qpEncEol), bin2hex(quoted_printable_encode($qpEncEol)));
}
?>
--EXPECT--
610d0a62     => 610d0a62
610d62       => 613d304462
610a62       => 613d304162
200d0a       => 3d32300d0a
200d         => 3d32303d3044
200a         => 203d3041
747261696c696e6720 => 747261696c696e6720
747261696c696e6709 => 747261696c696e673d3039
--CLEAN--
<?php
unset($qpEncEol);
