--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
// Creating a symbolic link needs a privilege on Windows that the test runner does
// not have; the unix VFS is what this pins.
if (PHP_OS === 'WINNT') { echo 'skip symlink() needs SeCreateSymbolicLinkPrivilege on Windows'; }
?>
--TEST--
A symbolic link is visible to the functions that have to report it
--DESCRIPTION--
is_link() and filetype() are php_stat(FS_IS_LINK) and php_stat(FS_TYPE), and both
LSTAT: stat() follows the link and answers about its target, so the S_ISLNK arm can
never be reached through it. PHL's unix VFS called stat() for both, which made
is_link() answer false for every symlink in existence and filetype() answer "file"
for one pointing at a file. readlink() was missing outright — no builtin, and no
entry point on ph7_vfs to build one from — so the link's target was unreachable
even once the link could be recognized. SplFileInfo::isLink()/getType()/
getLinkTarget() are the three methods that sit directly on all of it.
--FILE--
<?php
$symDir = sys_get_temp_dir() . '/phl_symlink_' . getmypid();
@mkdir($symDir);
file_put_contents("$symDir/target.txt", "content\n");
@mkdir("$symDir/dir");
symlink('target.txt', "$symDir/rel.link");
symlink("$symDir/dir", "$symDir/dir.link");

function symShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* A link is a link, whatever it points at. */
symShow('is_link on a link', fn() => is_link("$symDir/rel.link"));
symShow('is_link on its target', fn() => is_link("$symDir/target.txt"));
symShow('is_link on a directory link', fn() => is_link("$symDir/dir.link"));
symShow('is_link on nothing', fn() => is_link("$symDir/absent"));

/* filetype() reports the LINK; the follow-through answers stay the target's. */
symShow('filetype of a link', fn() => filetype("$symDir/rel.link"));
symShow('filetype of a directory link', fn() => filetype("$symDir/dir.link"));
symShow('filetype of the target', fn() => filetype("$symDir/target.txt"));
symShow('filetype of the directory', fn() => filetype("$symDir/dir"));
symShow('a link still IS its target', fn() => [
    is_file("$symDir/rel.link"), is_dir("$symDir/dir.link"), file_exists("$symDir/rel.link"),
]);

/* readlink() answers the link's own text — relative stays relative. */
symShow('readlink', fn() => readlink("$symDir/rel.link"));
symShow('readlink on a directory link', fn() => readlink("$symDir/dir.link") === "$symDir/dir");
symShow('readlink on a plain file', fn() => @readlink("$symDir/target.txt"));
symShow('readlink on nothing', fn() => @readlink("$symDir/absent"));
symShow('readlink declares one string parameter', fn() => array_map(
    fn($p) => $p->getName() . ':' . $p->getType(),
    (new ReflectionFunction('readlink'))->getParameters()));

@unlink("$symDir/rel.link");
@unlink("$symDir/dir.link");
@unlink("$symDir/target.txt");
@rmdir("$symDir/dir");
@rmdir($symDir);
--EXPECT--
is_link on a link => true
is_link on its target => false
is_link on a directory link => true
is_link on nothing => false
filetype of a link => 'link'
filetype of a directory link => 'link'
filetype of the target => 'file'
filetype of the directory => 'dir'
a link still IS its target => array (  0 => true,  1 => true,  2 => true,)
readlink => 'target.txt'
readlink on a directory link => true
readlink on a plain file => false
readlink on nothing => false
readlink declares one string parameter => array (  0 => 'path:string',)
