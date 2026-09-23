--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
disk_free_space()/disk_total_space() answer a float, and false with a warning
--FILE--
<?php
/* php's return type here is float|false. PHL answered an int on success and the
 * VFS's raw int(-1) on failure -- truthy, and a plausible byte count for a caller
 * that only tests `if ($free)` or compares against a threshold. */
$dir  = sys_get_temp_dir();
$miss = $dir . DIRECTORY_SEPARATOR . 'phl_diskspace_missing_xyz';
@unlink($miss);

foreach (['disk_free_space', 'disk_total_space', 'diskfreespace'] as $fn) {
	$r = $fn($dir);
	echo str_pad($fn, 18), get_debug_type($r), ' positive=', var_export($r > 0, true), "\n";
}
echo 'free <= total: ', var_export(disk_free_space($dir) <= disk_total_space($dir), true), "\n";

/* On failure: FALSE, and a warning whose text is the C library's own reason for
 * it -- so the assertion is on the qualifier php puts in front of it, not on the
 * platform's wording. */
set_error_handler(function ($no, $msg) {
	echo '[', $no, '] ', substr($msg, 0, strpos($msg, ':') + 1), "\n";
	return true;
});
foreach (['disk_free_space', 'disk_total_space', 'diskfreespace'] as $fn) {
	echo str_pad($fn, 18), var_export($fn($miss), true), "\n";
}
restore_error_handler();
?>
--EXPECT--
disk_free_space   float positive=true
disk_total_space  float positive=true
diskfreespace     float positive=true
free <= total: true
disk_free_space   [2] disk_free_space():
false
disk_total_space  [2] disk_total_space():
false
diskfreespace     [2] diskfreespace():
false
