<p class="post-header" id="<?php print $post->pid ?>">
  #<?php print $post->pid ?>
  <span class="ppp-icon ppp-icon-<?php print $post->icon ?>"></span>
  <span class="ppp-postdate">written <?php print format_date($post->created, 'custom', 'l, F j Y H:i') ?></span>
  <a class="ppp-icon ppp-icon-profile" href="<?php print url('member/' . $user->uid) ?>" alt="Profile" title="View Profile"></a>
  <?php if (isset($user->fields['url'])): ?>
  <a class="ppp-icon ppp-icon-homepage" href="<?php print $user->fields['url'] ?>" alt="Homepage" title="View Homepage"></a>
  <?php endif; ?>
</p>
<hr size="1" class="ppp-rule" />
<div class="post-body"><?php print check_markup($post->body, variable_get('ppp_post_format', 'xbbcode')) ?></div>
<hr size="1" class="ppp-rule" />
<span class="ppp-postcount">Posts: <strong><?php print l($user->posts, 'member/' . $user->uid . '/posts') ?></strong></span> | 
<?php if (isset($user->fields['location'])): ?>
<span class="ppp-location">From: <strong><?php print $user->fields['location'] ?></strong></span> | 
<?php endif; ?>
<span class="ppp-registered">Registered: <strong><?php print format_date($user->joined, 'custom', 'l, F j Y H:i') ?></strong></span>
