--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a refused $context performs NOTHING (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED DIVERGENCE. php fetches the context resource with a macro that
 * raises the TypeError and RETURNS — the C function then runs to completion
 * with a null context, so the file is written, the file is deleted, the
 * directory is created and the bytes are echoed BEFORE the exception surfaces.
 * PHL refuses first: a builtin that throws does not go on to act (the
 * throw-aborts-the-builtin rail every host function here rides). The message
 * and the class are identical; only the side effect differs. */
$d = sys_get_temp_dir() . '/phl_ctxse_' . getmypid();
@mkdir($d);
$f = "$d/a.txt";
file_put_contents($f, "ORIG\n");
$bad = fopen($f, 'r');

try { file_put_contents($f, 'WROTE', 0, $bad); } catch (Throwable $e) { echo get_class($e), "\n"; }
echo 'content unchanged: ', var_export(file_get_contents($f) === "ORIG\n", true), "\n";

$g = "$d/gone.txt";
file_put_contents($g, 'x');
try { unlink($g, $bad); } catch (Throwable $e) {}
echo 'still there: ', var_export(is_file($g), true), "\n";

try { mkdir("$d/sub", 0777, false, $bad); } catch (Throwable $e) {}
echo 'not created: ', var_export(!is_dir("$d/sub"), true), "\n";

ob_start();
try { readfile($f, false, $bad); } catch (Throwable $e) {}
$out = ob_get_clean();
echo 'echoed nothing: ', var_export($out === '', true), "\n";

fclose($bad);
unlink($f); unlink($g); rmdir($d);
?>
--EXPECT--
TypeError
content unchanged: true
still there: true
not created: true
echoed nothing: true
