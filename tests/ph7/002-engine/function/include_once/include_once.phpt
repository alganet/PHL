--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
include_once should include file only once
--FILE--
<?php
$tmp = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_include_once_test.php';
file_put_contents($tmp, '<?php echo "INCL";');
include_once $tmp; // prints INCL
include_once $tmp; // should not print again
?>
--EXPECT--
INCL

--CLEAN--
<?php
@unlink($tmp);
unset($tmp);
?>
