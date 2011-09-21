<?php
// $Id$


/**
 * @file ppp_data.load.inc
 *   Contains all functions that load data from the PPP database.
 */

/**
 * Load a user either with the most current data, or the data at a particular moment in time.
 *
 * @param $uid
 *   User's ID, required integer.
 *
 * @param $time
 *   Unix timestamp, optional integer.
 *
 * @return
 *   A fully loaded user object whose profile fields show when they were discovered and lost.
 */
function ppp_data_load_user($uid, $time = NULL) {
  // Protect input
  $uid = (int)$uid;
  $user = db_query('SELECT * FROM {ppp_user} WHERE `uid` = :uid', array(':uid' => $uid))->fetch();
  if (!$user) {
    return FALSE;
  }

  $query = db_select('ppp_user_profile', 'p');
  $query->innerJoin('ppp_user_field', 'f', 'f.fid = p.field');
  $query->addField('p', 'aid');
  $query->addField('f', 'fid');
  $query->addField('f', 'name');
  $query->addField('p', 'value');
  $query->addField('p', 'created');
  $query->addField('p', 'expired');
  $query->addExpression('`expired` IS NULL', 'current');

  $query->condition('uid', $uid);

  if ($time) {
    $time = (int)$time;
    $old = db_and()->condition('created', $time, '<')->condition('expired', $time, '>');
    $old_or_new = db_or()->condition($old)->condition('expired', NULL, 'IS NULL');
    $query->condition($old_or_new);
  }
  else {
    $query->condition('expired', NULL, 'IS NULL');
  }
  $query->orderBy('current')->orderBy('created', 'DESC');
  $fields = $query->execute();

  $user->fields = array();
  while ($field = $fields->fetch()) {
    if (!isset($user->fields[$field->name])) {
      // How often can I say field on one line?
      $user->fields[$field->name] = $field->value;
    }
  }

  return $user;
}

/**
 * Returns the names, numbers and labels of all profile fields. Statically cached.
 *
 * @return
 *   An array of [ int $fid => object { string $name, string $value }]
 */
function ppp_data_load_fields() {
  $query = db_query('SELECT `fid`, `name`, `label` FROM {ppp_user_field}');
  $fields = array();
  while ($row = $query->fetch()) {
    $fields[$row->fid] = $row;
  }
  return $fields;
}

/**
 * Load a single post record.
 *
 * @param $pid
 *   The int post ID.
 *
 * @return
 *   A post object.
 */
function ppp_data_load_post($pid) {
  return db_query('SELECT * FROM {ppp_post} WHERE pid = :pid', array(':pid' => (int)$pid))->fetch();
}

/**
 * Load a topic record. No posts are loaded.
 *
 * @param $tid
 *   The int topic ID of a topic.
 *
 * @return
 *   A topic object.
 */
function ppp_data_load_topic($tid) {
  return new PPPTopic(db_query('SELECT * FROM {ppp_topic} WHERE tid = :tid', array(':tid' => (int)$tid))->fetch());
}

/**
 * Load a forum record
 *
 * @param $fid
 *   The int ID of the forum.
 *
 * @return
 *   A forum object
 */
function ppp_data_load_forum($fid) {
  return new PPPForum(db_query('SELECT * FROM {ppp_forum} WHERE fid = :fid', array(':fid' => (int)$fid))->fetch());
}

/**
 * Load all fora in a category.
 *
 * @param $category
 *   [Optional] A category cid int that fora should be in, or all fora.
 *
 * @return
 *   An array of [ int fid : object forum { name, description, posts, topics, category } ]
 */
function ppp_data_load_fora($category = NULL) {
  $query = db_select('ppp_forum', 'f');
  $query->addField('f', 'fid');
  $query->addField('f', 'name');
  $query->addField('f', 'description');
  $query->addField('f', 'category');

  if ($category) {
    $query->condition('category', (int)$category);
  }
  $result = $query->execute();

  $fora = array();
  while ($forum = $result->fetch()) {
    $fora[$forum->fid] = $forum;
  }
  if (!$fora) {
    return $fora;
  }
  $query = db_select('ppp_post', 'p');
  $query->addField('p', 'forum');
  $query->addExpression('COUNT(pid)', 'posts');
  $query->addExpression('COUNT(DISTINCT topic)', 'topics');
  $query->condition('forum', array_keys($fora), 'IN');
  $query->groupBy('forum');
  $result = $query->execute();

  while ($stat = $result->fetch()) {
    $stats[$stat->forum] = $stat;
  }

  foreach ($fora as $fid => $forum) {
    $fora[$fid]->posts = isset($stats[$fid]) ? $stats[$fid]->posts : 0;
    $fora[$fid]->topics = isset($stats[$fid]) ? $stats[$fid]->topics : 0;
  }

  return $fora;
}

