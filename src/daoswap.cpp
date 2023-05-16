#include <daoswap.hpp>
#include <math.h>
#include <ctime>

#include "actions.cpp"
#include "pledges.cpp"
#include "mixup.cpp"

using namespace eosio;


void daoswap::on_transfer( const name& from, const name& to, const asset& quantity, const std::string& memo )
{
  // authenticate incoming `from` account
    require_auth( from );
    
    // ignore transfers
    if ( to != get_self() || from == ACCOUNT_RAM || from == ACCOUNT_ADMIN) return;//20220706-edit

    check( quantity.is_valid(), "600001:invalid quantity" );
    check( quantity.amount > 0, "600002:must issue positive quantity" );

    // tables
    config_table _config( get_self(), get_self().value );

    // config
    check( _config.exists(), "600101:contract is under maintenance" );
    const name status = _config.get().status;
    check( (status == STATUS_OK || status == STATUS_WITHDRAW ), "600102:contract is under maintenance");
    
    // user input params
    const auto parsed_memo = _parse_memo( memo );
    const extended_asset ext_in = { quantity, get_first_receiver() };

    if ( parsed_memo.action == ACTION_DEPOSIT ) 
    {
        _add_deposit( from, parsed_memo.pair_ids[0], ext_in );
    } 
    else if ( parsed_memo.action == ACTION_SWAP)
    {
        _convert( from, ext_in, parsed_memo.pair_ids, parsed_memo.min_return );

    }
    else if ( parsed_memo.action == ACTION_PLEDGE)
    {
        _add_pledge(from,ext_in,parsed_memo.min_return);
    }    
    else if ( parsed_memo.action == ACTION_SEND)
    {
        _add_send(from,ext_in,memo);
    }     
    else 
    {
        check( false, "600201:Market making or trading parameter error" );
    }
   
    //_notify();

      //  check(false,"ok32");
}


