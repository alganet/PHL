<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * Re-take a .phpt corpus at every display_errors x log_errors combination, in
 * BOTH engines, and report the tests whose verdicts stop agreeing.
 *
 * Every other instrument here runs the one combination a stock CLI starts with
 * (display_errors off, log_errors on). That is the single setting in which a
 * diagnostic on the wrong stream, in the wrong shape, or missing a whole copy
 * still reads as correct -- which is how a compile diagnostic that ignored both
 * directives outright passed 4784 rows.
 *
 * The verdicts are compared, not the bytes: at display_errors=1 hundreds of
 * tests legitimately fail in BOTH engines (the display copy is output the
 * --EXPECT-- section was never written for), and a corpus-wide byte diff would
 * drown the signal. A test that passes in one engine and fails in the other,
 * under a combination where both agreed before, is the finding.
 *
 * A test SKIPPED by either engine is not comparable and is left out: the corpus
 * carries `phl-only` cases and `_zend` twin pairs whose whole purpose is to run
 * on one engine.
 *
 * The oracle's four runs depend on the corpus and on php, never on the engine
 * under test, so they are cached in the build directory under a fingerprint of
 * both -- a re-run after an engine change costs four passes, not eight.
 */

$mx_argv = $argv;
if (count($mx_argv) > 0 && strpos($mx_argv[0], '--') !== 0) {
    $mx_argv = array_slice($mx_argv, 1);
}
$mx_target = '';
$mx_oracle = '';
$mx_dir    = 'tests/ph7/002-integration';
$mx_work   = 'build/ini-matrix';
$mx_runner = 'tests/phpt.php';
while (!empty($mx_argv)) {
    $mx_arg = array_shift($mx_argv);
    switch ($mx_arg) {
        case '--target-executable': $mx_target = array_shift($mx_argv); break;
        case '--oracle':            $mx_oracle = array_shift($mx_argv); break;
        case '--target-dir':        $mx_dir    = array_shift($mx_argv); break;
        case '--work-dir':          $mx_work   = array_shift($mx_argv); break;
        case '--runner':            $mx_runner = array_shift($mx_argv); break;
        default:
            echo "Unknown option: $mx_arg\n";
            exit(1);
    }
}
if ($mx_target === '' || $mx_oracle === '') {
    echo "Usage: ini_matrix.php --target-executable <phl> --oracle <php> [--target-dir <dir>]\n";
    exit(1);
}

/* Every combination gets its own copy of the corpus: the runner writes each
 * test's --FILE-- next to the .phpt, so two runs over one directory overwrite
 * each other's scratch files. */
function mx_copy_tree($src, $dst)
{
    if (!is_dir($dst)) {
        mkdir($dst, 0777, true);
    }
    $d = opendir($src);
    while (($e = readdir($d)) !== false) {
        if ($e === '.' || $e === '..') {
            continue;
        }
        if (is_dir($src . '/' . $e)) {
            mx_copy_tree($src . '/' . $e, $dst . '/' . $e);
        } else {
            copy($src . '/' . $e, $dst . '/' . $e);
        }
    }
    closedir($d);
}
function mx_rm_tree($path)
{
    if (!is_dir($path)) {
        return;
    }
    $d = opendir($path);
    while (($e = readdir($d)) !== false) {
        if ($e === '.' || $e === '..') {
            continue;
        }
        if (is_dir($path . '/' . $e)) {
            mx_rm_tree($path . '/' . $e);
        } else {
            unlink($path . '/' . $e);
        }
    }
    closedir($d);
    rmdir($path);
}
/* What the oracle's answers depend on: the corpus text and the oracle build. */
function mx_fingerprint($dir, $oracle)
{
    $acc = array();
    $stack = array($dir);
    while (!empty($stack)) {
        $cur = array_pop($stack);
        $d = opendir($cur);
        while (($e = readdir($d)) !== false) {
            if ($e === '.' || $e === '..') {
                continue;
            }
            $p = $cur . '/' . $e;
            if (is_dir($p)) {
                $stack[] = $p;
            } elseif (substr($p, -5) === '.phpt') {
                $acc[] = $p . ':' . filesize($p) . ':' . md5_file($p);
            }
        }
        closedir($d);
    }
    sort($acc);
    $ver = mx_capture('"' . $oracle . '" --version');
    return md5(implode("\n", $acc) . "\n" . $ver);
}
function mx_capture($cmd)
{
    $fp = popen($cmd, 'r');
    if ($fp === false) {
        return '';
    }
    $out = '';
    while (!feof($fp)) {
        $chunk = fgets($fp);
        if ($chunk === false) break;
        $out .= $chunk;
    }
    pclose($fp);
    return $out;
}
/* One run: TAP in, "name status" lines out, keyed by the test's path INSIDE the
 * corpus so the two engines' private copies compare. */