/**
 * Load the last post on a forum.
 */
function ppp_data_load_lastpost($forum) {
  $query = db_select('ppp_topic', 't');
  $query->addField('t', 'tid');
  $query->addField('t', 'title');
  $query->addField('t', 'ended');

  $query->join('ppp_post', 'p', 't.tid = p.topic');

  $query->addField('p', 'author');
  $query->condition('t.forum', $forum);
  $query->orderBy('p.created', 'desc');
  $query->range(0, 1);
  return $query->execute()->fetch();
}

/**
 * Load all categories. Uses static caching.
 *
 * @return
 *   All categories as an array [ int cid : string name ]
 */
function ppp_data_load_categories() {
  static $cat = array();
  if (empty($cat)) {
    $query = db_query('SELECT * FROM {ppp_category}');
    while ($row = $query->fetch()) {
      $cat[$row->cid] = $row->name;
    }
  }
  return $cat;
}

/**
 * Load posts by condition.
 *
 * @param $condition
 *   An array [ string $field : $value ]. Value MUST match the type of the field explicitly (string "0" does not work).
 * @param $sort
 *   [optional] An array [ "field" : string $field, "direction" : ("ASC" | "DESC") ]
 * @param $range
 *   [optional] An array [ "start" : int $start , "count" : int $count ]
 *
 * @return
 *   A post object.
 */
function ppp_data_load_posts($condition, $limit = 25, $header = NULL) {
  $query = db_select('ppp_post', 'p')->extend('PagerDefault');

  $query->addField('p', 'pid');
  $query->addField('p', 'topic');
  $query->addField('p', 'forum');
  $query->addField('p', 'author');
  $query->addField('p', 'created');
  $query->addField('p', 'icon');
  $query->addField('p', 'body');

  foreach ($condition as $field => $value) {
    $query->condition($field, $value);
  }
  if ($header) {
    $query->extend('TableSort');
    $query->orderByHeader($header);
  }
  $query->limit($limit);
  $result = $query->execute();

  $posts = array();
  while ($row = $result->fetch()) {
    $posts[$row->pid] = new PPPPost($row); // PPPPowerbook!
  }
  return $posts;
}

/**
 * Load topics by condition.
 *
 * @param $condition
 *   An array [ string $field : $value ]. Value MUST match the type of the field explicitly (string "0" does not work).
 * @param $sort
 *   [optional] An array [ "field" : string $field, "direction" : ("ASC" | "DESC") ]
 * @param $range
 *   [optional] An array [ "start" : int $start , "count" : int $count ]
 *
 * @return
 *   An array of [ int pid : object post ]
 */
function ppp_data_load_topics($condition, $header = NULL, $limit = 25) {
  $query = db_select('ppp_topic', 't')->extend('PagerDefault')->extend('TableSort');
  $query->limit($limit);
  $query->addField('t', 'tid');
  $query->addField('t', 'post');
  $query->addField('t', 'forum');
  $query->addField('t', 'author');
  $query->addField('t', 'length');
  $query->addField('t', 'title');
  $query->addField('t', 'closed');
  $query->addField('t', 'sticky');
  $query->addField('t', 'started');
  $query->addField('t', 'ended');
  $query->addField('t', 'icon');

  $query->join('ppp_user', 'u', 't.author = u.uid');
  $query->join('ppp_post', 'p', 't.tid = p.topic AND t.ended = p.created');
  $query->join('ppp_user', 'ul', 'p.author = ul.uid');
  $query->addField('u', 'pdn', 'author_pdn');
  $query->addField('ul', 'uid', 'last_uid');
  $query->addField('ul', 'pdn', 'last_pdn');

  foreach ($condition as $field => $value) {
    $query->condition($field, $value);
  }
  if ($header) {
    $query->orderByHeader($header);
  }
  $result = $query->execute();

  $topics = array();
  while ($row = $result->fetch()) {
    $topics[$row->tid] = $row;
  }
  return $topics;
}

/**
 * Load all topics that must be recrawled.
 *
 * @return
 *   An array of topics [ int post : object topic {int archived, int updated, int forum, int author, int length, string title}]
 */
function ppp_data_load_crawl() {
  $query = db_select('ppp_crawl', 'c');
  $query->addField('c', 'post');
  $query->addField('c', 'archived');
  $query->addField('c', 'updated');
  $query->addField('c', 'forum');
  $query->addField('c', 'author');
  $query->addField('c', 'length');
  $query->addField('c', 'title');

  $condition = db_or()->condition('`archived` < `updated`', array(), NULL)->condition('`archived` IS NULL', array(), NULL);

  $query->condition($condition);
  $result = $query->execute();

  $topics = array();
  while ($row = $result->fetch()) {
    $topics[$row->post] = $row;
  }
  return $topics;
}

