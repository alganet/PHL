--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
disk_total_space() should return a number greater than zero

--FILE--
<?php
$val = disk_total_space(sys_get_temp_dir());
echo "is_numeric=" . (is_numeric($val) ? 'true' : 'false') . PHP_EOL;
echo "gt_zero=" . ($val > 0 ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
is_numeric=true
gt_zero=true
--CLEAN--
<?php
unset($val);
