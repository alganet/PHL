--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test open truncated zip (too short)
--FILE--
<?php
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_trunc.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));
$fp = fopen($fn,'r+');
ftruncate($fp, 10);
fclose($fp);

echo 'exists='.(file_exists($fn)?'1':'0')."\n";
echo 'len='.filesize($fn)."\n";

$zip = @zip_open($fn);
echo 'zip_open='.(is_resource($zip)?'ok':'failed')."\n";
if (is_resource($zip)){
    zip_close($zip);
}
unlink($fn);
?>
--EXPECTF--
%Aexists=1%Alen=10%Azip_open=failed%A
--CLEAN--
<?php
unset($fn, $zip_b64, $fp, $zip);
