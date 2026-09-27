--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A directory listing is ordered by BYTES, not by php's value comparison
--DESCRIPTION--
glob(3) sorts its whole answer with strcoll(), which in the C locale php runs in is
a plain byte compare, and scandir()'s php_stream_dirent_alphasort is the same
comparison. Both were sorted here with php's DEFAULT comparison instead, the one
that reads a numeric string as a NUMBER -- so every name that looks like one moved:
`9` came before `10`, `00` before `0`, `+9` sorted as the number nine rather than
under `+`, and `1e3` as a thousand. Nothing announced it; the listing simply came
back in an order php never produces. The fix is SORT_STRING at the five sites that
order a listing (scandir's two, glob's three).
--FILE--
<?php
/* php lists a directory in the order strcoll() puts the BYTES in -- glob(3) sorts
 * its whole answer that way, and scandir() sorts with the same comparison. This
 * engine sorted by VALUE, php's numeric-aware default, so every name that looks
 * like a number changed places: `9` came before `10`, `00` before `0`, and a
 * leading `+` or `-` sorted as a SIGN. Nothing announced it -- the listing was
 * simply in a different order than the one php hands back. */
$ordDir = sys_get_temp_dir() . '/phl_order_' . getmypid();
@mkdir($ordDir);
/* No two of these differ only in CASE: a case-insensitive filesystem would
 * collide them and this test would be measuring that instead. The upper/lower
 * split still shows in the ORDER -- `A` and `Z` sort before `a b`. */
$ordNames = ['0', '00', '1', '2', '9', '10', '100', '0x1A', '1e3', '+9', '-1',
             '2.5', '02', 'A', 'B', 'Z', 'a b', 'a-b', 'a0', '_x', '~y'];
foreach ($ordNames as $ordN) { file_put_contents("$ordDir/$ordN", ''); }
foreach (['1', '10', '9', 'd'] as $ordD) { @mkdir("$ordDir/$ordD.d"); }

function ordShow($label, $list) {
    global $ordDir;
    if (!is_array($list)) { echo str_pad($label, 22), ' => ', var_export($list, true), "\n"; return; }
    /* GLOB_MARK appends the PLATFORM's separator, so it is normalised here:
     * what this pins is the order, and which names came back marked. */
    $list = array_map(fn($x) => str_replace([$ordDir . '/', '\\'], ['', '/'], $x), $list);
    echo str_pad($label, 22), ' => ', json_encode($list), "\n";
}

ordShow('glob *',        glob("$ordDir/*"));
ordShow('glob *.d',      glob("$ordDir/*.d"));
ordShow('glob mark',     glob("$ordDir/*", GLOB_MARK));
ordShow('glob onlydir',  glob("$ordDir/*", GLOB_ONLYDIR));
ordShow('glob dirslash', glob("$ordDir/*/"));
ordShow('glob [0-9]*',   glob("$ordDir/[0-9]*"));
ordShow('glob brace',    glob("$ordDir/{1*,9*}", GLOB_BRACE));
ordShow('glob nested',   glob("$ordDir/*.d/*"));
ordShow('scandir asc',   scandir($ordDir));
ordShow('scandir desc',  scandir($ordDir, SCANDIR_SORT_DESCENDING));
/* SORT_NONE is the raw directory order, which no two filesystems agree on;
 * what it pins is that the COUNT is the same and nothing was dropped. */
ordShow('scandir none n', [count(scandir($ordDir, SCANDIR_SORT_NONE))]);

foreach (['1.d', '10.d', '9.d', 'd.d'] as $ordD) { @rmdir("$ordDir/$ordD"); }
foreach ($ordNames as $ordN) { @unlink("$ordDir/$ordN"); }
@rmdir($ordDir);
--EXPECT--
glob *                 => ["+9","-1","0","00","02","0x1A","1","1.d","10","10.d","100","1e3","2","2.5","9","9.d","A","B","Z","_x","a b","a-b","a0","d.d","~y"]
glob *.d               => ["1.d","10.d","9.d","d.d"]
glob mark              => ["+9","-1","0","00","02","0x1A","1","1.d\/","10","10.d\/","100","1e3","2","2.5","9","9.d\/","A","B","Z","_x","a b","a-b","a0","d.d\/","~y"]
glob onlydir           => ["1.d","10.d","9.d","d.d"]
glob dirslash          => ["1.d\/","10.d\/","9.d\/","d.d\/"]
glob [0-9]*            => ["0","00","02","0x1A","1","1.d","10","10.d","100","1e3","2","2.5","9","9.d"]
glob brace             => ["1","1.d","10","10.d","100","1e3","9","9.d"]
glob nested            => []
scandir asc            => ["+9","-1",".","..","0","00","02","0x1A","1","1.d","10","10.d","100","1e3","2","2.5","9","9.d","A","B","Z","_x","a b","a-b","a0","d.d","~y"]
scandir desc           => ["~y","d.d","a0","a-b","a b","_x","Z","B","A","9.d","9","2.5","2","1e3","100","10.d","10","1.d","1","0x1A","02","00","0","..",".","-1","+9"]
scandir none n         => ["27"]
