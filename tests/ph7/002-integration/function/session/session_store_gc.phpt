--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session_start() OPENS the store (creating it 0600), session_gc() collects it, and an unusable save path refuses the start
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessgc_" . getmypid();
@mkdir($dir);
$log = [];
$warn = [];
$pid = getmypid();
set_error_handler(function ($no, $msg) use (&$warn, $dir, $pid) {
    // Respect '@': the store layer suppresses the wrapper's own diagnostic and
    // reports the failure in php's words instead.
    if (!(error_reporting() & $no)) {
        return true;
    }
    $warn[] = preg_replace(['/sess_[0-9a-f]{32}/', '/ \((started|sent) from .*$/'],
        ['sess_<fresh>', ''],
        str_replace([$dir, "gcA" . $pid, "gcB" . $pid, DIRECTORY_SEPARATOR . "sess_"],
                    ["<dir>", "<idA>", "<idB>", "/sess_"], $msg));
    return true;
});
$tag = "gcA" . getmypid();

// The store exists, empty and 0600, from the moment the session starts — not from
// the first write.
ini_set("session.save_path", $dir);
ini_set("session.gc_probability", "0");
ini_set("session.gc_maxlifetime", "1");
session_id($tag);
session_start();
$f = $dir . "/sess_" . $tag;
$log[] = "created: " . var_export(file_exists($f), true) . " size=" . filesize($f)
    . " perm=" . substr(sprintf("%o", fileperms($f)), -4);

// The collector is the STORE's, so php only reaches it through an open session —
// and only the sess_ prefix is its business.
$_SESSION["v"] = 1;
touch($dir . "/sess_stale1", time() - 100000);
touch($dir . "/sess_stale2", time() - 100000);
touch($dir . "/keepme.txt", time() - 100000);
$log[] = "gc active: " . var_export(session_gc(), true);
$names = array_map("basename", glob($dir . "/*"));
sort($names);
$log[] = "left: " . str_replace($tag, "<id>", implode(",", $names));
session_write_close();
$log[] = "gc closed: " . var_export(session_gc(), true);
foreach (glob($dir . "/*") as $g) { @unlink($g); }

// A save path the store cannot be opened under is a REFUSED start, not an empty
// session: reading a missing file as "no data yet" throws away every write.
ini_set("session.save_path", "/phl-nonexistent-save-path");
session_id("gcB" . getmypid());
$log[] = "bad path: " . var_export(session_start(), true) . " status=" . session_status();

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n", implode("\n", $warn), "\n";
?>
--EXPECT--
created: true size=0 perm=0600
gc active: 2
left: keepme.txt,sess_<id>
gc closed: false
bad path: false status=1
== warnings ==
session_gc(): Session cannot be garbage collected when there is no active session
session_start(): open(/phl-nonexistent-save-path/sess_<idB>, O_RDWR) failed: No such file or directory (2)
session_start(): Failed to read session data: files (path: /phl-nonexistent-save-path)
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessgc_" . getmypid();
foreach (glob($dir . "/*") as $f) { @unlink($f); }
@rmdir($dir);
