--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with label references and multiple jumps
--FILE--
<?php
// Test label reference tracking in goto compilation
$count = 0;
goto label1;
label2:
$count += 2;
goto end;
label1:
$count += 1;
goto label2;
end:
echo $count . PHP_EOL;
?>
--EXPECT--
3
--CLEAN--
<?php
unset($count);
