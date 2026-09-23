--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
popen() drops exactly one 'b' from the mode, and only on POSIX
--SKIPIF--
<?php
if (PHP_OS_FAMILY === "Windows") {
    echo "skip the b-drop is POSIX-only: php hands cmd.exe the mode as written, where both of these rows are refusals instead. The four accepted modes are covered cross-platform by popen_mode.phpt";
}
?>
--FILE--
<?php
/* The 'b' drop is POSIX-only, and php drops exactly ONE: 'br' becomes 'r' and
 * opens, while 'rbb' becomes 'rb' -- which passes php's check and is then
 * refused by popen(3) itself, so it is an open FAILURE rather than a ValueError.
 * On cmd.exe php keeps the mode as written, so both of these are refusals there. */
$popen_cmd = 'echo hi > /dev/null';
foreach (['br', 'rbb'] as $popen_mode) {
    printf("%-5s ", var_export($popen_mode, true));
    try {
        $popen_fp = @popen($popen_cmd, $popen_mode);
        if ($popen_fp === false) {
            echo "FALSE\n";
        } else {
            echo "resource\n";
            @pclose($popen_fp);
        }
    } catch (ValueError $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
'br'  resource
'rbb' FALSE
