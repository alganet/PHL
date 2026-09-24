--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a refused $context still performs the operation (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's context fetch raises the TypeError and returns, so the rest of the C
 * function runs with a null context: the write, the unlink, the mkdir and the
 * echo all happen and the exception surfaces afterwards. PHL's half of this
 * pair refuses first. */
$d = sys_get_temp_dir() . '/phl_ctxse_' . getmypid();
@mkdir($d);
$f = "$d/a.txt";
file_put_contents($f, "ORIG\n");
$bad = fopen($f, 'r');

try { file_put_contents($f, 'WROTE', 0, $bad); } catch (Throwable $e) { echo get_class($e), "\n"; }
echo 'content unchanged: ', var_export(file_get_contents($f) === "ORIG\n", true), "\n";

$g = "$d/gone.txt";
file_put_contents($g, 'x');
try { @unlink($g, $bad); } catch (Throwable $e) {}
echo 'still there: ', var_export(is_file($g), true), "\n";

try { mkdir("$d/sub", 0777, false, $bad); } catch (Throwable $e) {}
echo 'not created: ', var_export(!is_dir("$d/sub"), true), "\n";

ob_start();
try { readfile($f, false, $bad); } catch (Throwable $e) {}
$out = ob_get_clean();
echo 'echoed nothing: ', var_export($out === '', true), "\n";

fclose($bad);
@unlink($f); @unlink($g); @rmdir("$d/sub"); rmdir($d);
?>
--EXPECT--
TypeError
content unchanged: false
still there: false
not created: false
echoed nothing: false
