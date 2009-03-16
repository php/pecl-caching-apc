<?php
/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2008 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Ralf Becker <beckerr@php.net>                               |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  |          Ilia Alshanetsky <ilia@prohost.org>                         |
  |          Brian Shire <shire@tekrat.com>                              |
  +----------------------------------------------------------------------+

   All other licensing and usage conditions are those of the PHP Group.

 */
$VERSION='$Id$';

/* Use (internal) authentication - best choice if no other 
*    authentication is available
*  - If set to 0: 
*      There will be no further authentication. You
*      will have to handle this by yourself!
*  - If set to 1:
*      You need to change ADMIN_PASSWORD to make
*      this work!
*/
define('USE_AUTHENTICATION', true);
define('ADMIN_USERNAME',     'apc');         // Admin Username
define('ADMIN_PASSWORD',     'password');    // Admin Password - CHANGE THIS TO ENABLE!!!

/**** authentication ****/ 
if (USE_AUTHENTICATION) {
  if (ADMIN_PASSWORD == 'password') {
    echo <<<EOB
      <html><body>
      <h1>Authentication not configured.</h1>
      <big>Please configure proper authentication to view this page.</big><br/>&nbsp;<br/>&nbsp;
      </body></html>
EOB;
    exit;
  }

  if( $_SERVER['PHP_AUTH_USER'] != ADMIN_USERNAME ||
      $_SERVER['PHP_AUTH_PW'] != ADMIN_PASSWORD) {
    Header("WWW-Authenticate: Basic realm=\"APC Login\"");
    Header("HTTP/1.0 401 Unauthorized");

    echo <<<EOB
      <html><body>
      <h1>Rejected!</h1>
      <big>Wrong Username or Password!</big><br/>&nbsp;<br/>&nbsp;
      </body></html>
EOB;
    exit;
  }
}

if (!function_exists('apc_cache_info')) {
	echo "No cache info available.  APC does not appear to be running.";
  exit;
}

if (file_exists("apc.conf.php")) include(dirname(__FILE__)."/apc.conf.php");

//defaults('DATE_FORMAT', "d.m.Y H:i:s"); // German
define('DATE_FORMAT', 'Y/m/d H:i:s');   // US

//define('PROXY', 'tcp://127.0.0.1:8080');

/* Setup some actions */
$actions = array( 
                  'Caches' => 'CACHES',
                  'Browse' => 'BROWSE',
                  'Segments' => 'SEGMENTS',
                  'Breakdown' => 'BREAKDOWN',
                  'Exports' => 'EXPORTS',
                  'Version Check' => 'VERSION',
                );
$i=0;
foreach ($actions as $key=>$value) {
  define($value, $i);
  $i++;
}

$a_values = array_values($actions);
$a_keys = array_keys($actions);
$action_value = isset($_GET['a']) ? $_GET['a'] : 0;
$action_title = $a_keys[$action_value];
$action_func = 'action_' . $a_values[$action_value];
if (!isset($a_values[$action_value])) {
   $action_func = 'action_ERROR';
   $error = "An invalid action was specified.";
}

$action_func($GLOBALS);

function display_header($g) { 
  global $apc_start;
  $apc_start = microtime(true);
  ?>
<html>
  <head>
    <? display_css(); ?>
  </head>
  <? display_js(); ?>
  <body>
  <div id="blanket" class="blanket" onclick="popup_hide()"></div>
  <div id="popup" class="popup">
    <div style="padding: 2px;">
      <div style="width: 100%; height: 100%; overflow: auto; background: #ffffff;">
       <div style="padding: 10px;" id="popup_content">
       </div>
      </div>
    </div>
  </div>
  <div>
    <div class="head">
      <div class="apc"><a href="http://pecl.php.net/package/APC">APC <?=phpversion('apc');?></a></div>
      <div class="nameinfo">Alternative PHP Cache: <b><?=$g['action_title']?></b></div>
      <div class="menu">
        <? foreach($g['actions'] as $key=>$value) { ?>
          <a href="?a=<?=constant($value)?>"><?=$key?></a>
        <? } ?>
      </div>
    </div>
    <div class="headbar"/>
  </div>
  <div id="content" class="content">
<? }


function display_css() { ?>
  <style>
    div.blanket {
      display: none;
      background-color: #000000;
      opacity: 0.65;
      position: absolute;
      z-index: 9001;
      top: 0px;
      left: 0px;
      width: 100%;
      height: 100%;
    }
    div.popup {
      display: none;
      position: absolute;
      background-color: #000000;
      left: 25%;
      top: 10%;
      width: 700px;
      height: 450px;
      z-index: 9002;
    }
    div.head {
      background: #000000;
      margin: 0;
      padding: 0.1em 1em 0.5em 1em;
      height: 38px;
    }
    div.apc a {
      color: #5050ee;
      font-size: 1.7em;
    }
    div.nameinfo {
      color: white;
      display: inline;
      font-size: 0.9em;
    }
    div.menu a {
      color: white;
      position: absolute;
      font-size: 1.0em;
      position: relative;
      top: -15px;
      left: 400px;
      width: 40%;
      padding-left: 15px;
      padding-right: 15px;
    }
    div.headbar {
      background: rgb(102,102,102);
      border-style: none;
      height: 3px;
      margin: 0;
      margin-top: 0px;
      padding: 0;
    }

    div.content {
      margin-left: 20px;
      margin-top: 10px;
      margin-right: 20px;
      margin-bottom: 10px;
    }

    div.table {
      background: rgb(102,102,153);
      border-style: solid;
      border: #000000 1px;
    }

    body {
      background:white;
      margin:0;
      padding:0;
    }

    body,p,td,th,input,submit {
      font-size:0.8em;
      font-family:arial,helvetica,sans-serif;
    }

    a {
      color:black;
      font-weight:none;
      text-decoration:none;
    }

    a:hover {
      text-decoration:underline;
    }

    div.content { i
      padding:1em 1em 1em 1em;
      position:absolute;
      width:97%;
      z-index:100;
    }

    div.sprk_graph {
      display: none;
    }
    #spk_point {
      position: absolute;
      width: 1px;
      height: 1px;
      background: #878B86;
      overflow; hidden;
    }

    th {
      text-align: left;
    }

  </style>
<?  }

