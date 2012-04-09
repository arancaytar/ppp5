<?php

/**
 * @file
 * Page callback for the topic page.

/**
 * Display one topic.
 */
function _ppp_display_page_topic($topic) {
  if (!$topic) {
    return MENU_NOT_FOUND;
  }

  $forum = $topic->get_forum();
  drupal_set_breadcrumb(array(
      l(t('Archives'), 'archives'),
      l($forum->name, 'forum/' . $forum->fid),
      l($topic->title, 'topic/' . $topic->tid),
  ));

  $pager = array('#theme' => 'pager');
  $posts = $topic->load_posts();
  $page = array(
    'pager_top' => $pager,
    'topic' => array(
      '#theme' => 'table',
      '#sticky' => FALSE,
      '#header' => array(t('Author'), t('Topic: !topic', array('!topic' => $topic->title))),
      '#rows' => array(),
      '#attributes' => array('class' => array('topic-table')),
    ),
    'pager_bottom' => $pager,
  );

  foreach ($posts as $pid => $post) {
    $page['topic']['#rows'][$pid] = array(
      'author' => array(
        'data' => theme('ppp_display_post_sidebar', array(
          'topic' => $topic,
          'post' => $post,
          'user' => $post->get_author(),
        )),
        'valign' => 'top',
      ),
      'content' => theme('ppp_display_post_body', array(
        'topic' => $topic,
        'post' => $post,
        'user' => $post->get_author(),
      )),
    );
  }
  return $page;
}

