--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (PHP_OS === 'WINNT') {
    echo "skip POSIX only";
}
?>
--TEST--
chgrp() with an unknown name warns and returns false
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$chgrpInvTmp = tempnam(sys_get_temp_dir(), 'phl_');
file_put_contents($chgrpInvTmp, 'test');
echo var_export(chgrp($chgrpInvTmp, 'nonexistentgroup12345'), true), "\n";
unlink($chgrpInvTmp);
restore_error_handler();
?>
--EXPECT--
[2] chgrp(): Unable to find gid for nonexistentgroup12345
false
--CLEAN--
<?php
