--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' || function_exists('zend_version')) {
    echo "skip";
}
?>
--TEST--
chgrp basic functionality
--FILE--
<?php
$tmp = tempnam(sys_get_temp_dir(), 'ph7_');
$result = chgrp($tmp, 0);
var_dump($result);
unlink($tmp);
?>
--EXPECT--
bool(FALSE)
--CLEAN--
<?php
if (file_exists($tmp)) unlink($tmp);
unset($tmp, $result);
