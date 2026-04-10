--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 indented heredoc closing marker followed by semicolon
--FILE--
<?php
$x = <<<EOT
    one
    two
    EOT;
$y = "next";
echo "[$x][$y]\n";
--EXPECT--
[one
two][next]
--CLEAN--
<?php
unset($x, $y);
