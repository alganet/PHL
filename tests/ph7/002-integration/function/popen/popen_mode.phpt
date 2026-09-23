--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
popen() takes php's four modes, and an empty command is not its business
--FILE--
<?php
/* php's popen() has exactly ONE rule for $mode, and it is not "starts with r or
 * w": one 'b' -- C's binary flag, which popen(3) itself refuses -- is dropped on
 * POSIX, and what is left must be "r", "w", "rb" or "wb". PHL read mode[0] and
 * handed the REST to popen(3) unexamined, so 'rb', the ordinary binary
 * spelling, answered FALSE and 'rr' opened a pipe php refuses. */
$popen_cmd = 'echo hi > ' . (PHP_OS_FAMILY === 'Windows' ? 'NUL' : '/dev/null');
foreach (['r', 'w', 'rb', 'wb', 'rt', 'r+', 'x', 'rr', ''] as $popen_mode) {
    printf("%-4s ", var_export($popen_mode, true));
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

/* An EMPTY command is not refused: php hands it to the shell, which answers
 * success and prints nothing. (exec/system/passthru/shell_exec DO refuse it --
 * that is their own rule, not the pipe's.) */
$popen_fp = popen('', 'r');
var_dump(is_resource($popen_fp), fread($popen_fp, 10), pclose($popen_fp));
?>
--EXPECT--
'r'  resource
'w'  resource
'rb' resource
'wb' resource
'rt' ValueError: popen(): Argument #2 ($mode) must be one of "r", "rb", "w", or "wb"
'r+' ValueError: popen(): Argument #2 ($mode) must be one of "r", "rb", "w", or "wb"
'x'  ValueError: popen(): Argument #2 ($mode) must be one of "r", "rb", "w", or "wb"
'rr' ValueError: popen(): Argument #2 ($mode) must be one of "r", "rb", "w", or "wb"
''   FALSE
bool(true)
string(0) ""
int(0)
