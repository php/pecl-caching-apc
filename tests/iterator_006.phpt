--TEST--
APC: APCIterator formats 
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

$formats = array( APC_ITER_KEY,
                  APC_ITER_VALUE,
                  APC_ITER_INFO,
                  APC_ITER_ALL,
                  APC_ITER_KEY | APC_ITER_VALUE,
                  APC_ITER_KEY | APC_ITER_INFO,
                  APC_ITER_VALUE | APC_ITER_INFO,
                  APC_ITER_KEY | APC_ITER_VALUE | APC_ITER_INFO
                );

$it_array = array();

foreach ($formats as $idx => $format) {
  $it_array[$idx] = new APCIterator('user', NULL, $format);
}

for($i = 0; $i < 11; $i++) {
  apc_store("key$i", "value$i");
}

foreach ($it_array as $idx => $it) {
  print_it($it, $idx);
}

function print_it($it, $idx) {
  echo "IT #$idx\n";
  echo "============================\n";
  foreach ($it as $key=>$value) {
    var_dump($key);
    var_dump($value);
  }
  echo "============================\n\n";
}

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
IT #0
============================
string(4) "key8"
NULL
string(4) "key2"
NULL
string(4) "key0"
NULL
string(4) "key6"
NULL
string(4) "key4"
NULL
string(4) "key9"
NULL
string(4) "key3"
NULL
string(4) "key1"
NULL
string(4) "key7"
NULL
string(4) "key5"
NULL
string(5) "key10"
NULL
============================

IT #1
============================
int(0)
string(6) "value8"
int(1)
string(6) "value2"
int(2)
string(6) "value0"
int(3)
string(6) "value6"
int(4)
string(6) "value4"
int(5)
string(6) "value9"
int(6)
string(6) "value3"
int(7)
string(6) "value1"
int(8)
string(6) "value7"
int(9)
string(6) "value5"
int(10)
string(7) "value10"
============================

IT #2
============================
int(0)
array(10) {
  ["info"]=>
  string(4) "key8"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(1)
array(10) {
  ["info"]=>
  string(4) "key2"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(2)
array(10) {
  ["info"]=>
  string(4) "key0"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(3)
array(10) {
  ["info"]=>
  string(4) "key6"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(4)
array(10) {
  ["info"]=>
  string(4) "key4"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(5)
array(10) {
  ["info"]=>
  string(4) "key9"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(6)
array(10) {
  ["info"]=>
  string(4) "key3"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(7)
array(10) {
  ["info"]=>
  string(4) "key1"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(8)
array(10) {
  ["info"]=>
  string(4) "key7"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(9)
array(10) {
  ["info"]=>
  string(4) "key5"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
int(10)
array(10) {
  ["info"]=>
  string(5) "key10"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
============================

IT #3
============================
string(4) "key8"
array(11) {
  ["info"]=>
  string(4) "key8"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value8"
}
string(4) "key2"
array(11) {
  ["info"]=>
  string(4) "key2"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value2"
}
string(4) "key0"
array(11) {
  ["info"]=>
  string(4) "key0"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value0"
}
string(4) "key6"
array(11) {
  ["info"]=>
  string(4) "key6"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value6"
}
string(4) "key4"
array(11) {
  ["info"]=>
  string(4) "key4"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value4"
}
string(4) "key9"
array(11) {
  ["info"]=>
  string(4) "key9"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value9"
}
string(4) "key3"
array(11) {
  ["info"]=>
  string(4) "key3"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value3"
}
string(4) "key1"
array(11) {
  ["info"]=>
  string(4) "key1"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value1"
}
string(4) "key7"
array(11) {
  ["info"]=>
  string(4) "key7"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value7"
}
string(4) "key5"
array(11) {
  ["info"]=>
  string(4) "key5"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value5"
}
string(5) "key10"
array(11) {
  ["info"]=>
  string(5) "key10"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(7) "value10"
}
============================

IT #4
============================
string(4) "key8"
string(6) "value8"
string(4) "key2"
string(6) "value2"
string(4) "key0"
string(6) "value0"
string(4) "key6"
string(6) "value6"
string(4) "key4"
string(6) "value4"
string(4) "key9"
string(6) "value9"
string(4) "key3"
string(6) "value3"
string(4) "key1"
string(6) "value1"
string(4) "key7"
string(6) "value7"
string(4) "key5"
string(6) "value5"
string(5) "key10"
string(7) "value10"
============================

IT #5
============================
string(4) "key8"
array(10) {
  ["info"]=>
  string(4) "key8"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key2"
array(10) {
  ["info"]=>
  string(4) "key2"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key0"
array(10) {
  ["info"]=>
  string(4) "key0"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key6"
array(10) {
  ["info"]=>
  string(4) "key6"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key4"
array(10) {
  ["info"]=>
  string(4) "key4"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key9"
array(10) {
  ["info"]=>
  string(4) "key9"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key3"
array(10) {
  ["info"]=>
  string(4) "key3"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key1"
array(10) {
  ["info"]=>
  string(4) "key1"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key7"
array(10) {
  ["info"]=>
  string(4) "key7"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key5"
array(10) {
  ["info"]=>
  string(4) "key5"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(5) "key10"
array(10) {
  ["info"]=>
  string(5) "key10"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
============================

IT #6
============================
int(0)
array(11) {
  ["info"]=>
  string(4) "key8"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value8"
}
int(1)
array(11) {
  ["info"]=>
  string(4) "key2"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value2"
}
int(2)
array(11) {
  ["info"]=>
  string(4) "key0"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value0"
}
int(3)
array(11) {
  ["info"]=>
  string(4) "key6"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value6"
}
int(4)
array(11) {
  ["info"]=>
  string(4) "key4"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value4"
}
int(5)
array(11) {
  ["info"]=>
  string(4) "key9"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value9"
}
int(6)
array(11) {
  ["info"]=>
  string(4) "key3"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value3"
}
int(7)
array(11) {
  ["info"]=>
  string(4) "key1"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value1"
}
int(8)
array(11) {
  ["info"]=>
  string(4) "key7"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value7"
}
int(9)
array(11) {
  ["info"]=>
  string(4) "key5"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value5"
}
int(10)
array(11) {
  ["info"]=>
  string(5) "key10"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(7) "value10"
}
============================

IT #7
============================
string(4) "key8"
array(11) {
  ["info"]=>
  string(4) "key8"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value8"
}
string(4) "key2"
array(11) {
  ["info"]=>
  string(4) "key2"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value2"
}
string(4) "key0"
array(11) {
  ["info"]=>
  string(4) "key0"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value0"
}
string(4) "key6"
array(11) {
  ["info"]=>
  string(4) "key6"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value6"
}
string(4) "key4"
array(11) {
  ["info"]=>
  string(4) "key4"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value4"
}
string(4) "key9"
array(11) {
  ["info"]=>
  string(4) "key9"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value9"
}
string(4) "key3"
array(11) {
  ["info"]=>
  string(4) "key3"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value3"
}
string(4) "key1"
array(11) {
  ["info"]=>
  string(4) "key1"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value1"
}
string(4) "key7"
array(11) {
  ["info"]=>
  string(4) "key7"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value7"
}
string(4) "key5"
array(11) {
  ["info"]=>
  string(4) "key5"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(6) "value5"
}
string(5) "key10"
array(11) {
  ["info"]=>
  string(5) "key10"
  ["ttl"]=>
  int(0)
  ["type"]=>
  string(4) "user"
  ["num_hits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["value"]=>
  string(7) "value10"
}
============================

===DONE===
