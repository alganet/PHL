--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CURLFile and CURLStringFile: the two upload boxes, and curl_file_create()
--DESCRIPTION--
The only classes in ext/curl a script may construct, and the only ones that are
not final: `class MyUpload extends CURLFile {}` is ordinary php, which is why
the mime builder recognises a SUBCLASS too.

Their shapes are not each other's. CURLFile gives its three slots an empty
default and CURLStringFile gives its three none at all, so a CURLStringFile
property that no constructor wrote is UNINITIALIZED where the CURLFile one
reads back "". CURLFile refuses serialization and CURLStringFile does not --
php's own asymmetry, and a sensible one: the string box carries its bytes with
it while the file box only names something another process may not have.

The five accessors are the php-4-era spelling of three public properties that
are readable and writable anyway, and their return types are TENTATIVE (php's
`@tentative-return-type`), so getReturnType() answers null for each.

Only the FILENAME is screened for a NUL byte -- not the mime type, not the
posted name -- and the refusal names the DECLARING class, so a subclass still
reports `CURLFile::__construct()`. curl_file_create() is the same three writes
under its own name, which its own refusal repeats.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
class CurlBoxSub extends CURLFile
{
}

$curlBoxShow = static function ($o) {
    $out = get_class($o) . '(';
    foreach (get_object_vars($o) as $k => $v) {
        $out .= $k . '=' . str_replace("\0", '<NUL>', var_export($v, true)) . ' ';
    }
    return rtrim($out) . ')';
};

/* the declared shape */
foreach (array('CURLFile', 'CURLStringFile') as $class) {
    $r = new ReflectionClass($class);
    printf("%s final=%s serializable=%s\n", $class, var_export($r->isFinal(), true),
        var_export(!method_exists($class, '__serialize'), true));
    foreach ($r->getProperties() as $p) {
        printf("  %s %s $%s%s\n", implode(' ', Reflection::getModifierNames($p->getModifiers())),
            (string) $p->getType(), $p->getName(),
            $p->hasDefaultValue() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
    }
    foreach ($r->getMethods() as $m) {
        $args = array();
        foreach ($m->getParameters() as $p) {
            $args[] = (string) $p->getType() . ' $' . $p->getName()
                . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
        }
        printf("  %s(%s) declared=%s tentative=%s\n", $m->getName(), implode(', ', $args),
            var_export($m->hasReturnType() ? (string) $m->getReturnType() : null, true),
            var_export($m->hasTentativeReturnType() ? (string) $m->getTentativeReturnType() : null, true));
    }
}

/* construction, and the three writes */
echo $curlBoxShow(new CURLFile('/tmp/one')), "\n";
echo $curlBoxShow(new CURLFile('/tmp/two', 'text/plain')), "\n";
echo $curlBoxShow(new CURLFile('/tmp/three', 'text/plain', 'posted.txt')), "\n";
echo $curlBoxShow(new CURLFile('/tmp/four', null, 'posted.txt')), "\n";
echo $curlBoxShow(curl_file_create('/tmp/five', 'a/b', 'c.txt')), "\n";
echo $curlBoxShow(new CURLStringFile('bytes', 'n.bin')), "\n";
echo $curlBoxShow(new CURLStringFile("with\0nul", 'n.bin', 'text/plain')), "\n";
echo $curlBoxShow(new CurlBoxSub('/tmp/sub')), "\n";

/* the getters and setters read and write the same slots */
$f = new CURLFile('/tmp/six');
printf("getters: %s %s %s\n", var_export($f->getFilename(), true),
    var_export($f->getMimeType(), true), var_export($f->getPostFilename(), true));
$f->setMimeType('image/png');
$f->setPostFilename('shot.png');
echo $curlBoxShow($f), "\n";
$f->name = '/tmp/written-directly';
printf("direct write: %s\n", var_export($f->getFilename(), true));

/* the one screened argument, from all three doors */
foreach (array(
    'CURLFile' => static function () {
        return new CURLFile("a\0b");
    },
    'subclass' => static function () {
        return new CurlBoxSub("a\0b");
    },
    'curl_file_create' => static function () {
        return curl_file_create("a\0b");
    },
) as $label => $fn) {
    try {
        $fn();
        printf("%-16s no refusal\n", $label);
    } catch (Throwable $e) {
        printf("%-16s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}
/* and the two that are NOT screened */
$n = new CURLFile('/tmp/seven', "a\0b", "p\0n");
printf("unscreened: %s %s\n", var_export($n->getMimeType(), true),
    var_export($n->getPostFilename(), true));

/* serialization: refused for one box, ordinary for the other */
foreach (array(new CURLFile('/tmp/eight'), new CURLStringFile('d', 'n')) as $o) {
    try {
        printf("%-14s %s\n", get_class($o), serialize($o));
    } catch (Throwable $e) {
        printf("%-14s %s: %s\n", get_class($o), get_class($e), $e->getMessage());
    }
}
echo $curlBoxShow(unserialize(serialize(new CURLStringFile('d', 'n')))), "\n";
?>
--EXPECT--
CURLFile final=false serializable=true
  public string $name = ''
  public string $mime = ''
  public string $postname = ''
  __construct(string $filename, ?string $mime_type = NULL, ?string $posted_filename = NULL) declared=NULL tentative=NULL
  getFilename() declared=NULL tentative='string'
  getMimeType() declared=NULL tentative='string'
  getPostFilename() declared=NULL tentative='string'
  setMimeType(string $mime_type) declared=NULL tentative='void'
  setPostFilename(string $posted_filename) declared=NULL tentative='void'
CURLStringFile final=false serializable=true
  public string $data
  public string $postname
  public string $mime
  __construct(string $data, string $postname, string $mime = 'application/octet-stream') declared=NULL tentative=NULL
CURLFile(name='/tmp/one' mime='' postname='')
CURLFile(name='/tmp/two' mime='text/plain' postname='')
CURLFile(name='/tmp/three' mime='text/plain' postname='posted.txt')
CURLFile(name='/tmp/four' mime='' postname='posted.txt')
CURLFile(name='/tmp/five' mime='a/b' postname='c.txt')
CURLStringFile(data='bytes' postname='n.bin' mime='application/octet-stream')
CURLStringFile(data='with' . "\0" . 'nul' postname='n.bin' mime='text/plain')
CurlBoxSub(name='/tmp/sub' mime='' postname='')
getters: '/tmp/six' '' ''
CURLFile(name='/tmp/six' mime='image/png' postname='shot.png')
direct write: '/tmp/written-directly'
CURLFile         ValueError: CURLFile::__construct(): Argument #1 ($filename) must not contain any null bytes
subclass         ValueError: CURLFile::__construct(): Argument #1 ($filename) must not contain any null bytes
curl_file_create ValueError: curl_file_create(): Argument #1 ($filename) must not contain any null bytes
unscreened: 'a' . "\0" . 'b' 'p' . "\0" . 'n'
CURLFile       Exception: Serialization of 'CURLFile' is not allowed
CURLStringFile O:14:"CURLStringFile":3:{s:4:"data";s:1:"d";s:8:"postname";s:1:"n";s:4:"mime";s:24:"application/octet-stream";}
CURLStringFile(data='d' postname='n' mime='application/octet-stream')
--CLEAN--
<?php
unset($curlBoxShow, $f, $n, $o, $r, $m, $p, $class, $args, $label, $fn, $e);
?>
