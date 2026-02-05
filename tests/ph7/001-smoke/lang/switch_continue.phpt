--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
continue in switch statement acts like break
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$i = 0;
while ($i < 2) {
    switch ($i) {
        case 0:
            echo "case 0\n";
            $i = 1;
            continue; // should break switch, not continue while
        case 1:
            echo "case 1\n";
            $i = 2;
    }
}
echo "end\n";
?>
--EXPECT--
case 0
case 1
end
--CLEAN--
<?php
unset($i);
