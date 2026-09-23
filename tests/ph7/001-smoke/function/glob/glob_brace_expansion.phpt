--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
GLOB_BRACE expands alternatives in order, per-alternative sorted, nesting included
--FILE--
<?php
$d = sys_get_temp_dir() . "/ph7_glob_brace";
@mkdir($d);
touch("$d/a.txt");
touch("$d/b.log");
touch("$d/c.txt");
function glob_brace_show($hits, $d) {
    if ($hits === false) { echo "false\n"; return; }
    $out = array();
    foreach ($hits as $hit) {
        if (strncmp($hit, "$d/", strlen($d) + 1) === 0) {
            $hit = substr($hit, strlen($d) + 1);
        }
        $out[] = $hit;
    }
    echo "[", implode(" ", $out), "]\n";
}
// Alternatives run IN ORDER and each sub-glob sorts its own answers — php
// never re-sorts across them, so the .txt pair comes before the .log hit.
glob_brace_show(glob("$d/*.{txt,log}", GLOB_BRACE), $d);
glob_brace_show(glob("$d/{a,b}.*", GLOB_BRACE), $d);
// Without the flag the braces are literal pattern characters: no match.
glob_brace_show(glob("$d/*.{txt,log}"), $d);
// GLOB_NOCHECK applies per EXPANDED pattern.
glob_brace_show(glob("$d/{x,y}.none", GLOB_BRACE | GLOB_NOCHECK), $d);
// Nested groups expand through the recursion.
glob_brace_show(glob("x{1,{2,3}}y", GLOB_BRACE | GLOB_NOCHECK), $d);
unlink("$d/a.txt");
unlink("$d/b.log");
unlink("$d/c.txt");
rmdir($d);
?>
--EXPECT--
[a.txt c.txt b.log]
[a.txt b.log]
[]
[x.none y.none]
[x1y x2y x3y]
--CLEAN--
<?php
unset($d);
