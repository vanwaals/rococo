#include "all.h"

#include "bench/tpcc_real_dist/sharding.h"

namespace rococo {

void TpccPiece::reg_order_status() {
  BEGIN_PIE(TPCC_ORDER_STATUS, // RO
          TPCC_ORDER_STATUS_0, // piece 0, R customer secondary index, c_last -> c_id
          DF_NO) {
    // #################################################################
    verify(input.size() == 3);
    Log::debug("TPCC_ORDER_STATUS, piece: %d", TPCC_ORDER_STATUS_0);
    // #################################################################

    mdb::MultiBlob mbl(3), mbh(3);
    mbl[0] = input[2].get_blob();
    mbh[0] = input[2].get_blob();
    mbl[1] = input[1].get_blob();
    mbh[1] = input[1].get_blob();
    Value c_id_low(std::numeric_limits<i32>::min());
    Value c_id_high(std::numeric_limits<i32>::max());

    mbl[2] = c_id_low.get_blob();
    mbh[2] = c_id_high.get_blob();
    c_last_id_t key_low(input[0].get_str(), mbl, &(C_LAST_SCHEMA));
    c_last_id_t key_high(input[0].get_str(), mbh, &(C_LAST_SCHEMA));
    std::multimap<c_last_id_t, rrr::i32>::iterator it, it_low, it_high, it_mid;
    bool inc = false, mid_set = false;
    it_low = C_LAST2ID.lower_bound(key_low);
    it_high = C_LAST2ID.upper_bound(key_high);
    int n_c = 0;
    for (it = it_low; it != it_high; it++) {
        n_c++;
        if (mid_set) if (inc) {
            it_mid++;
            inc = false;
        }
        else
            inc = true;
        else {
            mid_set = true;
            it_mid = it;
        }
    }
    Log_debug("w_id: %d, d_id: %d, c_last: %s, num customer: %d",
            input[1].get_i32(), input[2].get_i32(),
            input[0].get_str().c_str(), n_c);
    verify(mid_set);
    i32 oi = 0;
    output[oi++] = Value(it_mid->second);

    *res = SUCCESS;
  } END_PIE

  BEGIN_CB(TPCC_ORDER_STATUS, 0)
    TpccChopper* tpcc_ch = (TpccChopper*)ch;
    verify(!tpcc_ch->order_status_dep_.piece_last2id);
    tpcc_ch->order_status_dep_.piece_last2id = true;
    tpcc_ch->inputs_[1][2] = output[0];
    tpcc_ch->status_[1] = READY;
    tpcc_ch->inputs_[2][2] = output[0];
    tpcc_ch->status_[2] = READY;

    return true;
  END_CB

  BEGIN_PIE(TPCC_ORDER_STATUS, // RO
          TPCC_ORDER_STATUS_1, // Ri customer
          DF_NO) {
    // #################################################################
    verify(dtxn != nullptr);
    Log::debug("TPCC_ORDER_STATUS, piece: %d", TPCC_ORDER_STATUS_1);
    verify(input.size() == 3);
    // #################################################################

    mdb::Table *tbl = dtxn->GetTable(TPCC_TB_CUSTOMER);
    // R customer
    Value buf;
    mdb::MultiBlob mb(3);
    //cell_locator_t cl(TPCC_TB_CUSTOMER, 3);
    mb[0] = input[2].get_blob();
    mb[1] = input[1].get_blob();
    mb[2] = input[0].get_blob();
    mdb::Row *r = dtxn->Query(tbl, mb, ROW_CUSTOMER);

    i32 oi = 0;
    dtxn->ReadColumn(r, 3, &output[oi++], TXN_BYPASS);// read c_first
    dtxn->ReadColumn(r, 4, &output[oi++], TXN_BYPASS);// read c_middle
    dtxn->ReadColumn(r, 5, &output[oi++], TXN_BYPASS);// read c_last
    dtxn->ReadColumn(r, 16, &output[oi++], TXN_SAFE, TXN_DEFERRED);// read c_balance

    *res = SUCCESS;
  } END_PIE

  BEGIN_PIE(TPCC_ORDER_STATUS, // RO
          TPCC_ORDER_STATUS_2, // Ri order
          DF_NO) {
    Log::debug("TPCC_ORDER_STATUS, piece: %d", TPCC_ORDER_STATUS_2);
    verify(input.size() == 3);

    mdb::MultiBlob mb_0(3);
    mb_0[0] = input[1].get_blob();
    mb_0[1] = input[0].get_blob();
    mb_0[2] = input[2].get_blob();
    mdb::Row *r_0 = dtxn->Query(dtxn->GetTable(TPCC_TB_ORDER_C_ID_SECONDARY),
                                mb_0,
                                ROW_ORDER_SEC);

    mdb::MultiBlob mb(3);
    mb[0] = input[1].get_blob();
    mb[1] = input[0].get_blob();
    mb[2] = r_0->get_blob(3); // FIXME add lock before reading

    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_ORDER),
                              mb,
                              ROW_ORDER);
    i32 oi = 0;
    dtxn->ReadColumn(r, 2, &output[oi++], TXN_BYPASS); // output[0] ==> o_id
    dtxn->ReadColumn(r, 4, &output[oi++], TXN_BYPASS); // output[1] ==> o_entry_d
    dtxn->ReadColumn(r, 5, &output[oi++], TXN_SAFE, TXN_DEFERRED); // output[2] ==> o_carrier_id
//        Log::debug("piece: %d, o_id: %d", TPCC_ORDER_STATUS_2, output[0].get_i32());
    *res = SUCCESS;
  } END_PIE

  BEGIN_CB(TPCC_ORDER_STATUS, 2)
    TpccChopper* tpcc_ch = (TpccChopper*)ch;
    verify(output.size() == 3);
    verify(!tpcc_ch->order_status_dep_.piece_order);
    tpcc_ch->order_status_dep_.piece_order = true;
    tpcc_ch->inputs_[3][2] = output[0];
    tpcc_ch->status_[3] = READY;
    return true;
  END_CB

  BEGIN_PIE(TPCC_ORDER_STATUS, // RO
          TPCC_ORDER_STATUS_3, // R order_line
          DF_NO) {
    Log::debug("TPCC_ORDER_STATUS, piece: %d", TPCC_ORDER_STATUS_3);
    verify(input.size() == 3);
    mdb::MultiBlob mbl(4), mbh(4);
    Log_debug("ol_d_id: %d, ol_w_id: %d, ol_o_id: %d",
            input[2].get_i32(), input[1].get_i32(), input[0].get_i32());
    mbl[0] = input[1].get_blob();
    mbh[0] = input[1].get_blob();
    mbl[1] = input[0].get_blob();
    mbh[1] = input[0].get_blob();
    mbl[2] = input[2].get_blob();
    mbh[2] = input[2].get_blob();
    Value ol_number_low(std::numeric_limits<i32>::min()),
            ol_number_high(std::numeric_limits<i32>::max());
    mbl[3] = ol_number_low.get_blob();
    mbh[3] = ol_number_high.get_blob();

    mdb::ResultSet rs = dtxn->QueryIn(dtxn->GetTable(TPCC_TB_ORDER_LINE),
                                      mbl,
                                      mbh,
                                      mdb::ORD_DESC,
                                      header.pid);
    mdb::Row *r = NULL;
    //cell_locator_t cl(TPCC_TB_ORDER_LINE, 4);
    //cl.primary_key[0] = input[2].get_blob();
    //cl.primary_key[1] = input[1].get_blob();
    //cl.primary_key[2] = input[0].get_blob();
    std::vector<mdb::Row *> row_list;
    row_list.reserve(15);
    while (rs.has_next()) {
      row_list.push_back(rs.next());
    }

    verify(row_list.size() != 0);

    std::vector<mdb::column_lock_t> column_locks;
    column_locks.reserve(5 * row_list.size());

    int i = 0;
    Log::debug("row_list size: %u", row_list.size());

    i = 0;
    i32 oi = 0;
    while (i < row_list.size()) {
      r = row_list[i++];
      dtxn->ReadColumn(r, 4, &output[oi++], TXN_BYPASS); // output[0] ==> ol_i_id
      dtxn->ReadColumn(r, 5, &output[oi++], TXN_BYPASS); // output[1] ==> ol_supply_w_id
      dtxn->ReadColumn(r, 6, &output[oi++], TXN_SAFE, TXN_DEFERRED); // output[2] ==> ol_delivery_d
      dtxn->ReadColumn(r, 7, &output[oi++], TXN_BYPASS); // output[3] ==> ol_quantity
      dtxn->ReadColumn(r, 8, &output[oi++], TXN_BYPASS); // output[4] ==> ol_amount
    }

    *res = SUCCESS;
  } END_PIE
}
} // namespace rococo
