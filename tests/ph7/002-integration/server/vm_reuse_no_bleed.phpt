--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server reuses the compiled VM per request without state bleed
--SKIPIF--
<?php
$fp = popen("curl --version 2>/dev/null", "r");
$out = fgets($fp);
fclose($fp);
if (strlen($out) == 0) { echo "skip curl not available"; }
?>
--FILE--
<?php
/* The server compiles each script once and re-executes it per request, calling
 * ph7_vm_reset() in between. This asserts that every per-execution vector is
 * cleared: a top-level global ($x), a function static (ctr), a class static
 * (C::$s), a runtime closure ($cl) and a superglobal ($_GET) — each request
 * prints identical counters (all 1) with q reflecting that request only. It also
 * asserts that *definitions* persist: an include_once'd config file (run only on
 * the first request) and the constant it define()s must remain visible on every
 * subsequent request (cfg=1.0), not vanish once the file is no longer re-run. */
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_reusetest_' . getmypid();
$port = 19300 + (getmypid() % 100);
mkdir($tmpdir);

file_put_contents($tmpdir . '/cfg.php', "<?php define('CFG_VERSION', '1.0');\n");

$script = <<<'EOF'
<?php
include_once __DIR__ . '/cfg.php';
$x = ($x ?? 0) + 1;
function ctr(){ static $n = 0; return ++$n; }
class C { public static int $s = 0; }
C::$s++;
$cl = fn($v) => $v + 1;
echo "x=$x ctr=" . ctr() . " s=" . C::$s . " q=" . ($_GET["q"] ?? "none") . " cl=" . $cl(40)
   . " cfg=" . (defined('CFG_VERSION') ? CFG_VERSION : 'UNDEF') . "\n";
EOF;
file_put_contents($tmpdir . '/bleed.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

foreach (array('a', 'b', 'c') as $q) {
    $fp2 = popen('curl -s "http://localhost:' . $port . '/bleed.php?q=' . $q . '" 2>/dev/null', 'r');
    $out = '';
    while (!feof($fp2)) { $out .= fgets($fp2); }
    fclose($fp2);
    echo $out;
}

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/bleed.php');
unlink($tmpdir . '/cfg.php');
rmdir($tmpdir);
?>
--EXPECT--
x=1 ctr=1 s=1 q=a cl=41 cfg=1.0
x=1 ctr=1 s=1 q=b cl=41 cfg=1.0
x=1 ctr=1 s=1 q=c cl=41 cfg=1.0
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $out, $script, $q);
