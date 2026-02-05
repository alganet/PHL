--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
require_once should include file only once
--FILE--
<?php
$tmp = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_require_once_test.php';
file_put_contents($tmp, '<?php echo "REQ";');
require_once $tmp; // prints REQ
require_once $tmp; // should not print again
?>
--EXPECT--
REQ
--CLEAN--
<?php
@unlink($tmp);
unset($tmp);
