void daoswap::pledgeparam(const name& mgr,const uint64_t& pledgeid,const name& contract, const symbol_code& sym,const uint64_t& annualized,const uint64_t& duration)
{
    _verify_mgr(mgr);

    pledgepara_table  _pledgepara(get_self(), get_self().value);
    auto pledgepara_itr = _pledgepara.find(pledgeid);
    if (pledgepara_itr == _pledgepara.end()) 
    {
        _pledgepara.emplace(get_self(), [&](auto& r) 
        {
            r.pledgeid = pledgeid;
            r.contract = contract;
            r.sym = sym;
            r.annualized = annualized;
            r.duration = duration;  
            r.status = 1;                       
        });
    } 
    else 
    {
        _pledgepara.modify(pledgepara_itr, same_payer, [&](auto& r) {
            r.contract = contract;
            r.sym = sym;
            r.annualized = annualized;
            r.duration = duration;  
        });   
    }
}


void daoswap::pledgecond(const name& mgr,const uint64_t& pledgeid,const asset& min_amount,const asset& max_amount, const uint8_t& oracle,const uint64_t& oracleParam)
{
    _verify_mgr(mgr);

    pledgepara_table  _pledgepara(get_self(), get_self().value);
    auto pledgepara_itr = _pledgepara.find(pledgeid);
    if (pledgepara_itr != _pledgepara.end()) 
    {
        _pledgepara.modify(pledgepara_itr, same_payer, [&](auto& r) {
            r.min_amount = min_amount;
            r.max_amount = max_amount;
            r.oracle = oracle;
            r.oracleParam = oracleParam;  
        });  
    } 
}


void daoswap::pledgestatus(const name& mgr,const uint64_t& pledgeid, const uint8_t& status)
{
    _verify_mgr(mgr);
    
    pledgepara_table  _pledgepara(get_self(), get_self().value);
    auto pledgepara_itr = _pledgepara.find(pledgeid);
    if (pledgepara_itr != _pledgepara.end()) 
    {
        if(status == 4 )
        {
            _pledgepara.erase(pledgepara_itr);
        }
        else
        {
            _pledgepara.modify(pledgepara_itr, same_payer, [&](auto& r) {
                r.status = status;
            });
        }
    } 
}


void daoswap::_add_pledge(const name& owner,const extended_asset& ext_quantity,const uint64_t& pledgeid)
{
    uint64_t acttime = current_time_point().sec_since_epoch();

    check( ext_quantity.quantity.is_valid(), "600001:invalid quantity" );
    check( ext_quantity.quantity.amount > 0, "600002:must issue positive quantity" );
        
    pledgepara_table  _pledgepara(get_self(), get_self().value);
    auto pledgepara_itr = _pledgepara.find(pledgeid);
    check(pledgepara_itr != _pledgepara.end(),"600901:There is no pledge record for the current batch");
    if (pledgepara_itr != _pledgepara.end()) 
    {
         const asset min_quantity = pledgepara_itr->min_amount;
         const name contract = pledgepara_itr->contract;
         const symbol_code sym = pledgepara_itr->sym;
         const uint64_t duration = pledgepara_itr->duration;
         const uint64_t expired_time = acttime + duration;
         check(pledgepara_itr->status == 1,"600832:Staking for the current batch has stopped");
         check(ext_quantity.quantity.amount >= min_quantity.amount ,"600402:amount too small");

         const symbol_code pair_id = ext_quantity.quantity.symbol.code();
         check(pair_id ==  sym,"600401:invalid extended symbol");
         check(contract ==  ext_quantity.contract,"600409:The contract account does not exist");    

         pledgelist_table  _pledgelist(get_self(),owner.value);
         _pledgelist.emplace(get_self(), [&](auto& p) {
                p.cdsid =  max(1, _pledgelist.available_primary_key());
                p.pledgeid = pledgeid;
                p.liquidity = ext_quantity;
                p.receive_time = acttime;   
                p.expired_time = expired_time; 

             _pledgeslog(pair_id,owner,ACTION_PLEDGE,pledgeid,ext_quantity,acttime,expired_time,p.cdsid);
        }); 
        
        _add_bank(ext_quantity.contract,ext_quantity.quantity);
    } 
}

