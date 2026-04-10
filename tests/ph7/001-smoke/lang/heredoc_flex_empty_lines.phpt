--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 indented heredoc preserves blank body lines
--FILE--
<?php
$x = <<<EOT
    first

    third

    fifth
    EOT;
echo "[$x]\n";
echo strlen($x) . "\n";
--EXPECT--
[first

third

fifth]
19
--CLEAN--
<?php
unset($x);