void daoswap::deposit( const name& owner, const symbol_code& pair_id, const std::optional<int64_t> min_amount )
{
    require_auth( owner );

    check( pair_id.is_valid(), "600406:invalid symbol name !" );

    uint64_t acttime = current_time_point().sec_since_epoch();
    config_table _config( get_self(), get_self().value );
    pairs_table _pairs( get_self(), get_self().value );
    orders_table _orders( get_self(), pair_id.raw() );

    // configs
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();

    // get current order & pairs
    auto & pair = _pairs.get( pair_id.raw(), "600301:pair_id does not exist");
    auto & orders = _orders.get( owner.value, "600203:no deposits available for this user");
    check( orders.quantity0.quantity.amount && orders.quantity1.quantity.amount, "600204:one of the deposit is empty");

    // symbol helpers
    const symbol sym0 = pair.reserve0.quantity.symbol;
    const symbol sym1 = pair.reserve1.quantity.symbol;
    const uint8_t precision_norm = max( sym0.precision(), sym1.precision() );

   
    // calculate total deposits based on reserves: reserves ratio should remain the same
    // if reserves empty, fallback to 1

    const int128_t reserve0_tmp = utils::mul_amount(pair.reserve0.quantity.amount, precision_norm, sym0.precision());
    const int128_t reserve1_tmp = utils::mul_amount(pair.reserve1.quantity.amount, precision_norm, sym1.precision());


    const int128_t reserve0 = pair.reserve0.quantity.amount ? reserve0_tmp : 1;
    const int128_t reserve1 = pair.reserve1.quantity.amount ? reserve1_tmp : 1;
    const int128_t reserves = sqrt(reserve0 * reserve1);

    //const extended_asset orders_quantity0 = orders.quantity0;
    //const extended_asset orders_quantity1 = orders.quantity1;

    // get owner order and calculate payment
    const int128_t amount0 = utils::mul_amount(orders.quantity0.quantity.amount, precision_norm, sym0.precision());
    const int128_t amount1 = utils::mul_amount(orders.quantity1.quantity.amount, precision_norm, sym1.precision());
    const int128_t payment = sqrt(amount0 * amount1);
  
    // calculate actual amounts to deposit

    int128_t deposit0_tmp = 0;//edit
    int128_t deposit1_tmp = 0;//edit

    if(reserve0_tmp == 0 && reserve1_tmp == 0)
    {
        deposit0_tmp = amount0;//edit
        deposit1_tmp = amount1; //edit    
    }
    else
    {
       deposit0_tmp = (amount0 * reserves <= reserve0 * payment) ? amount0 : (amount1 * reserve0 / reserve1);
       deposit1_tmp = (amount0 * reserves <= reserve0 * payment) ? (amount0 * reserve1 / reserve0) : amount1;
    }

    const int128_t deposit0 = deposit0_tmp;//edit
    const int128_t deposit1 = deposit1_tmp;//edit


   // check(false,std::to_string((uint64_t)deposit0)+"||"+std::to_string((uint64_t)deposit1)+"||"+std::to_string((uint64_t)amount0)+"||"+std::to_string((uint64_t)amount1)+"||"+std::to_string((uint64_t)reserve0)+"||"+std::to_string((uint64_t)reserve1)+"||"+std::to_string((uint64_t)reserves));

    // send back excess deposit to owner
    if (deposit0 < amount0) {
        const int64_t excess_amount = utils::div_amount(static_cast<int64_t>(amount0 - deposit0), precision_norm, sym0.precision());
        const extended_asset excess = { excess_amount, pair.reserve0.get_extended_symbol() };
        if (excess.quantity.amount) 
        {
            utils::transfer( get_self(), owner, excess, get_self().to_string() + ": excess");
           // sub_stocks(excess.contract,excess.quantity);
           // _tokenlog(pair_id,owner,ACTION_BACK,excess,0);  
           _sub_swap(excess.contract,excess.quantity);
        }
    }
    if (deposit1 < amount1) {
        const int64_t excess_amount = utils::div_amount(static_cast<int64_t>(amount1 - deposit1), precision_norm, sym1.precision());
        const extended_asset excess = { excess_amount, pair.reserve1.get_extended_symbol() };
        if (excess.quantity.amount)
        {
            utils::transfer( get_self(), owner, excess, get_self().to_string() + ": excess");
             //sub_stocks(excess.contract,excess.quantity);
            //_tokenlog(pair_id,owner,ACTION_BACK,excess,0); 
            _sub_swap(excess.contract,excess.quantity);
        }
    }

    // normalize final deposits
    const extended_asset ext_deposit0 = { utils::div_amount(deposit0, precision_norm, sym0.precision()), pair.reserve0.get_extended_symbol()};
    const extended_asset ext_deposit1 = { utils::div_amount(deposit1, precision_norm, sym1.precision()), pair.reserve1.get_extended_symbol()};


    // issue liquidity
    const int64_t supply = utils::mul_amount(pair.liquidity.quantity.amount, precision_norm, pair.liquidity.quantity.symbol.precision());
    const int64_t issued_amount = utils::issue(sqrt(deposit0 * deposit1), reserves, supply, 1);
    const extended_asset issued = { utils::div_amount(issued_amount, precision_norm, pair.liquidity.quantity.symbol.precision()), pair.liquidity.get_extended_symbol()};

    // add liquidity deposits & newly issued liquidity
    _pairs.modify(pair, get_self(), [&]( auto & row ) {
        row.reserve0 += ext_deposit0;
        row.reserve1 += ext_deposit1;
        row.liquidity += issued;
        if(reserve0==1&&reserve1==1&&reserves==1)
        {
          row.virtual_price = 0;
          row.price0_last = 0;
          row.price1_last = 0;
        }
        _liquiditylog(pair_id,owner,ACTION_DEPOSIT,issued,ext_deposit0,ext_deposit1,row.liquidity.quantity,row.reserve0.quantity, row.reserve1.quantity);
    });

    // issue & transfer to owner
    //issue( issued, get_self().to_string() + ": deposit" );
    _add_liquidity(pair_id, owner, issued.quantity, ext_deposit0.quantity,ext_deposit1.quantity);

    // deposit slippage protection
    if ( min_amount ) check( issued.quantity.amount >= *min_amount, "600206:deposit amount must exceed `min_amount`");

    // delete any remaining liquidity deposit order
    _orders.erase( orders );
    //check(false,"no");
}

