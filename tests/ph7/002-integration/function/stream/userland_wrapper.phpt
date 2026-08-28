--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_wrapper_register — userland streamWrapper protocol
--FILE--
<?php
class MemW {
  public $context;
  public $key = "";
  private $pos = 0;
  private $data = "";
  public static $store = [];
  public function stream_open($path, $mode, $options, &$opened_path) {
    $this->key = substr($path, strlen("memw://"));
    $this->data = self::$store[$this->key] ?? "";
    if (str_contains($mode, "w")) { $this->data = ""; }
    $this->pos = 0;
    return true;
  }
  public function stream_read($count) { $r = substr($this->data, $this->pos, $count); $this->pos += strlen($r); return $r; }
  public function stream_write($data) { $this->data .= $data; self::$store[$this->key] = $this->data; return strlen($data); }
  public function stream_eof() { return $this->pos >= strlen($this->data); }
  public function stream_tell() { return $this->pos; }
  public function stream_seek($o, $w) { $this->pos = $o; return true; }
  public function stream_stat() { return ["size" => strlen($this->data)]; }
  public function stream_close() {}
}
var_export(stream_wrapper_register("memw", "MemW")); echo "\n";
$h = fopen("memw://a", "w"); fwrite($h, "hello wrapper"); fclose($h);
$h = fopen("memw://a", "r"); echo fread($h, 5), "|", ftell($h), "\n";
fseek($h, 6); echo fread($h, 100), "\n";
fclose($h);
echo file_get_contents("memw://a"), "\n";
var_export(in_array("memw", stream_get_wrappers())); echo "\n";
var_export(stream_wrapper_unregister("memw")); echo "\n";
?>
--EXPECT--
true
hello|5
wrapper
hello wrapper
true
true
--CLEAN--
<?php
unset($h);
