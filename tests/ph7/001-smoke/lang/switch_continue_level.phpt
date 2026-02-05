--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
continue with level in switch inside loop
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$i = 0;
while ($i < 3) {
    switch ($i) {
        case 0:
            echo "case 0\n";
            $i = 1;
            continue 2; // should continue the while loop
        case 1:
            echo "case 1\n";
            $i = 2;
            break;
        case 2:
            echo "case 2\n";
            $i = 3;
    }
}
echo "end\n";
?>
--EXPECT--
case 0
case 1
case 2
end
--CLEAN--
<?php
unset($i);
