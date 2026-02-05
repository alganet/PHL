--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chgrp() should return FALSE when invalid args provided
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo chgrp(array(), 'nogroup') ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php