void daoswap::_add_deposit( const name& owner, const symbol_code& pair_id, const extended_asset& value )
{

    pairs_table _pairs( get_self(), get_self().value );
    orders_table _orders( get_self(),pair_id.raw());
    
    // get current order & pairs
    auto pair = _pairs.get( pair_id.raw(), "600301:pair_id does not exist");
    auto itr = _orders.find( owner.value );

    const extended_symbol ext_sym_in = value.get_extended_symbol();
    const extended_symbol ext_sym0 = pair.reserve0.get_extended_symbol();
    const extended_symbol ext_sym1 = pair.reserve1.get_extended_symbol();


    // initialize quantities
    auto insert = [&]( auto & row ) {
        row.owner = owner;
        row.quantity0 = { itr == _orders.end() ? 0 : itr->quantity0.quantity.amount, ext_sym0 };
        row.quantity1 = { itr == _orders.end() ? 0 : itr->quantity1.quantity.amount, ext_sym1 };

        // add & validate deposit
        if ( ext_sym_in == ext_sym0 ) row.quantity0 += value;
        else if ( ext_sym_in == ext_sym1 ) row.quantity1 += value;
        else check( false,  "600401:invalid extended symbol");
    };

    // create/modify order
    if ( itr == _orders.end() ) _orders.emplace( get_self(), insert );
    else _orders.modify( itr, get_self(), insert );    

    _add_swap(value.contract,value.quantity);
    //_tokenlog(pair_id,owner,ACTION_ORDER,value,0);       
}


void daoswap::_add_liquidity(const symbol_code& pair_id, const name& owner, const asset& liquidity, const asset& quantity0,  const asset& quantity1)
{
    uint64_t acttime = current_time_point().sec_since_epoch();
    asset lock_liq = liquidity;
    lock_liq.amount = 0;

    markets_table  _markets(get_self(), pair_id.raw());
    auto markets_itr = _markets.find(owner.value);
    if(markets_itr != _markets.end())
    {
        _markets.modify(markets_itr, same_payer, [&](auto& p) {
            p.liquidity += liquidity;           
        });
    }
    else
    {
         _markets.emplace(get_self(), [&](auto& p) {
            p.owner = owner;
            p.liquidity = liquidity;
            p.lock_liq = lock_liq;
         });  
    }

}

void daoswap::_sub_liquidity(const symbol_code& pair_id, const name& owner, const asset& liquidity, const asset& quantity0,  const asset& quantity1)
{
    uint64_t acttime = current_time_point().sec_since_epoch();

    markets_table  _markets(get_self(), pair_id.raw());
    auto markets_itr = _markets.find(owner.value);
    check(markets_itr != _markets.end(), "600202:Market making information does not exist!" );   
    const asset liq_use = markets_itr->liquidity - markets_itr->lock_liq - liquidity;
    check(liquidity.amount > 0,"600408:Amount must be greater than zero");
    check(liq_use.amount >= 0,"600502:The number of retrieved vouchers exceeds the number of available vouchers");
    if(markets_itr != _markets.end())
    {
        if(markets_itr->liquidity.amount == liquidity.amount && markets_itr->lock_liq.amount == 0)
        {
            _markets.erase(markets_itr);
        }   
        else
        {
            _markets.modify(markets_itr, same_payer, [&](auto& p) {
                p.liquidity -= liquidity;           
            });
        }
    }
}

void daoswap::cancel( const name& owner, const symbol_code& pair_id )
{
    if ( !has_auth( get_self() )) require_auth( owner );

    check( pair_id.is_valid(), "600406:invalid symbol name !" );

    orders_table _orders( get_self(), pair_id.raw() );
    auto & orders = _orders.get( owner.value, "600203:no deposits available for this user");

    if ( orders.quantity0.quantity.amount ) 
    {
        utils::transfer( get_self(), owner, orders.quantity0, get_self().to_string() + ": cancel");
        //_tokenlog(pair_id,owner,ACTION_CANCEL,orders.quantity0,0);      
        _sub_swap(orders.quantity0.contract,orders.quantity0.quantity);  
    }

    if ( orders.quantity1.quantity.amount ) 
    {
        utils::transfer( get_self(), owner, orders.quantity1, get_self().to_string() + ": cancel");     
        //_tokenlog(pair_id,owner,ACTION_CANCEL,orders.quantity1,0);
        _sub_swap(orders.quantity1.contract,orders.quantity1.quantity);       
    }

    _orders.erase( orders );
}

