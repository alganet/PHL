--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 heredoc body indented deeper than closing marker
--FILE--
<?php
$x = <<<EOT
  first
      deeper
  last
  EOT;
echo "[$x]\n";
--EXPECT--
[first
    deeper
last]
--CLEAN--
<?php
unset($x);
