<?php
function pos_api_expose_product_search($token, $query) {
	if(!empty($token)) {
		$uid = db_result(db_query("SELECT paet.uid FROM {pos_api_expose_tokens} paet WHERE paet.token = '%s'", $token));
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value LIKE '%s%%'";

			$sku_sql = "SELECT p.vid, 1 AS relevance
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model LIKE '%s%%' OR
							  ctp.field_prod_unit_barcode_value LIKE '%s%%' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance FROM {node_revisions} nv
							 HAVING relevance > 0";

			//UNION ALL appears to be significantly faster than UNION.
			$sql = 'SELECT n.nid, n.title, p.model, p.list_price, p.sell_price, f.filepath
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid

					INNER JOIN (('.$sku_sql.') UNION ALL ('.$fulltext_sql.')) fts ON n.vid = fts.vid

					ORDER BY fts.relevance DESC
					LIMIT 0, 13';

			$dataset = array();

			$items = db_query($sql, $query, $query, $query, $query);
			while($item = db_fetch_object($items)) {
				if(empty($item->filepath)) {
					$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
				}

				$dataset[] = array(
					'nid' => $item->nid,
					'sku' => $item->model,
					'list_price' => $item->list_price,
					'sell_price' => $item->sell_price,
					'name' => trim($item->title),
					'image' => imagecache_create_url('product_grid_slim', $item->filepath)
				);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}