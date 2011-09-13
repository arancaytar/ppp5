<?php
// $Id: ppp_db.save.inc 478 2010-08-16 11:58:31Z arancaytar $

/**
 * @file ppp_db.save.inc
 *   Functions for saving PPP records to the database.
 */

/**
 * Save one topic. Does NOT save any post records.
 * @param $topic
 *   A fully processed topic object. Required fields include:
 *    - int tid, 
 *    - int post, 
 *    - int forum, 
 *    - int author, 
 *    - int started, 
 *    - int ended, 
 *    - string title, 
 *    - bool sticky, 
 *    - bool closed
 */
function ppp_db_save_topic($topic) {
  $query = db_merge('ppp_topic');
  $query->key(array('tid' => (int)$topic->tid));
  $query->fields(array(
    'post'    => (int)$topic->post,
    'forum'   => (int)$topic->forum,
    'author'  => (int)$topic->author,
    'started' => (int)$topic->started,
    'ended'   => (int)$topic->ended,
    'title'   => "$topic->title",
    'sticky'  => !empty($topic->sticky) ? 1 : 0,
    'closed'  => !empty($topic->closed) ? 1 : 0,
    'icon'    => (int)$topic->icon,
    'length'  => (int)$topic->length,
  ));
  $query->execute();
}

/**
 * Save one post.
 *
 * @param $post 
 *    A fully processed post object. Required fields include:
 *     - int pid,
 *     - int topic,
 *     - int forum,
 *     - int author,
 *     - string body,
 *     - int icon,
 *     - string ip
 */
function ppp_db_save_post($post) {
  $query = db_merge('ppp_post');
  $query->key(array('pid' => (int)$post->pid));
  $query->fields(array(
    'topic'   => (int)$post->topic,
    'forum'   => (int)$post->forum,
    'author'  => (int)$post->author,
    'created' => (int)$post->created,
    'body'    => "$post->body",
    'icon'    => (int)$post->icon,
    'ip'      => "$post->ip"
  ));
  $query->execute();
}

/**
 * Save one forum record.
 * 
 * @param $forum
 *   One forum object. Required fields include:
 *    - int fid,
 *    - string name,
 *    - string description,
 *    - int category
 */
function ppp_db_save_forum($forum) {
  $query = db_merge('ppp_forum');
  $query->key(array('fid' => (int)$forum->fid));
  $query->fields(array(
    'name' => "$forum->name",
    'description' => "$forum->description",
    'category' => (int)$forum->category,
  ));
  $query->execute();
}

/**
 * Save one category.
 * 
 * @param $category
 *   One category object. Required fields:
 *    - int cid,
 *    - string name
 */
function ppp_db_save_category($category) {
  $query = db_merge('ppp_category');
  $query->key(array('cid' => (int)$category->cid));
  $query->fields(array(
    'name' => "$category->name",
  ));
  $query->execute();
}

/**
 * Save a single user. This function automatically takes care of archiving any fields that have changed.
 *
 * @param $user
 *   A fully populated user object. This object must have the following fields:
 *    - int uid,
 *    - array fields [string $name => object $field { string $value }],
 *    - int updated,
 *    - int joined,
 *    - int login,
 *    - int posts,
 *   Optional values include:
 *    - string notes,
 */
function ppp_db_save_user($user) {
  module_load_include('inc', 'ppp_db', 'ppp_db.save');
  $old_user = ppp_db_load_user($user->uid);
  $fields = ppp_db_load_fields();
  
  // If a user is already here, use a lot of intricate logic to update fields.
  if ($old_user) {
    $expired = array();
    $added = array();
    
    // Find values that have been deleted, added or changed.
    foreach ($fields as $fid => $name) {
      if (isset($old_user->fields[$name])) {
        if (!isset($user->fields[$name]) || $old_user->fields[$name]->value != $user->fields[$name]->value) {
          $expired[$old_user->fields[$name]->aid] = $field->aid;
        }
      }
      
      if (isset($user->fields[$name])) {
        if (!isset($old_user->fields[$name]) || $old_user->fields[$name]->value != $user->fields[$name]->value) {
          $added[$fid] = $user->fields[$field]->value;
        }
      }
    }
    
    // Add an expiration timestamp to old fields.
    db_update('ppp_user_profile')
      ->fields(array('expired' => $user->updated))
      ->condition('aid', $expired, 'IN')
      ->execute();
  }
  else {
    // Since the user does not exist yet, all fields are considered new, and no fields are expired.
    foreach ($fields as $fid => $name) {
      if (isset($user->fields[$name])) {
        $added[$fid] = $user->fields[$value];
      }
    }
  }
  
  // Insert new fields, whether the user exists or not.
  $query = db_insert('ppp_user_profile');
  foreach ($added as $fid => $value) {
    $query->values(array(
      'uid' => (int)$user->uid, 
      'fid' => (int)$fid, 
      'value' => "$value", 
      'created' => (int)$user->updated
    ));
  }
  $query->execute();
  
  // Finally, save the user, whether it exists or not.
  db_merge('ppp_user')
    ->key(array('uid' => (int)$user->uid))
    ->fields(array(
      'joined' => (int)$user->joined,
      'posts' => (int)$user->posts,
      // Ensure valid pseudo-enum.
      'notes' => isset($user->notes) ? "$user->notes" : NULL,
      'updated' => (int)$user->updated,
    ))
    ->execute();
}

/**
 * Call this when indexing a forum to discover new topics, or when marking a topic as archived.
 *
 * @param $topic
 *   A topic object that must contain EITHER: 
 *   {int post, int forum, int updated, int author, int length, string title}
 *   {int post, int archived for topics that are already saved}
 *   The latter has precedence, so an archived timestamp will cause other properties to be ignored.
 */ 
function ppp_db_save_crawl($topic) {
  if (empty($topic->archived)) {
    $query = db_merge('ppp_crawl');
    $query->key(array('post' => (int)$topic->post));
    $query->fields(array(
      'updated' => (int)$topic->updated,
      'forum'   => (int)$topic->forum,
      'author'  => (int)$topic->author,
      'length'  => (int)$topic->length,
      'title'   => "$topic->title",
    ));
  }
  else {
    $query = db_update('ppp_crawl');
    $query->fields(array('archived' => (int)$topic->archived));
    $query->condition('post', (int)$topic->post);
  }
  
  $query->execute();
}