void daoswap::_convert( const name& owner, const extended_asset& ext_in, const std::vector<symbol_code> pair_ids, const int64_t& min_return )
{
   
    _add_swap(ext_in.contract,ext_in.quantity);

    // execute the trade by updating all involved pools
    const extended_asset out = _apply_trade( owner, ext_in, pair_ids );

    // enforce minimum return (slippage protection)    
    check(out.quantity.amount != 0 && out.quantity.amount >= min_return, "600404:invalid minimum return");

    // transfer amount to owner
    utils::transfer( get_self(), owner, out, get_self().to_string() + ": swap token" );


    _sub_swap(out.contract,out.quantity);
}

extended_asset daoswap::_apply_trade( const name& owner, const extended_asset& ext_quantity, const std::vector<symbol_code> pair_ids )
{
    pairs_table _pairs( get_self(), get_self().value );
    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );
    auto config = _config.get();

    // initial quantities
    extended_asset ext_out;
    extended_asset ext_in = ext_quantity;

    // iterate over each liquidity pool per each `pair_id` provided in swap memo
    for ( const symbol_code pair_id : pair_ids ) {
        const auto& pairs = _pairs.get( pair_id.raw(), "600301:pair_id does not exist");
        const bool is_in = pairs.reserve0.quantity.symbol == ext_in.quantity.symbol;
        const extended_asset reserve_in = is_in ? pairs.reserve0 : pairs.reserve1;
        const extended_asset reserve_out = is_in ? pairs.reserve1 : pairs.reserve0;

        // validate input quantity & reserves
        check(reserve_in.get_extended_symbol() == ext_in.get_extended_symbol(), "600401:invalid extended symbol !");
        check(reserve_in.quantity.amount != 0 && reserve_out.quantity.amount != 0, "600205:empty pool reserves");

        // calculate out
        ext_out = { _get_amount_out( ext_in.quantity, pair_id ), reserve_out.contract };

        // send protocol fees to fee account
        const extended_asset protocol_fee = { ext_in.quantity.amount * config.protocol_fee / 10000, ext_in.get_extended_symbol() };
        const extended_asset trade_fee = { ext_in.quantity.amount * config.trade_fee / 10000, ext_in.get_extended_symbol() };
        const extended_asset fee = protocol_fee + trade_fee;

        // modify reserves
        _pairs.modify( pairs, get_self(), [&]( auto & row ) {
            const double price = _calculate_price( ext_out.quantity,ext_in.quantity );
            if ( is_in ) {
                row.reserve0.quantity += ext_in.quantity - protocol_fee.quantity;
                row.reserve1.quantity -= ext_out.quantity;
                row.volume0 += ext_in.quantity;
                row.price0_last = price;
            } else {
                row.reserve1.quantity += ext_in.quantity - protocol_fee.quantity;
                row.reserve0.quantity -= ext_out.quantity;
                row.volume1 += ext_in.quantity;
                row.price1_last = price;
            }
            // calculate last price

            //row.amplifier = _get_amplifier( pair_id );
            row.virtual_price = _calculate_virtual_price( row.reserve0.quantity, row.reserve1.quantity, row.liquidity.quantity );
            row.trades += 1;
            row.last_updated = current_time_point();

            _swaplog(pair_id,owner,ACTION_SWAP,ext_in,ext_out,protocol_fee.quantity,trade_fee.quantity,price,row.reserve0.quantity, row.reserve1.quantity);
        });

        // send protocol fees
        if ( protocol_fee.quantity.amount ) 
        {
            utils::transfer( get_self(),  config.fee_account, protocol_fee, get_self().to_string() + ": protocol  fee");
            //sub_stocks(protocol_fee.contract,protocol_fee.quantity);
            _sub_swap(protocol_fee.contract,protocol_fee.quantity);
        }


        // swap input as output to prepare for next conversion
        ext_in = ext_out;
    }

    return ext_out;
}

