--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: captures variables referenced inside a heredoc
--FILE--
<?php
$who = "PHL";
$n = 7;
$f = fn() => <<<EOT
hello $who value=$n
EOT;
echo $f(), "\n";
?>
--EXPECT--
hello PHL value=7
--CLEAN--
<?php
unset($f, $who, $n);