function mx_run($runner, $host, $target, $dir, $display, $log)
{
    $cmd = '"' . $host . '" "' . $runner . '"'
         . ' --target-executable "' . $target . '"'
         . ' --target-dir "' . $dir . '"'
         . ' --ini display_errors=' . $display
         . ' --ini log_errors=' . $log
         . ' --output-format tap 2>&1';
    $tap = mx_capture($cmd);
    $out = array();
    foreach (explode("\n", $tap) as $line) {
        if (strncmp($line, 'ok ', 3) !== 0 && strncmp($line, 'not ok ', 7) !== 0) {
            continue;
        }
        $bad = (strncmp($line, 'not ok ', 7) === 0);
        $pos = strpos($line, ' - ');
        if ($pos === false) {
            continue;
        }
        $name = substr($line, $pos + 3);
        $hash = strpos($name, ' #');
        $status = $bad ? 'FAIL' : 'PASS';
        if ($hash !== false) {
            if (strpos(substr($name, $hash), '# skip') !== false) {
                $status = 'SKIP';
            }
            $name = substr($name, 0, $hash);
        }
        if (strncmp($name, $dir . '/', strlen($dir) + 1) === 0) {
            $name = substr($name, strlen($dir) + 1);
        }
        $out[$name] = $status;
    }
    return $out;
}

$mx_combos = array(array(0, 0), array(0, 1), array(1, 0), array(1, 1));
$mx_print  = mx_fingerprint($mx_dir, $mx_oracle);
$mx_cache  = $mx_work . '/oracle-' . $mx_print . '.txt';
if (!is_dir($mx_work)) {
    mkdir($mx_work, 0777, true);
}
$mx_oracle_status = array();
if (is_file($mx_cache)) {
    foreach (explode("\n", file_get_contents($mx_cache)) as $line) {
        if ($line === '') {
            continue;
        }
        $f = explode(' ', $line);
        $mx_oracle_status[$f[0]][$f[1]] = $f[2];
    }
    echo "oracle: cached (" . substr($mx_print, 0, 12) . ")\n";
} else {
    $lines = array();
    foreach ($mx_combos as $c) {
        $tag = 'php-' . $c[0] . $c[1];
        $copy = $mx_work . '/' . $tag;
        mx_rm_tree($copy);
        mx_copy_tree($mx_dir, $copy);
        $mx_oracle_status[$tag] = mx_run($mx_runner, $mx_target, $mx_oracle, $copy, $c[0], $c[1]);
        mx_rm_tree($copy);
        foreach ($mx_oracle_status[$tag] as $name => $st) {
            $lines[] = $tag . ' ' . $name . ' ' . $st;
        }
        echo "oracle display_errors=$c[0] log_errors=$c[1]: "
           . count($mx_oracle_status[$tag]) . " rows\n";
    }
    /* One cache at a time: the fingerprint changes with every corpus edit, and
     * a stale file is answers to a question nobody will ask again. */
    $d = opendir($mx_work);
    while (($e = readdir($d)) !== false) {
        if (strncmp($e, 'oracle-', 7) === 0 && $mx_work . '/' . $e !== $mx_cache) {
            unlink($mx_work . '/' . $e);
        }
    }
    closedir($d);
    file_put_contents($mx_cache, implode("\n", $lines) . "\n");
}

$mx_bad = 0;
foreach ($mx_combos as $c) {
    $tag  = 'php-' . $c[0] . $c[1];
    $copy = $mx_work . '/phl-' . $c[0] . $c[1];
    mx_rm_tree($copy);
    mx_copy_tree($mx_dir, $copy);
    $mine = mx_run($mx_runner, $mx_target, $mx_target, $copy, $c[0], $c[1]);
    mx_rm_tree($copy);
    $theirs = isset($mx_oracle_status[$tag]) ? $mx_oracle_status[$tag] : array();
    $diff = array();
    foreach ($mine as $name => $st) {
        if (!isset($theirs[$name]) || $st === 'SKIP' || $theirs[$name] === 'SKIP') {
            continue;
        }
        if ($st !== $theirs[$name]) {
            $diff[] = "  $name: phl=$st php=" . $theirs[$name];
        }
    }
    printf("display_errors=%d log_errors=%d: %d comparable, %d disagree\n",
        $c[0], $c[1], count($mine), count($diff));
    foreach ($diff as $line) {
        echo $line . "\n";
        $mx_bad++;
    }
}
if ($mx_bad > 0) {
    echo "INI MATRIX: $mx_bad disagreement(s)\n";
    exit(1);
}
echo "INI MATRIX: ok\n";
