<?php
// $Id: ppp_display.pages.inc 483 2010-08-16 22:48:02Z arancaytar $

/**
 * @file ppp_display.pages.inc
 * The pages that show the PPP content.
 */

/**
 * Determine whether the current user may view a certain forum or topic
 *
 * @param $forum
 *   The number of the forum that is being requested, or the forum the topic is in.
 *
 * @return
 *   A user access boolean.
 */
function ppp_display_access_forum($forum) {
  $mod_boards = variable_get('ppp_classified_fora', array());
  
  if (!empty($mod_boards[$forum])) {
    return user_access('view classified content');
  }
  else {
    return user_access('view archived content');
  }
}

/**
 * Display the archive main page
 */
function ppp_display_main() {
  module_load_include('inc', 'ppp_db', 'ppp_db.load');
  $categories = ppp_db_load_categories();
  $out = '';
  foreach ($categories as $cid => $name) {
    $fora = ppp_db_load_fora($cid);
    
    // Run the fora through the access layer
    foreach ($fora as $fid => $forum) {
      if (!ppp_display_access_forum($fid)) {
        unset($fora[$fid]);
      }
      else {
        $fora[$fid]->lastpost = ppp_db_load_lastpost($fid);
      }
    }
    //$cat_head = '<tr class="cat-head"><td colspan="5">' . check_plain($category->name) . '</td></tr>';
    $out .= theme('ppp_display_forumlist', $fora, check_plain($name));
    //$out .= str_replace('<thead>', "<thead>$cat_head", $fora);
  }
  return $out;
}

/**
 * Display one forum
 */
function ppp_display_forum($forum_id) {
  $forum = ppp_db_load_forum($forum_id);
  $topics = ppp_db_load_topics(array('forum_id' => $forum_id));
  
  // TODO: User config.
  $limit = 25;
  
  drupal_set_title($forum->name);
  $breadcrumb = drupal_set_breadcrumb();
  $breadcrumb[] = l(t('Archives'), 'archives');
  drupal_set_breadcrumb($breadcrumb);
  
  $pager = theme('pager', array(), $limit);
  $topiclist = theme('ppp_display_topiclist', $topics);
  
  $out = '<p class="forum-description">' . $forum->description . '</p>';
  $out .= $pager . $topiclist . $pager;
  return $out;
}

/**
 * Render a list of fora.
 */
function theme_ppp_display_forumlist($fora, $caption) {
  $attributes = array('class' => 'forum-list');
  
  // This is not sortable.
  $header = array(
    '#',
    'Forum',
    'Topics',
    'Posts',
    'Last post',
  );
  
  $data = array();
  
  foreach ($fora as $id => $forum) {
    $link['attributes'] = array(
      'class' => 'forum-link',
      'id' => "forum-link-$id",
    );
    $themed_name = l($forum->name, "forum/$id", $link) . '<p class="forum-desc">' . check_plain($forum->description) . '</p>';  
    $data[] = array(
      $id,
      $themed_name,
      $forum->topics,
      $forum->posts,
      theme('ppp_display_last_post', $forum->lastpost),
    );
  }
  
  return theme('table', $header, $data, $attributes, $caption);
}

/**
 * Render the last post field.
 */
function theme_ppp_display_last_post($lastpost) {
  return t('In <a href="@url">%topic</a> by !user at %time', array('@url' => url('topic/' . $lastpost->tid), '%topic' => $lastpost->title, '!user' => ppp_display_userlink($lastpost->author, $lastpost->ended), '%time' => format_date($lastpost->ended)));
}