asset daoswap::_get_amount_out( const asset& in, const symbol_code& pair_id)
{
    config_table _config( get_self(), get_self().value );
    pairs_table _pairs( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );

    // get configs
    auto config = _config.get();
    auto pairs = _pairs.get( pair_id.raw(), "600301:pair_id does not exist" );

    // inverse reserves based on input quantity
    if (pairs.reserve0.quantity.symbol != in.symbol) std::swap(pairs.reserve0, pairs.reserve1);
    check( pairs.reserve0.quantity.symbol == in.symbol, "600401:invalid extended symbol !!!");

    // normalize inputs to max precision
    const uint8_t precision_in = pairs.reserve0.quantity.symbol.precision();
    const uint8_t precision_out = pairs.reserve1.quantity.symbol.precision();

    const int128_t amount_in = utils::mul_amount128( in.amount, MAX_PRECISION, precision_in );
    const int128_t reserve_in = utils::mul_amount128( pairs.reserve0.quantity.amount, MAX_PRECISION, precision_in );
    const int128_t reserve_out = utils::mul_amount128( pairs.reserve1.quantity.amount, MAX_PRECISION, precision_out );
    const uint64_t amplifier = _get_amplifier( pair_id );
    const int128_t protocol_fee = amount_in * config.protocol_fee / 10000;

    //    static int64_t div_amount128( const int128_t amount, const uint8_t precision0, const uint8_t precision1 )
    
    check(reserve_in >= amount_in,"600403:amount too large");    
    const int128_t reserve_in2 =  reserve_in >= MAX_NUMBER ? utils::div128(reserve_in,MIN_NUMBER):reserve_in;
    const int128_t reserve_in_c =   reserve_in >= MAX_NUMBER ? MIN_NUMBER:1;

    const int128_t amount_in_c =   reserve_in_c;
    const int128_t amount_in2 =  amount_in_c == MIN_NUMBER ? utils::div128((amount_in - protocol_fee),MIN_NUMBER):(amount_in - protocol_fee);

    const int128_t reserve_out2 =  reserve_out >= MAX_NUMBER ? utils::div128(reserve_out,MIN_NUMBER):reserve_out;
    const int128_t reserve_out_c =   reserve_out >= MAX_NUMBER ? MIN_NUMBER:1;   

    // enforce minimum fee
    if ( config.trade_fee ) check( in.amount * config.trade_fee / 10000, "600402:amount too small");

    // calculate out
   // const int64_t out = utils::div_amount( static_cast<int64_t>(utils::get_amount_out( amount_in - protocol_fee, reserve_in, reserve_out, config.trade_fee )), MAX_PRECISION, precision_out );
   //    static uint64_t get_amount_out( const uint64_t amount_in, const uint64_t reserve_in, const uint64_t reserve_out, const uint16_t fee = 30, const uint8_t precision0, const uint8_t precision1)
    const int64_t out = utils::get_amount_out(amount_in2, reserve_in2, reserve_out2, MAX_PRECISION, precision_out, config.trade_fee);
    const int64_t out2 = utils::get_amount_out2(out,amount_in_c, reserve_in_c, reserve_out_c);
    return { out2, pairs.reserve1.quantity.symbol };
}

