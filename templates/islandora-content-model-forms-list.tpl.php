<?php

/**
 * @file
 *
 * @TODO: needs documentation + list of all available variables
 */
?>
<div id="content-model-form-main">
  <div id="content-model-form-table">
    <table>
      <tr>
        <th><?php print t('Content model'); ?></th>
        <th><?php print t('Type'); ?></th>
        <th><?php print t('Datastream ID'); ?></th>
        <th><?php print t('Title field'); ?></th>
        <th><?php print t('Form'); ?></th>
        <th><?php print t('Transform'); ?></th>
        <th><?php print t('Has template'); ?></th>
        <th><?php print t('Operations'); ?></th>
      </tr>
      <?php foreach ($list as $association) : ?>
        <tr>
          <td><?php print $association['content_model'] ?></td>
          <td><?php print ($association['type'] == 'hook') ? t('Built-in') : t('Custom') ?></td>
          <td><?php print $association['dsid'] ?></td>
          <td><?php print $association['title_field'] ?></td>
          <td><?php print $association['form_name'] ?></td>
          <td><?php print $association['transform'] ?></td>
          <td><?php print ($association['template']) ? t('Yes') : t('No') ?></td>
          <?php if($association['type'] == 'hook'): ?>
            <td>
              <?php if($association['disabled']): ?>
                <?php print l(t("Enable"), "admin/islandora/model/forms/disable/" . $association['id'] . "/false") ?>
              <?php else: ?>
                <?php print l(t("Disable"), "admin/islandora/model/forms/disable/" . $association['id'] . "/true") ?>
              <?php endif; ?>
            </td>
          <?php else: ?>
            <td>
              <?php print l(t("Delete"), "admin/islandora/model/forms/remove/" . $association['id']) ?>
            </td>
          <?php endif; ?>
        </tr>
      <?php endforeach; ?>
      </table>
    </div>
    <div id="content-model-actions">
    <?php print render($form) ?>
  </div>
</div>