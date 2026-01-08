--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fread basic test
--FILE--
<?php
$tmp = tempnam(sys_get_temp_dir(), 'ph7_');
file_put_contents($tmp, 'Hello World');
$fp = fopen($tmp, 'r');
$data = fread($fp, 5);
fclose($fp);
unlink($tmp);
echo $data;
?>
--EXPECT--
Hello
--CLEAN--
<?php
if (isset($tmp) && file_exists($tmp)) unlink($tmp);
?>