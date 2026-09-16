--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break 3 inside two loops is php's "Cannot 'break' 3 levels" (was a bare skip freezing PHL's not-in-context wording, which ignored the level entirely)

--FILE--
<?php
$result = '';
for ($i = 0; $i < 2; $i++) {
    $result .= "outer$i ";
    for ($j = 0; $j < 2; $j++) {
        if ($i == 1 && $j == 0) {
            break 3; // Invalid level
        }
        $result .= "inner$j ";
    }
}
$result .= "end";
echo $result;
?>
--EXPECTF--
%AFatal error:%ACannot 'break' 3 levels%A
--CLEAN--
<?php
unset($result);
