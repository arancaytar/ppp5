<?php
// $Id$


/**
 * @file ppp_display.pages.inc
 * The pages that show the PPP content.
 */

/**
 * Display the archive main page
 */
function ppp_display_page_main() {
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
      if (!ppp_display_access_forum($fid)) {
        unset($fora[$fid]);
        continue;
      }
      $last = ppp_data_load_lastpost($fid);

      $page[$cid]['#rows'][$fid] = array(
        'id' => $fid,
        'name' => theme('ppp_display_forum_name', array('forum' => $forum)),
        'topics' => $forum->topics,
        'posts' => $forum->posts,
        'last' => $last ? theme('ppp_display_forum_last', array('post' => $last)) : t('<span style="white-space:nowrap">No content</span>'),
      );
    }
  }
  return $page;
}

/**
 * Display one forum
 */
function ppp_display_page_forum($forum_id) {
  $forum = ppp_data_load_forum($forum_id);
  $topics = ppp_data_load_topics(array('forum_id' => $forum_id));

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

function theme_ppp_display_forum_name($variables) {
  $forum = $variables['forum'];
  $link['attributes'] = array(
    'class' => 'forum-link',
    'id' => "forum-link-{$forum->fid}",
  );
  return l($forum->name, "forum/{$forum->fid}", $link) . '<p class="forum-desc">' . check_plain($forum->description) . '</p>';
}

/**
 * Render the last post field.
 */
function theme_ppp_display_forum_last($variables) {
  $post = $variables['post'];
  return t('<span style="white-space: nowrap">In <a href="@url">%topic</a></span> <span style="white-space: nowrap">by !user</span> <span style="white-space: nowrap"> at %time</p>', array('@url' => url('topic/' . $post->tid), '%topic' => $post->title, '!user' => ppp_display_userlink($post->author, $post->ended), '%time' => format_date($post->ended)));
}