function info_stats($cache_arg=NULL) {
  if (!$cache_arg) {
    $caches_info = apc_cache_info(0, true);
    $sma_info = apc_sma_info(true);
  } else {
    $caches_info = array($cache_arg);
    $sma_info = apc_sma_info(true);
    $sma_info = array('segments' => array($sma_info['segments'][$cache_arg['segment_idx']]));
  }

  $r = array();
  foreach($caches_info as $cache) {
    foreach($cache['hit_stats'] as $key=>$value) {
      $r['hit_graph'][$key] += $value;
      $r['request_graph'][$key] += $value;
      $r['hits_win'] += $value;
    }
    foreach($cache['miss_stats'] as $key=>$value) {
      $r['miss_graph'][$key] += $value;
      $r['request_graph'][$key] += $value;
      $r['misses_win'] += $value;
    }
    foreach($cache['insert_stats'] as $key=>$value) {
      $r['insert_graph'][$key] += $value;
      $r['request_graph'][$key] += $value;
      $r['inserts_win'] += $value;
    }
    $r['hits'] += $cache['num_hits'];
    $r['misses'] += $cache['num_misses'];
    $r['inserts'] += $cache['num_inserts'];
    $r['mem_used'] += $cache['mem_size'];
  }
  if ($sma_info) {
    foreach($sma_info['segments'] as $seg) {
      $r['mem_avail'] += $seg['avail'];
    }
  }
  $r['mem_total'] += $r['mem_used'] + $r['mem_avail'];

  $r['requests'] = $r['hits'] + $r['misses'] + $r['inserts'];
  $r['requests_win'] = $r['hits_win'] + $r['misses_win'] + $r['inserts_win'];

  $r['hitmiss_ratio'] = $r['hits'] ? (int)(($r['hits'] / ($r['hits'] + $r['misses'])) * 100) : 0;
  $r['hitmiss_ratio_win'] = $r['hits_win'] ? (int)(($r['hits_win'] / ($r['hits_win'] + $r['misses_win'])) * 100) : 0;
  foreach($r['hit_graph'] as $key=>$value) {
    $r['hitmiss_ratio_graph'][$key] = $r['hit_graph'][$key] || $r['miss_graph'][$key] ? (int)(($r['hit_graph'][$key] / ($r['hit_graph'][$key] + $r['miss_graph'][$key])) * 100) : 0;
  }
  $r['hits_percent'] = $r['hits'] ? (int)(($r['hits'] / $r['requests']) * 100) : 0;
  $r['hits_percent_win'] = $r['hits_win'] ? (int)(($r['hits'] / $r['requests_win']) * 100) : 0;
  $r['misses_percent'] = $r['misses'] ? (int)(($r['misses'] / $r['requests']) * 100) : 0;
  $r['misses_percent_win'] = $r['misses_win'] ? (int)(($r['misses_win'] / $r['requests_win']) * 100) : 0;
  $r['inserts_percent'] = $r['inserts'] ? (int)(($r['inserts'] / $r['requests']) * 100) : 0;
  $r['inserts_percent_win'] = $r['inserts_win'] ? (int)(($r['inserts_win'] / $r['requests_win']) * 100) : 0;
  if ($sma_info) {
    $r['mem_used_percent'] = (int)(($r['mem_used'] / $r['mem_total']) * 100);
    $r['mem_avail_percent'] = (int)(($r['mem_avail'] / $r['mem_total']) * 100);
  }

  $r['stats'] = array();
  $r['stats']['requests'] = array( 'Requests',  $r['requests'], array(100, "#999999", 0), $r['request_graph'] );
  $r['stats']['hits'] = array( 'Hits',   $r['hits'], array($r['hits_percent'], "#00cc00", 0), $r['hit_graph'] );
  $r['stats']['misses'] = array( 'Misses', $r['misses'], array($r['misses_percent'], "#cc0000", $r['hits_percent']), $r['miss_graph'] );
  $r['stats']['inserts'] = array( 'Inserts', $r['inserts'], array($r['inserts_percent'], "#0000cc", $r['hits_percent'] + $r['misses_percent']), $r['insert_graph'] );
  $r['stats']['hitmiss_ratio'] = array( 'Hit Ratio', $r['hitmiss_ratio'], array($r['hitmiss_ratio'], "#00cc00", 0), $r['hitmiss_ratio_graph'] );

  $r['stats_win'] = array();
  $r['stats_win']['requests'] = array( 'Requests',  $r['requests_win'], array(100, "#999999", 0), $r['request_graph'] );
  $r['stats_win']['hits'] = array( 'Hits',   $r['hits_win'], array($r['hits_percent'], "#00cc00", 0), $r['hit_graph'] );
  $r['stats_win']['misses'] = array( 'Misses', $r['misses_win'], array($r['misses_percent_win'], "#cc0000", $r['hits_percent_win']), $r['miss_graph'] );
  $r['stats_win']['inserts'] = array( 'Inserts', $r['inserts_win'], array($r['inserts_percent_win'], "#0000cc", $r['hits_percent_win'] + $r['misses_percent_win']), $r['insert_graph'] );
  $r['stats_win']['hitmiss_ratio'] = array( 'Hit Ratio', $r['hitmiss_ratio_win'], array($r['hitmiss_ratio_win'], "#00cc00", 0), $r['hitmiss_ratio_graph'] );

  if ($sma_info) {
    $r['stats']['mem_total'] = array( 'Mem Total', bsize($r['mem_total']), array(100,"#999999",0) );
    $r['stats']['mem_used'] = array( 'Mem Used', bsize($r['mem_used']), array($r['mem_used_percent'],"#cc0000",0) );
    $r['stats']['mem_avail'] = array( 'Mem Avail', bsize($r['mem_avail']), array($r['mem_avail_percent'],"#00cc00",$r['mem_used_percent']) );
  }

  return $r;
}

