--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc with tab-indented closing marker
--FILE--
<?php
$x = <<<EOT
	alpha
	beta
	gamma
	EOT;
echo "[$x]\n";
echo strlen($x) . "\n";
--EXPECT--
[alpha
beta
gamma]
16
--CLEAN--
<?php
unset($x);