void daoswap::unpledge(const name& owner,const uint64_t& cdsid)
{
    require_auth( owner );

    uint64_t acttime = current_time_point().sec_since_epoch();
    pledgelist_table  _pledgelist(get_self(),owner.value);
    auto pledgelist_itr = _pledgelist.find(cdsid);
    check(pledgelist_itr!=_pledgelist.end(),"600811:Pledge record does not exist");    
    const uint64_t expired_time = pledgelist_itr->expired_time;   
    const extended_asset liquidity = pledgelist_itr->liquidity;  
    const symbol_code pair_id = liquidity.quantity.symbol.code();

    check(acttime >= expired_time ,"600821:Release time has not been reached");
    if(pledgelist_itr != _pledgelist.end())
    {
         //const uint64_t cdsid = pledgelist_itr->cdsid;
         const uint64_t pledgeid = pledgelist_itr->pledgeid;
         const uint64_t receive_time =  pledgelist_itr->receive_time;
         const uint64_t expired_time = pledgelist_itr->expired_time;

         _pledgelist.erase(pledgelist_itr);
         _pledgeslog(pair_id,owner,ACTION_UNPLEDGE,pledgeid,liquidity,receive_time,expired_time,cdsid);
         utils::transfer( get_self(), owner, liquidity, get_self().to_string() + ": unpledge");

         _sub_bank(liquidity.contract,liquidity.quantity);
    }
}

void daoswap::_add_bank(const name& scope,const asset& value)
{
    bank_table  _bank(get_self(),scope.value);
    auto bank_itr = _bank.find(value.symbol.code().raw());
    if(bank_itr != _bank.end())
    {
        _bank.modify( bank_itr, same_payer, [&]( auto& r ) {
            r.balance += value;
        });
    }
    else
    {
        _bank.emplace( get_self(), [&]( auto& r ){
            r.balance = value;
        });
    }
}

void daoswap::_sub_bank(const name& scope,const asset& value ) 
{
    int64_t amount = 0;
    bank_table  _bank(get_self(),scope.value);
    auto bank_itr = _bank.find(value.symbol.code().raw());
    check(bank_itr != _bank.end(),"100203:Insufficient transfer amount!");
    if(bank_itr != _bank.end())
    {
        _bank.modify( bank_itr, same_payer, [&]( auto& r ) {
            r.balance -= value; 
            amount = r.balance.amount;           
        });
    }
    check(amount >=0,"100202:The quantity cannot be zero !!!");
}

void daoswap::_add_swap(const name& scope,const asset& value)
{
    swap_table  _swap(get_self(),scope.value);
    auto swap_itr = _swap.find(value.symbol.code().raw());
    if(swap_itr != _swap.end())
    {
        _swap.modify( swap_itr, same_payer, [&]( auto& r ) {
            r.balance += value;
        });
    }
    else
    {
        _swap.emplace( get_self(), [&]( auto& r ){
            r.balance = value;
        });
    }
}

void daoswap::_sub_swap(const name& scope,const asset& value ) 
{
    int64_t amount = 0;
    swap_table  _swap(get_self(),scope.value);
    auto swap_itr = _swap.find(value.symbol.code().raw());
    check(swap_itr != _swap.end(),"100203:Insufficient transfer amount!");
    if(swap_itr != _swap.end())
    {
        _swap.modify( swap_itr, same_payer, [&]( auto& r ) {
            r.balance -= value; 
            amount = r.balance.amount;           
        });
    }
    check(amount >=0,"100202:The quantity cannot be zero !!!");
}


