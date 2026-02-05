--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Heredoc and nowdoc with trailing spaces
--FILE--
<?php
// Test heredoc with trailing spaces in closing delimiter
$heredoc = <<<EOT
This is a heredoc
EOT   ;
echo "Heredoc: '$heredoc'\n";

// Test nowdoc with trailing spaces
$nowdoc = <<<'EOD'
This is a nowdoc
EOD   ;
echo "Nowdoc: '$nowdoc'\n";
?>
--EXPECT--
Heredoc: 'This is a heredoc'
Nowdoc: 'This is a nowdoc'
--CLEAN--
<?php
unset($heredoc, $nowdoc);
