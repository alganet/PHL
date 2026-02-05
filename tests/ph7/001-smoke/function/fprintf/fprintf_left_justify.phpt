--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf with left justify flag
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_left_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "%-10s", 'test');
    fclose($fp);
    $content = file_get_contents($fname);
    echo bin2hex($content) . PHP_EOL; // Use hex to see spaces
}
?>
--EXPECT--
74657374202020202020
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp, $content);
