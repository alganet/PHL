--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exception getTrace() captures the full call chain with per-frame file attribution
--SKIPIF--
skip: macos
--DESCRIPTION--
Regression: a Throwable's trace was stamped from only the innermost frame, so
getTrace() always returned depth 1 (getTraceAsString showed just "#0 ... #1 {main}")
and getFile() reported the include-stack top rather than the file where `new` ran.
The trace now walks the full frame chain (shared with debug_backtrace) and each
frame's file is the CALL SITE's file (the caller's defining file), so a call chain
spanning multiple files attributes each frame correctly — matching php. The running
script's own basename is normalized to MAIN so the --EXPECT-- is stable.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/etd_' . getmypid();
@mkdir($dir);
file_put_contents($dir . '/lib.php', "<?php\nfunction lib_level2(\$x){ return new RuntimeException('boom'); }\nfunction lib_level1(\$x){ return lib_level2(\$x); }\n");
require $dir . '/lib.php';
function top() { return lib_level1('arg'); }
$e = top();
$self = basename(__FILE__);
$norm = function ($s) use ($self) { return str_replace($self, 'MAIN', $s); };
echo "class=", get_class($e), "\n";
echo "getFile=", basename($e->getFile()), "\n";
echo "getLine=", $e->getLine(), "\n";
echo "frames=", count($e->getTrace()), "\n";
foreach ($e->getTrace() as $i => $f) {
    echo "#$i fn=", $f['function'], " file=", $norm(basename($f['file'])), " line=", $f['line'], "\n";
}
echo "[trace]\n";
echo $norm(preg_replace('#[^ (]*[\\\\/]#', '', $e->getTraceAsString())), "\n";
@unlink($dir . '/lib.php');
@rmdir($dir);
?>
--EXPECT--
class=RuntimeException
getFile=lib.php
getLine=2
frames=3
#0 fn=lib_level2 file=lib.php line=3
#1 fn=lib_level1 file=MAIN line=6
#2 fn=top file=MAIN line=7
[trace]
#0 lib.php(3): lib_level2()
#1 MAIN(6): lib_level1()
#2 MAIN(7): top()
#3 {main}
