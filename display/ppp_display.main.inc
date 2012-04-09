<?php

/**
 * @file
 * Callbacks for the main archive page.
 */

/**
 * Display the main archive page.
 */
function _ppp_display_page_main() {
  module_load_include('inc', 'ppp_data', 'ppp_data.load');
  $categories = ppp_data_load_categories();
  $page = array();
  $header = array(
    'id' => t('#'),
    'forum' => t('Forum'),
    'topics' => t('Topics'),
    'posts' => t('Posts'),
    'last' => t('Last activity'),
  );
  foreach ($categories as $cid => $name) {
    $page[$cid] = array(
      '#theme' => 'table',
      '#caption' => $name,
      '#header' => $header,
      '#rows' => array(),
    );

    $fora = ppp_data_load_fora($cid);

    foreach ($fora as $fid => $forum) {
      // Run the fora through the access layer
      if (!_ppp_display_access_forum($forum)) {
        unset($fora[$fid]);
        continue;
      }
      $last = ppp_data_load_lastpost($fid);

      $page[$cid]['#rows'][$fid] = array(
        'id' => $fid,
        'name' => l($forum->name, 'forum/' . $forum->fid, array('attributes' => array('class' => 'forum-link', 'id' => 'forum-link-' . $forum->fid))) . '<p class="forum-desc">' . check_plain($forum->description) . '</p>',
        'topics' => $forum->topics,
        'posts' => $forum->posts,
        'last' => $last ? t('!topic by !user at !time', array('!topic' => l($last->title, 'topic/' . $last->tid), '!user' => _ppp_display_userlink($last->author, $last->ended), '!time' => format_date($last->ended))) : t('<span class="no-content">No content</span>'),
      );
    }
  }
  return $page;
}

