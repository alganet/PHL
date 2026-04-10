--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 flexible heredoc keeps legacy column-0 closing marker working
--FILE--
<?php
$x = <<<EOT
line1
line2
EOT;
echo "[$x]\n";

$y = <<<'EOT'
raw $var
EOT;
echo "[$y]\n";
--EXPECT--
[line1
line2]
[raw $var]
--CLEAN--
<?php
unset($x, $y);