function action_browse() {
  global $action_value, $sort, $rsort;
  $sort = $_GET['sort'];
  $rsort = isset($_GET['rsort']) ? !$_GET['rsort'] : 0;
  $limit = isset($_GET['limit']) ? $_GET['limit'] : 25;
  $search = $_GET['search'] == '' ? NULL : $_GET['search'];
  $cache_id = intval($_GET['cache'] == '' ? APC_CACHE_FILE | 1 : $_GET['cache']);
  $type = $cache_id & APC_CACHE_FILE ? APC_CACHE_FILE : APC_CACHE_USER;
  $list = isset($_GET['list']) ? $_GET['list'] : APC_LIST_ACTIVE;
  if (isset($_GET['clear'])) {
      apc_clear_cache($cache_id);
  }
  if (isset($_GET['key'])) {
    if ($type == APC_CACHE_USER) {
      if ($_GET['info_action'] == 'remove') {
        apc_delete($_GET['key']);
      } else if ($_GET['info_action'] == 'update') {
        apc_store($_GET['key'], eval('return '.$_POST['value'].';'));
      }
      $val = apc_fetch($_GET['key'], $success, $cache_id);
      if (!$success) {
        echo '[Value no longer in cache]';
      } else {
        $url = preg_replace('/info_action=(view|edit|remove)/', '', $_SERVER['QUERY_STRING']); 
        $id = intval($_GET['id']);
        $key = $_GET['key'];
        if ($_GET['info_action'] == 'edit') {
          echo '<form name="info'.$id.'_edit" action="?'.$url.'&info_action=update" method="POST">';
          echo '<textarea name="value" style="height: 300px; width: 100%;">';
          var_export($val);
          echo '</textarea>';
          echo '</form>';
          echo "<div style=\"font-size: 0.8em; margin-top: 10px; margin-bottom: 15px;\">";
          echo "<a onclick=\"info_show('?".$url."&info_action=view', 'info".$id."', 0, '');\">Cancel</a>";
          echo "<span style=\"margin: 20px;\"></span>";
          echo "<a onclick=\"info_show('?".$url."&info_action=update', 'info".$id."', 0, 'value='+document.info".$id."_edit.elements['value'].value);\">Update</a>";
          echo "</div>";
        } else {
          ob_start();
          var_dump($val);
          $out = ob_get_contents();
          ob_end_clean();
          echo '<span style="width: 100%;">';
          if (is_array($val) && count($val) > 20) {
            echo '<div style="overflow: auto; height: 300px;">';
          } else {
            echo '<div style="overflow: auto;">';
          }
          echo '<pre>';
          echo preg_replace('/=>\n /', ' =>', $out);
          echo '</pre>';
          echo '</div>';
          echo '</div>';
          echo "<div style=\"font-size: 0.8em; margin-top: 10px; margin-bottom: 15px;\">";
          echo "<a onclick=\"info_show('?".$url."&info_action=edit', 'info".$id."', 0, '');\">Edit Value</a>";
          echo "<span style=\"margin: 20px;\"></span>";
          echo "<a onclick=\"info_show('?".$url."&info_action=remove', 'info".$id."', 0, '');\">Remove Entry</a>";
          echo "</div>";
        }
      }
    } else {
      echo "<i>No extra information available at this time.</i>";
    }
    return;
  }
  display_header($GLOBALS);
  if (ini_get('magic_quotes_gpc')) {
    $search = stripslashes($search);
  }
  $search = str_replace('/', '\/', $search);
  switch ($sort) {
    case 'key':
          $iter_flags = APC_ITER_KEY;
          break;
    case 'num_hits':
          $iter_flags = APC_ITER_NUM_HITS;
          break;
    case 'mem_size':
          $iter_flags = APC_ITER_MEM_SIZE;
          break;
    case 'ref_count':
          $iter_flags = APC_ITER_REFCOUNT;
          break;
    case 'ttl':
          $iter_flags = APC_ITER_TTL;
          break;
    case 'device':
          $iter_flags = APC_ITER_DEVICE;
          break;
    case 'inode':
          $iter_flags = APC_ITER_INODE;
          break;
    case 'mtime':
          $iter_flags = APC_ITER_MTIME;
          break;
    case 'ctime':
          $iter_flags = APC_ITER_CTIME;
          break;
    case 'dtime':
          $iter_flags = APC_ITER_DTIME;
          break;
    case 'atime':
          $iter_flags = APC_ITER_ATIME;
          break;
    default:
          $iter_flags = APC_ITER_NONE;
  }
  $iterator = new APCIterator($cache_id, '/'.str_replace('/', '\/', $search).'/', $iter_flags, $limit, $list);

  $count = 1;
  $entries = array();
  foreach($iterator as $key=>$info) {
    $entries[$key] = $info[$sort];
    if ($sort && ($count % $limit == 0)) {
        if ($rsort) { 
          arsort($entries);
        } else {
          asort($entries);
        }
        $entries = array_splice($entries, 0, $limit);
    }
    if (!$sort && count($entries) == $limit) break;
    $count++;
  }

  if ($sort) {
    if ($rsort) { 
      arsort($entries);
    } else {
      asort($entries);
    }
    $entries = array_splice($entries, 0, $limit);
  }

  $iterator_full = new APCIterator($cache_id, array_keys($entries), APC_ITER_ALL, $limit, $list);
  $entries_full = array();
  foreach($iterator_full as $key => $value) {
    $entries_full[$key] = $value;
  }
  $entries = array_merge($entries, $entries_full);
  
  $info = apc_cache_info($cache_id, true);
  $totals['search']['count'] = $iterator->getTotalCount();
  $totals['search']['num_hits'] = $iterator->getTotalHits();
  $totals['search']['mem_size'] = $iterator->getTotalSize();
  $totals['search']['mem_size_p'] = $info['mem_size'] == 0 ? 0 : (int)(($totals['search']['mem_size'] / $info['mem_size']) * 100);
  $totals['page']['count'] = count($entries);
  if ($search) { 
    $totaliterator = new APCIterator($cache_id, NULL, APC_ITER_NONE, 0, $list);
    $totals['cache']['count'] = $totaliterator->getTotalCount();
    $totals['cache']['num_hits'] = $totaliterator->getTotalHits();
    $totals['cache']['mem_size'] = $totaliterator->getTotalSize();
  } else {
    $totals['cache'] = $totals['search'];
  }
  foreach ($entries as $entry) {
    $totals['page']['num_hits'] += $entry['num_hits'];
    $totals['page']['mem_size'] += $entry['mem_size'];
  }
  $url = '?a='.$action_value.'&rsort='.$rsort.'&cache='.$cache_id.'&search='.$search.'&limit='.$limit;
  display_browse($cache_id, $info, $entries, $url, $totals);
  display_footer();
}

