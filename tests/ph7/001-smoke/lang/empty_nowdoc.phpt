--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty nowdoc strings
--FILE--
<?php
$empty = <<<'EOD'
EOD;
echo "Empty nowdoc: '$empty'\n";
echo "Length: " . strlen($empty) . "\n";
echo "Done\n";
?>
--EXPECT--
Empty nowdoc: ''
Length: 0
Done
--CLEAN--
<?php
unset($empty);