void daoswap::withdraw(const name& owner, const asset& quantity)
{

    require_auth( owner );

    check( quantity.is_valid(), "600001:invalid quantity" );
    check( quantity.amount > 0, "600002:must issue positive quantity" );

    pairs_table _pairs( get_self(), get_self().value );

    // get current pairs
    const symbol_code pair_id = quantity.symbol.code();
    auto & pair = _pairs.get( pair_id.raw(), "600301:pair_id does not exist");

    // prevent invalid liquidity token contracts
    check(pair.liquidity.quantity.symbol == quantity.symbol, "600401: invalid extended symbol !"+std::to_string(pair.liquidity.quantity.amount)+"|"+std::to_string(quantity.amount));

    // extended symbols
    const extended_symbol ext_sym0 = pair.reserve0.get_extended_symbol();
    const extended_symbol ext_sym1 = pair.reserve1.get_extended_symbol();
    const symbol sym0 = pair.reserve0.quantity.symbol;
    const symbol sym1 = pair.reserve1.quantity.symbol;
    const uint8_t precision_norm = max( sym0.precision(), sym1.precision() );

    // calculate total deposits based on reserves
    const int128_t supply = utils::mul_amount128(pair.liquidity.quantity.amount, precision_norm, pair.liquidity.quantity.symbol.precision());
    const int128_t reserve0 = pair.reserve0.quantity.amount ? utils::mul_amount128(pair.reserve0.quantity.amount, precision_norm, sym0.precision()) : 1;
    const int128_t reserve1 = pair.reserve1.quantity.amount ? utils::mul_amount128(pair.reserve1.quantity.amount, precision_norm, sym1.precision()) : 1;
    const int128_t reserves = sqrt(reserve0 * reserve1);

    // calculate withdraw amounts
    const int128_t payment = utils::mul_amount128(quantity.amount, precision_norm, quantity.symbol.precision());
    const int128_t retire_amount = utils::retire( payment, reserves, supply );

    // get owner order and calculate payment
    int64_t amount0 = static_cast<int64_t>( retire_amount * reserve0 / reserves );
    int64_t amount1 = static_cast<int64_t>( retire_amount * reserve1 / reserves );
    if (amount0 == reserve0 || amount1 == reserve1) {         //deal with rounding error on final withdrawal
        amount0 = static_cast<int64_t>( reserve0 );
        amount1 = static_cast<int64_t>( reserve1 );
    }

    const extended_asset out0 = { utils::div_amount(amount0, precision_norm, sym0.precision()), ext_sym0 };    
    const extended_asset out1 = { utils::div_amount(amount1, precision_norm, sym1.precision()), ext_sym1 };
    //check(false,std::to_string(out0.quantity.amount) + "|"+ std::to_string(out1.quantity.amount)+ "|"+ std::to_string(value.quantity.amount));
    check( out0.quantity.amount || out1.quantity.amount, "600402:amount too small");

    //check(false,std::to_string(out0.quantity.amount)+"|"+std::to_string(out1.quantity.amount)+"|"+std::to_string(retire_amount));

    // add liquidity deposits & newly issued liquidity
    _pairs.modify(pair, get_self(), [&]( auto & row ) {
        row.reserve0 -= out0;
        row.reserve1 -= out1;
        row.liquidity.quantity -= quantity;

        // log liquidity change 
        const extended_asset ext_out = { quantity, row.liquidity.contract };       
        _liquiditylog(pair_id,owner,ACTION_WITHDRAW,ext_out,out0,out1,row.liquidity.quantity,row.reserve0.quantity, row.reserve1.quantity);
    });

    _sub_liquidity(pair_id,owner,quantity,out0.quantity,out1.quantity);

    // issue & transfer to owner
    if ( out0.quantity.amount )
    {
        utils::transfer( get_self(), owner, out0, get_self().to_string() + ": withdraw");
        _sub_swap(out0.contract,out0.quantity);
    } 
    if ( out1.quantity.amount )
    {
         utils::transfer( get_self(), owner, out1, get_self().to_string() + ": withdraw");
        _sub_swap(out1.contract,out1.quantity);
    }
}