function display_browse($cache_id, $info, $entries, $url, $totals) { ?>
  <? global $action_value; ?>
  <div style="position: relative; top: -10px; background: #eaeaff; height: 30px; padding-top: 10px;">
    <form method="GET" action="?">
    <input type='hidden' name='a' value='<?=$action_value?>'/>
    <span style="padding: 20px;">
     Cache: &nbsp;
     <select style="font-size: 0.8em;" name="cache">
      <? foreach (apc_cache_info(0, true) as $key=>$cache) {?>
        <option value='<?=$cache['id']?>' <?= $cache['id'] == $cache_id ? 'selected' : '';?>><?=$cache['name']?> - <?=$cache['const_name']?></option>
      <? } ?>
     </select>
    </span>
    <span style="padding: 20px;">
     List: &nbsp;
     <select name="list" style="font-size: 0.8em;">
      <option value="<?=APC_LIST_ACTIVE?>" <?= $_GET['list'] == APC_LIST_ACTIVE ? 'selected' : ''?>>Active</option>
      <option value="<?=APC_LIST_DELETED?>" <?= $_GET['list'] == APC_LIST_DELETED ? 'selected' : ''?>>Deleted</option>
      <? if ($info['expunge_method'] == 'LFU') { echo '<option value="'.APC_LIST_LFU.'"'.($_GET['list'] == APC_LIST_LFU ? 'selected' : '').'>LFU</option>'; } ?>
      <? if ($info['expunge_method'] == 'LFU') { echo '<option value="'.APC_LIST_MFU.'"'.($_GET['list'] == APC_LIST_MFU ? 'selected' : '').'>MFU</option>'; } ?>
     </select>
    </span>
    <span style="padding: 20px;">
      Limit: &nbsp; <input type='text' value='<?= isset($_GET['limit']) ? $_GET['limit'] : 25; ?>' name='limit' size="10"></input>
    </span>
    <span style="padding: 20px;">
      Search: &nbsp;
      <input type='text' value='<? if (ini_get('magic_quotes_gpc')) { echo stripslashes($_GET['search']); } else { echo $_GET['search']; } ?>' name='search' size="40"></input>
    </span>
    <span style="padding: 20px;">
      <input type="submit" value="Update"/>
    </span>
    <span style="padding: 20px;">
      <input type="button" value="Clear Cache" onclick="clear_cache('?<?=$_SERVER['QUERY_STRING'].'&clear=1'?>');"/>
    </span>
    </form>
  </div>
  <center>
  <table width="95%" cellSpacing="0">
    <tr align="left">
      <th> </th>
      <th> <a href='<?=$url?>&sort=key&'>Key</a> </th>
      <th> <a href='<?=$url?>&sort=num_hits'>Hits</a> </th>
      <th> <a href='<?=$url?>&sort=mem_size'>Size</a> </th>
      <th> <a href='<?=$url?>&sort=ref_count'>Refs</a> </th>
      <th> <a href='<?=$url?>&sort=ttl'>TTL</a> </th>
      <? if ($entries[0]['device']) { ?>
        <th> <a href='<?=$url?>&sort=device'>Device</a> </th>
        <th> <a href='<?=$url?>&sort=inode'>Inode</a> </th>
      <? } ?>
      <th> <a href='<?=$url?>&sort=mtime'>Modified</a> </th>
      <th> <a href='<?=$url?>&sort=creation_time'>Created</a> </th>
      <th> <a href='<?=$url?>&sort=deletion_time'>Deleted</a> </th>
      <th> <a href='<?=$url?>&sort=access_time'>Accessed</a> </th>
    </tr>
    <tr><th height="10px"></th></tr>
    <tr style="background: #eaeaee;"> <td colspan="2">Total <b><?=$totals['cache']['count']?></b> entries:</td> <td><?=$totals['cache']['num_hits']?> </td>  <td><?=bsize($totals['cache']['mem_size'])?></td>
<td rowspan="3" colspan="5" bgcolor="#ffffff">
<div style="float: left; width: 100px;">
<div style="float: left; width: 10px; margin-left: 5px; margin-right: 5px; height: 10px; background-color: #cccccc;"></div> <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Total</div>
<div style="float: left; width: 10px; margin-left: 5px; margin-right: 5px;  height: 10px; background-color: #ff9999;"></div> <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Found</div>
<div style="float: left; width: 10px; margin-left: 5px; margin-right: 5px;  height: 10px; background-color: #ffff99;"></div> <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Showing</div>
</div>
<div style="float: left;">
          <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Count</div>
          <?=display_pie('pie_browse_count', array(($totals['cache']['count'] - $totals['search']['count']), ($totals['search']['count'] - $totals['page']['count']), $totals['page']['count']), array('"#cccccc"', '"#ff9999"','"#ffff99"'), 60);?>
</div>
<div style="float: left;">
          <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Hits</div>
          <?=display_pie('pie_browse_num_hits', array(($totals['cache']['num_hits'] - $totals['search']['num_hits']), ($totals['search']['num_hits'] - $totals['page']['num_hits']), $totals['page']['num_hits']), array('"#cccccc"', '"#ff9999"','"#ffff99"'), 60);?>
</div>
          <div style="font-size: 0.8em; color: #888888; vertical-align: top;">Size</div>
          <?=display_pie('pie_browse_mem_size', array(($totals['cache']['mem_size'] - $totals['search']['mem_size']), ($totals['search']['mem_size'] - $totals['page']['mem_size']), $totals['page']['mem_size']), array('"#cccccc"', '"#ff9999"','"#ffff99"'), 60);?>
</td>
</tr>
    <tr style="background: #ffeeee;"> <td colspan="2">Found <b><?=$totals['search']['count']?></b> entries:</td> <td><?=$totals['search']['num_hits']?> </td>  <td><?=bsize($totals['search']['mem_size'])?> (<?=$totals['search']['mem_size_p']?>%)</td> </tr>
    <tr style="background: #ffffee;"> <td colspan="2">Showing <b><?=$totals['page']['count']?></b> entries:</td> <td><?=$totals['page']['num_hits']?> </td>  <td><?=bsize($totals['page']['mem_size'])?></td> </tr>
    <tr><th height="10px"></th></tr>
    <? $count = 0; ?>
    <? foreach($entries as $info) { ?>
      <? $color = $count % 2 == 0 ? '#efefff' : '#dedeff'; ?>
      <tr bgcolor="<?=$color?>" OnMouseOver="this.bgColor='#ffffff'" OnMouseOut="this.bgColor='<?=$color?>'" onclick="info_show('<?=$url?>&info_action=view&key=<?=$info['key']?>&id=<?=$count?>', 'info<?=$count?>', 1, '');">
        <td><font color="#aaaaaa"><?= $count ?></font>&nbsp;&nbsp;</td>
        <td><?= $info['filename'] ? $info['filename'] : $info['key']; ?></td>
        <td><?= $info['num_hits']; ?></td>
        <td><?= bsize($info['mem_size']); ?></td>
        <td><?= $info['ref_count']; ?></td>
        <? if (isset($info['ttl'])) { ?>
          <td><?= $info['ttl']; ?></td>
        <? } ?>
        <? if ($info['device']) { ?>
          <td><?= $info['device']; ?></td>
          <td><?= $info['inode']; ?></td>
        <? } ?>
        <td><?= apc_date($info['mtime']); ?></td>
        <td><?= apc_date($info['creation_time']); ?></td>
        <td><?= apc_date($info['deletion_time']); ?></td>
        <td><?= apc_date($info['access_time']); ?></td>
      <tr>
      <tr style="display: none;">
        <td colspan="12" style="background-color: <?=$color?>;">
          <div style="font-size: 1.1em;" id="info<?=$count?>"></div>
        </td>
      </tr>
      <? $count++; ?>
    <? } ?>
  </table>
  </center>
<? }

function action_segments() {
  $info = apc_sma_info();
  if (!$_GET['ajax']) display_header($GLOBALS);
  $count = 0;
  foreach ($info['segments'] as $segment) {
    $block_size = bsize($segment['size'] / 256);
    $used = abs($segment['avail'] - $segment['size']);
    $used_p = ($used / $segment['size']) * 100;
    $free_p = ($segment['avail'] / $segment['size']) * 100;
    ?>
      <br/><br/>
      <b>Segment</b> <?= $count ?>
      <table cellpadding="0" cellspacing="0">
      <tr>
      <td>
      <table>
      <tr><td> size: </td> <td> <?= bsize($segment['size']) ?> </td> <td><? display_barline(100, "#999999", 0) ?></td> </tr>
      <tr><td> used: </td> <td> <?= bsize(abs($segment['avail'] - $segment['size'])) ?> </td> <td><? display_barline($used_p, "#cc0000", 0); ?></td> </tr>
      <tr><td> free: </td> <td> <?= bsize($segment['avail']) ?> </td> <td><? display_barline($free_p, "#00cc00", $used_p); ?></td></tr>
      <tr><td> Fragments: </td> <td> <?= count($segment['block_list']) ?> </td> </tr>
      <tr><td> unmap: </td> <td> <?= $segment['unmap'] == true ? 'Yes' : 'No'; ?> </td></tr>
      </table>
      </td>
      <td width="50"></td>
      <td width="200"> <? display_segmentmap($segment['allocmap'], $block_size, 'Average Alloc Size',255,0,0); ?> </td>
      <td width="200"> <? display_segmentmap($segment['freemap'], $block_size, 'Average Free Size',0,255,0); ?> </td>
      <td width="200"> <? display_segmentmap($segment['fragmap'], $block_size, 'Fragmentation Count', 0,0,255); ?> </td>
      </tr>
      </table>
      <?
      $count++;
  }
  if (!$_GET['ajax']) display_footer();
}

