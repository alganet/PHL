--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc and nowdoc directly subscripted with [N]
--FILE--
<?php
$c = (<<<EOT
    abcdef
    EOT)[2];
echo "[$c]\n";

$d = (<<<EOT
    zyxwv
    EOT)[0];
echo "[$d]\n";

$e = (<<<'EOT'
    raw $var literal
    EOT)[4];
echo "[$e]\n";
--EXPECT--
[c]
[z]
[$]
--CLEAN--
<?php
unset($c, $d, $e);
