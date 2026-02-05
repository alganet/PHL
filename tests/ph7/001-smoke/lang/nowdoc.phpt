--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nowdoc string handling
--FILE--
<?php
$txt = <<<'EOD'
$notvar
EOD;
echo $txt . "\n";
?>
--EXPECT--
$notvar
--CLEAN--
<?php
unset($txt);
