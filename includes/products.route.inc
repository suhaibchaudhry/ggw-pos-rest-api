<?php
function pos_api_expose_product_search($query) {
	$sku_sql = "SELECT p.vid FROM {uc_products} p WHERE p.model LIKE '%s%%'";

	$fulltext_sql = "SELECT nv.vid FROM {node_revisions} nv
					 WHERE MATCH(nv.title) AGAINST ('%s')";

	$sql = 'SELECT n.nid, n.title, p.model, p.list_price, p.sell_price, f.filepath
			FROM {uc_products} p
			INNER JOIN {node} n ON n.vid = p.vid

			LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
			LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid

			WHERE n.vid IN ('.$sku_sql.' UNION '.$fulltext_sql.')
			ORDER BY n.title ASC
			LIMIT 0, 13';

	$dataset = array();

	$items = db_query($sql, $query, $query);
	while($item = db_fetch_object($items)) {
		if(empty($item->filepath)) {
			$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
		}

		$dataset[] = array(
			'nid' => $item->nid,
			'sku' => $item->model,
			'list_price' => $item->list_price,
			'sell_price' => $item->sell_price,
			'name' => $item->title,
			'image' => imagecache_create_url('product_grid_slim', $item->filepath)
		);
	}

	print json_encode($dataset);
}