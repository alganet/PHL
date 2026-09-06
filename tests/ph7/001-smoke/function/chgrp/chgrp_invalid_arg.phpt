--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chgrp() rejects a non-string filename with php's TypeError
--FILE--
<?php
try {
    chgrp(array(), 'nogroup');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
chgrp(): Argument #1 ($filename) must be of type string, array given
--CLEAN--
<?php
