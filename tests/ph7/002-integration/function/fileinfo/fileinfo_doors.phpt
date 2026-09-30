--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/fileinfo: what each door screens, opens and answers
--SKIPIF--
<?php
/* a capability guard: CI's Windows php is built without ext/fileinfo */
if (!extension_loaded('fileinfo')) {
    die("skip this php has no ext/fileinfo\n");
}
--FILE--
<?php
/* The doors rather than the answers: which argument each one refuses and how,
 * what a path that will not open says, the directory shortcut, the per-call
 * flag override, and the stream a mime_content_type() may be handed. */
error_reporting(E_ALL);
set_error_handler(function (int $n, string $m): bool { echo "  [$n] $m\n"; return true; });

$dir = sys_get_temp_dir() . '/phlt-finfo-' . getmypid();
@mkdir($dir);
$file = $dir . '/plain.txt';
file_put_contents($file, "hello\n");

function t(string $label, callable $f): void
{
    try { $r = $f(); } catch (Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-44s %s\n", $label, var_export($r, true));
}
$finfo = new finfo();
$typed = new finfo(FILEINFO_MIME_TYPE);

echo "-- an empty name, whose ValueError names argument #2 either way\n";
t('the method',           fn() => $finfo->file(''));
t('the function',         fn() => finfo_file($finfo, ''));
t('mime_content_type names its own', fn() => mime_content_type(''));

echo "-- a NUL byte is the ordinary path refusal\n";
t('the method',           fn() => $finfo->file("$file\0x"));
t('the function',         fn() => finfo_file($finfo, "$file\0x"));
t('the constructor screens its database', fn() => new finfo(FILEINFO_NONE, "x\0y"));

echo "-- a path that will not open\n";
t('the method',           fn() => $finfo->file('/phlt-no-such-file'));
t('the function',         fn() => finfo_file($finfo, '/phlt-no-such-file'));
t('mime_content_type',    fn() => mime_content_type('/phlt-no-such-file'));
t('a scheme nothing implements', fn() => $finfo->file('phlt-nope://x'));

echo "-- a directory is one word, whatever the flags say\n";
t('the description face', fn() => $finfo->file($dir));
t('the type face',        fn() => $typed->file($dir));
t('both mime faces',      fn() => (new finfo(FILEINFO_MIME))->file($dir));
t('mime_content_type',    fn() => mime_content_type($dir));

echo "-- the flags argument is this call's only, and 0 means \"keep mine\"\n";
t('the object\'s own',    fn() => $finfo->file($file));
t('overridden',           fn() => $finfo->file($file, FILEINFO_MIME_TYPE));
t('...and not kept',      fn() => $finfo->file($file));
t('0 keeps the object\'s',fn() => $typed->file($file, FILEINFO_NONE));
t('buffer() overrides too',fn() => $typed->buffer('hello', FILEINFO_MIME_ENCODING));
t('...for one call',      fn() => $typed->buffer('hello'));
t('set_flags is the lasting one', fn() => $typed->set_flags(FILEINFO_NONE));
t('...and it lasts',      fn() => $typed->buffer('hello'));

echo "-- finfo_open\n";
t('with nothing',         fn() => finfo_open()->buffer('hello'));
t('with flags',           fn() => finfo_open(FILEINFO_MIME_TYPE)->buffer('hello'));
t('an EMPTY database is the default', fn() => finfo_open(FILEINFO_MIME_TYPE, '')->buffer('hello'));
t('the procedural set_flags', function () use ($file) {
    $h = finfo_open();
    finfo_set_flags($h, FILEINFO_MIME_TYPE);
    return finfo_file($h, $file);
});

echo "-- the streams a name may reach\n";
t('data://',              fn() => $typed->file('data://text/plain,hello'));
t('data:// with php in it',fn() => $typed->file('data://text/plain,<?php echo 1;'));
t('php://memory is empty', fn() => $typed->file('php://memory'));

echo "-- mime_content_type also takes an OPEN stream, from its beginning\n";
$handle = fopen($file, 'r');
fread($handle, 3);
t('the stream',           fn() => mime_content_type($handle));
t('and it is left where it was', fn() => ftell($handle));
fclose($handle);
t('a closed one',         fn() => mime_content_type($handle));
t('something that is neither', fn() => mime_content_type([]));

echo "-- a userland wrapper answers both doors\n";
class PhltFinfoWrapper
{
    public $context;
    private int $pos = 0;
    private string $data = "GIF89a\x10\x00\x20\x00\x00\x00\x00\x00\x00";

    public function stream_open(string $path, string $mode, int $options, ?string &$opened): bool
    {
        $this->pos = 0;
        return true;
    }

    public function stream_read(int $count): string
    {
        $chunk = substr($this->data, $this->pos, $count);
        $this->pos += strlen($chunk);
        return $chunk;
    }

    public function stream_eof(): bool
    {
        return $this->pos >= strlen($this->data);
    }

    public function stream_stat(): array
    {
        return ['mode' => 0100644, 'size' => strlen($this->data)];
    }

    public function url_stat(string $path, int $flags): array
    {
        if (str_ends_with($path, '/adir')) {
            return ['mode' => 040755, 'size' => 0];
        }
        return ['mode' => 0100644, 'size' => strlen($this->data)];
    }
}
stream_wrapper_register('phltfinfo', PhltFinfoWrapper::class);
t('a wrapped file',       fn() => $typed->file('phltfinfo://afile'));
t('its description',      fn() => $finfo->file('phltfinfo://afile'));
t('a wrapped DIRECTORY',  fn() => $typed->file('phltfinfo://adir'));
stream_wrapper_unregister('phltfinfo');

unlink($file);
rmdir($dir);
--EXPECT--
-- an empty name, whose ValueError names argument #2 either way
the method                                   'ValueError: finfo::file(): Argument #2 ($flags) must not be empty'
the function                                 'ValueError: finfo_file(): Argument #2 ($filename) must not be empty'
mime_content_type names its own              'ValueError: mime_content_type(): Argument #1 ($filename) must not be empty'
-- a NUL byte is the ordinary path refusal
the method                                   'ValueError: finfo::file(): Argument #1 ($filename) must not contain any null bytes'
the function                                 'ValueError: finfo_file(): Argument #2 ($filename) must not contain any null bytes'
the constructor screens its database         'ValueError: finfo::__construct(): Argument #2 ($magic_database) must not contain any null bytes'
-- a path that will not open
  [2] finfo::file(/phlt-no-such-file): Failed to open stream: No such file or directory
the method                                   false
  [2] finfo_file(/phlt-no-such-file): Failed to open stream: No such file or directory
the function                                 false
  [2] mime_content_type(/phlt-no-such-file): Failed to open stream: No such file or directory
mime_content_type                            false
  [2] finfo::file(): Unable to find the wrapper "phlt-nope" - did you forget to enable it when you configured PHP?
  [2] finfo::file(): Unable to find the wrapper "phlt-nope" - did you forget to enable it when you configured PHP?
  [2] finfo::file(phlt-nope://x): Failed to open stream: No such file or directory
a scheme nothing implements                  false
-- a directory is one word, whatever the flags say
the description face                         'directory'
the type face                                'directory'
both mime faces                              'directory'
mime_content_type                            'directory'
-- the flags argument is this call's only, and 0 means "keep mine"
the object's own                             'ASCII text'
overridden                                   'text/plain'
...and not kept                              'ASCII text'
0 keeps the object's                         'text/plain'
buffer() overrides too                       'us-ascii'
...for one call                              'text/plain'
set_flags is the lasting one                 true
...and it lasts                              'ASCII text, with no line terminators'
-- finfo_open
with nothing                                 'ASCII text, with no line terminators'
with flags                                   'text/plain'
an EMPTY database is the default             'text/plain'
the procedural set_flags                     'text/plain'
-- the streams a name may reach
data://                                      'ASCII text, with no line terminators'
data:// with php in it                       'PHP script, ASCII text, with no line terminators'
php://memory is empty                        'empty'
-- mime_content_type also takes an OPEN stream, from its beginning
the stream                                   'text/plain'
and it is left where it was                  3
a closed one                                 'TypeError: mime_content_type(): supplied resource is not a valid stream resource'
something that is neither                    'TypeError: mime_content_type(): Argument #1 ($filename) must be of type resource|string, array given'
-- a userland wrapper answers both doors
  [2] finfo::file(): PhltFinfoWrapper::stream_cast is not implemented!
a wrapped file                               'GIF image data, version 89a, 16 x 32'
  [2] finfo::file(): PhltFinfoWrapper::stream_cast is not implemented!
its description                              'GIF image data, version 89a, 16 x 32'
  [2] finfo::file(): PhltFinfoWrapper::stream_cast is not implemented!
a wrapped DIRECTORY                          'GIF image data, version 89a, 16 x 32'
