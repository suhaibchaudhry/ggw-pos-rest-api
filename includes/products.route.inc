<?php
function _pos_api_expose_load_special_prices($vid) {
	$prices = array();
	$role_prices = db_query("SELECT rid, price FROM {uc_price_per_role_prices} WHERE vid = '%d'", $vid);
	while($role_price = db_fetch_object($role_prices)) {
		$prices[] = array('role' => $role_price->rid, 'price' => $role_price->price);
	}

	return $prices;
}

function _pos_api_expose_map_datum($item) {
	return array(
		'id' => $item->nid,
		'sku' => $item->model,
		'list_price' => $item->list_price,
		'sell_price' => $item->sell_price,
		'special_prices' => _pos_api_expose_load_special_prices($item->vid),
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath)
	);
}

function pos_api_expose_product_search($token, $query) {
	if(!empty($token)) {
		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value LIKE '%s%%'";

			$sku_sql = "SELECT p.vid, 1 AS relevance
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model LIKE '%s%%' OR
							  ctp.field_prod_unit_barcode_value LIKE '%s%%' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$fulltext_sql = "SELECT nv.vid, MATCH(nv.title) AGAINST ('%s') AS relevance
							 FROM {node_revisions} nv
							 HAVING relevance > 0";

			//UNION ALL appears to be significantly faster than UNION.
			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid

					INNER JOIN (('.$sku_sql.') UNION ALL ('.$fulltext_sql.')) fts ON n.vid = fts.vid

					ORDER BY fts.relevance DESC
					LIMIT 0, 12';

			$dataset = array();

			$items = db_query($sql, $query, $query, $query, $query);
			while($item = db_fetch_object($items)) {
				if(empty($item->filepath)) {
					$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
				}

				$dataset[] = _pos_api_expose_map_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_product_scan() {
	$failed_response = array(
		'scan' => false,
		'error' => 'Could not scan barcode.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$barcode = trim($request->barcode);

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			$additional_barcode_sql = "SELECT cfab.vid FROM {content_field_additional_barcodes} cfab WHERE cfab.field_additional_barcodes_value = '%s'";

			$sku_sql = "SELECT p.vid
						FROM {uc_products} p
						LEFT JOIN {content_type_product} ctp ON p.vid = ctp.vid
						WHERE p.model = '%s' OR
							  ctp.field_prod_unit_barcode_value = '%s' OR
							  p.vid IN (".$additional_barcode_sql.")";

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid

					WHERE p.vid IN ('.$sku_sql.')
					LIMIT 0, 1';

			$product = db_fetch_object(db_query($sql, $barcode, $barcode, $barcode));

			$datum = _pos_api_expose_map_datum($product);

			if($product) {
				$response = array(
					'scan' => true,
					'product' => $datum
				);
			} else {
				$response = array(
					'scan' => false,
					'error' => 'Could not find barcode.'
				);
			}

			pos_api_expose_respond($response, true);
		}
	}

	pos_api_expose_respond($failed_response);
}