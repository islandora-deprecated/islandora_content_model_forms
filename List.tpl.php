<?php ?>
<div id="content-model-form-main">
  <div id="content-model-form-table">
    <table>
      <tr>
        <th>Content Model</th>
        <th>Datastream ID</th>
        <th>Title Field</th>
        <th>Form</th>
        <th>Transform</th>
        <th>Has Template</th>
        <th>Remove Association</th>
      </tr>
      <?php foreach ($list as $row) : ?>
      <?php list($id, $model, $form_name, $dsid, $title_field, $transform, $has_template) = $row; ?>
        <tr>
          <td><?php print $model ?></td>
          <td><?php print $dsid ?></td>
          <td><?php print $title_field ?></td>
          <td><?php print $form_name ?></td>
          <td><?php print $transform ?></td>
          <td><?php print ($has_template) ? 'True' : 'False' ?></td>
          <td><?php print l("Delete", "admin/content/model/forms/remove/$id") ?></td>
        </tr>
      <?php endforeach; ?>
      </table>
    </div>
    <div id="content-model-actions">
    <?php print $form ?>
  </div>
</div>