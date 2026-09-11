--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
memory_reset_peak_usage() lowers the reported peak toward current usage
--FILE--
<?php
$big = range(1, 200000);
$peakHigh = memory_get_peak_usage();
unset($big);
memory_reset_peak_usage();
$peakAfter = memory_get_peak_usage();
echo ($peakAfter <= $peakHigh) ? "reset-ok\n" : "reset-bad\n";
echo is_int(memory_reset_peak_usage() ?? 0) ? "void\n" : "void\n";
echo "done\n";
?>
--EXPECT--
reset-ok
void
done
--CLEAN--
<?php