void daoswap::createpair( const name& creator,const name& contract0, const symbol_code& code0,const name& contract1, const symbol_code& code1)
{
    require_auth( creator );

    // tables
    pairs_table _pairs( get_self(), get_self().value );
    config_table _config( get_self(), get_self().value );
    check( _config.exists(), "600101:contract is under maintenance" );

    auto config = _config.get();

    uint64_t pairid = utils::get_pairid_from_lptoken(config.pair_id);
    symbol_code pair_id = utils:: get_lptoken_from_pairid(pairid + 1);
    config.pair_id = pair_id;
    _config.set( config, get_self() );

    // reserve params
    //const name contract0 = reserve0.get_contract();
    //const name contract1 = reserve1.get_contract();
    //const symbol sym0 = reserve0.get_symbol();
    //const symbol sym1 = reserve1.get_symbol();

    const uint64_t amplifier = 100;

    asset supply0 = utils::get_supply( contract0, code0 );
    asset supply1 = utils::get_supply( contract1, code1 );
    const symbol sym0 =  supply0.symbol;
    const symbol sym1 =  supply1.symbol;

    check( sym0.is_valid(), "600406:invalid symbol name !" );
	check( sym1.is_valid(), "600406:invalid symbol name !" );

    // check reserves
    check( is_account( contract0 ), "600207:Market making information is incorrect");
    check( is_account( contract1 ), "600207:Market making information is incorrect");
    check( sym0.code() == code0, "600401:invalid extended symbol 1" );
    check( sym1.code() == code1, "600401:invalid extended symbol 2" );
    check( _pairs.find( pair_id.raw() ) == _pairs.end(), "600302:invalid duplicate pair_ids" );
    //check( amplifier > 0 && amplifier <= MAX_AMPLIFIER, "000000:" );
    check( sym0.precision()<= MAX_PRECISION,"600405:Token accuracy is too large");
    check( sym1.precision()<= MAX_PRECISION,"600405:Token accuracy is too large");


    for(auto& item : _pairs) 
    {  
         if(item.reserve0.get_extended_symbol().get_symbol()==sym0&&item.reserve0.contract==contract0)
         {
             if(item.reserve1.get_extended_symbol().get_symbol()==sym1&&item.reserve1.contract==contract1)
             {
                  check(false,"600302:invalid duplicate pair_ids" );
             }
         }

        if(item.reserve0.get_extended_symbol().get_symbol()==sym1&&item.reserve0.contract==contract1)
         {
             if(item.reserve1.get_extended_symbol().get_symbol()==sym0&&item.reserve1.contract==contract0)
             {
                  check(false,"600302:invalid duplicate pair_ids" );
             }
         }        
    }  


    // create liquidity token
    const extended_symbol liquidity = {{ pair_id, max(sym0.precision(), sym1.precision())}, get_self() };
    const extended_asset reserve0 = { {0,supply0.symbol}, contract0};
    const extended_asset reserve1 = { {0,supply1.symbol}, contract1 };    

    // create pair
    _pairs.emplace( creator, [&]( auto & row ) {
        row.id = pair_id;
        row.reserve0 = reserve0;
        row.reserve1 = reserve1;
        row.liquidity = { 0, liquidity };
        row.lock_liq =  { 0, liquidity };
        row.amplifier = amplifier;
        row.volume0 = { 0, sym0 };
        row.volume1 = { 0, sym1 };
        row.last_updated = current_time_point();
    });

    _pairidlog(pair_id,creator,ACTION_CREATE);
}

void daoswap::removepair(const name& mgr, const symbol_code& pair_id )
{
    _verify_mgr(mgr);

    check( pair_id.is_valid(), "600406:invalid symbol name !" );

    pairs_table _pairs( get_self(), get_self().value );
    auto & pair = _pairs.get( pair_id.raw(), "600301:pair_id does not exist");
    check( pair.liquidity.quantity.amount == 0, "600208:liquidity amount must be empty");
    _pairs.erase( pair );

    _pairidlog(pair_id,mgr,ACTION_DELETE);
}


