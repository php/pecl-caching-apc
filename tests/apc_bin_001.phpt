--TEST--
APC: bindump user cache
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache(APC_CACHE_USER);
apc_store('testkey','testvalue');
$dump = apc_bin_dump(APC_CACHE_USER, NULL);
apc_clear_cache(APC_CACHE_USER);
var_dump(apc_fetch('testkey'));
apc_bin_load($dump, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32, APC_CACHE_USER);
var_dump(apc_fetch('testkey'));
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
bool(false)
string(9) "testvalue"
===DONE===
