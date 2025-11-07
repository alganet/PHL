# phpt.php diagnostics tests

These are not part of the PHL test suite, and only exist to diagnose
the `phpt.php` test runner. As so, they are marked as disabled.

You can run those diagnostics using:

```sh
phl -x tests/phpt.php --target-dir tests/ph7/phptrunner --file-extension disabled
```