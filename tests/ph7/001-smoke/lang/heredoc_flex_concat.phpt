--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc closing marker followed by concatenation operator
--FILE--
<?php
$x = <<<EOT
    concat
    EOT . "!";
echo "[$x]\n";

$y = "prefix-" . <<<EOT
    middle
    EOT . "-suffix";
echo "[$y]\n";
--EXPECT--
[concat!]
[prefix-middle-suffix]
--CLEAN--
<?php
unset($x, $y);
