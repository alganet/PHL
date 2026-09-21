--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Flag-family constants hold php's VALUES, and the families combine as bitmasks
--DESCRIPTION--
The per-constant tests in this directory assert one value each. This one pins the
families as a whole: a bitmask family must be powers of two that OR together without
collisions, and the enum families must start where php starts. The GLOB_* mask is
php 8.5's own portable set (main/php_glob.h, the default non---enable-system-glob
build); php <= 8.4 on glibc reported the host <glob.h> values instead. PHL shipped its own
ladders for PATHINFO_*, GLOB_* and INI_SCANNER_* (and three JSON_* flags), which
silently changed the meaning of valid php source — a literal 64 passed to json_encode()
selected a different flag in each engine.
--FILE--
<?php
foreach ([
    'PATHINFO' => ['PATHINFO_DIRNAME', 'PATHINFO_BASENAME', 'PATHINFO_EXTENSION', 'PATHINFO_FILENAME'],
    'GLOB'     => ['GLOB_ERR', 'GLOB_MARK', 'GLOB_NOCHECK', 'GLOB_NOSORT', 'GLOB_BRACE', 'GLOB_NOESCAPE', 'GLOB_ONLYDIR'],
    'JSON'     => ['JSON_HEX_TAG', 'JSON_HEX_AMP', 'JSON_HEX_APOS', 'JSON_HEX_QUOT', 'JSON_FORCE_OBJECT',
                   'JSON_NUMERIC_CHECK', 'JSON_UNESCAPED_SLASHES', 'JSON_PRETTY_PRINT', 'JSON_UNESCAPED_UNICODE',
                   'JSON_THROW_ON_ERROR'],
    'FILE'     => ['FILE_USE_INCLUDE_PATH', 'FILE_IGNORE_NEW_LINES', 'FILE_SKIP_EMPTY_LINES', 'FILE_APPEND'],
    'LOCK'     => ['LOCK_SH', 'LOCK_EX', 'LOCK_NB'],
] as $ffv_family => $ffv_names) {
    $ffv_seen = 0;
    $ffv_ok = true;
    foreach ($ffv_names as $ffv_n) {
        $ffv_v = constant($ffv_n);
        // every member is a distinct power of two
        if ($ffv_v < 1 || ($ffv_v & ($ffv_v - 1)) !== 0 || ($ffv_seen & $ffv_v) !== 0) {
            $ffv_ok = false;
            echo "BAD $ffv_n=$ffv_v\n";
        }
        $ffv_seen |= $ffv_v;
    }
    echo $ffv_family, ": ", $ffv_ok ? "distinct-bits" : "COLLIDES", " mask=", $ffv_seen, "\n";
}
// enum families start at php's first value
echo "PATHINFO_ALL=", PATHINFO_ALL, "\n";
echo "INI_SCANNER=", INI_SCANNER_NORMAL, ",", INI_SCANNER_RAW, "\n";
echo "SEEK=", SEEK_SET, ",", SEEK_CUR, ",", SEEK_END, "\n";
echo "SCANDIR=", SCANDIR_SORT_ASCENDING, ",", SCANDIR_SORT_DESCENDING, ",", SCANDIR_SORT_NONE, "\n";
echo "EXTR=", EXTR_OVERWRITE, ",", EXTR_SKIP, ",", EXTR_PREFIX_ALL, "\n";
echo "SORT=", SORT_REGULAR, ",", SORT_NUMERIC, ",", SORT_STRING, "\n";
?>
--EXPECT--
PATHINFO: distinct-bits mask=15
GLOB: distinct-bits mask=1073746108
JSON: distinct-bits mask=4194815
FILE: distinct-bits mask=15
LOCK: distinct-bits mask=7
PATHINFO_ALL=15
INI_SCANNER=0,1
SEEK=0,1,2
SCANDIR=0,1,2
EXTR=0,1,3
SORT=0,1,2
--CLEAN--
<?php
