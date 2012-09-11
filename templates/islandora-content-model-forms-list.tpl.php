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
        <th><?php print t('Datastream ID'); ?></th>
        <th><?php print t('Title field'); ?></th>
        <th><?php print t('Form'); ?></th>
        <th><?php print t('Transform'); ?></th>
        <th><?php print t('Has template'); ?></th>
        <th><?php print t('Remove association'); ?></th>
      </tr>
      <?php foreach ($list as $row) : ?>
      <?php list($id, $model, $form_name, $dsid, $title_field, $transform, $has_template) = $row; ?>
        <tr>
          <td><?php print $model ?></td>
          <td><?php print $dsid ?></td>
          <td><?php print $title_field ?></td>
          <td><?php print $form_name ?></td>
          <td><?php print $transform ?></td>
          <td><?php print ($has_template) ? t('Yes') : t('No') ?></td>
          <td><?php print l(t("Delete"), "admin/islandora/model/forms/remove/$id") ?></td>
        </tr>
      <?php endforeach; ?>
      </table>
    </div>
    <div id="content-model-actions">
    <?php print render($form) ?>
  </div>
</div>