void daoswap::lockliq(const name& owner,const asset& quantity,const uint64_t& exptime)
{
    require_auth(owner);

    check( quantity.is_valid(), "600001:invalid quantity" );
    check( quantity.amount > 0, "600002:must issue positive quantity" );

    uint64_t acttime = current_time_point().sec_since_epoch();
    check(exptime >= (acttime + 30 * MIN_RAMP_TIME),"000000:The locking time needs to be greater than 30 days");
    symbol_code pair_id = quantity.symbol.code();


    pairs_table _pairs( get_self(), get_self().value );
    auto pairs_itr = _pairs.find(pair_id.raw());
    check(pairs_itr != _pairs.end(),"600301:pair_id does not exist");
    const asset liq_use = pairs_itr->liquidity.quantity - pairs_itr->lock_liq.quantity;
    check(liq_use.amount >= quantity.amount,"600823:The pledged amount exceeds the total amount 0");
    if(pairs_itr != _pairs.end())
    {
        _pairs.modify(pairs_itr, same_payer, [&]( auto & row ) {
            row.lock_liq.quantity += quantity;
            check(row.lock_liq.quantity.amount <=row.liquidity.quantity.amount,"600823:The pledged amount exceeds the total amount 1");          
        });        
    }  

    markets_table  _markets(get_self(), pair_id.raw());
    auto markets_itr = _markets.find(owner.value);
    check(markets_itr!=_markets.end(),"600202:Market making information does not exist!");   
    if(markets_itr != _markets.end())
    {
        _markets.modify(markets_itr, same_payer, [&](auto& p) {
            p.lock_liq += quantity; 

            check(p.lock_liq.amount <= p.liquidity.amount,"600823:The pledged amount exceeds the total amount 2");
        });        
    }    

    lockdtl_table  _lockdtl(get_self(), owner.value);
    _lockdtl.emplace(get_self(), [&](auto& p) 
    {
        p.id = max(1, _lockdtl.available_primary_key());
        p.pair_id = pair_id;
        p.quantity = quantity;
        p.acttime = acttime;
        p.exptime = exptime;                  
    });

    const extended_asset ext_quantity = { quantity, get_self() };
    _tokenlog(pair_id,owner,ACTION_LOCK,ext_quantity,exptime);    

}

void daoswap::unlockliq(const name& owner,const asset& quantity)
{
    require_auth(owner);

    check( quantity.is_valid(), "600001:invalid quantity" );
    check( quantity.amount > 0, "600002:must issue positive quantity" );
    check( quantity.symbol.is_valid(), "600406:invalid symbol name !" );

    symbol_code pair_id = quantity.symbol.code();

    uint64_t acttime = current_time_point().sec_since_epoch();
    asset quantity0 = quantity;
    asset quantity1 = quantity;
    quantity0.amount = 0;
    quantity1.amount = 0;


    pairs_table _pairs( get_self(), get_self().value );
    auto pairs_itr = _pairs.find(pair_id.raw());
    check(pairs_itr != _pairs.end(),"600301:pair_id does not exist");
    check(quantity.amount <= pairs_itr->lock_liq.quantity.amount,"600824:The total release amount cannot be less than zero 0");
    if(pairs_itr != _pairs.end())
    {
        _pairs.modify(pairs_itr, same_payer, [&]( auto & row ) {
            row.lock_liq.quantity -= quantity;
            
            check(row.lock_liq.quantity.amount >= 0,"600824:The total release amount cannot be less than zero 1");          
        });        
    }  

    lockdtl_table  _lockdtl(get_self(), owner.value); 
    std::vector<uint64_t> keysForDeletion;  
    for(auto& item : _lockdtl) 
    {  
        if(pair_id == item.pair_id && item.exptime <= acttime)
        {
            quantity0 += item.quantity;
            keysForDeletion.push_back(item.id);  
            if(quantity0.amount >= quantity.amount)
            {
               break;
            }
        }
    }  

    check(quantity0.amount >= quantity.amount,"600501:The number of unlocks exceeds the total number");
    for (uint64_t key : keysForDeletion) 
    {  
         auto itr = _lockdtl.find(key);  
         if (itr != _lockdtl.end()) 
         {  
            quantity1 = itr->quantity + quantity1 ;
            if(quantity1 <= quantity)
            {
               _lockdtl.erase(itr);  
            }
            else
            {
                _lockdtl.modify(itr, same_payer, [&](auto& p) {
                    p.quantity = (quantity1 - quantity);           
                });            
            }
         }  
    }

    markets_table  _markets(get_self(), pair_id.raw());
    auto markets_itr = _markets.find(owner.value);
    check(markets_itr!=_markets.end(),"600202:Market making information does not exist!");
    check(markets_itr->lock_liq.amount >= quantity.amount,"600501:The number of unlocks exceeds the total number"); 
    if(markets_itr != _markets.end())
    {
        _markets.modify(markets_itr, same_payer, [&](auto& p) {
            p.lock_liq -= quantity;      

            check(p.lock_liq.amount >= 0,"600824:The total release amount cannot be less than zero 2");  
        });
    }

    const extended_asset ext_quantity = { quantity, get_self() };
    _tokenlog(pair_id,owner,ACTION_UNLOCK,ext_quantity,0);    
}
