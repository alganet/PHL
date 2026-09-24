--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SessionHandler::gc() is the FILES sweep, not the collector that dispatches to it
--DESCRIPTION--
`SessionHandler` is php's built-in files store exposed as a class, so that a
program can put itself in front of it. Every one of its methods called the
file-level primitive directly — except `gc()`, which called the DISPATCHER: the
routine that asks "is a user handler installed?" and calls that handler's `gc()`.
With `session_set_save_handler(new SessionHandler())` the answer is yes, and that
handler's `gc()` is this method, so the two called each other until the native
nesting cap fired. It did not have to be asked for: php's default
`session.gc_probability` runs the collector from one `session_start()` in a
hundred, so the documented decorator idiom carried a 1%-per-request fatal — which
is also how session_save_handler.phpt came to fail one gate run in fifty.
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessgc_" . getmypid();
@mkdir($dir);
foreach (glob("$dir/sess_*") as $f) { @unlink($f); }
session_save_path($dir);
ini_set("session.gc_probability", "0");
ini_set("session.gc_maxlifetime", "1440");

/* A decorator over the built-in store — the idiom the class exists for. */
class SgcDeco extends SessionHandler {
    public $seen = [];
    public function read($id): string { $this->seen[] = "read"; return parent::read($id); }
    public function write($id, $d): bool { $this->seen[] = "write"; return parent::write($id, $d); }
    public function gc($max): int { $this->seen[] = "gc"; return parent::gc($max); }
}
$d = new SgcDeco();
session_set_save_handler($d, true);
session_id("sgcB");
session_start();
$_SESSION["b"] = 2;

/* One store older than the lifetime, one this request is using. */
touch("$dir/sess_sgcOld", time() - 100000);
var_dump(file_exists("$dir/sess_sgcOld"));

/* The collector reaches the decorator, and through it the real sweep. */
var_dump(session_gc() >= 1);
var_dump($d->seen);
var_dump(file_exists("$dir/sess_sgcOld"));

session_write_close();
var_dump(file_exists("$dir/sess_sgcB"));

foreach (glob("$dir/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
echo "END\n";
?>
--EXPECT--
bool(true)
bool(true)
array(2) {
  [0]=>
  string(4) "read"
  [1]=>
  string(2) "gc"
}
bool(false)
bool(true)
END
