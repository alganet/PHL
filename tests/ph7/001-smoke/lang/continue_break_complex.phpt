--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
continue and break in complex nested structures
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
for ($i = 0; $i < 3; $i++) {
    switch ($i) {
        case 0:
            echo "case 0\n";
            continue;
        case 1:
            echo "case 1\n";
            break;
        case 2:
            echo "case 2\n";
            continue 2;
    }
    echo "after switch $i\n";
}
echo "end\n";
?>
--EXPECT--
case 0
after switch 0
case 1
after switch 1
case 2
end
--CLEAN--
<?php
// Clean up if needed

