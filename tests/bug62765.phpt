--TEST--
Bug #62765 (apc_bin_dumpfile report Fatal error when there is "goto" in function)
--INI--
apc.enabled=1
apc.enable_cli=1
apc.stat=0
apc.cache_by_default=1
apc.filters=
--FILE--
<?php
apc_compile_file(__FILE__);
apc_bin_dumpfile(array(__FILE__), null, dirname(__FILE__)  . "/bug62765.bin");
function dummy ($a) {
    if ($a) {
        echo 1;
    } else {
        echo 2;
    }
    goto ret;
ret:
    return 2;
}

echo "okey\n";
?>
--CLEAN--
<?php
unlink(dirname(__FILE__)  . "/bug62765.bin");
?>
--EXPECT--
okey