function display_breakdown_table($cache, $info, $breakdown_maps) {
  $count_t = $info['num_entries'];
  $size_t = $info['mem_size'];
  $hits_t = $info['num_hits'];
  $groups = $breakdown_maps[$cache];
  echo '<div style="margin-top: 25px; font-weight: bold;">'.$info['name'].' ('.$info['const_name'].')</div>';
  echo '<div style="margin-left: 25px; margin-top: 5px;"><table width="95%" cellSpacing="0">';
  echo '<tr><th>Title</th><th>Regex</th><th>Count</th><th>Count %</th><th>Size</th><th>Size %</th><th>Hits</th><th>Hits %</th></tr>';
  $count_r = 0;
  foreach ($groups as $group) {
    list($title, $regex) = $group;
    $it = new APCIterator($cache, $regex, APC_ITER_ALL, 100, APC_LIST_ACTIVE);
    $count = $it->getTotalCount();
    $size = $it->getTotalSize();
    $hits = $it->getTotalHits();
    if ($count_t) 
      $count_p = (int)(($count/$count_t) * 100);
    if ($size_t)
      $size_p = (int)(($size/$size_t) * 100);
    if ($hits_t)
      $hits_p = (int)(($hits/$hits_t) * 100);

    $count_r++;
    if ($count_r % 2 == 0) {
      ?> <tr bgcolor="#efefff" OnMouseOver="this.bgColor='#ffffff'" OnMouseOut="this.bgColor='#efefff'" onclick="popup('<?=$url?>&key=<?=$info['info']?>');"> <?
    } else {
      ?> <tr bgcolor="#dedeff" OnMouseOver="this.bgColor='#ffffff'" OnMouseOut="this.bgColor='#dedeff'" onclick="popup('<?=$url?>&key=<?=$info['info']?>');"> <?
    }
    echo '<td>'.$title.'</td>';
    echo '<td>'.$regex.'</td>';
    echo '<td>'.$count.'</td>';
    echo '<td>'.$count_p.'%</td>';
    echo '<td>'.bsize($size).'</td>';
    echo '<td>'.$size_p.'%</td>';
    echo '<td>'.$hits.'</td>';
    echo '<td>'.$hits_p.'%</td>';
    echo '<tr>';
  }
  echo '</table></div>';
}

function action_breakdown() {
  global $breakdown_maps;
  display_header($GLOBALS);
  if (!$breakdown_maps) {
    ?>
      No $breakdown_maps array has been created in apc.conf.php.  An example $breakdown_maps for files would be:
      <pre>
      /* Breakdown maps */
      $breakdown_maps = array();
      $breakdown_maps[APC_FILE] = array(
        array('Library', '/\/lib\//'),
        array('HTML', '/\/html\//'),
        array('PHP', '/.php$/')
      );

    </pre>
      <?
  } else {
    $info = apc_cache_info(0, true);
    foreach($info as $cache=>$info) {
      display_breakdown_table($cache, $info, $breakdown_maps);
    }
  }
  display_footer();
}

