<?php
function _pos_api_expose_load_special_prices($vid) {
	$prices = array();
	$role_prices = db_query("SELECT rid, price FROM {uc_price_per_role_prices} WHERE vid = '%d'", $vid);
	while($role_price = db_fetch_object($role_prices)) {
		$prices[] = array('role' => $role_price->rid, 'price' => $role_price->price);
	}

	return $prices;
}

function _pos_api_expose_get_category($nid) {
	return db_result(db_query("SELECT gsrt.invoice_category FROM {term_node} tn INNER JOIN {ggw_state_reporting_terms} gsrt ON tn.tid = gsrt.vid WHERE tn.nid = '%d'", $nid));
}

function _pos_api_expose_map_product_datum($item) {
	if(empty($item->filepath)) {
		$item->filepath = 'sites/general-goods.com/files/imagefield_default_images/notfound_0.png';
	}

	$product = array(
		'id' => $item->nid,
		'sku' => $item->model,
		'list_price' => $item->list_price,
		'packaging' => $item->field_prod_packing_value,
		'sell_price' => $item->sell_price,
		'special_prices' => _pos_api_expose_load_special_prices($item->vid),
		'name' => trim($item->title),
		'image' => imagecache_create_url('pos_client_search', $item->filepath),
		'thumbnail' => imagecache_create_url('uc_thumbnail', $item->filepath)
	);

	$category = _pos_api_expose_get_category($item->nid);
	if($category) {
		$product['category'] = $category;
	}

	if(!empty($item->qty)) {
		$product['qty'] = (int)$item->qty;
	}

	return $product;
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
			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					INNER JOIN (('.$sku_sql.') UNION ALL ('.$fulltext_sql.')) fts ON n.vid = fts.vid

					ORDER BY fts.relevance DESC
					LIMIT 0, 12';

			$dataset = array();

			$items = db_query($sql, $query, $query, $query, $query);
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item);
			}

			pos_api_expose_respond($dataset, true);
		}
	}

	pos_api_expose_respond(array());
}

function pos_api_expose_load_ticket_products() {
	$failed_response = array(
		'status' => false,
		'error' => 'Could not load ticket products.'
	);

	if(array_key_exists('request', $_POST)) {
		$request = json_decode($_POST['request']);
		$token = $request->token;
		$ticketId = $request->ticketId;

		$uid = _pos_api_expose_uid_from_token($token);
		if($uid) {
			_pos_api_expose_log_ticket_load($uid, $ticketId);

			$ticketSql = "SELECT nid, qty FROM {uc_order_products} WHERE order_id = '%d'";

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value, ts.qty
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					INNER JOIN ('.$ticketSql.') ts ON ts.nid = n.nid';

		    $items = db_query($sql, $ticketId);
			$dataset = array();
			while($item = db_fetch_object($items)) {
				$dataset[] = _pos_api_expose_map_product_datum($item);
			}	

			pos_api_expose_respond(array('status' => true, 'products' => $dataset), true);
		}
	}

	pos_api_expose_respond($failed_response);
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

			$sql = 'SELECT n.nid, n.vid, n.title, p.model, p.list_price, p.sell_price, f.filepath, ctp_wrap.field_prod_packing_value
					FROM {uc_products} p
					INNER JOIN {node} n ON n.vid = p.vid

					LEFT JOIN {content_field_image_cache} cfic ON n.vid = cfic.vid
					LEFT JOIN {files} f ON f.fid = cfic.field_image_cache_fid
					LEFT JOIN {content_type_product} ctp_wrap ON n.vid = ctp_wrap.vid

					WHERE p.vid IN ('.$sku_sql.')
					LIMIT 0, 1';

			$product = db_fetch_object(db_query($sql, $barcode, $barcode, $barcode));

			$datum = _pos_api_expose_map_product_datum($product);

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