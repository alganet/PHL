--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Control structures
--FILE--
<?php
$a = 5;
if ($a > 3) {
    echo "greater";
} else {
    echo "not greater";
}
echo "\n";
if ($a < 3) {
    echo "less";
} else {
    echo "not less";
}
?>
--EXPECT--
greater
not less
--CLEAN--
<?php
unset($a);
?>