function action_caches() {
  global $actions;
  display_header($GLOBALS);
  $info = apc_cache_info(NULL, true);
  foreach ($info as $cache) {
    ?>
      <div style="height: 20px;"></div>
      <div style="font-weight: bold; width: 100%; border-bottom: 1px #eeeeee solid;"><a href="?a=<?=BROWSE?>&cache=<?= $cache['id']?>">&nbsp;&nbsp;<?=$cache['name'];?> - <?=$cache['const_name']?></a></div>
      <table>
      <tr>
      <td valign="top" width="250">
      <table cellpadding="0" cellspacing="0">
      <tr height="15px"></tr>
      <tr>  <td width="10px"></td>   <td width="150">Segment:</td>     <td align="right"><?= $cache['segment_idx']?></td>                 </tr>
      <tr height="15px"></tr>
      <tr> <td width="10px"></td> <td>entries:</td>               <td align="right"><?= $cache['num_entries'] ?></td>     </tr>
      <tr>  <td width="10px"></td>   <td>expunges:</td>                <td align="right"><?= $cache['expunges'] ?></td>                   </tr>
      <tr height="15px"></tr>
      <? if ($cache['size_hint'] < $cache['num_entries']) {
        $style="color: #ff0000; font-weight: bold;"; 
      } else {
        $style=""; 
      }
    ?>
      <tr>  <td width="10px"></td>   <td style="<?=$style?>">entries hint:</td> 
      <td align="right" style="<?=$style?>"><?= $cache['size_hint'] ?> </td> 
      </tr>
      <tr> <td width="10px"></td> <td>expunge_method:</td>        <td align="right"><?= $cache['expunge_method']?></td>   </tr>
      <tr> <td width="10px"></td> <td>gc_ttl:</td>                <td align="right"><?= $cache['gc_ttl'] ?></td>          </tr>
      <tr> <td width="10px"></td> <td>ttl:</td>                   <td align="right"><?= $cache['ttl'] ?></td>             </tr>
      <? if (($cache['id'] & APC_CACHE_USER) == APC_CACHE_USER) { ?>
        <tr>  <td width="10px"></td>   <td>file_upload_progress:</td>    <td align="right"><?= $cache['file_upload_progress']; ?></td>      </tr>
      <? } ?>
      <tr height="15px"></tr>
      <? if (($cache['id'] & APC_CACHE_FILE) == APC_CACHE_FILE) { ?>
        <tr>  <td width="10px"></td>   <td>cache_by_default:</td>        <td align="right"><?= $cache['cache_by_default']; ?></td>          </tr>
          <tr>  <td width="10px"></td>   <td>file_update_protection:</td>  <td align="right"><?= $cache['file_update_protection']; ?></td>    </tr>
          <tr>  <td width="10px"></td>   <td>max_file_size:</td>           <td align="right"><?= bsize($cache['max_file_size']); ?></td>      </tr>
          <tr>  <td width="10px"></td>   <td>stat:</td>                    <td align="right"><?= $cache['stat']; ?></td>                      </tr>
          <tr>  <td width="10px"></td>   <td>stat_ctime:</td>              <td align="right"><?= $cache['stat_ctime']; ?></td>                </tr>
          <tr>  <td width="10px"></td>   <td>write_lock:</td>              <td align="right"><?= $cache['write_lock']; ?></td>                </tr>
          <? } ?>
          </table>
          </td>
          <td width="25"></td>
          <td>
          <table>
          <tr><td colspan="2">Since Start (<?= duration($cache['start_time']); ?> )</td></tr>
          <tr>
          <td width="105">
          <?=display_pie('pie_req_'.$cache['const_name'], array($cache['num_hits'],$cache['num_misses'],$cache['num_inserts']), array('"#aaffaa"', '"#ffaaaa"','"#aaaaff"'), 100);?>
          </td>
          <td>
          <? $requests = $cache['num_hits'] + $cache['num_misses'] + $cache['num_inserts']; ?>
          <table cellpadding="0" cellspacing="0">
          <tr>
          <td></td>
          <td width="100">Requests</td>
          <td width="50" align="right"><?=$requests?></td>
          <td width="50" align="right">100%</td>
          <td width="10"></td>
          </tr>
          <tr>
          <td width="14"><div style="width: 10px; height: 10px; background-color: #aaffaa;"></div></td>
          <td width="100">Hits</td>
          <td width="50" align="right"><?=$cache['num_hits']?></td>
          <td width="50" align="right"><?=$requests ? intval(($cache['num_hits']/$requests)*100) : 0?>%</td>
          <td width="10"></td>
          </tr>
          <tr>
          <td width="14"><div style="width: 10px; height: 10px; background-color: #ffaaaa;"></div></td>
          <td width="100">Misses</td>
          <td width="50" align="right"><?=$cache['num_misses']?></td>
          <td width="50" align="right"><?=$requests ? intval(($cache['num_misses']/$requests)*100) : 0?>%</td>
          <td width="10"></td>
          </tr>
          <tr>
          <td width="14"><div style="width: 10px; height: 10px; background-color: #aaaaff;"></div></td>
          <td width="100">Inserts</td>
          <td width="50" align="right"><?=$cache['num_inserts']?></td>
          <td width="50" align="right"><?=$requests ? intval(($cache['num_inserts']/$requests)*100) : 0?>%</td>
          <td width="10"></td>
          </tr>
          <tr height="5"></tr>
          <tr>
          <td></td>
          <td width="100">Hit Ratio</td>
          <td width="50" align="right">-</td>
          <td width="50" align="right"><?= ($cache['num_misses'] + $cache['num_hits']) != 0 ? intval($cache['num_hits']/($cache['num_misses']+$cache['num_hits'])*100) : 0?>%</td>
          <td width="10"></td>
          </tr>
          </table>
          </td>

          <td width="50"></td>

          <? 
          $sma = apc_sma_info(true);
    $total = $sma['segments'][$cache['segment_idx']]['size'];
    $used = $cache['mem_size']; 
    $free = $sma['segments'][$cache['segment_idx']]['avail'];
    $other = $total - ($free + $used); 
    ?>
      <td width="105">
      <?=display_pie('pie_mem_'.$cache['const_name'], array($used,$free,$other), array('"#cdb6e7"', '"#b4eef0"','"#eeeeee"'), 100);?>
      </td>
      <td>
      <table cellpadding="0" cellspacing="0">
      <tr>
      <td></td>
      <td width="100">Mem Total</td>
      <td width="50" align="right"><?=bsize($total)?></td>
      <td width="50" align="right">100%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #cdb6e7;"></div></td>
      <td width="100">Mem Used</td>
      <td width="50" align="right"><?=bsize($used)?></td>
      <td width="50" align="right"><?=$total ? intval(($used/$total)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #b4eef0;"></div></td>
      <td width="100">Mem Free</td>
      <td width="50" align="right"><?=bsize($free)?></td>
      <td width="50" align="right"><?=$total ? intval(($free/$total)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #eeeeee;"></div></td>
      <td width="100">Mem Other</td>
      <td width="50" align="right"><?=bsize($other)?></td>
      <td width="50" align="right"><?=$total ? intval(($other/$total)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      </table>
      </td>
      </tr>

      <tr height=30><td></td></tr>
      <tr><td colspan="2">Last Hour</td></tr>

      <tr>
      <td width="105">
      <?=display_pie('pie_winreq_'.$cache['const_name'], array($cache['num_hits'],$cache['num_misses'],$cache['num_inserts']), array('"#55ff55"', '"#ff5555"','"#5555ff"'), 100);?>
      </td>
      <td>
      <? $requests = $cache['num_hits'] + $cache['num_misses'] + $cache['num_inserts']; ?>
      <table cellpadding="0" cellspacing="0">
      <tr>
      <td></td>
      <td width="100">Requests</td>
      <td width="50" align="right"><?=$requests?></td>
      <td width="50" align="right">100%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #55ff55;"></div></td>
      <td width="100">Hits</td>
      <td width="50" align="right"><?=$cache['num_hits']?></td>
      <td width="50" align="right"><?= $requests ? intval(($cache['num_hits']/$requests)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #ff5555;"></div></td>
      <td width="100">Misses</td>
      <td width="50" align="right"><?=$cache['num_misses']?></td>
      <td width="50" align="right"><?=$requests ? intval(($cache['num_misses']/$requests)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      <tr>
      <td width="14"><div style="width: 10px; height: 10px; background-color: #5555ff;"></div></td>
      <td width="100">Inserts</td>
      <td width="50" align="right"><?=$cache['num_inserts']?></td>
      <td width="50" align="right"><?=$requests ? intval(($cache['num_inserts']/$requests)*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      <tr height=5></tr>
      <tr>
      <td></td>
      <td width="100">Hit Ratio</td>
      <td width="50" align="right">-</td>
      <td width="50" align="right"><?= ($cache['num_misses'] + $cache['num_hits']) != 0 ? intval($cache['num_hits']/($cache['num_misses']+$cache['num_hits'])*100) : 0?>%</td>
      <td width="10"></td>
      </tr>
      </table>
      </td>

      <td width="50"></td>

      <? 
      $sma = apc_sma_info(true);
    $total = $sma['segments'][$cache['segment_idx']]['size'];
    $used = $cache['mem_size']; 
    $free = $sma['segments'][$cache['segment_idx']]['avail'];
    $other = $total - ($free + $used); 
    ?>
      <td width="400" colspan="2">
      <font style="color: #888888; font-size: 0.8em;">Hits</font><br/>

      <canvas id="graph_hits_<?=$cache['const_name']?>" height="35" width="320"></canvas>
      <script>draw_graph("graph_hits_<?=$cache['const_name']?>", [<?=join($cache['hit_stats'], ',')?>], <?=max($cache['hit_stats'])?>, "#00ff00");</script>

      <br/><font style="color: #888888; font-size: 0.8em;">Misses</font><br/>
      <canvas id="graph_misses_<?=$cache['const_name']?>" height=35" width="320"></canvas>
      <script>draw_graph("graph_misses_<?=$cache['const_name']?>", [<?=join($cache['miss_stats'], ',')?>], <?=max($cache['miss_stats'])?>, "#ff0000");</script>

      <br/><font style="color: #888888; font-size: 0.8em;">Inserts</font><br/>
      <canvas id="graph_inserts_<?=$cache['const_name']?>" height="35" width="320"></canvas>
      <script>draw_graph("graph_inserts_<?=$cache['const_name']?>", [<?=join($cache['insert_stats'], ',')?>], <?=max($cache['insert_stats'])?>, "#0000ff");</script>
      </td>
      </tr>
      </table>
      </td>
      </tr>
      </table>
      <?
  }
  display_footer();
}

function action_exports() {
  $exports = array(
      'Raw apc_cache_info()' => 'raw_apc_cache_info',
      'Raw apc_sma_info()' => 'raw_apc_sma_info',
      '' => ''
      );
  if (!isset($_GET['e'])) {
    display_header($GLOBALS);
    display_exports($exports);
    display_footer();
  } else {
    $export_values = array_values($exports);
    $func = 'display_exports_'.$export_values[$_GET['e']];
    $func();
  }
}

function display_exports($exports) { ?>
  <? global $action_value; ?>
    <? $i=0; ?>
    <? foreach($exports as $key=>$value) { ?>
      <div><a href="?a=<?=$action_value?>&e=<?=$i?>"><?=$key?></a></div>
        <? $i++; ?>
        <? } ?>

        <? }

        function display_exports_raw_apc_cache_info() {
          display_header($GLOBALS);
          echo '<pre>';
          var_dump(apc_cache_info());
          echo '</pre>';
          display_footer();
        }
function display_exports_raw_apc_sma_info() {
  display_header($GLOBALS);
  echo '<pre>';
  var_dump(apc_sma_info(true));
  echo '</pre>';
  display_footer();
}

function action_version() {
  $url = "http://pecl.php.net/feeds/pkg_apc.rss";
  $apcver = phpversion('apc');
  if (defined('PROXY')) {
    $ctxt = stream_context_create( array( 'http' => array( 'proxy' => PROXY, 'request_fulluri' => True ) ) );
    $rss = @file_get_contents($url, False, $ctxt);
  } else {
    $rss = @file_get_contents($url);
  }
  display_header($GLOBALS);
  echo '<div style="text-align: center; margin-top: 25px;">';
  if (!$rss) {
    echo '<div style="color: #aa2222; background-color: #ffdddd;">';
    echo 'Unable to fetch version information. '.$url;
    echo '</div>';
  } else {
    preg_match('!<title>APC ([0-9.]+)</title>!', $rss, $match);
    if (version_compare($apcver, $match[1], '>=')) {
      echo '<div style="color: #22aa22; background-color: #ddffdd;">';
      echo 'You are running the latest version of APC ('.$apcver.')';
          echo '</div>';
          $i=3;
          } else {
          echo '<div style="color: #aa2222; background-color: #ffdddd;">';
          echo 'You are running an older version of APC ('.$apcver.'),
          newer version '.$match[1].' is available at <a href="http://pecl.php.net/package/APC/'.$match[1].'" style="font-weight: bold; color: #aa2222;">
          http://pecl.php.net/package/APC/'.$match[1].'</a>';
          echo '</div>';
          $i=-1;
          }
          echo '</div>';

          echo '<div style="float: left; width: 25%;">&nbsp;</div>';
          echo '<div style="float: left; margin-top: 25px;">';
          echo '<b>Change Log:</b> <br/><br/>';
          echo '<div style="margin-left: 7em;">';
          preg_match_all('!<(title|description)>([^<]+)</\\1>!', $rss, $match);
          next($match[2]); next($match[2]);
          while (list(,$v) = each($match[2])) {
          list(,$ver) = explode(' ', $v, 2);
          if ($i < 0 && version_compare($apcver, $ver, '>=')) {
            break;
          } else if (!$i--) {
            break;
          }
          echo "<b><a href=\"http://pecl.php.net/package/APC/$ver\">".htmlspecialchars($v)."</a></b><br><blockquote>";
          echo nl2br(htmlspecialchars(current($match[2])))."</blockquote>";
          next($match[2]);
          }
          echo '</div>';
  }

  echo '</div>';
  echo '<div style="float: left; width: 25%;">&nbsp;</div>';
  display_footer();
}

function display_segmentmap($map, $block_size, $title, $rv, $gv, $bv) {
  static $count=0; $count++;
  $min = min($map);
  $max = max($map);
  if ($min == $max) {
    $min = 0;
    if ($max == 0) {
      $max = 1;
    }
  }
  ?>
    <div style="float: left;">
    <div style="font-size: 0.8em;"><?=$title?></div>
    <div style="font-size: 0.75em;">1 block = <?=$block_size?></div>
    <canvas id="fragmap_<?=$count?>" width="127" height="132" style="border: 1px solid #eeeeee;">
    <?= join(',', $map); ?>
    </canvas>
    <div style="width: 127px;"><div style="font-size: 0.8em; float: left;" id="fragmap_min_<?=$count?>"></div><div style="font-size: 0.8em; float: right;" id="fragmap_max_<?=$count?>"></div></div>
    <script>
    var canvas = document.getElementById('fragmap_<?=$count?>');
  var ctx = canvas.getContext('2d');
  var values = canvas.innerHTML.split(",");
  var y=0;
  var x=0;
  var min=<?= $min; ?>;
  var max=<?= $max; ?>;
  var ratio = 255 / (max - min);

  document.getElementById('fragmap_min_<?=$count?>').innerHTML = "<?=bsize($min);?>";
  document.getElementById('fragmap_max_<?=$count?>').innerHTML = "<?=bsize($max);?>";

  var lingrad = ctx.createLinearGradient(0,128,128,132);
  lingrad.addColorStop(0, 'rgb(250,250,250)');
  lingrad.addColorStop(1, 'rgb(<?=$rv?>,<?=$gv?>,<?=$bv?>)');
  ctx.fillStyle = lingrad;
  ctx.fillRect(0,128,128,132);

  for (i=0; i < values.length; i++) {
    values[i] = values[i] - min;
    ctx.fillStyle = 'rgb(' + (Math.floor(250-values[i]*ratio)+<?=$rv?>) + ',' + (Math.floor(250-values[i]*ratio)+<?=$gv?>) + ',' + (Math.floor(250-values[i]*ratio)+<?=$bv?>) + ')';
        ctx.fillRect((x*8),(y*8),7,7);
        //ctx.fillRect((x*8),(y*8),8,8);
        console.log(values[i] + ' ' + values[i+1]);
        if (values[i] == (values[i+1] - min)) {
        //ctx.fillRect((x*8)+7,(y*8),1,7);
        }
        if (values[i] == (values[i + 16] - min)) {
        //ctx.fillStyle = 'rgb(255,0,0)';
        //ctx.fillRect((x*8),(y*8)+7,7,1);
        }
        x++;
        if (x % 16 == 0) {
        y++;
        x=0;
        }
        }
        </script>
        </div>
        <? }

    function display_pie($id, $data, $colors, $width) {
      ?>
        <canvas id="<?=$id?>" width="<?=$width?>" height="<?=$width?>" style="vertical-align: middle;">
        </canvas>
        <script>
        drawPie('<?=$id?>', [<?=join($data, ',');?>], [<?=join($colors, ',');?>]); 
      </script>
        <?
    }

function display_barline($percent, $color="rgb(100,100,250)", $start=0) {
  $percent = (int)$percent;
  if ($color == '') {
    $color="#ff0000";
  }
  echo '<div style="width: 130px;">';
  echo '  <div style="width: 100px;">';
  echo '    <div style="float: left; position: relative; top: 4px; height: 2px; opacity: 0; width: '.$start.'%;"></div>';                                   // space padding
  echo '    <div style="float: left; position: relative; top: 4px; height: 2px; width: '.$percent.'%; background: '.$color.';"></div>';   // line
  echo '  </div>';
  echo '  <div style="float: right; font-size: 0.75em;">'.$percent.'%</div>';                                                // label
  echo '</div>';
}

function display_graph($id, $values, $width, $height) {
  if (!is_array($values)) return;
  echo '<canvas id="graph_'.$id.'" height="'.$height.'" width="'.$width.'"></canvas>';
  echo '<script>draw_graph("graph_'.$id.'", ['.join($values, ',').'], '.max($values).', "#000000");</script>';
}


function action_error() {
  return $GLOBALS;
}

function display_js() { ?>
  <script language="JavaScript">

    function draw_graph(id, values, max, color) {
      var canvas = document.getElementById(id);
      var ctx = canvas.getContext('2d');
      var width = canvas.width - 10;
      var height = canvas.height - 10;
      ctx.beginPath();
      ctx.moveTo(10,height);
      var xmult = width / values.length;
      var ymult = height / (max+1);
      for (x=0; x<values.length; x++) {
        ctx.lineTo((x*xmult)+10,height-(values[x]*ymult));
      }
      ctx.strokeStyle = color;
      ctx.stroke();

      ctx.strokeStyle = "#cccccc";
      ctx.beginPath();
      ctx.moveTo(5,height);
      ctx.lineTo(5,0);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(10,canvas.height-5);
      ctx.lineTo(canvas.width,canvas.height-5);
      ctx.stroke();
    }

    function drawPie(id, data, colors) {
      canvas = document.getElementById(id);
      var ctx = canvas.getContext('2d');
      var canvas_size = [canvas.width, canvas.height];
      var radius = Math.min(canvas_size[0], canvas_size[1]) / 2;
      var center = [canvas_size[0]/2, canvas_size[1]/2];

      var rtotal = 0;
      var total = 0;
      var value;

      for (var piece in data) {
        total += data[piece];
      }

      if (total == 0) {
          ctx.beginPath();
          ctx.moveTo(center[0], center[1]);
          ctx.arc(center[0], center[1], radius, 0, Math.PI * 2, false);
          ctx.closePath();
          ctx.fillStyle = "#cccccc";
          ctx.fill();

      } else {
        for (var piece in data) {
            value = data[piece] / total;

            ctx.beginPath();
            ctx.moveTo(center[0], center[1]);
            ctx.arc( center[0], center[1], radius,
                Math.PI * (- 0.5 + 2 * rtotal),
                Math.PI * (- 0.5 + 2 * (rtotal + value)),
                false
            );

            ctx.lineTo(center[0], center[1]);
            ctx.closePath();
            ctx.fillStyle = colors[piece];
            ctx.strokeStyle = "#ffffff";
            ctx.fill();
            ctx.stroke();

            rtotal += value;
        }
      }

    }

    function popup_show() {
      document.getElementById("blanket").style.display = "block";
      document.getElementById("popup").style.display = "block";
      document.getElementsByTagName("body")[0].style.overflow = "hidden";
    }
    function popup_hide() {
      document.getElementById("blanket").style.display = "none";
      document.getElementById("popup").style.display = "none";
      document.getElementsByTagName("body")[0].style.overflow = "auto";
    }

    function popup(url) {
       xmlhttp = new XMLHttpRequest();
       xmlhttp.open("GET", url, true);
       xmlhttp.onreadystatechange=function() {
         if (xmlhttp.readyState==4) {
           document.getElementById("popup_content").innerHTML = xmlhttp.responseText;
           popup_show();
         }
       }
       xmlhttp.send("")
    }

    function info_show(url, id, toggle, post) {
       if (toggle && document.getElementById(id).parentNode.parentNode.style.display == "table-row") {
         document.getElementById(id).parentNode.parentNode.style.display = "none";
       } else {
         xmlhttp = new XMLHttpRequest();
         xmlhttp.open("POST", url, true);
         xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
         xmlhttp.setRequestHeader("Content-length", post.length);
         xmlhttp.setRequestHeader("Connection", "close");
         xmlhttp.onreadystatechange=function() {
           if (xmlhttp.readyState==4) {
             document.getElementById(id).innerHTML = xmlhttp.responseText;
             document.getElementById(id).style.width = document.getElementById(id).parentNode.parentNode.parentNode.offsetWidth; 
             document.getElementById(id).parentNode.parentNode.style.display = "table-row";
           }
         }
         xmlhttp.send(post)
       }
    }

    function clear_cache(url) {
      if (confirm('"Clear Cache"... sounds dangerous, are you sure?')) {
         xmlhttp = new XMLHttpRequest();
         xmlhttp.open("GET", url, true);
         xmlhttp.onreadystatechange=function() {
           if (xmlhttp.readyState==4) {
             location.reload(true);
           }
         }
         xmlhttp.send('')
      }
    }

  </script>
<? }

function display_footer() { ?>
    <?
      global $apc_start;
      $apc_stop = microtime(true);
      $total = $apc_stop - $apc_start;
    ?>
    <br/><br/>
    <div style="font-size: 0.8em;">Gen Time: <?=$total?> seconds</div>
    <br/><br/>
    </div>
  </body>
</html>
<? }


function bsize($s) {
	foreach (array('','K','M','G') as $i => $k) {
		if ($s < 1024) break;
		$s/=1024;
	}
	return sprintf("%5.1f %s",$s,$k);
}

function apc_date($time) {
  if ($time == 0) return '-';
  return date(DATE_FORMAT, $time);
}

function duration($ts) {
    $time = time();
    $years = (int)((($time - $ts)/(7*86400))/52.177457);
    $rem = (int)(($time-$ts)-($years * 52.177457 * 7 * 86400));
    $weeks = (int)(($rem)/(7*86400));
    $days = (int)(($rem)/86400) - $weeks*7;
    $hours = (int)(($rem)/3600) - $days*24 - $weeks*7*24;
    $mins = (int)(($rem)/60) - $hours*60 - $days*24*60 - $weeks*7*24*60;
    $str = '';
    if($years==1) $str .= "$years year, ";
    if($years>1) $str .= "$years years, ";
    if($weeks==1) $str .= "$weeks week, ";
    if($weeks>1) $str .= "$weeks weeks, ";
    if($days==1) $str .= "$days day,";
    if($days>1) $str .= "$days days,";
    if($hours == 1) $str .= " $hours hour and";
    if($hours>1) $str .= " $hours hours and";
    if($mins == 1) $str .= " 1 minute";
    else $str .= " $mins minutes";
    return $str;
}

